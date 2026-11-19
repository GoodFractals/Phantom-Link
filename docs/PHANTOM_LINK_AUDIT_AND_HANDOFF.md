# Phantom Link — Audit, Remediation Plan & AI Handoff (Digest)

**Baseline:** `main` @ `a5ee129` · 2026-11-15 17:31:32 -0800  
**Audit date:** 2026-11-18  
**Full report:** `docs/PHANTOM_LINK_AUDIT_AND_HANDOFF.html` (52 verified defects, phase plan, test matrix)  
**Source of truth:** code > full audit > README > Project Bible

> This is the compact companion to the HTML. The HTML contains the complete defect register, line references, reference patches, risk register, and appendices. If you only attach one file to a new AI context, attach the HTML.

> **Post-audit implementation (2026-11-18):** `firmware/` now contains one `main.c` and dual CMake targets (`phantom_link_ap` with `IS_AP=1`, `phantom_link_sta` with `IS_AP=0`). Both UF2s build warning-free from that source with Pico SDK 2.3.1 on native Windows: `firmware/build/phantom_link_ap.uf2` (670,720 bytes) and `firmware/build/phantom_link_sta.uf2` (675,840 bytes). `IS_AP` selects only the Wi-Fi role; the OLED, ADC joystick, buttons, input polling, codec, parser, and render path are unconditional, and Wi-Fi bring-up is non-blocking. The source implements the 49-code 3-digit Base-6 transport, exact token parsing, `HELLO`/`HELLO_ACK`/`LINK_PING`/`LINK_PONG`, peer learning, a 1 s heartbeat, a 3 s timeout, and non-blocking TX. Host validation passes for the 48-glyph/180-byte font, all 49 codec values, a 21-character round-trip, the `V` fifth tap, the corrected numeric hitboxes, and the unconditional input/UI path. **No board was connected, so the hardware acceptance tests T2–T12 remain pending.** Pixel-level OCR corrected three UI facts: `SOS` is row 2, numeric bottom-right is the `LINK` indicator (not punctuation), and alpha `WXYZ` gains `V` as a fifth tap. See §4 and §9.

---

## 1. Verdict

Phantom Link is a real prototype with a working custom OLED UI, a working variable-width font pipeline, and a Base-6 codec whose arithmetic is correct. It is **not** yet a link, a handshake, or an encrypted device.

The single biggest problem is not a missing feature: the repository contains **two divergent firmwares**, and the one labeled TX is a pre-Base-6 snapshot that cannot type or send. Fix that first; everything else follows.

| Area | State |
|---|---|
| UI / asset pipeline | Amber — works, numeric page half-wired, text y-offset bug, boot art unused |
| Base-6 codec | Green for A–Z; red for space/digits/punctuation; two digits cannot cover the 49-character UI (48 glyphs + space) |
| Link / handshake | Red — no HELLO, ACK, peer address, heartbeat, timeout, CRC |
| Encryption | Red — not implemented; `PhantomPacket` and `calculate_crc()` are dead code |
| Reproducibility | Amber — no license, CI, tests, wiring doc, or UI image converter |
| Documentation truth | Red — README contradicts code on AP role, static IP, encryption, framing, “raw RF” |

---

## 2. The one architecture decision

The CYW43439/Pico SDK supports two Wi-Fi roles: **AP** (host the network) and **STA** (join it). There is no supported ad-hoc/IBSS mode, so **exactly one board must be AP**. That does **not** require two codebases.

- **AP/STA** is a radio-link role.
- **TX/RX** is what a packet is doing.
- They are orthogonal. Both roles compile the same transceiver, UI, input, codec, and parser path. Both boards carry the same BOM, so both can type, send, receive, decode, and render. `IS_AP` selects only the Wi-Fi role.

**Correct architecture:** one `main.c`, two build targets, one flag.

```cmake
add_executable(phantom_link_ap  main.c)
target_compile_definitions(phantom_link_ap  PRIVATE IS_AP=1)

add_executable(phantom_link_sta main.c)
target_compile_definitions(phantom_link_sta PRIVATE IS_AP=0)
```

```c
#ifndef IS_AP
#define IS_AP 1          // safe default; CMake overrides per target
#endif
```

“Identical firmware” means identical source and behavior, not byte-identical UF2s: the role constant is compiled in. The current implementation stores it in a `static const bool is_ap` and keeps the OLED, ADC, button, codec, and parser paths outside every role conditional. A future single-binary option is to read SELECT at boot (held = AP, released = STA) and set the same runtime variable.

