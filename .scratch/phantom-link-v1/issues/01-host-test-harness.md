# 01: Host test harness, warnings-as-errors, regression checklist

**What to build:** A host-only build target that compiles protocol modules without the Pico SDK, runs under `ctest` on Windows with no board attached, and gives every later ticket a place to prove itself. Firmware targets treat warnings as errors. The baseline from the gameplan becomes a one-screen checklist a human runs after each phase.

**Blocked by:** None (can start immediately)

**Status:** closed (host harness 18/18; hardware framing in 09)

- [x] `ctest` runs a placeholder protocol test on Windows with no board attached and exits zero.
- [x] Both firmware targets build cleanly with `-Wall -Wextra -Werror`.
- [x] `docs/REGRESSION.md` lists at most 12 checks covering both boards running the UI, static addressing, handshake, 21-glyph round-trip, non-blocking TX, and GP12 BACK.
- [x] A fresh clone can run the host tests without setting `PICO_SDK_PATH`.

## Verification

2026-11-19 â€” `ctest`: 1/1 passed (`spec`). Firmware: both targets built clean with
`-Werror`; UF2 sizes unchanged at 670,720 and 677,376 bytes. The checklist has
12 items. The host build uses MSVC `/W4 /WX` on this machine and
`-Wall -Wextra -Werror` elsewhere.

