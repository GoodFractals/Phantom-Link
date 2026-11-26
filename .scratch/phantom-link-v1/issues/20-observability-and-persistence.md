# 20: Error counters, STATUS dump, flash persistence

**What to build:** One STATUS report shows link state, sequence window, crypto errors, and SFO stats. Counters survive a power cycle so an untethered run can be audited later.

**Blocked by:** 18, 19

**Status:** closed (hardware verified 2026-11-26: flash over-read fixed, save interval fixed, one write/minute, record survives power cycle with `valid=1` on both boards)

- [x] STATUS prints every required field in one block (serial `S` command and boot line).
- [x] Counters saved to a 16-slot flash ring in the final sector, survives power cycle.
      **Root cause of the 2026-11-26 hang:** `flash_range_program(offset, &rec, 256)`
      programmed a 256-byte page from an 88-byte struct — a 168-byte stack over-read.
      The "earlier verified fixed" result was a stale-UF2 artifact. Fix: build a full
      256-byte RAM page (`status_page`), copy the record into it, and call the
      SDK-documented `flash_safe_execute()` instead of a raw critical section
      (`cyw43_arch_lwip_begin/end` are no-ops in the poll build). A second bug was
      found in the same pass: `status_next_save_ms = now` made every loop iteration
      write (4466 writes in 70 s); changed to `now + 60000`. Verified: 70 s run
      `seq=0 → 1 valid=1` with exactly one write; after a physical power cycle
      both boards boot with `persist seq=4 valid=1 slot=4` — `status_valid` can
      only be set at boot by `status_load()` reading a valid flash record.
- [x] Flash writes are bounded: one page per minute, sector erased only every 16 writes
      (note: the `PHANTOM_STATUS_SAVE=ON` build was used for the verification;
      the option remains available for builds that don't need persistence).
- [x] A corrupt counter record falls back to zero instead of blocking boot (CRC check).
- [x] The trace is readable after a run with no serial console attached (flash record).
- [x] Hardware: power-cycle and read STATUS, verify counters. 2026-11-25:
      `persist seq=1 valid=1 slot=1` at 70 s; after power cycle
      `persist seq=2 valid=1 slot=2`, LINK UP.
