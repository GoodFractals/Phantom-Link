# Phantom Link SFO — quick reference

| Concept | Meaning |
|---|---|
| True SFO | Reorders 802.11 **MAC frames**; needs monitor mode + raw frame injection. Not available on CYW43439. |
| Our SFO | Application-layer: 4 UDP datagrams per burst; their **arrival order** encodes 4 bits (one nibble). |
| Burst | Indices 0–3 are sent in an order chosen by a permutation. 16 of 24 permutations map to nibbles 0–15; 8 are unused and rejected. |
| Integrity | CRC8 over context + nibble; 3 repeated bursts resolved by majority decode. |
| Scope | Telemetry only. It reports decode quality; it never carries user message content. |
| Degraded | Failed bursts increment counters and log `SFO degraded`. Messages still deliver normally. |
| Not SFO | Fixed-length padding, decoy chatter, timing jitter, header masking — these are LPI shaping, a separate layer. |
| Honesty | Do not say "covert" until ticket 14 hardware acceptance is green. |

Primary sources: Sawicki et al. 2021 (PMC8471875), `docs/adr/0002`, `docs/GAMEPLAN.md` §4.4, `firmware/proto/proto_sfo.c`.
