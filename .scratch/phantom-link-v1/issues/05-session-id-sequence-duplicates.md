# 05: Session ID, sequence, duplicate suppression

**What to build:** Each session gets a hardware-random ID and per-direction sequence numbers. A captured frame replayed later is dropped before the app sees it.

**Blocked by:** 04

**Status:** ready-for-agent

- [ ] Session IDs differ across reboots and are never taken from a counter.
- [ ] A replayed datagram from a host harness is rejected and logged.
- [ ] Out-of-order frames inside the allowed window do not corrupt message assembly.
- [ ] Accepted, duplicate, and stale frames have separate counters on serial.
- [ ] Starting a new session resets the sequence state without carrying over the old one.
