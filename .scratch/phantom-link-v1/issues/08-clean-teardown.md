# 08: Clean teardown with BYE

**What to build:** A deliberate shutdown or link close sends `BYE`, and the peer moves to NO LINK immediately instead of waiting out the 3 s timeout.

**Blocked by:** 06, 07

**Status:** ready-for-agent

- [ ] A `BYE` frame moves the receiver to NO LINK within one heartbeat.
- [ ] A power-cycle without `BYE` still falls back to the timeout path.
- [ ] After `BYE`, a new handshake is accepted without stale session state.
- [ ] The teardown reason is logged on both sides.
