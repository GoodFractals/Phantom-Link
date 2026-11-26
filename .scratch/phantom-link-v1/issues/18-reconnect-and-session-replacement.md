# 18: Reconnect and session replacement

**What to build:** Either board can restart and the pair re-establishes with exponential backoff. The host tears down the old session when the client returns with a new one.

**Blocked by:** 14

**Status:** closed (hardware verified 2026-11-25: 10/10 AP restarts + 10/10 STA restarts, all re-linked with fresh sessions)

- [x] Host reconnect state machine with bounded backoff, epochs, and stale-session rejection.
- [x] AP restart: the STA re-links in under 10 s without touching the STA.
      10/10 hardware cycles, all PASS (`logs/reconnect18/cycles_ap_v2.md`).
- [x] STA restart: the AP replaces the session and shows LINK UP with no duplicate state.
      10/10 hardware cycles, all PASS (`logs/reconnect18/cycles_sta_final.md`).
- [x] Ten consecutive power cycles recover every time (hardware: 20 total,
      10 per direction, all PASS).
- [x] Backoff is bounded and exposed for logging (`reconnect_action_name`, `backoff_ms`).
- [x] A stale session cannot coexist with the replacement (host epoch checks).
- [x] Replace the ticket 08 STA self-reboot workaround with a real reconnect
  state machine; no `watchdog_reboot` path remains in `net_tick`.
- [x] Bounded backoff logged as `RECONNECTING attempt=N backoff=Nms state=...`.
- [x] Session epochs adopted on each new session via `reconnect_adopt_epoch`.
- [ ] Deliberate `RECONNECTING` HUD state (parked UI work, ticket 25; not a
      blocker for this ticket's protocol acceptance).
- [x] Hardware: 10 power cycles, AP/STA restart <10 s, no splash. Three defects
      found and fixed during this test: (1) `wifi_join_sta` hardcoded channel 3
      though the AP picks its channel at boot — now `CYW43_CHANNEL_NONE`;
      (2) `wifi_service` returned early when CYW43 said LINK_UP but the static
      IP was not restored after AP restart — now re-applies `sta_use_static_ip`;
      (3) `set_link_down("timeout")` did not force a Wi-Fi leave/re-join, so the
      STA sent HELLO into a dead association — now re-associates and re-IPs.

## Verification

2026-11-23 — `proto_reconnect` host module with 12 test cases (TDD red→green),
`ctest` 16/16. `main.c` integration deliberately left for a hardware-safe step.