The SDK 2.3.1 cyw43 build is compiled with `CYW43_NETUTILS=0`, so the AP has no DHCP server. The STA therefore stops DHCP and uses the fixed address `192.168.4.2/24`, gateway `192.168.4.1`. This is a PoC transport decision; a product build should add a DHCP server or make the address configurable.

For the PoC, the user confirmed: **Barebones = AP (host)**, **Jank Box 5000 = STA (client)**. The earlier audit recommendation (Jank Box = AP) is superseded. The README’s “Receiver acts as Access Point” is the line that is wrong.

---

## 3. P0 blockers (fix before calling it a PoC)

1. **Two divergent firmwares.** TX is a pre-Base-6 fork: one packet per character, no STA branch, no ADC/button init, no `handle_select_button()` call. Diff is +34/−124; all headers/assets identical. The two boards speak different protocols.
2. **TX is deaf.** No `adc_init`, no joystick/button polling, no handler calls. Menu works; SEND is unreachable.
3. **No handshake/link state.** No `HELLO`, `HELLO_ACK`, `peer_ip`, `link_state`, heartbeat, or timeout. Sends are blind.
4. **Base-6 transport corrupts non-A–Z.** `char_index = c - 'A'` clamps space/digits/punctuation to `'A'`; `"SOS SOS"` becomes `"SOSASOS"`. The UI's target character set is 49 characters (48 glyphs + space); the transport carries 26.
5. **Numeric page is vertically misaligned and half-wired.** `numeric_boxes` has no box for the baked `SOS` at `[2][1]` or `SPACE` at `[3][1]`, but treats the blank `[1][1]` as `SOS`; `[3][2]` is the `LINK` indicator, not punctuation. `IN RANGE?` jumps back to Alpha; `SOS` inserts a space. Group 13 maps to empty `[1][2]`. The alpha `WXYZ` group omits `V`. Net result: `V`, `-`, `(`, `)`, `[`, `]`, numeric `SOS`, and numeric `SPACE` are unreachable.
6. **Blocking send starves Wi-Fi.** `sleep_ms(100)` per packet inside the button handler, no `cyw43_arch_poll()`; a 20-char message freezes the link ~4 s.
7. **Broadcast send is unverified.** Sends to `192.168.4.255` without `SOF_BROADCAST`; `udp_sendto` return is ignored; the monitor prints success regardless.
8. **Reply address is hardcoded.** RX replies to `192.168.4.1` instead of the `addr` handed to the callback.
9. **Base-6 pairing desyncs on loss.** `static high_val` has no sequence/timeout/dedupe; one lost packet shifts every following character.
10. **END always prints SOS.** The RX lost the `strcmp(..., "SOS")` check that still exists in TX.
11. **No encryption/CRC/framing despite README claims.** `PhantomPacket` lacks `packed` and is unused; no XOR/Speck, sync word, length check, or CRC.

The full HTML also lists 20 P1 defects (parser prefixes, cursor navigation, OLED y-offset, message clearing, 3 s association timeout, no boot splash, no watchdog, per-byte SPI CS, blocking/repeating input loop, etc.) and 21 P2 defects (README truth, stale Bible, font-only `png2c.py`, duplicate trees, tracked intermediates, hardcoded Wi-Fi password, OneDrive/WSL risk, no license/CI/tests, antenna certification risk, no tags/backups, selection-box off-by-one).

---

## 4. UI / font gaps that block a clean demo

- **Missing characters.** `tap_groups[12]` is `"0=+\""`; the numeric art says `0=+-`. The minus sign is in the font but has no input path. `tap_groups[7]` is `"WXYZ"`; `V` is in the font but has no input path. The alpha bottom-right maps group 8 (`".,?!"`), but its art says `.,()[]?!`; brackets are unreachable there.
- **Numeric art truth.** Pixel-level OCR of `numeric_ui.h` shows `123 / IN RANGE? / [ . ]`, then `456` alone, then `789 / SOS / BAT:`, then `0=+- / SPACE / LINK`. `SOS` is `[2][1]`, `SPACE` is `[3][1]`, and `[3][2]` is the `LINK` indicator. The audit's first pass repeated the old row offset and wrongly called `[3][2]` punctuation. There is no numeric punctuation key.
- **Leftover actions.** Numeric `IN RANGE?` is reused as “back” and `SOS` as “space” from an older layout.
- **Cursor skip.** `move_cursor` checks only one step and does not skip empty boxes; any left move on numeric jumps to Alpha.
- **Text bug.** `oled_draw_custom_string` ignores `y % 8` (so `y=30` draws at row 24) and assigns instead of OR-ing, erasing the box art.
- **Font pipeline bug.** The double-quote is split into two 1-px glyphs by the all-black-column scanner. `font_char_count = 49`, but the renderer maps only 48; index 48 is orphaned and the quote renders as a single mark.
- **Missing UI converter.** `png2c.py` is font-only. It cannot regenerate `alpha_ui.h`, `numeric_ui.h`, or `boot_ui.h`; the pipeline in the README is not reproducible.
- **Unused art.** `boot_ui.h` exists but is never included; boot is a 2 s blank wait + 500 ms clear.
- **Fake status.** `PREV:`/`CURR:`, `LINK`, and `BAT:` are baked art with no variables, link state, or battery measurement.

