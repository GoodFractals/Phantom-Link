# 38: Physical/tamper threat study

**What to build:** Honest analysis of wrench attacks, tamper detection, and
whether biometrics or enclosures add real value on this platform.

**Blocked by:** None

**Status:** ready-for-agent

- [ ] Document that possession defeats PSK and flash readout.
- [ ] Evaluate tamper-evident seals, enclosure switches, and fingerprint modules.
- [ ] Reject security theater with explicit reasoning.
- [ ] Recommend the smallest set of controls that raises attacker cost honestly.
- [ ] Feed conclusions into the datasheet Limitations section.

## Research findings — physical/tamper threat study (research only, no files changed)

### Sources and scope anchor

- Local spec: `.scratch/phantom-link-v1/spec.md` lines 15-21. **Out of scope**
  already includes "secure boot, or tamper-resistant key storage",
  "Battery-sense hardware and enclosures", and (implicitly) hostile physical
  custody. Anything below that adds hardware or OTP is a **spec/GAMEPLAN
  change**, not a ticket-level decision. Tickets 35/36 remain evaluation work
  until that changes.
- Related tickets: 35 (RP2350 hardware hardening) for the platform facts, 36
  (zeroization) for the RAM-side story, 14 (crypto hardware acceptance) for the
  baseline we must not regress.
