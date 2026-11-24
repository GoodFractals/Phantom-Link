# 32: AEAD suite (ChaCha20-Poly1305)

**What to build:** Implement the reserved `pc_suite_aead` slot with
ChaCha20-Poly1305, 8-byte truncated Poly1305 tag, encrypt-then-MAC semantics.

**Blocked by:** 31

**Status:** ready-for-agent (host AEAD suite green; firmware/ops wiring pending)

- [x] Host KAT vectors for ChaCha20 and Poly1305 (RFC 8439 §§2.3.2, 2.5.2, 2.6.2, 2.8.2).
- [x] AEAD suite seals/opens through a suite-ops interface mirroring `pc_suite_ops_t`.
- [x] AAD is the 18-byte frame header; tag lands in the 8-byte auth field.
- [x] Verify-before-decrypt and constant-time tag compare preserved; exhaustive single-bit tamper coverage.
- [ ] Wire `pc_suite_aead` to this module: it uses a 32-byte key while the Speck slot is typed to 16 bytes; session layer must supply a 32-byte derived key.
- [ ] Suite selectable by compile-time define; Speck remains fallback.
- [x] Wire format unchanged; no timing constant touched.

## Verification

2026-11-23 — `proto_aead` host module with RFC 8439 KATs and tamper tests,
TDD red→green verified at MSVC `/W4 /WX /std:c11` and `/O2`. `ctest` 16/16.
