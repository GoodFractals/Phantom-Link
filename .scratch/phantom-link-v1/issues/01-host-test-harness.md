# 01: Host test harness, warnings-as-errors, regression checklist

**What to build:** A host-only build target that compiles protocol modules without the Pico SDK, runs under `ctest` on Windows with no board attached, and gives every later ticket a place to prove itself. Firmware targets treat warnings as errors. The baseline from the gameplan becomes a one-screen checklist a human runs after each phase.

**Blocked by:** None (can start immediately)

**Status:** ready-for-agent

- [ ] `ctest` runs a placeholder protocol test on Windows with no board attached and exits zero.
- [ ] Both firmware targets build cleanly with `-Wall -Wextra -Werror`.
- [ ] `docs/REGRESSION.md` lists at most 12 checks covering both boards running the UI, static addressing, handshake, 21-glyph round-trip, non-blocking TX, and GP12 BACK.
- [ ] A fresh clone can run the host tests without setting `PICO_SDK_PATH`.
