# 19: Hardware watchdog

**What to build:** A watchdog supervises the main loop. A forced hang resets the board and it returns to LINK UP without human intervention.

**Blocked by:** 14

**Status:** blocked-on-hardware (code committed in 0373663; forced-hang and 1 h soak not yet run on hardware)

- [x] Hardware watchdog armed **after** CYW43/UDP bring-up with 8 s timeout;
      main loop feeds it each tick. Arming before bring-up made slow init trip
      it and boot-loop the pair.
- [x] Reset reason logged as `RESET reason: watchdog` on watchdog reboot.
- [x] Compile-time `PHANTOM_WATCHDOG_TEST` build forces a hang 5 s after LINK UP.
- [ ] Hardware: forced hang resets in under 8 s and re-links.
- [ ] Hardware: normal operation never trips during a 1 h run (covered by ticket 21 soak).
- [ ] Long blocking work is split or fed to the watchdog explicitly.
