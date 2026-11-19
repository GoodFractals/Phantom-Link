# Phantom Link — agent guide

Two Raspberry Pi Pico 2 W boards exchange messages over a private Wi-Fi link using the **Bonnie & Clyde** protocol. One `firmware/main.c` is compiled twice: AP (Clyde) and STA (Bonnie). Both boards run the full OLED, joystick, and button UI.

`docs/GAMEPLAN.md` is authoritative for phase order, frame layout, crypto envelope, SFO scope, and the test matrix. `README.md` describes what exists today. Read both before changing protocol behavior.

## Build

Verified on native Windows with Pico SDK 2.3.1, CMake, Ninja, and Arm GNU Toolchain 14.2. `PICO_SDK_PATH`, `pioasm`, `picotool`, `ninja`, and the toolchain must be on `PATH`.

```powershell
$env:PICO_SDK_PATH = "C:\Users\<you>\.pico-sdk\sdk\2.3.1"
cmake -S firmware -B firmware/build -G Ninja -DPICO_BOARD=pico2_w "-DPICO_SDK_PATH=$env:PICO_SDK_PATH"
cmake --build firmware/build -j4
```

Artifacts: `firmware/build/phantom_link_ap.uf2` and `firmware/build/phantom_link_sta.uf2`. Flash by holding BOOTSEL, plugging USB, and copying the matching UF2 to the `RPI-RP2` volume.

Host checks: `python firmware/tests/validate_assets.py`.

## Hard rules

- One `main.c`, two targets. `IS_AP` selects only the Wi-Fi role.
- ADC init, GPIO setup, input polling, and `drawMainUI()` are unconditional and run before Wi-Fi.
- Both roles read input and run the UI. Never gate UI code on `IS_AP`.
- Every phase ends with the baseline in `docs/GAMEPLAN.md` §2 still green on hardware. Revert a change that breaks it; do not "fix it later".
- UI work (Phase 6) may not edit `proto_*`, `net_link.c`, or timing constants.
- No silent fallbacks. A crypto or SFO failure is logged and counted; it never downgrades to plaintext.
- `poc/plaintext-v1` is the known-good fallback tag. Keep it buildable.

## Domain docs

`GLOSSARY.md` and `docs/adr/` hold the shared vocabulary and the decisions that are hard to reverse. Read them before naming new concepts or proposing protocol changes.

## Agent skills

### Issue tracker

Issues and specs live as markdown under `.scratch/<feature>/`. See `docs/agents/issue-tracker.md`.

### Triage labels

Five canonical labels: `needs-triage`, `needs-info`, `ready-for-agent`, `ready-for-human`, `wontfix`. See `docs/agents/triage-labels.md`.

### Domain docs

Single-context: `GLOSSARY.md` plus `docs/adr/` at the repo root. See `docs/agents/domain.md`.
