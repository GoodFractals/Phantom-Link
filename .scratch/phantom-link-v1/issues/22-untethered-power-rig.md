# 22: Untethered power rig

**What to build:** Mount each 2000 mAh pack under its breadboard, measure current in idle, RX, and TX, prove the pack does not cut off at idle, and measure runtime.

**Blocked by:** None (can start immediately; runs in parallel with the code track)

**Status:** ready-for-human

- [ ] Both rigs run with no PC attached and the cable survives a tug.
- [ ] A current table covers idle, RX, TX, and radio power-save on/off.
- [ ] One hour idle with LINK UP and no auto-cutoff.
- [ ] Runtime is within 20% of the projection; the pack model is recorded.
- [ ] The packs are marked so charge state is readable without unzipping the rig.

## Verification

2026-11-20 — The AP stopped broadcasting `Phantom_Link` from one rear
Optiplex USB port while its UI stayed responsive and its flash verified
byte-for-byte against `phantom_link_ap.elf`. Moving it to the other board's
port restored the AP immediately; the STA then worked fine on the vacated
port. A marginal port/cable cannot sustain the AP's continuous beaconing
current while the MCU and OLED keep running. Check the power path before
blaming firmware or the CYW43, and revisit this when measuring current and
moving to packs.
