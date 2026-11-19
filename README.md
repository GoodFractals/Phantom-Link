# Phantom Link: Routerless Wireless Link [Handshake PoC]

Phantom Link is a standalone, bidirectional wireless link built from two Raspberry Pi Pico 2 W boards. Both boards carry the same BOM: an SH1106 OLED, a PSP-1000 analog joystick on GP26/GP27, and SELECT/DELETE/BACK buttons on GP14/GP15/GP12. One board hosts a private Wi-Fi network; the other joins it. No router, access point, or phone is required.

This is a handshake-first proof of concept. The link is **plaintext and unauthenticated** right now: the firmware implements the OLED UI, input, character codec, UDP transport, typed frames with CRC32, peer learning, heartbeat, and timeout. Encryption, authentication, replay protection, and true MAC-layer steganography are **planned but not implemented**. CRC32 catches corruption; it is not a security check. The project is presented as an embedded-systems prototype, not as a finished secure communicator.

## Status (2026-11-19)

**Working in source and host-verified**
- 128×64 SH1106 SPI driver with a 1024-byte framebuffer and custom variable-width font.
- Alpha and Numeric UI pages, joystick navigation, SELECT/DELETE edge handling, and multi-tap text entry.
- Full 49-character transport alphabet: 48 drawable glyphs plus space.
- 3-digit Base-6 character encoding, three symbols per character, 80 ms per symbol.
- Typed frames on the wire: 18-byte header, 0–64 byte payload, 8-byte auth field carrying a CRC32. `HELLO`, `HELLO_ACK`, `PING`, `PONG`, and `DATA` use their assigned type codes; `DATA` with `FINAL` ends a message.
- Session layer: each handshake gets an RP2350 hardware-random session ID, each direction has its own sequence starting at 0, and a 32-bit sliding replay window drops duplicates and stale frames before the app sees them.
- Peer address learning from the received UDP packet; no hardcoded STA reply address.
- Static STA address `192.168.4.2/24` on the AP's subnet, because the SDK AP build has no DHCP server; link-status transitions are logged.
- 1 s heartbeat, 3 s link timeout, `LINK UP` / `LINK LOST` serial logs, and link-state LED.
- Non-blocking transmit state machine; `cyw43_arch_poll()` runs continuously.
- Boot splash from `boot_ui.h`, restored UI after boot, and draft/response text on separate rows.
- Runtime status HUD on the numeric page: four-bar RSSI on the STA and a filled/hollow link bullet beside `LINK`. The battery cell is reserved but has no sensor yet.
- Two build targets from one `main.c`: `phantom_link_ap` (`IS_AP=1`) and `phantom_link_sta` (`IS_AP=0`). `IS_AP` selects only the Wi-Fi role; the OLED/input path is unconditional in both.
- Host validation: font table, all 49 codec values, a 21-character round-trip, `V` reachability, the corrected numeric hitboxes, and the unconditional UI/input path pass. `ctest` covers the frame codec, the replay window, a replayed-datagram harness, and a two-endpoint handshake simulation.

**Hardware status**
- Both UF2s build warning-free with Pico SDK 2.3.1 on native Windows, with `-Wall -Wextra -Werror`.
- `firmware/build/phantom_link_ap.uf2` (678,912 bytes, SHA256 `164A9E435075584C7A07D05BC60471B703B568647ADD41C3005B791750B08DE5`) and `firmware/build/phantom_link_sta.uf2` (685,056 bytes, SHA256 `0BCAD12924F865D5D1333CA03B8AC725E78D7599BDD381D5E46871E9B2C1D75A`) are the current framed builds.
- **No board was connected during the build session.** The framed build still needs the T2–T12 matrix, including handshake and bidirectional messaging, on both boards. See `docs/REGRESSION.md` and the audit matrix in `docs/PHANTOM_LINK_AUDIT_AND_HANDOFF.html` §8.

**Not implemented yet**
- Encryption (Speck128/128 CTR + CMAC is planned), key exchange, authentication, replay authentication (the window is not a MAC).
- ACK, whole-message retransmission, duplicate suppression, and a 20-attempt bounded retry with a `NO ACK` UI notice.
- Clean `BYE` teardown, transmit queue.
- MAC-layer StegoFrameOrder; the current SFO is an application-layer packet-name PoC.
- Battery monitoring (power-bank LEDs are the gauge), enclosure, range/power measurements, antenna modification.

