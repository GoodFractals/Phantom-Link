# Phantom Link SFO Resources

## Knowledge

- [Paper: StegoFrameOrder—MAC Layer Covert Network Channel for Wireless IEEE 802.11 Networks (Sawicki et al., 2021)](https://pmc.ncbi.nlm.nih.gov/articles/PMC8471875/)
  Primary source for true SFO: covert data is carried by the order of MAC-layer
  frames using non-deterministic MAC behavior. Use for: what real SFO requires.
- [Project ADR 0002: SFO is an application-layer telemetry channel](../../adr/0002-sfo-is-an-application-layer-telemetry-channel.md)
  Authoritative local decision: the CYW43439 exposes only normal STA/AP
  interfaces, so MAC-layer SFO is unavailable. Use for: why we do it one layer up.
- [GAMEPLAN §4.4: crypto envelope and SFO scope](../../GAMEPLAN.md)
  Authoritative spec for the 4-datagram burst, 3x repeat, and telemetry-only rule.
- [Implementation: `firmware/proto/proto_sfo.c`](../../../firmware/proto/proto_sfo.c)
  The exact permutation table, CRC8, and majority decode used by the firmware.
- [Ticket 15: SFO burst channel](../../.scratch/phantom-link-v1/issues/15-sfo-burst-channel.md)
  Acceptance criteria for the primitive; host-tested with 11/11 ctest.
- [Ticket 16: SFO telemetry and degraded mode](../../.scratch/phantom-link-v1/issues/16-sfo-telemetry-and-degraded-mode.md)
  The layer being added now: counters, degraded state, read-only accessor.

## Wisdom (Communities)

- No community recommended yet. The user has not opted into forums; keep this
  section empty unless they ask for external practitioner feedback.

## Gaps

- No measured detection-rate data yet. Ticket 17 produces honest field numbers;
  do not claim stealth before then.
