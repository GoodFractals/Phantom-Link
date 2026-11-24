# 18: Reconnect and session replacement

**What to build:** Either board can restart and the pair re-establishes with exponential backoff. The host tears down the old session when the client returns with a new one.

**Blocked by:** 14

**Status:** ready-for-agent (host state machine green; firmware integration pending)

- [x] Host reconnect state machine with bounded backoff, epochs, and stale-session rejection.
- [ ] AP restart: the STA re-links in under 10 s without touching the STA (hardware pending).
- [ ] STA restart: the AP replaces the session and shows LINK UP with no duplicate state (hardware pending).
- [ ] Ten consecutive power cycles recover every time (hardware pending).
- [x] Backoff is bounded and exposed for logging (`reconnect_action_name`, `backoff_ms`).
- [x] A stale session cannot coexist with the replacement (host epoch checks).
- [x] Replace the ticket 08 STA self-reboot workaround with a real reconnect
  state machine; no `watchdog_reboot` path remains in `net_tick`.
- [x] Bounded backoff logged as `RECONNECTING attempt=N backoff=Nms state=...`.
- [x] Session epochs adopted on each new session via `reconnect_adopt_epoch`.
- [ ] Deliberate `RECONNECTING` HUD state (parked UI work, ticket 25).
- [ ] Hardware: 10 power cycles, AP/STA restart <10 s, no splash — pending.

## Verification

2026-11-23 — `proto_reconnect` host module with 12 test cases (TDD red→green),
`ctest` 16/16. `main.c` integration deliberately left for a hardware-safe step.
