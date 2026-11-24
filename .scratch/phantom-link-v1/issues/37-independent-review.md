# 37: Independent security review

**What to build:** Commission or conduct an external cryptographic/code review
before any public "secure" claim. Treat this as a security control, not a formality.

**Blocked by:** 32

**Status:** ready-for-human

- [ ] Freeze a tagged build for review.
- [ ] Provide reviewer with threat model, ADRs, frame spec, and source.
- [ ] Review questions: KDF domain separation, nonce reuse, CMAC truncation,
      header masking, key confirmation, BSSID binding, replay interaction.
- [ ] Publish findings and remediate critical/high issues.
- [ ] Datasheet may say "reviewed" only after this completes.

---

## Independent review package (prepared 2026-11-23, local-files only)

This section is a **preparation package**, not a completed review. No files were
built, flashed, committed, tagged, or changed other than appending to this
ticket. All values below were read from local files / local `git` state.

### 1. Freeze checklist

**Gating note.** This ticket is blocked by 32, which is blocked by 31, which is
blocked by 14 (`.scratch/phantom-link-v1/issues/32-aead-suite.md`,
`31-forward-secrecy.md`). Tickets 14, 31, 32, 33, 34, 35, 36 and 38 still have
unchecked boxes, and the working tree is dirty (`git status`: modified
`firmware/main.c`, `firmware/host/CMakeLists.txt`, `tools/ap_replay_session.ps1`,
`tools/replay_probe.ps1`; untracked `firmware/proto/proto_fs.c/.h`,
`firmware/host/tests/test_fs.c`, `firmware/proto/proto_reconnect.c/.h`,
`firmware/proto/proto_netcfg.h`, `firmware/proto/netcfg_local.h`,
`firmware/host/tests/test_reconnect.c`, `docs/PHANTOM_LINK_DATASHEET.md`,
`tools/frame_probe.c`). **Do not cut the review tag from this tree.**

**Repo state at preparation time (record only — do not tag this):**

| Item | Exact value |
|---|---|
| HEAD commit | `ea2945dff94742bf16cc46b872e25464a2ba6759` (`ea2945d`, `main` = `origin/main`) |
| HEAD subject | `tickets: forward secrecy through physical threat study` |
| `git describe --tags --always` | `poc/plaintext-v1-48-gea2945d` |
| Existing tags | only `poc/plaintext-v1` → `d812b7f77a7160fdcc827c414fae62867240b957` (48 commits behind HEAD) |
| Review tag | **none exists**; must be created at freeze time |
| Remote | `https://github.com/GoodFractals/Phantom-Link.git` |

**Build identity mechanism.** The boot banner hash is the first 8 hex digits of
the SHA-256 of `firmware/main.c` (`firmware/CMakeLists.txt:11-17` →
`firmware/banner.h:8-26`), so a flash image is only traceable to source if the
full `main.c` hash and the UF2 hashes are recorded together.

**Current local UF2s are NOT review artifacts.** They were built 2026-11-24 UTC
from the dirty tree and must be discarded/regenerated after freeze:

| Local artifact | Size (bytes) | SHA-256 | Embedded banner ID |
|---|---:|---|---|
| `firmware/build/phantom_link_ap.uf2` | 692,736 | `60ECAE32C89A7C253F9FAF1764D10B21D2A1A12951769988B8EC199E952017D1` | `76b23b52` |
| `firmware/build/phantom_link_sta.uf2` | 700,416 | `4521DDB5076A84E61E801EC41B5C84BFF2CCA03E671B11016BCB3187A368C99C` | `76b23b52` |

`76b23b52` matches the local `firmware/main.c` SHA-256
`76B23B528D12F9C47BD2046664DCA6786F5D35CA86ED6380498339170C06C466`, confirming
these images come from uncommitted work.

**Conflicting historical hashes in docs — do not reuse any of them for review:**

