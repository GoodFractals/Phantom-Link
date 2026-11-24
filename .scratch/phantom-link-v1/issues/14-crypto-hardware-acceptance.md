# 14: Crypto hardware acceptance

**What to build:** Flash the encrypted build and prove the baseline still works, the wrong key fails, and a packet capture shows no readable text.

**Blocked by:** 13

**Status:** ready-for-human

- [ ] T2–T12 pass on both boards (plaintext pass on e4fadca7; encrypted link
      re-verified on 6333ab72 via PING/PONG and SFO, full matrix not rerun).
- [x] T15 replay: `RX stale type=PING seq=0 ... stale=7`, zero accepted.
- [x] T16 tamper: `CRYPTO FAIL: bad tag (90 bytes)` via firmware-side probe.
- [x] T17 wrong PSK: `AUTH FAILED: HELLO bad tag`, no LINK UP.
- [ ] A packet capture contains no readable message text (no sniffer available).
- [ ] Both serial logs are archived with the build hashes.
- [ ] `poc/plaintext-v1` still builds and flashes from the tagged tree.

## Verification

2026-11-23/24 — firmware probe mode (compile-time, STA only) injected a stale
PING and a single-bit-tampered DATA frame; the AP logged the rejections above.
Wrong-PSK STA was flashed, power-cycled, and produced `AUTH FAILED`; the
normal STA UF2 was then restored and the pair re-linked (PING/PONG flowing).
PC→Pico UDP injection was blocked in this environment, hence the firmware probe.
Note: overnight repeated resets were the ticket-08 self-reboot workaround firing
on link loss; after a clean power cycle the link is stable. SFO decode on the
STA showed `0/21 degraded` while AP showed `6/6` — telemetry-only asymmetry
to investigate under ticket 17; it does not affect delivery.
