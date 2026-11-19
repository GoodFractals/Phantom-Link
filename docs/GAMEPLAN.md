# Phantom Link — build-out gameplan

**Goal:** two untethered, battery-powered Pico 2 W units that find each other, hold a session, and exchange encrypted messages over the **Bonnie & Clyde** protocol, which layers a StegoFrame-order-inspired (SFO) side channel on top of a reliable session. The UI is finished last and never blocks protocol work.

This document is the roadmap. `README.md` describes what exists today; this file describes what to build and in what order.

---

## 1. Definition of done

The project is finished when all of these are true on hardware, not in simulation:

1. Both units boot from battery with no USB attached and show their UI within 3 s.
2. Cold boot to `LINK UP` in under 5 s.
3. A 21-character message round-trips exactly, in both directions, encrypted.
4. A modified ciphertext, a replayed frame, and a wrong key are all rejected and logged.
5. The SFO channel reports a measured decode rate over at least 100 bursts.
6. Either unit can be power-cycled and the session recovers without rebooting the other.
7. A 24-hour soak run produces no crash, no watchdog reset, and no wedged transmit.
8. Battery runtime is measured and documented.
9. A fresh reader can build, flash, and run both units from the README alone.

---

## 2. Baseline that must not regress

These are proven on hardware as of `d812b7f` and are the acceptance gate for every phase:

- One `main.c`, two targets, `IS_AP` selects only the Wi-Fi role.
- Both boards run the full OLED/joystick/button UI.
- Static addressing: AP `192.168.4.1`, STA `192.168.4.2`.
- `HELLO` / `HELLO_ACK` handshake, 1 s heartbeat, 3 s timeout.
- 49-glyph alphabet, 3-digit Base-6 transport, 21-character messages.
- Non-blocking TX; the UI stays alive while sending.
- GP12 BACK mirrors DELETE.

**Rule:** every phase ends with the previous phase still working. If a change breaks a baseline item, it is reverted, not "fixed later".

---

## 3. Architecture

Five layers, each with one job. The important property is that the app, the crypto, and the covert channel fail independently.

```text
+---------------------------------------------------+
|  UI / input  (art, joystick, buttons, HUD)        |  never touches bytes on the wire
+---------------------------------------------------+
|  app: message buffer, charset codec, TX cadence   |  knows only "send this text"
+---------------------------------------------------+
|  Bonnie & Clyde session                           |  framing, seq, ACK, replay, crypto
+---------------------------------------------------+
|  SFO channel (optional, parallel)                 |  ordered-packet side channel
+---------------------------------------------------+
|  UDP + lwIP + CYW43                               |  transport, no protocol meaning
+---------------------------------------------------+
```

Module map (new files under `firmware/`):

| File | Owns |
|---|---|
| `proto_frame.h/.c` | header encode/decode, CRC, bounds checks, fuzz target |
| `proto_session.h/.c` | sequence window, ACK, retransmit, duplicate suppression, state machine |
| `proto_crypto.h/.c` | key schedule, cipher, MAC, key confirmation, test vectors |
| `proto_sfo.h/.c` | burst encode/decode, permutation codec, telemetry counters |
| `net_link.c` | lwIP glue: socket, peer address, packet dispatch |
| `ui_layout.h` | named rectangles; the only file the art pass may change |
| `main.c` | wiring, timers, mode selection — stays thin |

The current `main.c` is a single file. Split it incrementally: move one module at a time, rebuild, reflash, keep the baseline green. Do not do a big-bang refactor.

---

## 4. Bonnie & Clyde protocol v1

### 4.1 Roles

- **Bonnie** = STA, the initiator and courier. Starts sessions, drives SFO bursts.
- **Clyde** = AP, the host and lookout. Accepts sessions, echoes ACKs.
- Naming only; the code keeps `is_ap` as the sole role switch.

### 4.2 Frame

```text
off  size  field
 0    2    magic 'B','C'
 2    1    version (1)
 3    1    type
 4    1    flags        bit0 ACK_REQ, bit1 ENCRYPTED, bit2 FINAL, bit3 SFO_SYNC
 5    1    payload_len  (0..64)
 6    4    session_id
10    4    seq          per-session, per-direction, starts at 0
14    4    ack          highest contiguous seq received
18    n    payload
18+n  8    mac          truncated CMAC over bytes 0..18+n
```

