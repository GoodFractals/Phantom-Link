# 09: Framing hardware acceptance

**What to build:** Flash both boards with the framed plaintext build and run the full baseline on hardware, including the new replay and abort checks.

**Blocked by:** 08

**Status:** ready-for-human

- [ ] T2–T12 pass with both serial logs captured.
- [ ] A replayed datagram is rejected with zero accepted replays.
- [ ] A photo of each OLED shows LINK UP and both pages rendering.
- [ ] Any failure is reported as the first failing test, not patched around.
- [ ] Both UF2 hashes are recorded with the logs.
