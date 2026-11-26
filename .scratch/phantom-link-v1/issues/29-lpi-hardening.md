# 29: Application-layer LPI hardening

**What to build:** Make the on-air traffic as unremarkable as this hardware allows, without claiming covertness. All items are application-layer; the CYW43439 MAC/PHY stays untouched.

**Blocked by:** 14 (hardware validation runs against this final build)

**Status:** ready-for-agent

- [x] DATA frames are always 64-byte padded plaintext before encryption, so wire payload length is constant.
- [x] Padding is random; real length is carried inside the encrypted payload.
- [x] A bounded-jitter constant-rate decoy generator emits harmless control frames while idle.
- [x] Decoys never carry message DATA and never inflate `msgdrop` or mask ACK/retransmit failures.
- [ ] Airtime and power cost of decoys is measured in ticket 22.
- [x] SSID renamed to `TMOBILE-1219`; replay tooling updated.
- [x] TX-power reduction investigated: Pico SDK 2.3.1 exposes no CYW43 TX-power API, so default unchanged.
- [x] Session/seq/ack header fields are masked with a key-derived stream so frame metadata is not plaintext.
- [x] A sniffer-based detection report is produced with ticket 14 evidence.
      `.scratch/phantom-link-v1/evidence/29-sniffer-detection-report.md`:
      re-analysis of the password-less ESP32 capture found 73 constant-length
      DATA frames (110 bytes on air), 7.615 bits/byte payload entropy and zero
      known-plaintext hits. No detection probability or jitter claim is made —
      the capture has no timestamps. Still not covert (SSID/BSSID/MAC visible).

## Explicit non-goals

- No hidden SSID, no MAC randomization, no "covert" or "stealthy" claims in docs.
- No MAC-layer SFO; the CYW43439 is a closed black box (ADR 0002).

## Verification

2026-11-22 — scope set from user direction. Implemented before ticket 14 so the
hardware acceptance run validates the final wire format.
