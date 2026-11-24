# 33: Per-message key ratchet

**What to build:** Rotate or evolve traffic keys per message so compromise of
one key does not expose the whole session.

**Blocked by:** 32

**Status:** ready-for-agent (host module green; firmware integration pending)

- [x] Host-testable ratchet with pinned KAT vectors and rollback rejection.
- [x] Key evolution binds message ID (direction supplied by the caller's master key).
- [x] Out-of-order delivery within the 8-slot window still decrypts, including uint32 wraparound.
- [x] Ratchet state is RAM-only; document that it does not survive reboot (session does not either).
- [x] No change to wire length budget or timing constants.

## Verification

2026-11-23 — `proto_ratchet` host module with 7 test functions, TDD red→green,
MSVC /W4 /WX clean, `ctest` 17/17. Firmware/session wiring remains.
