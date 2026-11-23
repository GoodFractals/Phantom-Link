# 05: Session ID, sequence, duplicate suppression

**What to build:** Each session gets a hardware-random ID and per-direction sequence numbers. A captured frame replayed later is dropped before the app sees it.

**Blocked by:** 04

**Status:** ready-for-human (code complete; reboot-ID observation joins ticket 09)

- [ ] Session IDs differ across reboots and are never taken from a counter.
- [x] A replayed datagram from a host harness is rejected and logged.
- [x] Out-of-order frames inside the allowed window do not corrupt message assembly.
- [x] Accepted, duplicate, and stale frames have separate counters on serial.
- [x] Starting a new session resets the sequence state without carrying over the old one.

## Verification

2026-11-19 — `proto_session` adds a 32-bit sliding replay window, per-direction
transmit sequence starting at 0, and accepted/duplicate/stale/wrong-session
counters. `main.c` draws the session ID from `get_rand_32()` on every handshake
attempt, adopts the initiator's ID on `HELLO`, and invalidates an in-progress
message on a sequence gap rather than surfacing garbled text. A 30 s `SESSION
stats` line reports all counters plus gap and late-frame counts.

Host `ctest` is 6/6. `test_session` covers ordered, out-of-order, window-edge,
wrong-session, reset, and 10k replay cases. `test_wire` pushes real encoded
frames through CRC and the session gate: 1,000 replays are accepted zero times,
and a corrupted payload never reaches the session. `test_handshake` drives two
session objects through HELLO/ACK and interleaved heartbeats. Both targets build
clean under `-Werror`: AP 675,840 bytes, STA 682,496 bytes.

Still open: flash both boards and observe different session IDs after two
reboots. That joins the ticket 09 hardware run.
