# 12: Encrypted DATA, encrypt-then-MAC

**What to build:** User messages travel as ciphertext with encrypt-then-MAC. The receiver verifies the tag before decrypting and never falls back to plaintext.

**Blocked by:** 11 (closed)

## Verification

2026-11-22 — DATA frames set `PROTO_FLAG_ENCRYPTED`, are sealed with
`pc_suite_speck_ctr_cmac` over the 18-byte header as AAD, and carry the 8-byte
CMAC in the existing auth field. Receiver verifies before decrypting and drops
on tag failure without touching session state. Nonce is
`0x01 || seq(4 bytes little-endian) || 7 zero bytes`; `seq` is unique per
direction within a session, so the nonce cannot repeat. Control frames keep
CRC32. Host suite tests cover AAD tamper, ciphertext tamper, and round-trip.
`ctest` 10/10, `HOST VALIDATION: PASS`, both firmware targets build clean.

**Status:** ready-for-human (host green; hardware crypto checks join ticket 14)

- [x] Serial logs show ciphertext length, never the user's plaintext (TX logs carry length only).
- [x] A tampered tag drops the frame and increments `crypto` counter (code complete).
- [x] A decryption failure never changes session state or surfaces partial text (code complete).
- [x] Both directions deliver a 21-glyph message correctly (host/interface complete; hardware in 14).
- [x] The nonce construction is documented and cannot repeat within a session.