| Source | Build ID | AP SHA-256 | STA SHA-256 |
|---|---|---|---|
| `docs/PHANTOM_LINK_DATASHEET.md:5,104-111` | `a6cdff30` | `CF7D7CE994E3315016A7ACCA7774C9DE2EB2EB01FB08C0DD8B6A845B09C209C5` | `11618771442E6242EB73D3E029DDB8AC708751BC811C4117D2C727B1E35ECC40` (plus `wrongpsk_ap/sta` hashes `D4118420…`/`A314F2DB…`) |
| `docs/PHANTOM_LINK_AUDIT_AND_HANDOFF.md:176-182` | pre-crypto | `E0C1C9CE3849B647B1865EF106C3F1004341D2E7EC5C3D349F0EDDF7EDD7D1E9` | `D4CE0FB79BF9A82A4ACE21ACB2E099981BB626781348F790A5E82AAC9B1C5EC7` |
| `docs/NEXT_CONTEXT_PROMPT.txt:64-68` | `05ce2908` | `6A39C9AA097A5DA07A7C1FDEC80A37103779FC1B475EAB032703E31A11BEF977` | `7745C6C34B45E20EBD8209F34A313BEFF6BFE330D6A5C9ECFEAD23FCE327E89A` |

**Freeze procedure (run when prerequisites close):**

- [ ] 1. Close tickets 14/31/32/33/34/35/36/38 and update the threat model +
      datasheet to match the shipped design (especially if X25519/AEAD/encrypted
      header land — the summary in §3 below describes the *current* Speck build).
- [ ] 2. Commit everything. Verify `git status --porcelain` is empty on `main`.
      Record the full HEAD commit hash; do not freeze a dirty tree.
- [ ] 3. Clean rebuild from that commit with the documented toolchain (Pico SDK
      2.3.1, CMake 4.0.0-rc1, Ninja 1.13.2, Arm GNU Toolchain 14.2 —
      `docs/PHANTOM_LINK_AUDIT_AND_HANDOFF.md:176`). Build both targets from one
      `firmware/main.c` (`firmware/CMakeLists.txt`), zero warnings.
- [ ] 4. Record: full HEAD SHA, full `firmware/main.c` SHA-256, the 8-hex build
      ID derived from it, SDK/toolchain versions, exact build commands.
- [ ] 5. Create an **annotated** tag, proposed name
      `security-review/37-v1` (alternative: `security-freeze-37`), at that exact
      commit: `git tag -a security-review/37-v1 -m "Security review freeze for ticket 37"`.
      Record the tag object hash, push the tag, and never move it.
- [ ] 6. Hash both UF2s into `SHA256SUMS` and archive them **outside git**
      (`.gitignore:2-9` excludes `build/`, `artifacts/`, `*.uf2`). Include the
      `wrongpsk_ap/sta` negative-test pair if wrong-PSK AUTH FAILED is re-run.
- [ ] 7. Flash each board and capture the serial boot banner; verify the build
      ID in the banner equals the recorded ID, and keep that log with the bundle.
- [ ] 8. After freeze: read-only. Any code/config/PSK change → new tag and new
      hashes; do not silently replace a reviewed image.

**Artifacts the reviewer needs:**

| Artifact | Concrete path / note |
|---|---|
| Tagged source + tests + docs | `git bundle` or tarball of `security-review/37-v1` (include `.scratch/phantom-link-v1/issues/`, not just tracked source) |
| Firmware images | AP + STA UF2s, `wrongpsk` pair if used, `SHA256SUMS`, byte sizes |
| Build provenance | toolchain versions, build commands, `main.c` SHA-256, banner/screen capture from both boards |
| Binary review aids | `firmware/build/*.elf`, `*.elf.map`, `*.dis`, `CMakeCache.txt` (useful for verifying `verify-before-decrypt` and constant-time compare actually compiled) |
| Frame/key spec | `docs/GAMEPLAN.md` §4 (frame, state machine, crypto envelope, reliability), §11 (non-goals), §12 (decisions); `.scratch/phantom-link-v1/spec.md` |
| ADRs | `docs/adr/0001-speck128-128-ctr-cmac.md`, `docs/adr/0004-two-party-membership-is-physical-possession.md` |
| Threat model / reference | `docs/teach_security/reference/security-layer.md`, §3 of this ticket |
| Claim surface to check | `docs/PHANTOM_LINK_DATASHEET.md` (§5, §7, §8), `docs/PHANTOM_LINK_AUDIT_AND_HANDOFF.md`, `docs/REGRESSION.md` |
| Test evidence | host `ctest` output (`firmware/host/tests/` incl. `test_crypto.c`, `test_handshake.c`, `test_wire.c`, `test_session.c`, `test_fs.c`), `.scratch/phantom-link-v1/evidence/09-acceptance.md`, packet captures, replay/tamper logs |
| Crypto config | `firmware/proto/proto_psk.h`, `firmware/proto/proto_spec.h`, `firmware/proto/proto_crypto.c/.h`, `firmware/proto/proto_session.c`, `firmware/proto/proto_fs.c/.h`, `tools/gen_psk.ps1`; freeze the exact per-build PSK in a sealed note, do not publish a deployed key |

