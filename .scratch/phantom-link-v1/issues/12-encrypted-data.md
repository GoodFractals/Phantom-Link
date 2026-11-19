# 12: Encrypted DATA, encrypt-then-MAC

**What to build:** User messages travel as ciphertext with encrypt-then-MAC. The receiver verifies the tag before decrypting and never falls back to plaintext.

**Blocked by:** 11

**Status:** ready-for-agent

- [ ] Serial logs show ciphertext length, never the user's plaintext.
- [ ] A tampered tag drops the frame and increments a crypto-error counter.
- [ ] A decryption failure never changes session state or surfaces partial text.
- [ ] Both directions deliver a 21-glyph message correctly.
- [ ] The nonce construction is documented and cannot repeat within a session.
