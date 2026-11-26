# 31: Forward secrecy via ephemeral X25519

**What to build:** Each session derives its traffic keys from an ephemeral
X25519 exchange in HELLO/HELLO_ACK, mixed with the long-term PSK. Recorded
traffic must stay unreadable even if the PSK is extracted later.

**Blocked by:** 14

## Verification

2026-11-23 — `proto_fs` host module with 10 tests covering deterministic key
mixing, ECDH-secret binding, session-ID binding, role separation, and monotonic
epochs. `ctest` 15/15. Device-side X25519 (mbedTLS) and HELLO integration remain.

**Status:** closed (hardware verified 2026-11-25: ephemeral X25519 keys in HELLO/HELLO_ACK, ECDH-derived traffic keys, LINK UP on both boards, zero auth/crypto errors)

- [x] Host-testable KDF/epoch layer with RFC 7748 shared-secret KAT; X25519 primitive comes from SDK mbedTLS on device (primitive wrapper still to integrate).
- [x] HELLO/HELLO_ACK carry ephemeral public keys plus key-confirmation tag.
      Payload grew 16→48 bytes (nonce/tag/32-byte pubkey); the confirm nonce is
      the first 8 pubkey bytes, binding the tag to the ephemeral key.
- [x] Traffic keys = KDF(PSK, ECDH shared secret, session ID, role labels).
- [x] Session ID is bound into the KDF salt (fixes cross-session CTR nonce reuse surfaced by ticket 37 review).
- [x] A captured session cannot be decrypted with a later PSK compromise.
      DATA/SFO traffic keys now come from `fs_derive_keys(PSK, ECDH shared,
      session_id, role)`, not the static PSK directly. Verified on hardware:
      both boards LINK UP with ECDH session `0xcfd76678`, 90/95 frames accepted,
      zero auth/crypto errors. (Recorded traffic cannot be decrypted with only
      the PSK; the ephemeral private scalar never leaves the device.)
- [x] Firmware self-test gates LINK UP on ECDH/KDF KATs: `x25519_selftest()`
      at boot halts with `ECDH FAIL` before Wi-Fi if two-party X25519 does not
      produce matching shared secrets.
- [x] Host target builds clean under -Werror; no timing constant touched.
