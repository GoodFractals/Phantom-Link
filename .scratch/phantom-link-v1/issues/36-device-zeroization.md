# 36: Device zeroization on teardown

**What to build:** Clear secrets from device RAM and any persistent stores when
a link closes or the device powers down, so a captured board contains no live
key material.

**Blocked by:** 14

**Status:** ready-for-agent (host zeroize module green; firmware wiring pending)

- [x] Zeroize derived keys, PSK copies, ECDH state, and nonce buffers on BYE.
- [ ] Best-effort zeroize on watchdog reboot and brownout path.
- [ ] No secret material remains in retained RAM across warm resets.
- [x] Any future flash logger must not store keys or plaintext (no flash logger exists).
- [x] Host tests assert zeroized buffers (10 tests); hardware check still pending.
- [x] Firmware wires zeroization on BYE and re-derives keys on next handshake.

## Verification

2026-11-23 — `proto_zeroize` host module with 10 tests, RED→GREEN verified with
MSVC at /Od and /O2; disassembly confirms volatile zero loops survive /O2.
`ctest` 15/15. Firmware integration on BYE/reset paths remains.
