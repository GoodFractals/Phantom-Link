# Ticket 29 — sniffer-based detection report (from ticket 14 evidence)

**Date:** 2026-11-26. **Scope:** re-analysis of the existing password-less ESP32
promiscuous capture from ticket 14. No new RF capture was taken, nothing was
flashed, and no hardware was touched.

## Source

| Item | Value |
|---|---|
| Capture | `logs/capture14/esp32_capture_20261125_145804.txt` |
| SHA-256 | `2D7DBB3D3A3A741DA1688804765795E62D5077C28D4761C0035FB586FDFFED6E` |
| Build on boards | `3b666adb` (production, Speck CTR+CMAC) |
| Observer | ESP32 promiscuous, not associated, no Wi-Fi password, no app PSK |
| Analyzer | `py -3 tools/analyze_sniffer_capture.py <capture>` |

## Method

Parse each `FRAME <hex>` line, keep 802.11 DATA frames in which any address is
the enrolled AP BSSID `88:a2:9e:5b:ff:69`, then inspect lengths and byte
entropy of the region after the 24-byte MAC header. The capture contains no
timestamps, so **inter-frame timing and jitter were not measured** here.

## Results

| Check | Result |
|---|---|
| FRAME lines parsed | 560 |
| DATA frames from enrolled AP BSSID | 101 |
| Long (110-byte) DATA frames | 73 |
| Short (30-byte) DATA frames | 28 |
| Distinct payload lengths after MAC header | 2 (86 bytes and 6 bytes) |
| Length of all 73 long frames | identical: 110 bytes on air, 86-byte payload region |
| Entropy of those 73 payloads (6,278 bytes) | 7.615 bits/byte (max 8.000) |
| Known message plaintext hits | 0 (per ticket 14 evidence report) |

## What this proves

- An observer with no Wi-Fi password and no app PSK sees **constant-length**
  application DATA frames: all 73 are exactly the same size, so message length
  does not leak through frame size. That is the ticket 29 padding claim, checked
  from the radio side rather than from source.
- The payload region is high-entropy and contains none of the known message
  strings. Combined with ticket 14's WPA2-decrypted pktmon capture, the claim
  "no readable message text" holds on both sides of the Wi-Fi layer.
- The short 6-byte frames are control traffic; their payload is a fixed
  `07 00 00 00 78 56` pattern, not message data.

## What this does NOT prove

- **No detection probability.** This is not a labeled detect/don't-detect
  experiment. Ticket 17 owns field measurement; no rate is claimed here.
- **No jitter measurement.** The capture has no timestamps, so the bounded-jitter
  decoy scheduler's timing effects were not measured.
- **No airtime or power cost.** Ticket 22 owns that measurement.
- **Not covert.** The SSID, BSSID, MAC header, frame sizes and the existence of
  traffic are plainly visible. This is LPI-oriented shaping, not MAC-layer SFO
  (tickets 15, 16, 29; Lesson 001).
- The 61 chance runs of printable ASCII in random bytes are not message text;
  the meaningful check is the zero hits for known plaintext strings.

## Gate status

This closes ticket 29's checkbox "A sniffer-based detection report is produced
with ticket 14 evidence". Ticket 29 remains open because "Airtime and power cost
of decoys is measured in ticket 22" is still outstanding.