## Architecture

The CYW43439/Pico SDK supports AP and STA roles, not ad-hoc/IBSS. Exactly one board must therefore be the AP. That does not require two codebases:

- **AP/STA** is the radio role.
- **TX/RX** is what a packet is doing.
- Both roles compile the same transceiver, UI, input, codec, and parser path. `IS_AP` selects only the Wi-Fi role; both boards can type, send, receive, decode, and render.

One `firmware/main.c` is compiled twice. `phantom_link_ap` hosts the private network; `phantom_link_sta` joins it and sends the first `HELLO`. The STA learns the AP address from `HELLO_ACK`; the AP learns the STA address from the received datagram. The AP build in this SDK has no DHCP server, so the STA uses the fixed address `192.168.4.2` on the AP's `192.168.4.0/24` subnet; DHCP is stopped at boot. Wi-Fi bring-up is non-blocking: the UI and input loop start before a STA join completes, and a failed join retries in the main loop.

Network defaults:

| Setting | Value |
|---|---|
| SSID | `Phantom_Link` |
| Password | `12345678` |
| UDP port | `4444` |
| AP address | `192.168.4.1` |
| STA address | `192.168.4.2` (static; AP has no DHCP server) |

The SSID/password are development defaults, not a security model. Change them before using the device outside a bench.

## Protocol

Every UDP datagram is one binary frame. The layout is fixed so Phase 2 can replace the CRC field with a MAC without changing the wire size:

| Offset | Size | Field |
|---|---|---|
| 0 | 2 | magic `B`, `C` |
| 2 | 1 | version (1) |
| 3 | 1 | type |
| 4 | 1 | flags |
| 5 | 1 | payload length (0–64) |
| 6 | 4 | session ID (hardware-random per handshake) |
| 10 | 4 | sequence (per direction, starts at 0) |
| 14 | 4 | ACK (zero until ticket 06) |
| 18 | n | payload |
| 18+n | 8 | auth field: CRC32 then four reserved zero bytes |

| Type | Code | Direction | Meaning |
|---|---:|---|---|
| `HELLO` | 1 | STA → AP | Request a link; AP learns the peer address |
| `HELLO_ACK` | 2 | AP → STA | Accept; STA learns the AP address |
| `DATA` | 3 | Both | One Base-6 symbol, or zero-byte payload with `FINAL` to end a message |
| `PING` | 5 | Both | 1 s heartbeat |
| `PONG` | 6 | Both | Heartbeat reply |

`ACK`, `BYE`, `SFO`, and `ERROR` have assigned codes but no behaviour yet. A frame with an unknown type is dropped and counted. A structure or CRC failure is logged with its status and counted separately.

A session starts with `HELLO`, which carries a session ID drawn from the RP2350 hardware RNG. Both directions start their own sequence at 0. The receiver keeps a 32-bit sliding bitmap: a sequence at or below `max-32` is stale, and one whose bit is already set is a duplicate; both are counted and dropped. If the sequence jumps, the receiver invalidates the in-progress message and discards symbols until the next `FINAL` rather than surfacing garbled text. Until ticket 06 adds ACK and retransmit, a lost symbol therefore loses the whole message instead of corrupting it.

Data symbols are the Base-6 digits 0–5, sent one per `DATA` frame. A character is `d0*36 + d1*6 + d2` over the alphabet:

```text
" ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789=+-.,?!()[]\""
```

CRC32 covers the header and payload. It catches corruption, not an attacker: there is no encryption or authentication yet, and the replay window is not a MAC. A lost symbol can still desynchronize a message; the receiver resets the three-symbol accumulator after a 500 ms gap and drops a partial final glyph at `FINAL`.

## Hardware

- 2× Raspberry Pi Pico 2 W (RP2350 + CYW43439).
- 2× Inland 1.3" SH1106 128×64 SPI OLED.
  - CLK GP18, MOSI GP19, RES GP20, DC GP16, CS GP17.
  - SH1106 column offset +2.
- PSP-1000 analog joystick: X GP26/ADC0, Y GP27/ADC1.
- SELECT GP14, DELETE GP15, BACK GP12; active-low to GND with internal pull-ups. BACK is an alias for DELETE (backspace / abort transmit).
- USB power for bench testing. The AP + OLED load has not yet been characterized on battery.

## Build

