# 28: Link states need an unmistakable visual reference

**What to build:** The HUD link cell draws `LINK UP` and `NO LINK` as clearly different shapes, and both states are captured on hardware so an observer can recognize the unlinked state without guessing.

**Blocked by:** 25

**Status:** ready-for-agent (non-urgent UI polish)

- [ ] `LINK UP` uses the filled indicator; `NO LINK` uses a visibly different indicator, not just a missing fill.
- [ ] The unlinked state is captured with the peer powered off — not inferred from code or a host mock.
- [ ] The two states stay distinguishable at arm's length in a photo.
- [ ] HUD art only: no `proto_*`, `net_link.c`, or timing constant is touched.
