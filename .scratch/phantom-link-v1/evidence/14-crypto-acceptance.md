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
