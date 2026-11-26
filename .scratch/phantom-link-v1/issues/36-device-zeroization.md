# 36: Device zeroization on teardown

**What to build:** Clear secrets from device RAM and any persistent stores when
a link closes or the device powers down, so a captured board contains no live
key material.

**Blocked by:** 14

**Status:** closed (BYE zeroization verified on hardware; two original criteria were untestable on RP2350 and reclassified as threat-model notes in ticket 38)

- [x] Zeroize derived keys, PSK copies, ECDH state, and nonce buffers on BYE.
- [x] Best-effort zeroize on **watchdog reboot** — **NOT ACHIEVABLE on RP2350**.
      Research 2026-11-25: the RP2350 watchdog register block has no interrupt
      or warning-threshold register (only CTRL/LOAD/REASON/SCRATCH). There is
      no pre-reset callback to hook. Moved to threat-model notes in ticket 38,
      not a test. Do not claim RAM is wiped on watchdog reset.
- [x] Brownout wipe — **not an acceptance criterion**. A voltage collapse gives
      no guaranteed execution window; record as a threat-model note in ticket
      38, not a test. Do not claim "no secrets in RAM across a power cut."
- [x] "No secret material in retained RAM across warm resets" — **not a test**.
      It depends on reset type: orderly teardown and watchdog (with the IRQ
      above) wipe; brownout/power-cut cannot be guaranteed. Rewritten as a
      threat-model note, not a checkbox.
- [x] Any future flash logger must not store keys or plaintext (no flash logger exists).
- [x] Host tests assert zeroized buffers (10 tests, `/Od` + `/O2`, disassembly
      confirms volatile zero loops survive optimization). This is the proof
      that the wipe actually happens, stronger than a serial print.
- [x] Firmware wires zeroization on BYE and re-derives keys on next handshake.
      **Hardware verified 2026-11-25** via a serial `Z` command that invokes
      the same `request_teardown()` path as the physical chord:
      STA: `TEARDOWN requested → BYE sent → LINK LOST (BYE) → SECRETS zeroized
      → SESSION new → LINK UP`; AP peer: `RX BYE → LINK LOST (BYE) → SECRETS
      zeroized (peer BYE) → LINK UP`. Both settled to a clean shared session
      with zero wrong-session frames. The peer-side zeroize was a real gap
      found and fixed during this test.

## Verification

2026-11-23 — `proto_zeroize` host module with 10 tests, RED→GREEN verified with
MSVC at /Od and /O2; disassembly confirms volatile zero loops survive /O2.
`ctest` 15/15. Firmware integration on BYE/reset paths remains.
