# 08: Clean teardown with BYE

**What to build:** A deliberate shutdown or link close sends `BYE`, and the peer moves to NO LINK immediately instead of waiting out the 3 s timeout.

**Blocked by:** 06, 07

**Status:** closed

- [x] A `BYE` frame moves the receiver to NO LINK within one heartbeat.
- [x] A power-cycle without `BYE` still falls back to the timeout path.
- [x] After `BYE`, a new handshake is accepted without stale session state.
- [x] The teardown reason is logged on both sides.
- [x] The gesture is now a 2.5 s hold of both physical buttons (select +
  delete/back), with typing and erasing suppressed while the chord is held.

## Verification

2026-11-19 — `PROTO_TYPE_BYE` is handled in `handle_frame`: the receiver calls
`set_link_down("BYE")` immediately. `request_teardown()` sends a `BYE` control
frame and tears down locally. `set_link_down()` now resets the session ID to 0
and the message state, so the next HELLO (which carries a fresh ID) is adopted
cleanly. Holding the delete/back button for 1.5 s triggers a user-requested
teardown. The rig has two physical buttons (select and delete/back), so the
original two-button chord was replaced. A single-button hold proved too easy
to trigger while erasing, and a stick-left combo proved unreliable because
the joystick direction is wired inverted. On 2026-11-21 it became a 2.5 s
hold of **both** physical buttons together, which is deliberate and does not
depend on page or stick orientation.

Host `ctest` 9/9. Both targets build clean under `-Werror`. Hardware
verification joins the ticket 09 run.

2026-11-21 — hardware: BYE chord PASS (peer shows `LINK LOST (BYE)` immediately
and re-links with a new session). Power-cycle case: the peer drops within 3 s,
but the STA could not re-associate after the AP returned. Verified that a full
STA reboot always recovers it; driver-level `connect_async`, leave/rejoin,
STA-mode disable/enable, ARP flush, and `cyw43_ll_wifi_rejoin()` all failed.
Accepted workaround: STA self-reboots after 3 * WIFI_RETRY_MS (15 s) without a
link. Known limitation: the splash screen flashes during recovery; ticket 18
replaces this with a proper reconnect and a deliberate RECONNECTING status.
