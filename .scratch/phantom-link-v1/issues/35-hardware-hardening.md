# 35: RP2350 hardware hardening

**What to build:** Evaluate and, where safe, enable secure boot, debug-port
lock, and flash encryption so a stolen board is not trivially readable.

**Blocked by:** 14

**Status:** ready-for-agent

- [ ] Document RP2350 OTP secure-boot and debug-disable procedure from datasheet.
- [ ] Evaluate flash-encryption support and key storage on RP2350.
- [ ] Protect against `picotool save` flash readout on a production rig.
- [ ] Provide a reversible development path and an irreversible production path.
- [ ] Hardware acceptance on spare board only; never burn OTP on primary rigs.
- [ ] Record which mitigations stop the wrench attack and which do not.

## Research findings — RP2350 / Pico SDK 2.3.1 (research only, nothing flashed or burned)

### Sources actually checked

Local SDK (`C:\Users\Windows 10\.pico-sdk\sdk\2.3.1`):

- `src\rp2350\hardware_regs\include\hardware\regs\otp_data.h` — datasheet-generated OTP
  register/field definitions (CRIT0/CRIT1, BOOT_FLAGS0/1, BOOTKEY0..3, OTPBOOT_*).
- `src\rp2350\hardware_structs\include\hardware\structs\otp.h` — OTP block register
  structs. Its header comment (line 18) cites
  `https://datasheets.raspberrypi.com/rp2350/rp2350-datasheet.pdf#tab-registerlist_otp`.
- `src\rp2350\hardware_regs\include\hardware\regs\trng.h` (lines 397-399) and
  `src\rp2350\hardware_structs\include\hardware\structs\trng.h` (line 116) —
  the *only* AES references in RP2350 register headers; these are TRNG
  SBOX/entropy-source settings, not flash encryption.
- `tools\CMakeLists.txt` lines 433-466 (`pico_sign_binary`), 496-589
  (`pico_encrypt_binary`), 767-769 (`--otp-key-page` passed to picotool).
- `test\pico_low_power_test\CMakeLists.txt` lines 29-36 and 43-52 — working
  SDK examples of signed + encrypted builds.
- `src\rp2_common\boot_bootrom_headers\include\boot\bootrom_constants.h` lines
  95, 203, 265-284 — bootrom OTP access/lock constants
  (`BOOTROM_LOCK_OTP`, `ROM_FUNC_OTP_ACCESS`, `OTP_CMD_*`).

Authoritative web sources:

- RP2350 datasheet (pip mirror):
  https://pip.raspberrypi.com/documents/RP-008373-DS-rp2350-datasheet.pdf
  (SDK header URL above is the canonical shortlink).
- Raspberry Pi security whitepaper "Understanding RP2350's security features":
  https://pip-assets.raspberrypi.com/categories/1260-security/documents/RP-009377-WP-1-Understanding%20RP2350_s%20security%20features.pdf
- picotool README (`encrypt`, `seal`, `otp`, `save` sections):
  https://github.com/raspberrypi/picotool#encrypt , #seal , #otp , #save
- Hacking Challenge results (fault-injection caveats):
  https://www.raspberrypi.com/news/security-through-transparency-rp2350-hacking-challenge-results-are-in/

### 1. Secure boot / signed firmware — supported, but authentication only

- `OTP_DATA_CRIT1_SECURE_BOOT_ENABLE` (`otp_data.h` lines 362-369) is the enable
  bit. Its own description: "Enable boot signature enforcement, and permanently
  disable the RISC-V cores." Setting it is **irreversible** and gives up the
  RISC-V boot path.
- Signature scheme is **SHA-256 + secp256k1 ECDSA** (security whitepaper,
  "Secure boot" section quoting datasheet §10.1.1; picotool `seal` docs say the
  signing key "must be for the secp256k1 curve, in PEM format"). This is *not*
  RSA; do not copy RSA-2048/3072 recipes from other vendors.
- Public-key fingerprints live in OTP `BOOTKEY0_0`..`BOOTKEY3_7` (`otp_data.h`
  lines 1488+; four key slots), with `BOOT_FLAGS1.KEY_VALID` / `KEY_INVALID`
  control bits (lines 734-758). picotool `seal --sign key.pem` writes the
  fingerprint into an OTP JSON file automatically.
- Related optional flags, all OTP and all irreversible: `ROLLBACK_REQUIRED`
  (anti-rollback, line 539), `SECURE_PARTITION_TABLE` / `HASHED_PARTITION_TABLE`
  (lines 548-562), `SINGLE_FLASH_BINARY` (line 574).
- Build-side support exists locally: `pico_sign_binary()` and `picotool seal`
  add the signature/hash metadata block. There is no need to invent an API.
