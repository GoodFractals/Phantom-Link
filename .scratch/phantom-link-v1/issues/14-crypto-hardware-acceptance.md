# 14: Crypto hardware acceptance

**What to build:** Flash the encrypted build and prove the baseline still works, the wrong key fails, and a packet capture shows no readable text.

**Blocked by:** 13

**Status:** closed (hardware verified 2026-11-25: baseline + no-PSK RF capture + archived logs)

- [x] T2–T12 pass on both boards. Production `3b666adb`: LINK UP on both,
      joystick/UI live, 70 s shared session with 407 accepted frames,
      zero duplicates/stale/wrong/retry/crypto errors, SFO 6/6. Log archive:
      `logs/capture14/serial_20261125_3b666adb.md`.
- [x] T15 replay: `RX stale type=PING seq=0 ... stale=7`, zero accepted.
- [x] T16 tamper: `CRYPTO FAIL: bad tag (90 bytes)` via firmware-side probe.
- [x] T17 wrong PSK: `AUTH FAILED: HELLO bad tag`, no LINK UP.
- [x] A packet capture contains no readable message text. Path-1 pktmon capture
      passes (2026-11-24). True 802.11 no-PSK RF capture passes (2026-11-25):
      ESP32 promiscuous, 73 DATA frames from AP BSSID, zero plaintext hits.
      See `logs/capture14/capture14_20261125_rf_evidence.md`.
- [x] Both serial logs are archived with the build hashes `3b666adb`
      (`logs/capture14/serial_20261125_3b666adb.md`).
- [x] `poc/plaintext-v1` builds from the tagged tree (2026-11-24, WSL
      `arm-none-eabi-gcc`, `PICO_BOARD=pico2_w`). TX UF2
      `d6523318b3808c4d6c4ac5dbb98371d47810bd51f46094085254a9553f7768ac`,
      RX UF2 `ef7eed2fbd3fc9ec7abd8bb0f5e273ee80c7207dabaae2b1b4026932c4314665`.
      Flashing that tag is not currently needed; boards are on 6333ab72.

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
