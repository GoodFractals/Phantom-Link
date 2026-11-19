# SFO is an application-layer telemetry channel

StegoFrameOrder reorders 802.11 MAC frames and needs monitor mode plus raw frame injection. The CYW43439 exposes a normal STA/AP interface through `cyw43`, so the paper's mechanism is not available on this hardware. We implement the idea one layer up: four small UDP datagrams whose arrival permutation carries four bits, repeated three times, verified against the message authentication tag.

## Consequences

- The project must never claim MAC-layer SFO or raw-frame access.
- SFO is telemetry-only in v1. If every burst fails, messages still arrive and the log reports `SFO degraded`.
- Decode rate is measured on hardware and reported honestly, including at long range.
- The key-hint variant stays behind `-DSFO_KEY_HINT=1` and off by default.
