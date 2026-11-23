# 24: UI layout, numeric page, BAT cell

**What to build:** Named rectangles replace every magic coordinate. The numeric page drops `IN RANGE?`, `SOS`, and the signal dot. The `BAT:` cell stops pretending to be a gauge.

**Blocked by:** 23

**Status:** ready-for-agent

- [ ] Drawing code contains no magic coordinates; every rect is named.
- [ ] `IN RANGE?`, `SOS`, and the signal dot are gone, with their dead handlers removed.
- [ ] The `BAT:` cell shows TX/RX activity or is blank; no fake gauge is drawn.
- [ ] Both pages render correctly on hardware.
- [ ] No protocol module, link module, or timing constant is touched.

## Design note

The blank top rows on **both** pages were deliberate in the original art, left
as headroom for full-width message usage. They are not reserved for the baked
grid. Longer/multi-line messages and raising `MAX_TEXT_CHARS` are intended
consumers of that space; do not treat the blank area as dead art.
