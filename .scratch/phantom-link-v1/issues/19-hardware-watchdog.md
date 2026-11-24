# 19: Hardware watchdog

**What to build:** A watchdog supervises the main loop. A forced hang resets the board and it returns to LINK UP without human intervention.

**Blocked by:** 14

**Status:** ready-for-agent

- [x] Hardware watchdog enabled at boot with 8 s timeout; main loop feeds it each tick.
- [x] Reset reason logged as `RESET reason: watchdog` on watchdog reboot.
- [x] Compile-time `PHANTOM_WATCHDOG_TEST` build forces a hang 5 s after LINK UP.
- [ ] Hardware: forced hang resets in under 8 s and re-links.
- [ ] Hardware: normal operation never trips during a 1 h run (covered by ticket 21 soak).
- [ ] Long blocking work is split or fed to the watchdog explicitly.
