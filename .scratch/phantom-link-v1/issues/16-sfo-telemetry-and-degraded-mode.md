# 16: SFO telemetry and degraded mode

**What to build:** SFO runs beside the session, reports good/total bursts, bits per second, and reorder rate every 30 s, and never gates delivery.

## Verification

2026-11-23 — TDD across four seams: telemetry state, degraded mode, delivery
independence, and report format. New `proto_sfo_telemetry` module with 12/12
host tests and `HOST VALIDATION: PASS`. Firmware wires a 30 s report, SFO burst
TX/RX with CRC8 + majority decode, `SFO degraded` logging, a read-only snapshot,
and a compile-time `SFO_DISABLED` switch. Hardware on build `eb17be76`:
`SFO good=3/3 bps=12 reorder=0 degraded=0` and later `good=5/5`, heartbeat
flowing, no AUTH/CRYPTO failures. Decoy power measurement deferred to 22.

**Blocked by:** 15 (closed)

**Status:** closed

- [x] Serial reports SFO good/total, bits per second, and reorder rate.
- [x] Forcing SFO off changes nothing about message delivery (`SFO_DISABLED`).
- [x] A failed burst is logged as `SFO degraded`, not as a message failure.
- [x] Counters are exposed through a read-only accessor for the UI.
- [x] No SFO failure can block, delay, or reorder a user message.
- [x] A background decoy generator emits harmless `PING`/`SYNC` control frames
  while idle, with a bounded rate. Decoys never carry message DATA and never
  inflate `msgdrop` or mask a real ACK failure.
- [ ] Decoy airtime/power cost is measured in ticket 22.
