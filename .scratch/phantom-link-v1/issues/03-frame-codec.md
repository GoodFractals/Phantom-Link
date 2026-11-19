# 03: Frame codec with CRC32

**What to build:** A pure encode/decode module for the 18-byte header, 0–64 byte payload, and CRC32 trailer. Malformed input is rejected without reading past the buffer.

**Blocked by:** 01

**Status:** ready-for-agent

- [ ] Host test round-trips frames at every payload length from 0 to 64.
- [ ] Every prefix of a valid frame is rejected.
- [ ] 100k random buffers never crash, hang, or allocate unbounded memory.
- [ ] Bad magic, bad version, bad length, and bad CRC produce distinct status codes.
- [ ] The module has no Pico SDK dependency.