### 2. Targeted questions for the reviewer

Ask these with the frozen source in hand; each cites the code that motivated it.

1. **KDF domain separation.** `pc_kdf` (`firmware/proto/proto_crypto.c:161-181`)
   splits the 32-byte master into two 16-byte CMAC keys and derives
   `salt || label || counter(0x01/0x02)`. `main.c:209-225` passes salt
   `"BONNIE-CLYDE-v1"` — **no session_id**, contrary to
   `docs/GAMEPLAN.md:124-128` (`salt = session_id || "BONNIE-CLYDE-v1"`).
   `firmware/proto/proto_fs.c:14-18` adds label `0x03` for ECDH mixing.
   - Are `0x01`/`0x02`/`0x03` genuinely domain-separated given that the same
     CMAC primitive is reused for KDF, frame tag, key confirmation and header
     mask, and that `0x03` puts ECDH material in the *salt* position?
   - Is it intentional that traffic keys are identical across sessions with the
     same PSK, while `frame_nonce` (`main.c:193-197`) resets with `seq=0` every
     session? Does that reuse a CTR keystream across sessions?
   - Is the two-half-master construction (two independent 128-bit CMAC keys)
     sound, and is the 64-byte input buffer bound (`salt_len > 62` → zero key)
     fail-closed in every caller?

2. **Nonce reuse across retransmits.** GAMEPLAN §4.5 retransmits the whole
   message under the same message ID/seq; `frame_nonce` is `0x01 || seq(4) ||
   0^7` with no session or direction bit beyond the fixed `0x01`, and DATA
   payloads are re-padded with fresh randomness on each send (`main.c:409-417`,
   `lpi_pad`). Additionally `HEADER_MASK_NONCE` is all-zero and reused for every
   frame (`main.c:199-207`).
   - Is re-encrypting a retransmit with the same `(key, nonce)` acceptable here,
     given fresh random padding changes the plaintext (two-time-pad XOR leak)?
   - What happens at 32-bit `seq` wrap, and across a session ID change with
     unchanged traffic keys?
   - Does the constant-zero header-mask nonce, using the same role key as
     payload CTR, create a cross-use keystream collision or allow an observer to
     cancel the masks of two frames? Recommend explicit domain labels.

3. **CMAC truncation to 8 bytes.** `pc_cmac_trunc` takes the first 8 bytes,
   MSB-first (`proto_crypto.c:147-153`; `docs/teach_security/reference/security-layer.md:17`).
   - Is a 64-bit tag adequate for the documented frame budget (≤63 frames per
     message, 10k replay/bit-tamper tests, 24 h soak, device lifetime)? State the
     forgery bound and when rekey/tag-length increase becomes mandatory.
   - Is the verify-before-decrypt compare
     (`proto_crypto.c:237-245`, `281-289`, XOR accumulated into `diff`) truly
     constant-time once compiled for RP2350, including early-exit in callers?
   - Is CMAC-over-`AAD||ciphertext` with a non-standard block cipher acceptable
     as an AEAD substitute, and does the reserved `pc_suite_aead = {NULL,NULL}`
     stub (`proto_crypto.c:249`) create a fail-open path if selected?

