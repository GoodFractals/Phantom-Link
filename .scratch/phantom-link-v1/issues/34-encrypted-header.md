# 34: Encrypt header fields

**What to build:** Move beyond masking: encrypt session_id, seq, and ack inside
the AEAD envelope, leaving only routing-visible fields in clear.

**Blocked by:** 32

**Status:** ready-for-agent (host module green; wire/firmware integration pending)

- [x] Documented encrypted-header envelope in `proto_hdr.h` (6-byte clear AAD, 12-byte encrypted secret, 8-byte tag).
- [ ] Receiver decrypts header before session/replay processing (firmware wiring pending).
- [x] AAD covers only clear fields; `hdr_open` requires seq from session state to rebuild the nonce.
- [x] Replay rejection compatible: decrypted seq is checked against the supplied session seq.
- [x] Host tests cover tamper, wrong-key, wrong-seq, and sentinel-unchanged-on-failure.

## Verification

2026-11-23 — `proto_hdr` host module with exhaustive single-bit AAD/CT/tag
tamper coverage, TDD red→green, MSVC /W4 /WX clean, `ctest` 18/18. Wire format
and firmware integration remain a deliberate hardware-safe follow-up.