Overhead is 26 bytes per datagram. That is acceptable at 63 datagrams per 21-character message (~1.9 KB on the wire, ~5 s of paced symbols). A later optimisation can pack one character (3 symbols) per datagram and cut overhead threefold; do not do it before framing is stable.

Types: `HELLO=1`, `HELLO_ACK=2`, `DATA=3`, `ACK=4`, `PING=5`, `PONG=6`, `BYE=7`, `SFO=8`, `ERROR=9`.

### 4.3 State machine

```text
IDLE --HELLO--> CONFIRMING --HELLO_ACK(valid)--> ESTABLISHED
ESTABLISHED --timeout--> IDLE (bump retry counter, re-HELLO with backoff)
ESTABLISHED --BYE--> IDLE
any --mac/seq failure--> drop frame, increment counters, never change state
```

Rules:

- A frame that fails length, magic, version, MAC, or replay checks is dropped **before** it reaches the app. No partial messages, ever.
- `HELLO` is the only frame accepted in `IDLE`. Everything else is dropped there.
- Session IDs come from the RP2350 hardware RNG (`pico/rand.h`), never from a counter.
- Rekey is out of scope for v1; note it as a limitation.

### 4.4 Crypto envelope

Cipher choice is a decision gate, see §12. The envelope shape is fixed either way:

```text
K_master  = 32-byte PSK (build-time constant, -DPHANTOM_PSK=... override)
salt      = session_id || "BONNIE-CLYDE-v1"
K_send    = KDF(K_master, salt || 0x01 || direction)
K_recv    = KDF(K_master, salt || 0x02 || direction)
nonce     = direction(1) || seq(4) || 0x000000   (96-bit, never reused)
```

- Encrypt-then-MAC. Verify the MAC before decrypting.
- Key confirmation: `HELLO` carries an 8-byte client nonce plus `CMAC(K_master, "KC"||client_nonce||session_id)[0..7]`; `HELLO_ACK` carries a server nonce plus a tag over both. A peer that cannot produce a valid tag never reaches `ESTABLISHED`.
- Replay window: 32-bit sliding bitmap per direction. Reject `seq <= max-32` and any already-set bit.
- Fail closed. A crypto error is logged, counted, and dropped; it never falls back to plaintext.
- Document honestly: the PSK lives in flash, so physical access defeats it. That is a stated limitation, not a bug.

### 4.5 Reliability

- Sender sets `ACK_REQ` on `FINAL` and every 8th frame.
- Receiver ACKs `highest contiguous seq`.
- On timeout, the sender retransmits **the whole message** under the same message ID. At 63 frames this is simpler than selective repeat and fast enough for a PoC.
- Receiver drops duplicates by `(session_id, seq)` before the app sees them. This closes the current "aborted TX leaves a partial message" gap.
- Message assembly is bounded: max 21 characters, max 3 frames out of order, hard cap on buffer growth.

---

## 5. SFO channel: what it is, and what it is not

**Not possible here:** the paper's channel reorders 802.11 MAC frames and needs monitor mode plus raw frame injection. `cyw43` exposes a normal STA/AP interface; it does not give raw 802.11 frames. Claiming otherwise would be false.

**What we build:** an application-layer ordered-packet channel. The transmitter sends `N=4` small UDP datagrams back to back; the *order in which they arrive* encodes 4 bits. With 24 permutations, 16 are used as codes and 8 are discarded as invalid.

Burst packet:

```text
"SFO" | burst_id(1) | index(1) | nibble(1) | crc8(1)
```

- Spacing 10 ms, burst repeated 3× for majority decode.
- The nibble mirrors the low 4 bits of the current message MAC. That makes the channel *verifiable*: if the order decodes to the same nibble, the burst is counted good.
- The app never depends on it. If SFO fails, the message still lands, and the log says `SFO degraded`.
- Telemetry: `SFO ok/good bursts`, `bits/s`, `reorder rate`, exposed on serial and (later) a debug screen.

