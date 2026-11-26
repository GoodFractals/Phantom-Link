# 03: Frame codec with CRC32

**What to build:** A pure encode/decode module for the 18-byte header, 0â€“64 byte payload, and CRC32 trailer. Malformed input is rejected without reading past the buffer.

**Blocked by:** 01

**Status:** closed (host codec tests; hardware framing in 09)

- [x] Host test round-trips frames at every payload length from 0 to 64.
- [x] Every prefix of a valid frame is rejected.
- [x] 100k random buffers never crash, hang, or allocate unbounded memory.
- [x] Bad magic, bad version, bad length, and bad CRC produce distinct status codes.
- [x] The module has no Pico SDK dependency.

## Verification

2026-11-19 â€” `ctest` 3/3 (`spec`, `frame`, `banner`). The frame test round-trips
all 65 payload lengths, rejects all 89 proper prefixes of a full frame, fuzzes
100,000 random buffers, and flips every covered bit plus every auth bit.
Tampering a structural field reports that field's status (magic, version, or
length); tampering anything else reports `PROTO_ERR_CRC`. `proto_frame.c`
includes only `string.h`, and the host build never sets `PICO_SDK_PATH`.