---

## 5. Base-6 capacity

| Digits | Codes | Bits/char | Frames/char | Coverage |
|---|---|---|---|---|
| 1 | 6 | 2.58 | 1 | Six symbols |
| 2 | 36 | 5.17 | 2 | A–Z + 0–9 exactly |
| 3 | 216 | 7.75 | 3 | 48 glyphs + space (49); full printable ASCII if desired |

**Recommendation: 3 digits.** Two digits cannot represent space or punctuation. Three digits still fit SFO; `SOS` becomes 9 frames, ~720 ms at 80 ms/frame.

Canonical table: `" ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789=+-.,?!()[]\""` (49 codes: leading space + 48 glyphs). The font has 48 drawable glyphs; space is a transport-only code and the renderer advances five pixels for it.

Encode: `d0 = code/36`, `d1 = (code/6)%6`, `d2 = code%6`.  
Decode: `acc = acc*6 + symbol`; every 3 symbols, `glyph_from_code(acc)`.

**Collision warning:** `ACK` is Base-6 symbol 1. Do not use it as a handshake reply. Use `HELLO`, `HELLO_ACK`, `LINK_PING`, `LINK_PONG`; check them before symbol decoding.

---

## 6. Handshake spec

| Token | Direction | Meaning |
|---|---|---|
| `HELLO` | STA → AP | Initiate; optional nonce later |
| `HELLO_ACK` | AP → STA | Accept; AP learned peer address |
| `LINK_PING` | Both | Heartbeat; reply `LINK_PONG` |
| `LINK_PONG` | Both | Heartbeat reply; update `last_heard` |

State: `LINK_DOWN` / `LINK_UP`, `peer_ip`, `peer_port`, `last_heard_ms`, `last_hello_ms`, `last_ping_ms`.

Behavior:
- STA sends `HELLO` every 1 s until `HELLO_ACK`.
- AP stores `peer_ip = *addr`, `peer_port = port`, replies `HELLO_ACK`.
- Both send `LINK_PING` every 1 s; reply `LINK_PONG`; any valid packet updates `last_heard`.
- 3 s without a packet → `LINK_DOWN`, print `LINK LOST`.
- Gate SEND on `LINK_UP`; keep the draft when down.
- LED on when up, off when down.

The AP learns the STA from `addr`; the STA learns the AP from `HELLO_ACK`. The STA still uses a fixed `192.168.4.2` address because the AP build has no DHCP server; peer learning is independent of how that address was assigned.

---

## 7. Phase plan

| Phase | Deliverable | Done when |
|---|---|---|
| 0 | Freeze: tag `pre-audit`, archive both UF2s, photograph wiring, record COM ports | Tag + backups + baseline log |
| 1 | Repo hygiene: branch `poc/handshake`, `.gitattributes`, untrack intermediates, LICENSE, docs | Clean status, no intermediates tracked |
| 2 | One firmware: `firmware/main.c`, AP/STA targets, unconditional input loop | Both UF2s from one source; both boards type and render |
| 3 | Handshake: tokens, peer learning, heartbeat, link state, gate | HELLO/HELLO_ACK; LINK LOST/recover in ≤3 s |
| 4 | Honest transport: 3-digit codec, exact parser, non-blocking TX, END/SOS fix, keep message | SOS + full glyph set round-trip; responsive send |
| 5 | UI truth: boxes/actions, cursor skip, OLED fix, status, boot splash | Every box has an action; text doesn’t eat art |
| 6 | Hardware proof: acceptance matrix, soak, current draw, range | Dated test log + photos |
| 7 | Docs/commit: README truth, wiring, test log, tag `poc-handshake` | Fresh clone builds/flashes from README |
| 8 | Deferred: Speck + nonce, CRC/framing, MAC-layer SFO, enclosure/battery | Only after Phase 6 passes |