- Platform facts for RP2350 in ticket 35: secure boot = SHA-256 + secp256k1
  ECDSA authentication; debug disable = `CRIT1.DEBUG_DISABLE` + `DEBUGEN_LOCK`;
  flash-at-rest protection = encrypted boot to SRAM with OTP-held AES key; no
  transparent XIP encryption found. All of it is OTP and irreversible.
  Sources: Pico SDK 2.3.1 register headers/`tools\CMakeLists.txt`, RP2350
  datasheet (https://datasheets.raspberrypi.com/rp2350/rp2350-datasheet.pdf),
  security whitepaper
  (https://pip-assets.raspberrypi.com/categories/1260-security/documents/RP-009377-WP-1-Understanding%20RP2350_s%20security%20features.pdf),
  picotool README (https://github.com/raspberrypi/picotool#encrypt , #otp , #save),
  Hacking Challenge results
  (https://www.raspberrypi.com/news/security-through-transparency-rp2350-hacking-challenge-results-are-in/).

### Wrench attacks: the honest analysis

- A "wrench attack" is coercion of the person, not an attack on the cipher.
  The attacker does not need to break Speck, defeat ECDSA, glitch the bootrom,
  or desolder the flash: they make the authorised operator unlock, reveal the
  PSK, or transmit while the attacker watches. **No property of the silicon
  changes this.** Cryptography proves what an honest key-holder did; it cannot
  distinguish a willing key-holder from a coerced one.
- Possession matters independently of coercion. A stolen, powered-on rig has
  derived keys, PSK copies and session state in SRAM (ticket 36 is exactly the
  attempt to reduce this). A powered-off rig still has whatever is in flash —
  which, in the current build, is readable with `picotool save -a` or a flash
  clip, so any long-lived credential stored there is gone.
- Therefore the honest goal is not "stop the wrench" but:
  1. minimise what a seized device can disclose (no long-lived secrets in
     flash; RAM zeroization on teardown);
  2. make passive theft of the *device* yield little (debug lock; encrypted
     boot if code/PSK confidentiality at rest is ever required);
  3. make coercion *detectable and recoverable* operationally (rotate the PSK
     after any loss of custody; treat any board out of sight as hostile);
  4. avoid spending complexity on controls that only look protective.

### Controls evaluated

| Control | Real, honest value | Limits / failure modes | Verdict |
|---|---|---|---|
| RAM zeroization on BYE / brownout (ticket 36) | Reduces live key material after an *orderly* close or reset. Cheap, fits current design. | Best-effort only; no help if the device is grabbed powered-on or the teardown path is skipped/glitched. Does not remove the PSK if it is also in flash. | **Keep — highest value/lowest cost.** |
| No long-lived secrets in flash; provision/rotate PSK out of band | A flash dump then contains no reusable credential. This is the real defence against passive device theft. | Requires an operational provisioning story; does nothing against coercion of the operator who knows the PSK. | **Keep — primary mitigation.** |
| `DEBUG_DISABLE` + `DEBUGEN_LOCK` (OTP) | Stops the cheap "attach a debug probe and read SRAM" path on a seized powered board; also protects decrypted code if encrypted boot is ever adopted. | Irreversible; kills field debugging; demonstrably bypassable by sufficiently funded fault injection (Hacking Challenge). Not a coercion defence. | **Conditional** — production/spare boards only, after spec change. |
| Encrypted boot with OTP key (ticket 35) | Makes external flash contents ciphertext, defeating `picotool save` and flash-clip readout of code/embedded secrets. | Irreversible OTP; forces SRAM/no_flash execution with major impact on Wi-Fi/OLED memory budget; decrypted data still exists in SRAM while running; side-channel/FI caveats. | **Only if confidentiality of code/PSK at rest becomes a requirement.** Otherwise scope creep. |
| Secure boot + signed updates (ticket 35) | Integrity: a stolen board cannot be reflagged with attacker firmware without the signing key; rollback flags add anti-downgrade. | Does not encrypt anything; does not stop readout or coercion; FI bypasses exist. | **Not needed for current threat model.** |
| Tamper-evident seals (numbered tape, epoxy, serialized label) | Honest value is **evidence**: shows whether a unit was opened during shipping/storage, supporting chain-of-custody and "rotate this PSK" decisions. | Does not prevent opening, readout, or coercion; an attacker can ignore, photograph/replace, or simply access the external flash without opening the enclosure. A seal is a log, not a lock. | **Acceptable as logistics detection only — never present as security.** |
| Enclosure switches / magnetic or Hall sensors with wipe-on-open | Can zeroize RAM keys if an attacker opens a *powered* case before attaching a probe; raises cost slightly for a smash-and-grab debug scenario. | Only works while powered and with the sensor path intact; attacker can work through vents/flash pads, cool the part, cut/short the sensor trace, shield the magnet, or glitch the detection path. False positives break field use. Never triggers if the operator is coerced without opening the case. | **Reject for v1** — marginal benefit, real complexity, and spec says enclosures are out of scope. |
| Fingerprint / biometric module | Provides a convenient local presence check. | No secure enclave on RP2350 binds the "match" result to key release; the sensor is just another SPI/UART/I2C peripheral, so the match signal can be replayed, the module swapped, or SRAM read after boot. Fingerprints are irrevocable and can be coerced ("put your finger here"). Sensor/BOM/firmware cost is high. | **Reject — security theater here.** |

### Security theater we explicitly reject

Each of these would make the project *feel* safer without changing attacker
cost, and should not be implemented or claimed:

- A blinking "SECURE"/"LOCKED" LED, "military-grade" silkscreen, or a
  tamper-evident sticker described as stopping theft. It changes no
  attacker capability.
- Signing firmware and calling the device "encrypted". Secure boot
  authenticates; it does not hide the plaintext image or the PSK in flash.
- Disabling SWD while leaving code and PSK in plaintext flash. This blocks the
  easiest SRAM dump but the flash clip still works; it is a partial control
  presented as a complete one.
- Requiring a fingerprint to "unlock" while the PSK is recoverable from SRAM
  or flash by other means. The biometric gate is a UI, not a key boundary.
- An enclosure switch whose wipe path lives in the same unprotected SRAM the
  attacker is trying to read; if the detector can be glitched or bypassed, the
  wipe is not a trust root.
- Claiming OTP/secure boot is unbreakable. Public research (Hacking Challenge:
  voltage glitch on boot vector, laser fault on signature check, double-EMFI
  OTP-page lock bypass) shows it is cost-raising, not absolute; glitch
  detectors reduce but do not eliminate the risk.

### Recommended smallest honest set of controls

1. **Keep secrets out of persistent flash.** No long-lived PSK in plaintext
   flash; provision out of band and rotate after any loss of physical custody.
   This is the only control that meaningfully addresses device theft.
2. **Finish ticket 36 properly.** Zeroize derived keys, PSK copies, ECDH state
   and nonce buffers on BYE and best-effort on brownout/watchdog; assert it in
   host tests and on cold/warm hardware resets. Verify no secret is logged.
3. **Document the wrench attack as an accepted residual risk** in the datasheet
   Limitations section: possession or coercion of an operator with the PSK
   defeats device-side controls; the protocol is designed to protect the link,
   not to resist duress. Add that recovered/lost boards must be treated as
   compromised and their PSK rotated.
4. **Do not add OTP, secure boot, enclosure hardware or biometrics in v1.**
   They are explicitly out of scope in `spec.md`, irreversible, and mostly
   address code confidentiality/integrity rather than the coercion threat.
   Revisit only if the threat model changes and GAMEPLAN is updated first;
   then ticket 35's production sequence (spare board only) is the path.
5. **Optional, labelled honestly:** serialized tamper-evident seal on shipped
   units as chain-of-custody evidence ("was this opened?"), with a documented
   check on receipt. It is not an anti-tamper control and should never be
   described as one.

### Proposed text for the datasheet Limitations section (for the owner to apply)

> Possession and coercion are out of scope. An attacker with physical access
> to a powered rig, or with the ability to compel an authorised operator, can
> obtain live keys or the PSK regardless of link-layer cryptography. Firmware
> and any provisioned secrets are stored in plaintext external flash in the
> v1 build and can be read with `picotool save` or an external flash
> programmer. RAM zeroization on link teardown is best-effort. RP2350 secure
> boot, debug disable and encrypted boot are not enabled; they are irreversible
> OTP changes and, per public fault-injection research, raise attacker cost
> rather than eliminate it. Lost or out-of-sight boards must be considered
> compromised and their PSK rotated.

### Caveats

- I did not perform hardware tests, burn OTP, or examine the enclosure design;
  the enclosure/biometric reasoning is architecture-level, based on RP2350
  having no secure element that could bind a sensor result to key release.
- This is a threat-model recommendation, not an implementation. Checkboxes
  above are left for the ticket owner; no code, CMake, datasheet or other
  ticket files were modified.