Measured honestly, this is a **covert-channel demo**, not a confidentiality mechanism. The plan treats it as a research feature with numbers attached, which is exactly what the paper's premise supports.

Optional phase 3b, only if 3a is stable: use the nibble as a key-confirmation hint during `HELLO`, behind `-DSFO_KEY_HINT=1`. Off by default.

---

## 6. Phases and steps

Each step has a definition of done (DoD). Steps are small on purpose. A phase is not finished until every step's DoD is met on hardware and the §2 baseline still passes.

### Phase 0 — Protect the working PoC

| # | Step | DoD |
|---|---|---|
| 0.1 | Tag the current state `poc/plaintext-v1` | Tag exists; both UF2s archived with hashes |
| 0.2 | Write the regression checklist from §2 into `docs/` | Checklist fits on one screen, 12 items max |
| 0.3 | Add a `host` CMake target that compiles protocol modules without the SDK | `ctest` runs on Windows with no board attached |
| 0.4 | Add `-Wall -Wextra -Werror` to the firmware targets | Clean build; no warnings |
| 0.5 | Add a boot banner with protocol version and build hash | Serial shows version + short hash on every boot |

### Phase 1 — Framing and reliability, still plaintext

Purpose: make the link structurally correct while the bytes are still readable in a serial log. Debugging crypto and framing at the same time is how projects die.

| # | Step | DoD |
|---|---|---|
| 1.1 | Implement `proto_frame` encode/decode with CRC32 | Host tests: round-trip, every truncation rejected, fuzz 100k random buffers without a crash |
| 1.2 | Replace raw tokens with typed frames; keep token semantics | Handshake and messages still work on both boards |
| 1.3 | Add `session_id`, `seq`, duplicate suppression | Replaying a captured datagram in a test harness is dropped and logged |
| 1.4 | Add ACK + whole-message retransmit | Induce 10% packet loss on a host harness; messages still arrive exactly once |
| 1.5 | Add `BYE` and clean teardown | Power-cycling the STA logs a clean teardown on the AP, not a 3 s silence |
| 1.6 | Replace the manual TX state machine with a frame queue | DELETE mid-send aborts and leaves no partial message in the receiver |
| 1.7 | Flash both boards, rerun T2–T12 | All baseline tests green with framing on |

Exit gate: the link is reliable and observable, and `main.c` no longer parses raw bytes.

### Phase 2 — Encrypted envelope

| # | Step | DoD |
|---|---|---|
| 2.1 | Decide cipher (§12) | Decision recorded in `docs/DECISIONS.md` with rationale |
| 2.2 | Implement cipher + KDF + MAC with published test vectors | Vector tests pass on host and on-device self-test |
| 2.3 | Add key confirmation to `HELLO`/`HELLO_ACK` | A peer with the wrong PSK reaches `AUTH FAILED`, never `LINK UP` |
| 2.4 | Encrypt `DATA`, verify MAC before decrypt | Serial logs show ciphertext length, never plaintext of user messages |
| 2.5 | Add the 32-bit replay window | Replay harness: 0 accepted replays out of 10k |
| 2.6 | Add tamper tests | One flipped bit anywhere in a frame is rejected 100% of the time |
| 2.7 | Add a `-DPHANTOM_PSK` override and document key handling | Two builds with different PSKs cannot link |

Exit gate: an eavesdropper with packet capture sees no readable text, and the two boards still pass T2–T12.

### Phase 3 — SFO channel

| # | Step | DoD |
|---|---|---|
| 3.1 | Implement permutation codec (24 → 16 codes) | Host test: every permutation maps deterministically; invalid ones rejected |
| 3.2 | Implement burst TX/RX with CRC8 and 3× repeat | Loopback harness decodes 100% on an in-order queue |
| 3.3 | Add telemetry counters and a serial report | `SFO good/total`, bits/s, reorder rate printed every 30 s |
| 3.4 | Run 100 bursts on hardware at 1 m, 5 m, 15 m | Table of decode rates recorded in docs |
| 3.5 | Add degraded-mode handling | Forcing SFO off changes nothing about message delivery |
| 3.6 (optional) | Wire the nibble into key confirmation behind a flag | Flag off by default; documented as experimental |

Exit gate: measured numbers exist, and the app provably does not depend on SFO.

