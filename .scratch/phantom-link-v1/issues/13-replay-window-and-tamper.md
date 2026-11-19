# 13: Replay window and tamper rejection

**What to build:** A 32-bit sliding replay window per direction, plus tamper tests that flip each bit in turn. Rejection is counted and logged, never silently ignored.

**Blocked by:** 12

**Status:** ready-for-agent

- [ ] 10k replayed frames are accepted zero times.
- [ ] Every single-bit flip in header, payload, and tag is rejected.
- [ ] Sequence numbers far outside the window are rejected.
- [ ] Counters distinguish replay, tamper, and malformed frames.
- [ ] A valid frame directly after a rejected one still passes.
