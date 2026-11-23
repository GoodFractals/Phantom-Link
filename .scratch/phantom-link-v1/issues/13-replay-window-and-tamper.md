# 13: Replay window and tamper rejection

**What to build:** A 32-bit sliding replay window per direction, plus tamper tests that flip each bit in turn. Rejection is counted and logged, never silently ignored.

**Blocked by:** 12 (closed)

## Verification

2026-11-22 — host `test_session` already covered 10k replays, window edges,
wrong-session rejection, and clean-session reset. Added exhaustive single-bit
tamper coverage in `test_crypto`: every bit of the 18-byte AAD header, the
ciphertext payload, and the 8-byte tag is rejected, and a valid frame directly
after the rejections still opens. `ctest` 10/10, `HOST VALIDATION: PASS`.

**Status:** ready-for-human (host green; hardware 10k/tamper checks join ticket 14)

- [x] 10k replayed frames are accepted zero times (host `test_session`).
- [x] Every single-bit flip in header, payload, and tag is rejected (host `test_crypto`).
- [x] Sequence numbers far outside the window are rejected (host `test_session`).
- [x] Counters distinguish replay, tamper, and malformed frames (`duplicate`/`stale`/`wrong`/`crypto`/`auth`).
- [x] A valid frame directly after a rejected one still passes.