- **What it does not do:** secure boot does not encrypt flash and does not hide
  secrets. A signed but plaintext image is still fully readable with
  `picotool save -a` or an external flash clip. It only guarantees that the
  code that runs was signed by the holder of the private key.

### 2. Debug-port disable via OTP — supported, irreversible if locked

- `OTP_DATA_CRIT1_DEBUG_DISABLE` ("Disable all debug access", `otp_data.h`
  lines 346-352) and `OTP_DATA_CRIT1_SECURE_DEBUG_DISABLE` ("Disable Secure
  debug access", lines 354-360). Also `CRIT0.ARM_DISABLE` / `RISCV_DISABLE`
  (lines 226-240).
- `OTP_CRITICAL` in `otp.h` lines 98-108 mirrors these flags at boot.
- Important nuance from `otp.h` lines 115-131: `OTP_DEBUGEN` can re-enable a
  debug feature that a CRIT1 flag disabled, unless the corresponding
  `OTP_DEBUGEN_LOCK` bit is written. For a genuinely permanent lock, program
  `DEBUG_DISABLE` **and** the `DEBUGEN_LOCK` bits.
- Alternative to blanket disable: enroll a debug key in OTP; debug is unlocked
  only when the matching key is supplied over SWD (whitepaper §"Debug Keys",
  datasheet §3.5.9.2). This preserves a controlled recovery path but adds key
  management.
- Glitch detectors exist as OTP-configurable flags
  (`OTP_DATA_CRIT1_GLITCH_DETECTOR_ENABLE` / `_SENS`, `otp_data.h` lines
  320-335). They raise attack cost; see caveats below.

### 3. Flash / XIP encryption — no transparent XIP encryption; "encrypted boot" to SRAM instead

- I found **no** flash-encryption or XIP-decryption engine in the RP2350 SDK
  register headers or the sources checked. The only AES tokens in RP2350
  headers are TRNG SBOX bits. Encryption is a **boot-time load-and-decrypt**
  flow, not transparent on-the-fly decryption of XIP flash.
- The supported mechanism is `picotool encrypt` / `pico_encrypt_binary()`:
  - SDK docs: `tools\CMakeLists.txt` lines 496-526. It encrypts with an AES key
    and signs the encrypted binary; `EMBED` embeds a decryption stage;
    `OTP_KEY_PAGE` selects the OTP page holding the key; `NO_CLEAR` skips SRAM
    clearing; `MBEDTLS` selects a faster decryption stage that the SDK itself
    says "offers no security against power or timing sniffing attacks" (lines
    512-514) — avoid it for this use case.
  - picotool README `encrypt`: key is a 256-bit (32-byte) AES key or a
    1024-bit (128-byte) 4-way key share (the decryption code needs the share in
    OTP); per-device 128-bit IV salt; key page default 29 with IV salt on the
    next page; a custom bootloader (`pico-examples/bootloaders/encrypted`) or
    `--embed` self-decrypting stage; `--clear` (on by default) zeroes SRAM on
    load.
  - Binary layout per picotool: metadata block, IV, ciphertext, padding,
    signature block — so `picotool save` of such a flash yields ciphertext.
  - OTP keys are soft-locked after use so later boot stages cannot read them
    (whitepaper); OTP can be hard/soft locked at 128-byte page granularity,
    and `picotool otp permissions` provisions those permissions.
  - Working local example: `test\pico_low_power_test\CMakeLists.txt` lines
    29-36 uses `pico_set_binary_type(... no_flash)` + `pico_package_uf2_output`
    + `pico_sign_binary` + `pico_encrypt_binary(... EMBED NO_CLEAR)`, then a
    second variant with `NO_CLEAR` omitted because persistent data is in
    XIP SRAM. This confirms encrypted images execute from SRAM / XIP SRAM.
- Consequences for Phantom Link: enabling this is a **significant architecture
  change**, not a flag. Code must fit and run from SRAM (~520 KB on RP2350,
  shared with the Wi-Fi/CYW43 driver, OLED framebuffer, stacks). Persistent
  data placement and the `NO_CLEAR` behaviour need explicit design. It is also
  fully OTP-based and therefore irreversible.
- The security whitepaper explicitly frames encrypted boot as protection
  against physical access to the flash ("loading the encrypted binary into
  SRAM and keeping it there avoids attacks based on physical access to [the
  flash]"). That is exactly the flash-readout defence — but it does **not**
  protect a powered device's SRAM from a debugger or from coercion.

### 4. Reversible development path vs irreversible production path

- Development (reversible): do not burn any security OTP. Keep SWD open, keep
  BOOTSEL USB open, use ordinary signed-or-unsigned builds. All of the above
  flags are 0→1 one-way; there is no "un-burn". Use the existing
  `poc/plaintext-v1` and normal UF2 flow for day-to-day work.
- Production (irreversible, spare board only): provision in this order —
  (1) generate secp256k1 signing key and AES key share + IV salt offline and
  keep them out of the repository; (2) build `no_flash` + package + sign +
  encrypt (`EMBED`, **no** `MBEDTLS`, **no** `NO_CLEAR` unless justified);
  (3) write BOOTKEY fingerprints, `BOOT_FLAGS1.KEY_VALID`, audit with
  `picotool otp dump`, and set OTP permissions/locks; (4) set
  `DEBUG_DISABLE` + `DEBUGEN_LOCK`; (5) set `SECURE_BOOT_ENABLE` and any
  rollback flags last; (6) only then disable BOOTSEL USB interfaces if a
  separate signed update path exists. Verify each step on the spare board.
  The picotool README's own warning: "if you set SECURE_BOOT_ENABLE but don't
  set a boot key, and disable the PICOBOOT interface, then your device will be
  unusable." Do not enable these on primary rigs.

### 5. What actually protects against `picotool save` flash readout

| Measure | Stops `picotool save` over USB? | Stops physical flash clip / desolder readout? | Irreversible? |
|---|---|---|---|
| `BOOT_FLAGS0.DISABLE_BOOTSEL_USB_PICOBOOT_IFC` (`otp_data.h` L476-481) | Yes — removes the picoboot USB interface `save` uses. picotool's `-f/--force` also relies on compatible USB stdio/BOOTSEL, so this closes that path too. | No — external flash is still plaintext. | Yes (OTP) |
| `BOOT_FLAGS0.DISABLE_BOOTSEL_USB_MSD_IFC` (L483-488) | Yes — removes the USB mass-storage drag-and-drop path. | No. | Yes (OTP) |
| `SECURE_BOOT_ENABLE` + signed image | No. A signed plaintext image is still readable. | No. | Yes (OTP) |
| `DEBUG_DISABLE` + `DEBUGEN_LOCK` | No (different interface). | No. | Yes (OTP) |
| Encrypted boot (`pico_encrypt_binary(... EMBED)` + OTP key) | `save` still works, but returns **ciphertext**; without the OTP key the dump is useless. | Same: attacker gets ciphertext, not firmware or embedded PSK. | Yes (OTP) |
| Enclosure / physical custody | Indirectly (no USB access if sealed) | Only if attacker cannot reach the flash; not a technical guarantee. | No |

Bottom line: the only mechanism in the checked sources that makes flash
contents themselves unreadable is **encrypted boot with an OTP-held key**.
Disabling BOOTSEL USB is a cheap speed bump for the "plug in a cable and run
`picotool save -a`" scenario, but it bricks the normal update path and must be
paired with a signed firmware-update mechanism. Note that any secret baked
into plaintext flash (e.g. a provisioning PSK) remains readable regardless of
secure boot or debug disable.

### 6. What does NOT stop the wrench attack

- Nothing above stops coercion of the operator. If an attacker can compel the
  authorised user to power on, unlock, or disclose the PSK, the cryptography
  does exactly what it is designed to do for the authorised user.
- Secure boot, debug disable and BOOTSEL disable protect *code integrity* and
  *interfaces*; they do not erase RAM keys on command.
- Encrypted boot protects flash *at rest*. A powered, seized board has
  decrypted code and live session/PSK material in SRAM; without `DEBUG_DISABLE`
  that is readable over SWD, and with it a coerced user can still disclose it.
- Issue 36's zeroization-on-BYE is best-effort and only helps if the device
  gets an orderly teardown signal. A wrench attack typically does not.
- Tamper seals/switches/biometrics are covered in ticket 38; none changes the
  coercion equation.

### Caveats / things I could not fully verify

- I did **not** find a datasheet statement promising transparent XIP decryption
  of flash; my conclusion is based on absence in the checked SDK register
  headers plus the positive description of load-to-SRAM encrypted boot. Treat
  as "not supported in the 2.3.1 SDK/datasheet material checked", not as a
  proof about all future silicon.
- Fault-injection research defeats parts of this on real hardware. Raspberry
  Pi's Hacking Challenge results (URL above) report: a supply-voltage glitch
  forcing an unverified boot vector; a laser fault bypassing the signature
  check; and a double EMFI fault that prevented an OTP page lock, allowing OTP
  readout "via the PICOBOOT interface" from non-secure bootloader code.
  Raspberry Pi says the laser issue was "likely to be addressed in a future
  stepping", and revised downward their estimate of glitch-detector
  effectiveness. See also https://courk.cc/rp2350-challenge-laser and
  https://www.usenix.org/system/files/woot25_slides_muench.pdf . Therefore
  treat RP2350 OTP/secure boot as **raised cost, not absolute trust**, and
  check the actual stepping of any production board.
- I did not run `picotool`, burn OTP, or flash anything; all provisioning
  sequences above are derived from docs/headers, not from hardware acceptance.
