# 39: BSSID identity binding

**What to build:** Bind the STA to the intended AP by hardware address, with
modular SSID/password/BSSID configuration and runtime identity verification.

**Blocked by:** 14

**Status:** closed (hardware verified 2026-11-23/25; provisioning script verified statically 2026-11-25)

- [x] Modular config in `proto_netcfg.h` + generated `netcfg_local.h`.
- [x] AP boots and prints its BSSID (`AP BSSID: ...`).
- [x] STA falls back to SSID-only join when no BSSID is enrolled.
- [x] Runtime BSSID check logs `BSSID MISMATCH`, increments `bssid`, disconnects.
- [x] Enrolled AP BSSID `88:a2:9e:5b:ff:69` in `netcfg_local.h`.
- [x] Hardware: clean power cycle links with BSSID lock active; `SFO good` advancing.
- [x] Provisioning script for others to generate SSID/password and enroll BSSID:
      `tools/provision_pair.ps1` reads the AP BSSID from serial, generates a
      random `TMOBILE-####` SSID + password, writes `netcfg_local.h`, rebuilds,
      and optionally flashes. Parses clean; `gen_psk.ps1` also verified.

## Verification

2026-11-23 — Windows scan confirmed the AP's advertised BSSID matches the
enrolled value on channel 3. Software resets wedged the CYW43; a true power
cycle restored the link. This is now a documented hardware test lesson.