### Phase 4 — Session robustness

| # | Step | DoD |
|---|---|---|
| 4.1 | Reconnect with exponential backoff | AP restart re-establishes in <10 s without touching the STA |
| 4.2 | Handle AP-side session replacement when the STA reboots | Old session is torn down, new one accepted, no duplicate state |
| 4.3 | Add a hardware watchdog with a supervised main loop | Forced hang resets in <8 s and recovers to `LINK UP` |
| 4.4 | Add error counters and a `STATUS` serial dump | One command prints link, seq window, crypto errors, SFO stats |
| 4.5 | 24-hour soak with a message every 5 minutes | Zero resets, zero wedged sends, bounded memory |
| 4.6 | Brownout/reset behaviour | Pulling power mid-TX leaves both sides recoverable |

Exit gate: the link survives a day of abuse without human intervention.

### Phase 5 — Untethered hardware (parallel track, start early)

| # | Step | DoD |
|---|---|---|
| 5.1 | Choose power path: LiPo → VSYS with a power-path charger | Schematic recorded; no back-feed to USB |
| 5.2 | Add `GP28/ADC2` battery divider (2:1, high value) | Multimeter voltage matches ADC reading within 2% |
| 5.3 | Implement battery sampling + 4-bucket mapping | `BAT:` cell shows real charge; USB vs battery detectable |
| 5.4 | Measure current in idle, RX, TX, and with radio PM on/off | Table in docs: mA per state, projected runtime |
| 5.5 | Choose PM policy from the measurements | Runtime target met without breaking the 3 s link timeout |
| 5.6 | Add power switch and bulk capacitor | No brownout during TX bursts |
| 5.7 | Enclosure: OLED window, joystick cap, 3 buttons, USB cutout | Both units assembled and hand-held |
| 5.8 | Untethered acceptance run | §1 items 1, 2, 8 pass on battery |

### Phase 6 — UI finish (last, by design)

Nothing in this phase may touch `proto_*`, `net_link.c`, or timing constants. If a UI change needs protocol data, the data is exposed through a read-only accessor first.

| # | Step | DoD |
|---|---|---|
| 6.1 | Extract `ui_layout.h` with named rects for text rows, signal, battery, link | Drawing code has zero magic coordinates |
| 6.2 | Redraw the numeric page: drop `IN RANGE?` and `SOS`, keep digit grid | Art matches layout rects; dead handlers deleted |
| 6.3 | Draw battery segments from the Phase 5 reading | Buckets match multimeter within one segment |
| 6.4 | Link wording: `LINK` / `NO LINK`, plus a lock glyph only when crypto is active | No `SEC` claim unless the session is actually authenticated |
| 6.5 | Optional alpha-page tidy: consistent label weight, punctuation placement | Both pages share one visual language |
| 6.6 | Add a one-screen help/legend reachable by long-press | New user can type and send without the README |
| 6.7 | Boot splash: version + role, 2.5 s, skippable by any button | Boot feels intentional, not slow |
| 6.8 | Contrast/legibility pass on a real OLED photo | Every glyph readable at arm's length |

### Phase 7 — Proof and packaging

| # | Step | DoD |
|---|---|---|
| 7.1 | Regenerate the audit doc against the shipped build | No stale claims; hashes match artefacts |
| 7.2 | Record the demo: boot, link, encrypted send both ways, tamper reject, SFO stats | 2–3 min, no cuts hiding failures |
| 7.3 | Write the protocol one-pager (frame diagram, key schedule, SFO caveat) | Fits on two pages, correct enough for review |
| 7.4 | Publish a clean `main` with tagged releases | `poc/plaintext-v1` and `v1.0` both build |
| 7.5 | Update `NEXT_CONTEXT_PROMPT.txt` to point at this plan | Next session can start cold |

---

## 7. Test matrix additions

Existing T2–T12 stay. New tests:

