# 04: Typed frames on the wire

**What to build:** Handshake, heartbeat, and control messages travel as typed frames while keeping their current semantics. A serial log shows type, sequence, and payload length instead of raw tokens.

**Blocked by:** 02, 03

**Status:** ready-for-agent

- [ ] Both boards boot, associate, handshake, and show LINK UP over framed messages.
- [ ] `HELLO`, `HELLO_ACK`, `PING`, and `PONG` use their assigned type codes.
- [ ] A frame with an unknown type is dropped and counted.
- [ ] The baseline checklist still passes on both boards.
- [ ] No raw token parsing remains in the app path.
