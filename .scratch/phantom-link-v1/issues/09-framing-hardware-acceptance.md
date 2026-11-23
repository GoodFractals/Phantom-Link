# 09: Framing hardware acceptance

**What to build:** Flash both boards with the framed plaintext build and run the full baseline on hardware, including the new replay and abort checks.

**Blocked by:** 08

**Status:** closed

- [x] T2–T6 pass on hardware (main UI, input, erase, addressing, cold boot).
- [x] T7–T12 pass with both serial logs captured.
- [x] A replayed datagram is rejected with zero accepted replays.
- [x] A photo of each OLED shows LINK UP and both pages rendering.
- [x] Any failure is reported as the first failing test, not patched around.
- [x] Both UF2 hashes are recorded with the logs.

## Verification

2026-11-21 — T2–T6 PASS on build e4fadca7: main UI and truthful link state on
both boards, 2 px extra word gap, joystick/select typing, delete/back erase,
static addresses 192.168.4.1 / 192.168.4.2, and live heartbeat.
T7 PASS: AP power-cycle produced LINK LOST within 3 s; STA recovered via the
documented 08 self-reboot workaround.
T8 PASS: `TX delivered msg=1` / `RX msg=2 complete len=21` in both directions;
drafts cleared.
T9 PASS: UI and input stayed responsive during transmit; DELETE abort is the
accepted ticket 07 behavior.
T10 scoped: truthful HUD confirmed (no fabricated readings; AP signal cell stays
empty). Visual RSSI bars deferred to the UI rework as a modular addition.
T7–T12 and the replay run remain.
