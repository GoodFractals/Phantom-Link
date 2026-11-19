# 08: Clean teardown with BYE

**What to build:** A deliberate shutdown or link close sends `BYE`, and the peer moves to NO LINK immediately instead of waiting out the 3 s timeout.

**Blocked by:** 06, 07

**Status:** ready-for-human (code complete; hardware verification pending)

- [x] A `BYE` frame moves the receiver to NO LINK within one heartbeat.
- [x] A power-cycle without `BYE` still falls back to the timeout path.
- [x] After `BYE`, a new handshake is accepted without stale session state.
- [x] The teardown reason is logged on both sides.

## Verification

2026-11-19 — `PROTO_TYPE_BYE` is handled in `handle_frame`: the receiver calls
`set_link_down("BYE")` immediately. `request_teardown()` sends a `BYE` control
frame and tears down locally. `set_link_down()` now resets the session ID to 0
and the message state, so the next HELLO (which carries a fresh ID) is adopted
cleanly. Holding DELETE + BACK for 1.5 s triggers a user-requested teardown.

Host `ctest` 9/9. Both targets build clean under `-Werror`. Hardware
verification joins the ticket 09 run.