The verified path is native Windows with Pico SDK 2.3.1, CMake 4.0.0-rc1, Ninja 1.13.2, and Arm GNU Toolchain 14.2. Put the SDK's `pioasm`, `picotool`, Ninja, and `arm-none-eabi` tools on `PATH`; otherwise CMake tries to build the host tools from source.

```powershell
$env:PICO_SDK_PATH = "C:\Users\<you>\.pico-sdk\sdk\2.3.1"
$env:PATH = "C:\Users\<you>\.pico-sdk\tools\2.3.1\pioasm;C:\Users\<you>\.pico-sdk\picotool\2.3.1\picotool;C:\Users\<you>\.pico-sdk\ninja\v1.13.2;C:\Program Files (x86)\Arm GNU Toolchain arm-none-eabi\14.2 rel1\bin;$env:PATH"

Set-Location "C:\Users\<you>\OneDrive\Desktop\Phantom_Link"
cmake -S firmware -B firmware/build -G Ninja -DPICO_BOARD=pico2_w "-DPICO_SDK_PATH=$env:PICO_SDK_PATH"
cmake --build firmware/build -j4

Get-Item firmware\build\phantom_link_ap.uf2, firmware\build\phantom_link_sta.uf2 |
    Select-Object Name, Length
```

The full audit contains the exact commands used on the build machine and a WSL alternative (unverified in the build session).

Host-side asset validation does not need the SDK or a board:

```powershell
python firmware\tests\validate_assets.py
```

Expected final line: `HOST VALIDATION: PASS`.

## Flash

Hold **BOOTSEL**, plug in USB, and copy the matching UF2 to the `RPI-RP2` volume. The board reboots automatically.

```powershell
Get-Volume | Where-Object FileSystemLabel -eq 'RPI-RP2'

# First board: AP
Copy-Item "firmware\build\phantom_link_ap.uf2" E:\

# Second board: STA
Copy-Item "firmware\build\phantom_link_sta.uf2" E:\
```

For the PoC, **Barebones = AP (host)** and **Jank Box 5000 = STA (client)**. Both boards use the same full UI/input firmware and BOM; only the Wi-Fi role differs, so flash the matching UF2 to each board.

Open one serial monitor per board at 115200 8N1. Expected boot banners:

```text
phantom-link v1 76acd4b8 AP
AP active: 192.168.4.1
UDP listening on 4444
```

```text
phantom-link v1 76acd4b8 STA
STA static IP: 192.168.4.2 (DHCP disabled)
STA connecting to Phantom_Link...
STA join continues in the main loop
STA status: joining (1)
STA status: up (3)
STA IP: 192.168.4.2
SESSION new id=0x6f3a91c2 old=0x00000000
TX type=HELLO seq=0 len=0
RX type=HELLO_ACK seq=0 len=0
LINK UP (HELLO_ACK) peer=192.168.4.1:4444
```

### Robust serial capture

The Pico's USB CDC port disappears when the board resets, so a monitor opened afterward can miss the banner. Start the logger first, then reset or flash the board. It reconnects automatically and writes a raw log under `logs\`.

```powershell
.\tools\serial_monitor.ps1 -Port COM5
.\tools\serial_monitor.ps1 -Port COM6
```

Use one window per board and press `Ctrl+C` to stop. PuTTY or Tera Term with log-to-file also works if started before the reset, but those do not reopen the port automatically.

## Repository layout

```text
firmware/            Unified AP/STA firmware and build definition
  main.c             UI, input, codec, UDP link, typed frames, timeout
  oled.h             SH1106 driver and framebuffer helpers
  font_ui.h          48-glyph variable-width font
  alpha_ui.h         1024-byte Alpha page art
  numeric_ui.h       1024-byte Numeric page art
  boot_ui.h          1024-byte boot splash art
  banner.h           Version and build-hash boot banner
  proto/             Frame spec, codec/CRC32, session window (no SDK dependency)
  host/              Host CMake project and ctest suites for protocol code
  CMakeLists.txt     phantom_link_ap + phantom_link_sta targets
  tests/             Host-side font/codec/UI/role validation (no SDK needed)
tools/               Auto-reconnecting serial logger for boot captures
docs/                Audit, gameplan, ADRs, agent config
docs/adr/            Locked architecture decisions
.scratch/            Local issue tracker: specs and tickets per feature
Phantom_Link_rx/     Original RX firmware, kept as rollback reference
Phantom_Link_tx/     Original TX tree, stale pre-Base-6 fork
```

