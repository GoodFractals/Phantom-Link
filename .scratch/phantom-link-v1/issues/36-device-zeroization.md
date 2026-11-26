# 36: Device zeroization on teardown

**What to build:** Clear secrets from device RAM and any persistent stores when
a link closes or the device powers down, so a captured board contains no live
key material.

**Blocked by:** 14

**Status:** closed (hardware 2026-11-25: both initiator and peer zeroize on BYE, keys re-derived on next handshake; host disassembly-verified; watchdog/brownout gap documented)

- [x] Zeroize derived keys, PSK copies, ECDH state, and nonce buffers on BYE.
- [ ] Best-effort zeroize on watchdog reboot and brownout path. **Documented
      residual risk:** RP2350 RAM persists across a watchdog reset and there is
      no pre-reset callback to wipe it. Not claimed as a feature. The physical
      teardown paths (BYE chord / serial `Z`) are the supported secret-clearing
      mechanism.
- [ ] No secret material remains in retained RAM across warm resets. **Not
      claimed** — same residual risk as above; only an orderly teardown wipes
      the derived keys. The static PSK remains in flash by design.
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
