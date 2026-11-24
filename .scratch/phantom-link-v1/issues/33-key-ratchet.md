# 33: Per-message key ratchet

**What to build:** Rotate or evolve traffic keys per message so compromise of
one key does not expose the whole session.

**Blocked by:** 32

**Status:** ready-for-agent

- [ ] Host-testable ratchet with KAT vectors and rollback rejection.
- [ ] Key evolution binds message ID and direction.
- [ ] Out-of-order delivery within the replay window still decrypts.
- [ ] Ratchet state survives reboot only if session survives (document it).
- [ ] No change to wire length budget or timing constants.