---

## 8. Acceptance tests (minimum green bar)

- **T1** Both targets build from one source, no duplicated `main.c`.
- **T2** Both boards boot the UI; joystick, buttons, and multi-tap work on each.
- **T3** STA prints `STA status: joining`, then `STA status: up` and `STA IP: 192.168.4.2`; no `bad auth` or `network not found`.
- **T4** `HELLO` → `HELLO_ACK`; both print `LINK UP` with the peer IP (`192.168.4.1` on the STA, `192.168.4.2` on the AP).
- **T5** Power-cycle the STA: the AP prints `LINK LOST` ≤3 s; after the STA boots, the AP returns to `LINK UP` without an AP reboot and the STA prints `LINK UP`.
- **T6** SOS round-trips both ways: the receiving serial prints `!!! SOS SIGNAL RECEIVED !!!` only for exact SOS; the other OLED shows `SOS`; no false positives.
- **T7** `TEST 123 =+-.,?!()[]"` round-trips exactly in both directions.
- **T8** `IN RANGE?` on either board gets a PONG/RTT.
- **T9** Power off the AP, wait for the STA to print `LINK LOST`, then SEND on the STA: `NO LINK` and the draft is kept. Repeat with the roles reversed.
- **T10** A 30-char send from either board stays responsive; DELETE cancels.
- **T11** Both boards show `boot.png` for ~2.5 s at boot, then the menu.
- **T12** 30-min soak with messages in both directions: ≥99% delivery, no desync/watchdog reset.

> Both boards carry the identical BOM, so every AP-side and STA-side test is valid once the second unit is wired to match the first.

---

## 9. Exact build / flash commands

**Verified 2026-11-18 on native Windows** (Pico SDK 2.3.1, CMake 4.0.0-rc1, Ninja 1.13.2, Arm GNU Toolchain 14.2). Both targets built warning-free from a clean build; AP UF2 670,720 bytes, STA UF2 675,840 bytes. The small size difference reflects the Wi-Fi role branch; the OLED, input, codec, and parser paths are shared.

Clean-build SHA256:

```text
04011281D53C5D4CDCEBD6B246A7351B8595A98652CAD92E4A85D4BFC1B2201A  phantom_link_ap.uf2
07685D0783E8E7F6A4209BBCBC6F8214188996A6BF320393684840DE054A4DDB  phantom_link_sta.uf2
```

```powershell
$env:PICO_SDK_PATH = "C:\Users\Windows 10\.pico-sdk\sdk\2.3.1"
$env:PATH = "C:\Users\Windows 10\.pico-sdk\tools\2.3.1\pioasm;C:\Users\Windows 10\.pico-sdk\picotool\2.3.1\picotool;C:\Users\Windows 10\.pico-sdk\ninja\v1.13.2;C:\Program Files (x86)\Arm GNU Toolchain arm-none-eabi\14.2 rel1\bin;$env:PATH"

Set-Location "C:\Users\Windows 10\OneDrive\Desktop\Phantom_Link\firmware"
Remove-Item build -Recurse -Force -ErrorAction SilentlyContinue
cmake -B build -G Ninja -DPICO_BOARD=pico2_w "-DPICO_SDK_PATH=$env:PICO_SDK_PATH"
cmake --build build -j4
Get-Item build\phantom_link_ap.uf2, build\phantom_link_sta.uf2 | Select-Object Name,Length
```

The `PATH` entries are required for the SDK's prebuilt `pioasm.exe` and `picotool.exe`; otherwise CMake tries to build those host tools from source and fails if no native compiler is installed.

Host-side asset validation (no SDK or board required):

```powershell
python firmware\tests\validate_assets.py
```

Expected final line: `HOST VALIDATION: PASS`. The script checks the 48-glyph/180-byte font, the merged quote, `V`, all 49 Base-6 codes, the 21-character round-trip, multi-tap reachability, the numeric hitboxes, and that the role flag no longer gates the UI or input path.

WSL alternative (unverified after the audit; WSL failed on the build machine):

```bash
cd "/mnt/c/Users/Windows 10/OneDrive/Desktop/Phantom_Link/firmware"
rm -rf build
cmake -B build -DPICO_BOARD=pico2_w -DPICO_SDK_PATH="$HOME/pico-sdk"
cmake --build build -j4
ls -lh build/phantom_link_ap.uf2 build/phantom_link_sta.uf2
```

