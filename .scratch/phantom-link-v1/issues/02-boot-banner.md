# 02: Boot banner with version and build hash

**What to build:** Every boot prints `phantom-link v1 <short-hash> <AP|STA>` before Wi-Fi starts, so a board on a serial line identifies its build and role without probing the flash.

**Blocked by:** None (can start immediately)

**Status:** ready-for-agent

- [ ] AP and STA logs show the banner before any Wi-Fi line.
- [ ] The banner contains the protocol version and a short build hash derived at build time.
- [ ] The hash changes when `main.c` changes and is stable across rebuilds of the same tree.
- [ ] The banner is absent from the UI; it is a serial diagnostic only.
