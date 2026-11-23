# 18: Reconnect and session replacement

**What to build:** Either board can restart and the pair re-establishes with exponential backoff. The host tears down the old session when the client returns with a new one.

**Blocked by:** 14

**Status:** ready-for-agent

- [ ] AP restart: the STA re-links in under 10 s without touching the STA.
- [ ] STA restart: the AP replaces the session and shows LINK UP with no duplicate state.
- [ ] Ten consecutive power cycles recover every time.
- [ ] Backoff is bounded and visible in the log.
- [ ] A stale session cannot coexist with the replacement.
- [ ] Replace the ticket 08 STA self-reboot workaround with a real reconnect
  that does not replay the boot splash. A deliberate `RECONNECTING` HUD state
  is preferred over silent retries or an unexpected splash.
