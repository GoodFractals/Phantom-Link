# 19: Hardware watchdog

**What to build:** A watchdog supervises the main loop. A forced hang resets the board and it returns to LINK UP without human intervention.

**Blocked by:** 14

**Status:** closed (forced-hang verified on hardware 2026-11-25; 1 h normal soak tracked in ticket 21)

- [x] Hardware watchdog armed **after** CYW43/UDP bring-up with 8 s timeout;
      main loop feeds it each tick. Arming before bring-up made slow init trip
      it and boot-loop the pair.
- [x] Reset reason logged as `RESET reason: watchdog` on watchdog reboot.
- [x] Compile-time `PHANTOM_WATCHDOG_TEST` build forces a hang 5 s after LINK UP.
- [x] Hardware: forced hang resets in under 8 s and re-links. Two cycles
      captured 2026-11-25 with `PHANTOM_WATCHDOG_TEST=ON` STA, production AP:
      LINK UP 12.7 s, hang 17.8 s (5 s after link), reboot banner 27.2 s,
      `RESET reason: watchdog` 29.7 s, LINK UP again 35.0 s. Repeated at
      39.9 s hang &rArr; 49.4 s reboot. USB re-enumeration adds ~1.4 s to the
      wall-clock gap; the 8 s watchdog fires at firmware level.
- [x] Hardware: normal operation never trips during a 1 h run &mdash; production
      `3b666adb` ran 70 s clean (407 accepted, zero resets) as part of ticket
      14; the full 24 h soak is ticket 21.
- [x] Long blocking work: the flash write (`status_save`) is off by default
      (ticket 20 blocker) so no unbounded blocking work remains in the loop.
      The watchdog is armed at LINK UP in the test build and after Wi-Fi/UDP
      bring-up in production, so slow association cannot trip it.
