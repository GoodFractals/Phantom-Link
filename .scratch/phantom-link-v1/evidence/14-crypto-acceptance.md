# Ticket 14 — crypto hardware acceptance evidence

## Build identity

Build ID: `6333ab72` (banner `phantom-link v1 6333ab72 AP/STA`)

| Artifact | SHA256 |
|---|---|
| `phantom_link_ap.uf2` (normal) | `F7AB9B042808FCD35A6E54805DCC423C3258E907CE7749244A547B0926B28692` |
| `phantom_link_sta.uf2` (normal) | `BC702A15BEBE7BE00A19659E8BA74E0D0A98E872D877DD5D596B446490C5F7CE` |
| `wrongpsk_ap.uf2` | `905DB20786C3353C5FAF0751D7AA10618C750B9DA15C33FBE74AE3D35A7930EE` |
| `wrongpsk_sta.uf2` | `61427F865C74E237688C697C0B7B2DFE106FC7E8FACFA76807D8C3F330D7D67B` |

## Replay rejection (T15)

```text
RX stale type=PING seq=0 id=0x040cbde1 (accepted=561 duplicate=1 stale=7 wrong=0)
```

Injected by the compile-time STA firmware probe. Zero replays accepted.

## CMAC tamper rejection (T16)

```text
CRYPTO FAIL: bad tag (90 bytes)
```

Single-bit flip in the ciphertext of a 64-byte padded DATA frame. Receiver
verified the tag before decrypting and dropped the frame.

## Wrong PSK (T17)

```text
AUTH FAILED: HELLO bad tag (id=0x013ac135 seq=711671848)
```

Wrong-PSK STA produced no `LINK UP`. Normal STA UF2 was restored and the pair
re-linked (PING/PONG flowing).

## Notes

- PC→Pico UDP injection was blocked in this environment; the firmware probe
  replaced it for replay/tamper.
- Overnight repeated resets were the ticket-08 self-reboot workaround firing
  on link loss, not leftover test code.
- Packet capture (no readable text) remains open: no Wireshark/TShark installed.

## Path-1 packet capture (2026-11-24, no hardware purchase)

`tools/frame_probe.c` gained a `capture` mode that emits a valid encrypted
DATA frame (90 bytes) built with the production `pc_kdf` and
`pc_suite_speck_ctr_cmac.seal`, carrying known plaintext
`LINK ALIVE NO TEXT!!!`. `tools/capture_probe.ps1` sends that frame as one UDP
datagram to the default gateway while Windows `pktmon` captures at full packet
size, converts the ETL to pcapng, and searches the raw bytes.

| Artifact | SHA256 |
|---|---|
| `capture14_20261124_170910.pcapng` | `45946C3635F26EB4BDAE1455AE336CED7826814DD13DF3D9B663D012188B2E3B` |
| `capture14_20261124_170910.etl` | `A94B44B5B5FABB3EF3CE3C80FE9C468E4F7B23C4A0C896A11F2F54CFEA119F52` |

Result: frame bytes present at pcapng offset 1646; known plaintext absent from
both the frame bytes and the capture. Host suite still 18/18 at `dbdae4e`.

Boundary: `pktmon` captures above WPA2, so this proves the application-layer
payload is unreadable **after** the Wi-Fi layer is decrypted. It is not an
802.11 monitor capture and does not cover the no-PSK RF case. That evidence is
still pending an ESP32 sniffer.

Two earlier capture attempts were discarded: one logged the datagram as a
transport-endpoint drop (`etl2pcap` excludes drops), the other delivered it
locally so it never crossed the NDIS hooks. The script now refuses to write
evidence unless the frame bytes are actually found in the pcapng.

## Integrated-build boot hang (2026-11-24, root cause)

The integrated `4e2b704f`/`075a7a51` build drew the main UI but never reached
the main loop: LED off, joystick dead, CDC opens timed out, and picotool could
not reboot the device. A temporary `PHANTOM_BRINGUP_TRACE` build (banner
`e4ea40b7`) printed checkpoints on the OLED and stopped at **T1**, i.e. inside
`status_load()` — not in Wi-Fi bring-up as first suspected.

Root cause: `STATUS_FLASH_BASE` is a flash *offset* (`0x3FF000`), but
`status_load()` cast it directly to a pointer (`0x003FF000`), which is
unmapped on RP2350 and hard-faults. The fix reads through `XIP_BASE +
STATUS_FLASH_BASE`; `flash_range_erase/program` keep the offset form.
Moving `watchdog_enable()` after Wi-Fi bring-up removed the misleading reboot
loop but was not the root cause. The watchdog placement change stays because
arming before slow CYW43 init was still unsafe.

### Follow-on fixes found during hardware verification

Once both boards ran the loop, three more timer/flash defects surfaced:

1. **Unsigned timer underflow.** Deadlines were initialized to future values
   (`status_next_save_ms = 60000`, `next_sfo_ms = SFO_BURST_MS`, ...) while the
   checks used `(uint32_t)(now - deadline) >= interval`. With `now < deadline`
   the subtraction wraps to ~4.29e9, so every timer fired immediately: SFO
   flooded the log and `LINK UP` was followed instantly by `LINK LOST`. All
   elapsed checks now use signed `(int32_t)(now - deadline)`, the pattern
   already used for `next_draw_ms`/`next_rssi_ms`.
2. **`status_save()` flash write hangs.** The first `flash_range_erase()` /
   `flash_range_program()` in the main loop stopped the loop permanently
   (observed with the watchdog disabled: the OLED counter froze at L7). Flash
   writes are now **off by default** behind `-DPHANTOM_STATUS_SAVE=1`; reading
   persisted records still works. The proper RP2350 flash/CYW43 quiescing fix
   is ticket 20.
3. **Hands-free flashing.** `picotool load --ser <serial> -f` only matches
   devices already in BOOTSEL, so it could not target an app-mode board when
   both Picos were connected. The firmware now reboots into BOOTSEL on a
   double-`B` CDC command; `tools/flash_pico.ps1` sends it, copies the UF2 by
   matching the BOOTSEL drive's USB serial, and waits for app mode to return.
   Verified hands-free on both boards (`STATUS=complete`, no physical button).

### Clean baseline (production build `3b666adb`)

| Check | Result |
|---|---|
| Both boards LINK UP, joystick live | PASS (user-observed) |
| 70 s stability, session `0xccda5986` | tx=407 accepted=407, dup=0 stale=0 wrong=0 gap=0 retry=0 fail=0 crypto=0 bssid=0 |
| SFO telemetry | 6/6, reorder=0, degraded=0 |
| STATUS dump (S) on AP and STA | link=UP, same session id, clean counters |
| Hands-free reflash + re-link | PASS, new session `0x44fc98fe` on both |
| Host suite / validator | 18/18, `HOST VALIDATION: PASS` |