| ID | Test | Pass criterion |
|---|---|---|
| T13 | Frame truncation | Every prefix 0..n of a valid frame is rejected |
| T14 | Fuzz parser | 100k random buffers, no crash, no unbounded allocation |
| T15 | Replay | 10k replayed frames, zero accepted |
| T16 | Tamper | Every single-bit flip rejected |
| T17 | Wrong PSK | No `LINK UP`; explicit auth failure |
| T18 | Crypto vectors | Published Speck/ChaCha vectors match on host and device |
| T19 | SFO decode rate | ≥100 bursts measured at three distances |
| T20 | Reconnect | AP and STA each power-cycled 10×, all recover |
| T21 | Watchdog | Forced hang resets and recovers |
| T22 | 24 h soak | No reset, no leak, no wedged send |
| T23 | Battery | Runtime within 20% of the measured projection |
| T24 | Untethered boot | Both units boot and link with no USB attached |

---

## 8. Risks

| Risk | Impact | Mitigation |
|---|---|---|
| CYW43 AP has no per-client RSSI | AP HUD cannot show real bars | Relay the STA's reading (Phase 2 piggyback), or show presence-only |
| UDP reordering on a busy channel | SFO decode rate collapses | SFO is never on the critical path; report the rate honestly |
| Rolling own crypto | Subtle bugs, false confidence | Published vectors, host tests, or use mbedTLS AEAD (§12) |
| PSK in flash | Physical attacker recovers the key | Documented limitation; no secure element on this board |
| Wi-Fi power save vs latency | Link timeouts, sluggish TX | Phase 5 measures PM modes; keep 3 s timeout honest |
| Flash/RAM growth with crypto | No room for UI art | RP2350 has 2 MB flash / 520 KB RAM; measure after each module |
| Art pass touching logic | Regressions at the worst time | `ui_layout.h` contract; Phase 6 forbids protocol edits |
| Battery sag on TX bursts | Brownout resets | Bulk capacitor + soft-start test in 5.6 |

---

## 9. Sequencing rules

1. **One phase at a time.** No crypto while framing is unstable, no art while crypto is unstable.
2. **Two-board test after every step.** A step is not done because it compiles.
3. **Every step is a commit.** Commit message names the step (`feat(proto): 1.3 session ids`).
4. **No silent fallbacks.** If crypto or SFO fails, the log says so and the link refuses to claim success.
5. **UI last, and only skin deep.** Phase 6 changes pixels, layout constants, and labels — nothing else.
6. **Keep the plaintext tag recoverable.** `poc/plaintext-v1` is the demo that always works if later phases stall.

---

## 10. Suggested session order

| Session | Work |
|---|---|
| 1 | Phase 0 complete; start 1.1–1.2 |
| 2 | 1.3–1.6, flash both boards, T2–T12 |
| 3 | 2.1–2.4 (cipher decision, key confirmation) |
| 4 | 2.5–2.7, tamper and replay tests |
| 5 | 3.1–3.4, SFO measurements |
| 6 | 4.1–4.3, reconnect and watchdog |
| 7 | 4.4–4.6, soak start |
| — | Hardware track in parallel from session 2 |
| 8 | Phase 5 measurements and enclosure |
| 9 | Phase 6 UI pass |
| 10 | Phase 7 docs, video, release |

---

## 11. Explicit non-goals

- Raw 802.11 frame access, monitor mode, or true MAC-layer SFO.
- Forward secrecy or post-compromise security in v1.
- Multi-peer sessions; the protocol is strictly two-party.
- Internet bridging, DHCP server, or phone pairing.
- Secure boot, flash encryption, or tamper-resistant key storage.
- A general-purpose mesh or store-and-forward network.

---

## 12. Open decisions (need a human answer)

1. **Cipher.** Speck128/128 in CTR + CMAC is compact, has published vectors, and fits the "Bonnie & Clyde" story. ChaCha20-Poly1305 via the SDK's mbedTLS is better engineering and less code to get wrong. Recommendation: build `proto_crypto` behind an interface, ship Speck for the demo, keep the AEAD slot.
2. **SFO role.** Decorative telemetry vs handshake key hint. Recommendation: telemetry in v1, key hint behind a flag in 3.6.
3. **Battery sense.** `GP28/ADC2` divider (needs two resistors) vs `GP29/VSYS` shared with the radio. Recommendation: divider.
4. **Enclosure.** Off-the-shelf project box vs printed shell. Affects Phase 5.7 only.
