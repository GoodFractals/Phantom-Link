# Non-UI/UX gameplan — Phantom Link

Prepared 2026-11-20 for work starting 2026-11-21.

## Scope

Everything needed to finish the project **except** UI/UX tickets 24 (layout),
25 (link truth/help/splash), 26 (legibility), and 28 (link-state reference).
Those are parked. Ticket 27's final demo will need the UI work eventually, but
no UI/UX code or art is part of this plan.

## Ground rules

- One ticket, one commit, named after the ticket number.
- A ticket closes only when its acceptance criteria pass on hardware.
- Do not open a later ticket to get ahead; the gates are deliberate.
- Any change that breaks a baseline in `docs/GAMEPLAN.md` §2 gets reverted.
- Both boards run the full UI; `IS_AP` selects only the Wi-Fi role.
- No `proto_*`, `net_link.c`, or timing-constant changes from UI work.
- Do not claim "encrypted" or "covert" until ticket 14 is green on hardware.
- SFO is telemetry-only; the key-hint variant stays behind `-DSFO_KEY_HINT=1`.
- Two-party only; group/mesh is a non-goal.
- Ask for serial logs or descriptions when a hardware result is ambiguous.
- Ask for permission before every flash; confirm board by UF2 name and serial.

## Starting state tomorrow

| Item | Value |
|---|---|
| Repo / branch | `GoodFractals/Phantom-Link`, `poc/handshake`, HEAD `3b088ef`, **pushed** |
| Fallback tag | `poc/plaintext-v1` (known-good plaintext build) |
| Build on both boards | `e4fadca7` — BYE = both-button 2.5 s chord; STA self-reboots after 15 s without link |
| AP UF2 | `phantom_link_ap.uf2`, 681,472 B, SHA256 `4560CB13…B36A2B9` |
| STA UF2 | `phantom_link_sta.uf2`, 688,128 B, SHA256 `82F19A25…C9CABA` |
| Host checks | `ctest` 9/9, `HOST VALIDATION: PASS` |
| 07, 08 | **Closed**, committed (`16c94fe`, `3b088ef`), pushed |
| 09 | T2–T6 pass; T7–T12 + replay + photos remain |
| 10–13, 15–16 | Blocked by 09 |
| Flash workflow | `flash-picos` skill + `tools/flash_pico.ps1`; no unplug, no BOOTSEL; asks once before writing |
| Power finding | AP radio failed on one Optiplex USB port; swapped port restored it. Logged in issue 22 |

## Decisions needed from you

| # | Decision | Blocks | Default if you don't choose |
|---|---|---|---|
| 1 | PSK for v1: compiled 32-byte constant vs first-boot pairing | 11 | Compiled constant (v1 recommendation) |
| 2 | Demo distance: 1 m, 5 m, or 15 m | 17 → 27 | Decide after ticket 17 measurements |
| 3 | BYE gesture: the rig has two buttons, so 08 now uses a 2.5 s hold of both buttons together. Resolved 2026-11-21; no decision needed | 08 | Resolved |
| 4 | Tidy scope: `u8g2/` (526 MB unused), legacy `Phantom_Link_rx/tx`, `MDA_Emulator` | None | Ask before moving/deleting anything |

## Dependency map

```text
plaintext:  07 → 08 → 09 ─┬─ closes 01–06
                          └→ 10 → 11 → 12 → 13 → 14
field:      14 → 18, 19 → 20 → 21 → 23
SFO:        15 → 16 → 17 ─→ (demo distance decision)
power:      22 (parallel, user-only) → 23
release:    (24–26 UI, out of scope) → 27
```

---

## Track 1 — Clear the plaintext gate (tomorrow, hardware)

Do these in order. I capture both serial logs throughout and verify counters.

### 07 — TX queue and abort — CLOSED (`16c94fe`)

**You:** 
1. Quick sanity: joystick moves cursor, SELECT types (T3); **GP12 BACK** erases a glyph (T4). There is no DELETE label on this rig — use BACK for erase and abort.
2. Type a 21-glyph message (e.g. `HELLO WORLD ABCDEFGHI`), send it. Cursor stays visible during transmit; draft **clears** on confirmed delivery; receiver shows the full message exactly once.
3. Start another message, press **BACK mid-send**. Receiver shows no partial text; sender's draft stays.
4. Send once more — starts cleanly and delivers.

**Me:** verify `msg`, `msgdrop`, `retry`, `fail` counters; close ticket; commit as `07`.

### 08 — Clean teardown — CLOSED (`3b088ef`)

**You:**
1. Hold BYE: on one board hold **both buttons together for 2.5 s**; peer shows `LINK LOST (BYE)` **immediately**, not after the 3 s timeout.
2. Power-cycle one board without BYE; peer shows `LINK LOST` within 3 s.
3. After each case, re-handshake: `LINK UP` and a **new** session ID.
4. Report the exact `LINK LOST` reason for each case.

**Me:** capture both logs, confirm timeout vs BYE path, close and commit as `08`.

