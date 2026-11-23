# 10: Speck128/128, KDF, CMAC

**What to build:** A cipher-agnostic crypto module with Speck128/128 in CTR mode, a KDF for per-direction keys, and CMAC truncated to 8 bytes. Published vectors run on host and as an on-device self-test.

## Verification

2026-11-22 — TDD red→green across five agreed seams: Speck primitive, generic
CMAC, KDF, CTR/suite with AEAD slot, boot self-test. Host `ctest` 10/10,
`HOST VALIDATION: PASS`, both firmware targets build clean under `-Werror`.
The Speck byte order was corrected against the Crypto++ vector after the first
KAT failed; research note updated. On-device boot self-test is wired into
`main()` but is verified on hardware with ticket 14.

**Blocked by:** 09 (closed)

**Status:** ready-for-human (host green; on-device self-test joins ticket 14)

- [x] Published Speck128/128 vectors pass on host (NSA guide KAT A + Crypto++)
- [x] CMAC vectors pass for empty, short, and multi-block inputs (RFC 4493)
- [x] The KDF produces different send and receive keys for the same session.
- [x] The interface exposes an AEAD slot that the session layer could adopt without changes.
- [x] A failed boot self-test is loud and blocks LINK UP (halt_error "CRYPTO FAIL").
- [x] The module compiles for both host tests and the firmware targets.
