# 02: Boot banner with version and build hash

**What to build:** Every boot prints `phantom-link v1 <short-hash> <AP|STA>` before Wi-Fi starts, so a board on a serial line identifies its build and role without probing the flash.

**Blocked by:** None (can start immediately)

**Status:** closed (hardware: banner observed on both boards before any Wi-Fi line; see `logs/boot_banner_02.md`)

- [x] AP and STA logs show the banner before any Wi-Fi line. Captured on both
      boards during ticket 19 hardware work; see `logs/boot_banner_02.md`.
- [x] The banner contains the protocol version and a short build hash derived at build time.
- [x] The hash changes when `main.c` changes and is stable across rebuilds of the same tree.
- [x] The banner is absent from the UI; it is a serial diagnostic only.

## Verification

2026-11-19 — host test asserts the exact AP and STA strings and truncation
behavior. Build hash is the first eight hex digits of `main.c`'s SHA256 and is
embedded in both ELFs. A temporary source edit changed the embedded hash from
`cc5116ad` to `24aae4da`; reverting and reconfiguring restored `cc5116ad`, and a
second build reported no work. Still pending: one serial boot per board to
confirm the line appears before `AP active` / `STA connecting`.
