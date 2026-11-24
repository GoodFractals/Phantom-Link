# 31: Forward secrecy via ephemeral X25519

**What to build:** Each session derives its traffic keys from an ephemeral
X25519 exchange in HELLO/HELLO_ACK, mixed with the long-term PSK. Recorded
traffic must stay unreadable even if the PSK is extracted later.

**Blocked by:** 14

## Verification

2026-11-23 — `proto_fs` host module with 10 tests covering deterministic key
mixing, ECDH-secret binding, session-ID binding, role separation, and monotonic
epochs. `ctest` 15/15. Device-side X25519 (mbedTLS) and HELLO integration remain.

**Status:** ready-for-agent (host KDF/epoch layer green; device integration pending)

- [x] Host-testable KDF/epoch layer with RFC 7748 shared-secret KAT; X25519 primitive comes from SDK mbedTLS on device (primitive wrapper still to integrate).
- [ ] HELLO/HELLO_ACK carry ephemeral public keys plus key-confirmation tag.
- [x] Traffic keys = KDF(PSK, ECDH shared secret, session ID, role labels).
- [x] Session ID is bound into the KDF salt (fixes cross-session CTR nonce reuse surfaced by ticket 37 review).
- [ ] A captured session cannot be decrypted with a later PSK compromise (requires device integration).
- [ ] Firmware self-test gates LINK UP on ECDH/KDF KATs.
- [x] Host target builds clean under -Werror; no timing constant touched.
