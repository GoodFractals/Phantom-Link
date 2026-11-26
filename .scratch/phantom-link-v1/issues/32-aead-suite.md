# 32: AEAD suite (ChaCha20-Poly1305)

**What to build:** Implement the reserved `pc_suite_aead` slot with
ChaCha20-Poly1305, 8-byte truncated Poly1305 tag, encrypt-then-MAC semantics.

**Blocked by:** 31

**Status:** closed (hardware verified 2026-11-26: AEAD build links on both boards, valid DATA decrypts, tampered DATA rejected with `crypto=1`, zero wrong-session/auth errors)

- [x] Host KAT vectors for ChaCha20 and Poly1305 (RFC 8439 §§2.3.2, 2.5.2, 2.6.2, 2.8.2).
- [x] AEAD suite seals/opens through a suite-ops interface mirroring `pc_suite_ops_t`.
- [x] AAD is the 18-byte frame header; tag lands in the 8-byte auth field.
- [x] Verify-before-decrypt and constant-time tag compare preserved; exhaustive single-bit tamper coverage.
- [x] Wire `pc_suite_aead` to this module: adapter expands the 16-byte suite key
  to 32 bytes via KDF; compile-time `PHANTOM_USE_AEAD` selects it.
- [x] Suite selectable by compile-time define; Speck remains default/fallback.
- [x] Wire format unchanged (8-byte tag, 18-byte AAD); no timing constant touched.
- [x] Hardware: `PHANTOM_USE_AEAD=ON` build flashed to both boards (AP `57381078`, STA `27E09212`). Both show `link=UP` with shared session `0x5fcb2585`, zero `auth`/`wrong` errors; boot `aead_self_test()` did not halt.
- [x] Valid DATA frame: STA `DATA probe: sent`, AP `ratchet rx_key=2` (both probe frames reached the verify/decrypt path), `crypto=1` — exactly the tampered frame rejected.
- [x] Tamper probe: STA `DATA tamper probe: sent` (auth byte flipped after sealing), AP logged bad-tag rejection and incremented `crypto` only once. Expected `gap=1` on AP is the rejected frame consuming a sequence number, not a delivery failure.
- [x] Speck remains the default; AEAD stays a compile-time opt-in, wire format unchanged.

## Verification

2026-11-23 — `proto_aead` host module with RFC 8439 KATs and tamper tests,
TDD red→green verified at MSVC `/W4 /WX /std:c11` and `/O2`. `ctest` 16/16.

2026-11-26 — Hardware gate passed. Built with `-DPHANTOM_USE_AEAD=ON` and a
temporary `PHANTOM_DATA_PROBE` serial hook (`T` = valid DATA, `X` = DATA with
one auth byte flipped after sealing). Both boards linked with shared session
`0x5fcb2585`, `errors auth=0 wrong=0`, `hdr_mask eph_tx/eph_rx` advancing. On
the STA both probes were sent (`ratchet tx_key=2`); on the AP both reached the
ratchet (`rx_key=2`) and exactly one was rejected (`crypto=1`). The one `gap=1`
is the expected sequence hole left by the rejected tampered frame.

**Boundary:** the full T2–T12 UI matrix was not re-run under this build; the
crypto-relevant subset (link, valid delivery, tamper rejection, counter
hygiene) is what was verified. Speck remains the default suite; the AEAD build
is a verified opt-in, not a shipping default change.
