# SFO is an application-layer telemetry channel

StegoFrameOrder reorders 802.11 MAC frames and needs monitor mode plus raw frame injection. The CYW43439 exposes a normal STA/AP interface through `cyw43`, so the paper's mechanism is not available on this hardware. We implement the idea one layer up: four small UDP datagrams whose arrival permutation carries four bits, repeated three times, verified against the message authentication tag.

The application-layer variant also uses **bounded transmission jitter** and a **background decoy generator**: harmless `PING`/`SYNC` control frames sent when no user message is active, so traffic is not purely burst-on-demand. Jitter is applied to the TX scheduler, never by changing protocol timing constants or MAC behaviour. Decoys are valid framed control packets that increment sequence and never carry message DATA.

## Consequences

- The project must never claim MAC-layer SFO or raw-frame access.
- SFO is telemetry-only in v1. If every burst fails, messages still arrive and the log reports `SFO degraded`.
- Decode rate is measured on hardware and reported honestly, including at long range.
- The key-hint variant stays behind `-DSFO_KEY_HINT=1` and off by default.
- Jitter and decoys reduce bursty-looking traffic; they do not make a two-party
  Wi-Fi link covert. Do not use "covert" or "stealthy" in docs before ticket 14
  is green on hardware.
- Decoys consume airtime and power; their rate must be measured in ticket 22.
- Decoys must never corrupt message assembly, inflate `msgdrop`, or mask a real
  ACK/retransmit failure.
