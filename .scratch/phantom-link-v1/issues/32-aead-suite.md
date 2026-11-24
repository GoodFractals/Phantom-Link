# 32: AEAD suite (ChaCha20-Poly1305)

**What to build:** Implement the reserved `pc_suite_aead` slot with
ChaCha20-Poly1305, 8-byte truncated Poly1305 tag, encrypt-then-MAC semantics.

**Blocked by:** 31

**Status:** ready-for-agent

- [ ] Host KAT vectors for ChaCha20 and Poly1305 (RFC 8439).
- [ ] `pc_suite_aead` seals/opens through the same ops interface as Speck suite.
- [ ] AAD is the 18-byte frame header; tag lands in the 8-byte auth field.
- [ ] Verify-before-decrypt and constant-time tag compare preserved.
- [ ] Suite selectable by compile-time define; Speck remains fallback.
- [ ] Wire format unchanged; no timing constant touched.