The pre-audit RX tree is retained for rollback. The unified firmware is the active source.

## Documentation

- `docs/PHANTOM_LINK_AUDIT_AND_HANDOFF.html` — complete forensic audit, defect register, protocol/UI designs, build order, acceptance matrix, and handoff packet.
- `docs/PHANTOM_LINK_AUDIT_AND_HANDOFF.md` — compact digest.
- `docs/GAMEPLAN.md` — phase-by-phase plan for the encrypted Bonnie & Clyde protocol, SFO channel, untethered hardware, and the final UI pass.
- `docs/adr/` — decisions that are hard to reverse, with the alternatives that were rejected.
- `.scratch/phantom-link-v1/` — the spec and 27-ticket backlog derived from the gameplan.
- `GLOSSARY.md` — the project's fixed vocabulary.
- `AGENTS.md` — build commands and hard rules for coding agents.
- `docs/NEXT_CONTEXT_PROMPT.txt` — copy-paste bootstrap for a new AI context.

## Gallery

### [Proof-of-Concept] First Success Sending and Receiving an Encrypted Payload
<img width="3072" height="3072" alt="IMG_20261101_175008064_HDR" src="https://github.com/user-attachments/assets/de9636a9-d38e-4655-bdd7-4e771551e8ba" />


### [Final Beta] UI/UX With Sony PSP-1000 Joystick Fully Functional
<img width="3072" height="3072" alt="IMG_20261110_190128444_HDR" src="https://github.com/user-attachments/assets/423bf832-ab03-407f-b7ee-3536018320ac" />


### 2.5 second startup screen mockup 128x64 [beta] using Lopaka
<img width="1919" height="1079" alt="Screenshot 2026-11-03 105327" src="https://github.com/user-attachments/assets/71efdb70-433e-48aa-a295-f887d4b67758" />


### 1:1 Pixel Representation achieved using a 1024-byte buffer [Final Beta]
<img width="3072" height="3072" alt="IMG_20261103_192202006_HDR" src="https://github.com/user-attachments/assets/6170b1bc-dd5d-44b3-99db-df62017fa892" />


### Custom variable-width font mapping for embedded OLEDs. Moving beyond standard ASCII blocks for a 1:1 pixel-perfect UI 512x8
<img width="1919" height="1079" alt="Screenshot 2026-11-13 183953" src="https://github.com/user-attachments/assets/e14f2639-0afc-4156-8ec6-10a791d84336" />


### Detailed Serial Port Monitoring [Fixed Brownout Issue]
<img width="1919" height="1079" alt="Screenshot 2026-11-01 143836" src="https://github.com/user-attachments/assets/3093cd82-b947-4402-a433-2caf3119a111" />


## Roadmap

- [x] Establish a routerless private AP / STA UDP link.
- [x] Develop the custom SPI OLED driver and 1024-byte UI pipeline.
- [x] Implement the Alpha/Numeric text-entry UI with joystick navigation.
- [x] Create a custom variable-width font and 49-character transport alphabet.
- [x] Unify both roles into one firmware source with two build targets.
- [x] Make OLED/input initialization unconditional; `IS_AP` selects only the Wi-Fi role.
- [x] Add `HELLO`/`HELLO_ACK`, peer learning, heartbeat, timeout, and link-state LED.
- [x] Add non-blocking transmit and typed frames with CRC32.
- [x] Add hardware-random session IDs, per-direction sequence, and a replay window.
- [ ] Pass the hardware acceptance matrix: T2–T12 in the audit.
- [ ] Add ACK, whole-message retransmission, and clean teardown.
- [ ] Add Speck encryption and key exchange.
- [ ] Implement MAC-layer StegoFrameOrder (the current SFO is application-layer).
- [ ] Run untethered from a zip-tied 2000 mAh power bank and measure runtime.
- [ ] [Undecided/Optional] Antenna trace modification for range testing; this voids certification and is bench-only.

## Security and scope

This repository is a learning prototype. It sends plaintext UDP over a private Wi-Fi network with a development password. CRC32 and the replay window catch corruption and repeats; neither is authentication. Do not use it for sensitive traffic. The audit deliberately gates encryption and steganography work behind hardware verification of the handshake and transport, because adding crypto to an unreliable link would hide two classes of bugs at once.