### 09 — Framing hardware acceptance

**You:**
1. ~~T2–T6: main UI, input, erase, addressing, cold boot.~~ **PASS on `e4fadca7`.**
2. T7: idle stability; power-cycle one board → `LINK LOST` within 3 s.
3. T8/T9/T10/T11: 21-glyph both ways, responsive TX, truthful HUD, abort.
   Visual RSSI bars are scoped out and deferred to the UI rework as a modular
   addition; truthful HUD means no fabricated readings and an empty AP cell.
4. T12: restart either board → pair re-links (with the documented STA self-reboot workaround).
5. Replay rejection: I arm the probe with the **final** session ID; you approve one UAC prompt. Must log `RX stale ... accepted=` unchanged.
6. Photos of both OLEDs (`LINK UP` and both pages) into `logs/`.

**Me:** capture banners and logs, update `run_ap_replay.cmd` with the final session ID, run the probe, archive evidence (including both UF2 hashes), close 01–06 hardware criteria, commit as `09` including the UI space fix and replay tooling.

---

## Track 2 — Host/protocol work (me, after 09 is green)

TDD throughout: red test first, then C, then green. No hardware needed until 14.

| Ticket | Work | Done when |
|---|---|---|
| 10 | Speck128/128 KDF + CMAC behind a cipher-agnostic interface with an AEAD slot | Host `ctest` green with known-answer vectors; no timing constants touched |
| 11 | Key confirmation and PSK provisioning | Decision 1 resolved; host tests cover mismatch and confirmation |
| 12 | Encrypted DATA payloads (CTR) + CMAC on frames | Host round-trip, tamper detection, no plaintext in encoded buffer |
| 13 | Authenticated replay window and tamper rejection | Host tests: stale, duplicate, single-bit tamper, wrong session |
| 15 | SFO burst channel (host side) | Burst schedule and counters host-tested |
| 16 | SFO telemetry and degraded mode | Telemetry-only; key-hint behind `-DSFO_KEY_HINT=1`, off by default |

**Rules:** encrypted build stays on a separate branch/tag; `poc/plaintext-v1` must
still build and flash. No "encrypted"/"covert" language in docs until 14 passes.

---

## Track 3 — Crypto hardware acceptance (you + me)

### 14 — Crypto acceptance (after 13)
**You:** flash encrypted build and wrong-PSK build; run T2–T12 plus T15–T17
(10k replays rejected, every single-bit tamper rejected, wrong PSK cannot link);
allow a packet capture.
**Me:** build both UF2s, ask before each flash, analyze capture for readable text,
archive logs + hashes, verify `poc/plaintext-v1` still builds. Close and commit.

---

## Track 4 — Field and robustness (you + me)

| Ticket | After | You do | Done when |
|---|---|---|---|
| 17 | 16 | 100 bursts each at 1 m, 5 m, 15 m | Honest attempts/good/rate table committed; worst result reported; demo distance chosen |
| 18 | 14 | 10 consecutive power cycles, both roles | All recover; AP restart re-links STA <10 s; new session, no duplicates; backoff visible |
| 19 | 14 | Flash forced-hang test build | Reset <8 s, re-link and resume; reset reason logged; 1 h normal run never trips |
| 20 | 18, 19 | Power-cycle and read `STATUS` | All fields in one block; counters survive reboot; bounded flash writes; trace readable with no console |
| 21 | 20 | Message every 5 min for 24 h; pull power mid-TX a few times | Zero resets/wedged sends; counters bounded; both sides recover; resets explained |
| 22 | none | Both rigs on packs, no PC; current table idle/RX/TX/pm on/off; 1 h idle; pack model recorded | Already unblocked — can start tomorrow in parallel; power-port finding logged |
| 23 | 21, 22 | Full pair on packs, no USB, repeat once | T23 runtime, T24 boot, 21-glyph both ways, pack LEDs checked |

---

## Track 5 — Release prep (me + you)

| Ticket | Who | Work |
|---|---|---|
| 27 | Me | Audit artifacts against tickets, write protocol note, prepare both release tags from a clean checkout |
| 27 | You | Record one uncut demo: boot, link, send both ways, tamper rejection, SFO stats — after UI tickets 24–26 are done (out of this plan's scope) |

---

## Tomorrow's first 15 minutes

1. **You:** confirm decision 3 (BYE gesture / available buttons).
2. **You:** give PSK decision (decision 1), or accept the compiled-constant default.
3. **You:** run the 07 sequence with **BACK**, not DELETE.
4. **Me:** capture logs, close 07, then walk 08 and 09 in order.

I will not flash without asking, and I will not start 10–13 until 09 is green.

## Definition of done (reminder)

- Hardware pass, not compile pass.
- One ticket, one commit, ticket number as commit name.
- Evidence: both serial logs, OLED photos, UF2 hashes, exact session IDs.
- Revert anything that breaks `docs/GAMEPLAN.md` §2.
