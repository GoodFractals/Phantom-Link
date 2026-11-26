# Ticket 27 — clean-checkout build evidence

**Date:** 2026-11-26. **Scope:** fresh worktree at HEAD `2f1f363`, no reuse of
the developer build directory. No hardware touched, no flashing.

## Command

```powershell
$env:PATH="C:\armtc\bin;$($env:PATH)"
$env:PICO_SDK_PATH='C:\Users\Windows 10\.pico-sdk\sdk\2.3.1'
git worktree add <temp> HEAD
cmake -S <temp>\firmware -B <temp>\firmware\build -G Ninja `
  -DCMAKE_MAKE_PROGRAM='C:\Users\Windows 10\.pico-sdk\ninja\v1.13.2\ninja.exe'
cmake --build <temp>\firmware\build
```

Toolchain: Pico SDK 2.3.1, Ninja 1.13.2, Arm GNU Toolchain 14.2 (`C:\armtc\bin`),
CMake 4.0.0-rc1. `PICO_BOARD=pico2_w`, `PICO_PLATFORM=rp2350-arm-s`.

## Result

| Check | Result |
|---|---|
| Configure | success |
| Build both targets | success, exit 0 |
| Compiler warnings | 0 |
| `phantom_link_ap.uf2` SHA-256 | `DAA693330A38C48DA902CDCA3DA6AF1A4E30839CAD19B3445B4DE5FF09DF07AB` |
| `phantom_link_sta.uf2` SHA-256 | `AD8F7C4FC59F702CAEDEE37A543D9ADEF0D88339AA8FFAC8F558BC09808690BA` |

Hashes differ from the soak build because the build directory path differs;
the boot banner hash is derived only from `firmware/main.c` and is unchanged.

## Bug found by this gate

The first clean build **failed**: `pico/cyw43_arch.h: No such file or directory`.
`PICO_BOARD` was set to `pico2_w` *after* `include(pico_sdk_import.cmake)`, and
the SDK import had already seeded the cache with the generic `pico` board — so
a fresh checkout configured for rp2040 and failed. The developer's build cache
had `pico2_w` from an earlier explicit `-D`, which hid the defect. Fixed by
moving `set(PICO_BOARD pico2_w CACHE STRING ...)` before the SDK import
(commit `2f1f363`). Re-running from a brand-new worktree passed.

## Gate status

This proves "the current tree builds both targets from a clean checkout with
zero warnings". The ticket still requires `poc/plaintext-v1` and a `v1.0` tag
to build too; no `v1.0` tag exists yet, so the checkbox remains open.
