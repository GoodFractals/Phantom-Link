# 20: Error counters, STATUS dump, flash persistence

**What to build:** One STATUS report shows link state, sequence window, crypto errors, and SFO stats. Counters survive a power cycle so an untethered run can be audited later.

**Blocked by:** 18, 19

**Status:** closed (hardware verified 2026-11-25: flash write quiesced, record survives power cycle)

- [x] STATUS prints every required field in one block (serial `S` command and boot line).
- [x] Counters saved to a 16-slot flash ring in the final sector, survives power cycle.
      Root cause of the earlier hang: the flash write ran while CYW43's PIO-SPI
      work was in flight. Fixed by bracketing `flash_range_erase/program` with
      `cyw43_arch_lwip_begin/end` (quiesces the async context) and feeding the
      watchdog around the interrupts-off section. Verified: `persist seq`
      0→1 after 60 s, then `persist seq=2 valid=1 slot=2` after a power cycle.
- [x] Flash writes are bounded: one page per minute, sector erased only every 16 writes
      (note: the `PHANTOM_STATUS_SAVE=ON` build was used for the verification;
      the option remains available for builds that don't need persistence).
- [x] A corrupt counter record falls back to zero instead of blocking boot (CRC check).
- [x] The trace is readable after a run with no serial console attached (flash record).
- [x] Hardware: power-cycle and read STATUS, verify counters. 2026-11-25:
      `persist seq=1 valid=1 slot=1` at 70 s; after power cycle
      `persist seq=2 valid=1 slot=2`, LINK UP.
