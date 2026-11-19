# 16: SFO telemetry and degraded mode

**What to build:** SFO runs beside the session, reports good/total bursts, bits per second, and reorder rate every 30 s, and never gates delivery.

**Blocked by:** 15

**Status:** ready-for-agent

- [ ] Serial reports SFO good/total, bits per second, and reorder rate.
- [ ] Forcing SFO off changes nothing about message delivery.
- [ ] A failed burst is logged as `SFO degraded`, not as a message failure.
- [ ] Counters are exposed through a read-only accessor for the UI.
- [ ] No SFO failure can block, delay, or reorder a user message.
