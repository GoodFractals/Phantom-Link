# 35: RP2350 hardware hardening

**What to build:** Evaluate and, where safe, enable secure boot, debug-port
lock, and flash encryption so a stolen board is not trivially readable.

**Blocked by:** 14

**Status:** ready-for-agent

- [ ] Document RP2350 OTP secure-boot and debug-disable procedure from datasheet.
- [ ] Evaluate flash-encryption support and key storage on RP2350.
- [ ] Protect against `picotool save` flash readout on a production rig.
- [ ] Provide a reversible development path and an irreversible production path.
- [ ] Hardware acceptance on spare board only; never burn OTP on primary rigs.
- [ ] Record which mitigations stop the wrench attack and which do not.