4. **Header masking vs encryption.** `mask_header_fields` XORs bytes 6..17
   (`session_id`, `seq`, `ack`) with a fixed-nonce CTR stream derived from the
   role key (`main.c:199-207`); DATA AAD is the *masked* header
   (`main.c:823-849`), while control frames get a CRC after masking
   (`main.c:338-348`).
   - Confirm this is obfuscation, not confidentiality: an attacker with the PSK
     can strip it, and traffic metadata (type, length, timing, addresses) stays
     visible. Does any doc claim more (`docs/PHANTOM_LINK_DATASHEET.md:33`)?
   - Does authenticating the masked header rather than the plaintext header
     leave a semantic gap (e.g. tampering that is only caught by CRC on control
     frames, or mismatch between `proto_frame_decode` outcomes before/after
     unmask at `main.c:828-869`)?
   - With ticket 34 moving the header inside the AEAD envelope, is that design
     sound for replay processing and AAD scope?

5. **Key confirmation binding.** `pc_confirm_tag` MACs
   `session_id(4)||seq(4)||nonce(8)||label(1)` (`proto_crypto.c:251-273`);
   HELLO uses outgoing key/label `0x01`, HELLO_ACK incoming key/label `0x02`
   (`main.c:392-399`), verified at `main.c:611-627`.
   - Does this bind the *traffic* keys (or, post-ticket-31, the ephemeral ECDH
     secret)? If confirmation only proves knowledge of the long-term PSK, is
     that enough to authenticate the session?
   - Are role/direction, protocol version, BSSID/peer identity and PSK identity
     bound? Can a reflected or cross-direction HELLO/HELLO_ACK tag be replayed
     in the other direction or in a different session?
   - Is the 8-byte confirmation tag sufficient against online guessing, and
     does `adopt_session` (`main.c:632-635`) before replay acceptance allow a
     key-holder to reset replay state with an old-but-valid HELLO?

6. **BSSID lock strength.** `ap_bssid` is a compiled constant, passed as the
   join target with a runtime check in the main loop (`main.c:175,184-190,1475-1484`);
   `bssid_enrolled` is only set if the configured value is non-zero
   (`main.c:1686`). Wi-Fi layer is WPA2-AES, SSID `TMOBILE-1219`
   (`docs/teach_security/reference/security-layer.md:5-6`).
   - BSSID is a spoofable MAC: with a cloned SSID/BSSID and the Wi-Fi password,
     can an on-path device become the UDP peer before the runtime check runs?
   - Does the check cover deauth/reassociation and the AP-reboot path
     (`main.c:1489-1495` comment about ARP/L2 mapping)?
   - Is the honest claim "reduces mis-association; no protection against a
     cloned BSSID", and is that reflected in the datasheet?

7. **Replay-window interaction with crypto failures.** On receipt, a bad CMAC
   or padding returns at `main.c:845-881` **before** `handle_frame`, so
   `proto_session_accept` (`main.c:644-654`, `firmware/proto/proto_session.c:22-70`)
   never sees the frame; CRC failures also return early (`main.c:854-862`).
   - Does this ordering create a timing/log oracle for tag validity, or let an
     attacker keep the window frozen while injecting high-seq frames?
   - Conversely, can a legitimate frame be shadowed because a failed frame did
     not advance the 32-bit bitmap, given `rx_started` fixes `rx_max` from the
     first accepted seq and whole-message retransmits reuse seq values?
   - Is replay rejection correctly separate from authentication (reference:
     `security-layer.md:9-10`), including for HELLO-driven session adoption?

