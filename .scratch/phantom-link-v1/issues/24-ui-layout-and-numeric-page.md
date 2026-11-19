# 24: UI layout, numeric page, BAT cell

**What to build:** Named rectangles replace every magic coordinate. The numeric page drops `IN RANGE?`, `SOS`, and the signal dot. The `BAT:` cell stops pretending to be a gauge.

**Blocked by:** 23

**Status:** ready-for-agent

- [ ] Drawing code contains no magic coordinates; every rect is named.
- [ ] `IN RANGE?`, `SOS`, and the signal dot are gone, with their dead handlers removed.
- [ ] The `BAT:` cell shows TX/RX activity or is blank; no fake gauge is drawn.
- [ ] Both pages render correctly on hardware.
- [ ] No protocol module, link module, or timing constant is touched.
