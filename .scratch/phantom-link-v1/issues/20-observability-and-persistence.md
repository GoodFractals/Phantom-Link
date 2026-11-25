# 20: Error counters, STATUS dump, flash persistence

**What to build:** One STATUS report shows link state, sequence window, crypto errors, and SFO stats. Counters survive a power cycle so an untethered run can be audited later.

**Blocked by:** 18, 19

**Status:** ready-for-agent

- [x] STATUS prints every required field in one block (serial `S` command and boot line).
- [ ] Counters saved to a 16-slot flash ring in the final sector, survives power cycle.
      **Blocked:** `status_save()` write hangs the RP2350 when CYW43 is active
      (frozen loop at 60 s). Reads work; writes are off by default behind
      `-DPHANTOM_STATUS_SAVE=1` until flash/CYW43 quiescing is fixed.
- [x] Flash writes are bounded: one page per minute, sector erased only every 16 writes
      (implementation intact, disabled by default per the blocker above).
- [x] A corrupt counter record falls back to zero instead of blocking boot (CRC check).
- [x] The trace is readable after a run with no serial console attached (flash record).
- [ ] Hardware: power-cycle and read STATUS, verify counters.
