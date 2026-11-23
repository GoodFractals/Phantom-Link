# 04: Typed frames on the wire

**What to build:** Handshake, heartbeat, and control messages travel as typed frames while keeping their current semantics. A serial log shows type, sequence, and payload length instead of raw tokens.

**Blocked by:** 02, 03

**Status:** ready-for-human (code complete; hardware criteria close in ticket 09)

- [ ] Both boards boot, associate, handshake, and show LINK UP over framed messages.
- [x] `HELLO`, `HELLO_ACK`, `PING`, and `PONG` use their assigned type codes.
- [x] A frame with an unknown type is dropped and counted.
- [ ] The baseline checklist still passes on both boards.
- [x] No raw token parsing remains in the app path.

## Verification

2026-11-19 — `main.c` now sends and receives `proto_frame` frames with CRC32.
`HELLO`, `HELLO_ACK`, `PING`, and `PONG` are zero-payload control frames; each
Base-6 symbol is a one-byte `DATA` frame and the final marker is a zero-byte
`DATA` frame with `FINAL`. The serial log prints `type`, `seq`, and `len`.
Unknown types increment `rx_unknown_type`; CRC, length, and payload failures
have separate counters. Host `ctest` is 3/3 and `validate_assets.py` rejects any
reintroduced `pkt_eq`/`TOK_` token parser. Both targets build clean under
`-Werror`: AP 672,768 bytes, STA 678,912 bytes.

Still open: flash both boards and confirm LINK UP and message round-trip. That
hardware evidence is the gate in ticket 09, not a compile result.