8. **PSK extractability and key lifecycle.** ADR 0001 line 9,
   `security-layer.md:19`, `docs/PHANTOM_LINK_DATASHEET.md:100` and
   `docs/GAMEPLAN.md:135` all state the PSK lives in flash and physical access
   defeats it; `GAMEPLAN.md:399` notes every build currently ships the same key
   unless `PHANTOM_PSK_BYTES` is overridden; `GAMEPLAN.md:367` rules out secure
   boot, flash encryption and tamper-resistant storage.
   - Confirm the practical extraction path (SWD/debug access, UF2/flash
     readback, build artifacts, logs) and whether any read protection is enabled.
   - Does `tools/gen_psk.ps1` produce a per-build key for the frozen image, and
     is that key kept out of public artifacts while still available to the reviewer?
   - Do tickets 31/33 (forward secrecy/ratchet) change *this* threat, and does
     ticket 36's zeroization actually clear RAM copies on teardown rather than
     just the flash-resident PSK?
   - Is the wording "PSK is extractable by anyone with physical flash access"
     present wherever the link is described as secure?

### 3. Scoped threat model summary (current Speck build)

Sources: `docs/adr/0001-speck128-128-ctr-cmac.md`,
`docs/adr/0004-two-party-membership-is-physical-possession.md`,
`docs/teach_security/reference/security-layer.md` (and `docs/GAMEPLAN.md` §4/§11
for framing). Scope is strictly the two-board v1 PoC.

- **Trust/membership.** Two parties; membership is physical possession of a
  rig holding the key. No directory, invite, or in-band way to add a third
  party; group/mesh is a security property exclusion, not a missing feature
  (ADR 0004 lines 3-9). UI must not imply more than two rigs.
- **Radio/link.** WPA2-AES to SSID `TMOBILE-1219` gives link-layer encryption,
  but the SSID is ambiguous and does not identify *which* AP. BSSID lock plus
  runtime `cyw43_wifi_get_bssid` check narrows mis-association but gives no
  protection against a cloned BSSID (`security-layer.md:5-6`).
- **Handshake.** HELLO/HELLO_ACK nonce + CMAC confirms the PSK; wrong PSK →
  `AUTH FAILED`. It does **not** provide forward secrecy
  (`security-layer.md:7`; ADR 0001 uses a compiled PSK).
- **Payload/integrity.** Speck128/128-CTR with nonce `0x01||seq||0^7` gives
  payload confidentiality (not traffic existence/size). Encrypt-then-MAC with
  an 8-byte truncated CMAC over header AAD + ciphertext gives tamper detection
  and verify-before-decrypt, but not replay protection
  (`security-layer.md:8-9`; ADR 0001 line 3).
- **Replay and metadata.** 32-bit sliding window + session IDs reject
  duplicates/stale frames but do not authenticate; an XOR mask over
  session/seq/ack reduces obviousness but is not header privacy
  (`security-layer.md:10-11`).
- **LPI.** Fixed length, decoys, jitter make traffic less bursty; explicitly
  *not* covert, and the datasheet must not claim covertness
  (`security-layer.md:12`; `docs/PHANTOM_LINK_DATASHEET.md:101`).
- **Crypto agility / limitations.** `pc_suite_aead` is reserved but stubbed;
  KDF is CMAC-based with role labels `0x01`/`0x02`; PSK is a pairing boundary
  readable by anyone with flash access (ADR 0001 lines 7-9;
  `security-layer.md:15-19`). Explicit non-goals: forward secrecy/post-compromise
  security, rekey in v1, secure boot, flash encryption, tamper-resistant
  storage, MAC-layer SFO, multi-peer, internet/phone pairing
  (`docs/GAMEPLAN.md:117,361-383`; `.scratch/phantom-link-v1/spec.md:15-20`).
  **Caveat:** tickets 31/32/33/34/36 are in progress; if they land before the
  freeze, this summary must be rewritten and re-baselined before it is handed to
  a reviewer.

### 4. No-security-claim rule

**No claim of "audited", "secure", "cryptographically reviewed", or
"independently reviewed" may be made in the README, datasheet, docs, release
notes, video, or any public communication until this ticket completes: findings
published and critical/high issues remediated.** Per this ticket's checklist,
the datasheet may say "reviewed" **only after** completion. The existing
`docs/PHANTOM_LINK_AUDIT_AND_HANDOFF.md` is an engineering/handoff audit of an
earlier plaintext PoC — it is **not** an independent cryptographic security
review and must not be cited as one. Until then the honest wording is "two-party
proof of concept, unattended security review pending, PSK extractable, not
covert".
