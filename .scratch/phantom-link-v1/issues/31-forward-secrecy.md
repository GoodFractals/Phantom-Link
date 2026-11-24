# 31: Forward secrecy via ephemeral X25519

**What to build:** Each session derives its traffic keys from an ephemeral
X25519 exchange in HELLO/HELLO_ACK, mixed with the long-term PSK. Recorded
traffic must stay unreadable even if the PSK is extracted later.

**Blocked by:** 14

**Status:** ready-for-agent

- [ ] Host-testable X25519 primitive (or mbedTLS wrapper) with KAT vectors.
- [ ] HELLO/HELLO_ACK carry ephemeral public keys plus key-confirmation tag.
- [ ] Traffic keys = KDF(PSK, ECDH shared secret, role labels).
- [ ] A captured session cannot be decrypted with a later PSK compromise.
- [ ] Firmware self-test gates LINK UP on ECDH/KDF KATs.
- [ ] Both targets build under -Werror; no timing constant touched.
