# 32: AEAD suite (ChaCha20-Poly1305)

**What to build:** Implement the reserved `pc_suite_aead` slot with
ChaCha20-Poly1305, 8-byte truncated Poly1305 tag, encrypt-then-MAC semantics.

**Blocked by:** 31

**Status:** ready-for-agent (host AEAD suite green; firmware/ops wiring pending)

- [x] Host KAT vectors for ChaCha20 and Poly1305 (RFC 8439 §§2.3.2, 2.5.2, 2.6.2, 2.8.2).
- [x] AEAD suite seals/opens through a suite-ops interface mirroring `pc_suite_ops_t`.
- [x] AAD is the 18-byte frame header; tag lands in the 8-byte auth field.
- [x] Verify-before-decrypt and constant-time tag compare preserved; exhaustive single-bit tamper coverage.
- [x] Wire `pc_suite_aead` to this module: adapter expands the 16-byte suite key
  to 32 bytes via KDF; compile-time `PHANTOM_USE_AEAD` selects it.
- [x] Suite selectable by compile-time define; Speck remains default/fallback.
- [x] Wire format unchanged (8-byte tag, 18-byte AAD); no timing constant touched.
- [ ] Hardware: enable `PHANTOM_USE_AEAD`, flash both, verify T2–T12 and tamper.

## Verification

2026-11-23 — `proto_aead` host module with RFC 8439 KATs and tamper tests,
TDD red→green verified at MSVC `/W4 /WX /std:c11` and `/O2`. `ctest` 16/16.