Flash: hold BOOTSEL, plug USB, copy the UF2 to the `RPI-RP2` volume. **Barebones = AP (host); Jank Box 5000 = STA (client).** Both boards carry the identical OLED/joystick/button BOM.

```powershell
Get-Volume | Where-Object FileSystemLabel -eq 'RPI-RP2'
Copy-Item ".\build\phantom_link_ap.uf2" E:\    # first board (AP)
Copy-Item ".\build\phantom_link_sta.uf2" E:\   # second board (STA)
```

Serial (Windows): PuTTY `COMx`, 115200 8N1.  
Serial (WSL): `minicom -D /dev/ttyACM0 -b 115200` (exit `Ctrl-A X`) or `screen /dev/ttyACM0 115200` (exit `Ctrl-A K`).

Backup:
```bash
mkdir -p artifacts/uf2
cp Phantom_Link_tx/build/phantom_link_tx.uf2 artifacts/uf2/phantom_link_tx_pre-audit.uf2
cp Phantom_Link_rx/build/phantom_link_rx.uf2 artifacts/uf2/phantom_link_rx_pre-audit.uf2
sha256sum artifacts/uf2/*.uf2 | tee artifacts/uf2/SHA256SUMS
```

Git:
```bash
git checkout -b poc/handshake
# ... changes ...
git add firmware/ docs/ .gitignore .gitattributes README.md
git commit -m "poc: unify AP/STA firmware, 3-digit Base-6, handshake, non-blocking TX"
git tag -a poc-handshake -m "Handshake-first PoC"
git push -u origin poc/handshake
git push origin poc-handshake
```

---

## 10. New-context handoff checklist

1. Attach `docs/PHANTOM_LINK_AUDIT_AND_HANDOFF.html` (or this digest).
2. Attach `main.c`, `oled.h`, `font_ui.h`, `CMakeLists.txt`, `lwipopts.h`, `README.md`.
3. State the commit hash, branch, which board is AP, and which COM ports.
4. State exactly what works and what fails, with the last serial output.
5. Lock the decisions: AP role, 2-digit vs 3-digit, UI status behavior, boot splash scope.
6. Name the next phase and its acceptance test.
7. Paste the bootstrap prompt from `docs/NEXT_CONTEXT_PROMPT.txt` or §10.3 of the HTML.

**Guardrails for the new AI:**
- Do not add Speck or MAC-layer SFO until the handshake and T1–T7, T9, T10 pass.
- Do not rewrite the UI art.
- Do not clean build until the working UF2s are archived.
- Do not trust README claims about encryption, static IP, or AP role.
- Do not claim “encrypted” or “covert” until implemented.
- Keep the Project Bible out of the public repo; it contains personal/job-search data.

---

## 11. One-paragraph handoff

> Phantom Link is two Pico 2 W boards with the same BOM: SH1106 OLED, PSP joystick on ADC0/ADC1, and SELECT/DELETE on GP14/GP15. The Barebones Pico is the AP host; the Jank Box 5000 is the STA client. One `firmware/main.c` now builds two targets: `phantom_link_ap` (`IS_AP=1`) hosts the private network; `phantom_link_sta` (`IS_AP=0`) joins it and initiates `HELLO`. `IS_AP` selects only the Wi-Fi role; the full UI, input loop, codec, parser, and screen path are unconditional, so both boards type, send, receive, decode, and render. Wi-Fi bring-up is non-blocking, so the UI and controls run while the STA joins. Because the SDK 2.3.1 AP build has no DHCP server, the STA uses the fixed address `192.168.4.2/24` with gateway `192.168.4.1`. The implemented PoC is a plaintext UDP link: 49-code 3-digit Base-6 transport, exact-length `HELLO`/`HELLO_ACK`/`LINK_PING`/`LINK_PONG`/`END` parsing, peer learning from the received address, a 1 s heartbeat, a 3 s timeout, and a non-blocking TX state machine. Both UF2s build warning-free, and host font/codec/UI validation passes. The first two-board logs showed the AP up and the STA waiting for an IP; the static-address fix targets that failure. Hardware acceptance T2–T12 is still pending. Encryption, CRC/framing, sequence/duplicate detection, and true MAC-layer SFO are deliberately not implemented. Do not add crypto or SFO until T1–T7, T9, T10 pass on hardware. The full audit is in `docs/PHANTOM_LINK_AUDIT_AND_HANDOFF.html`.
