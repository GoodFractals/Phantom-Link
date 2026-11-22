# Regression checklist

Run this after every phase, on hardware, before calling the phase done. It is
the acceptance gate from `docs/GAMEPLAN.md` §2. If a change breaks an item,
revert the change; do not fix it later.

**Build:** both targets build with `-Wall -Wextra -Werror` and no warnings.
**Host:** `ctest` passes and `python firmware/tests/validate_assets.py` prints `HOST VALIDATION: PASS`.

| # | Check | Pass condition |
|---|---|---|
| 1 | Boot banner | Serial shows `phantom-link v1 <hash> AP` or `STA` before any Wi-Fi line |
| 2 | UI on both boards | AP and STA each show the boot splash, then the main UI |
| 3 | Input on both boards | Joystick moves the cursor; SELECT types on both boards |
| 4 | Erase and back | GP12 BACK erases a glyph. This rig has no DELETE label on the OLED; the firmware also aliases DELETE on GP15 |
| 5 | Addressing | AP is `192.168.4.1`; STA reports `192.168.4.2` |
| 6 | Handshake | Cold boot reaches `LINK UP` in under 5 s |
| 7 | Heartbeat | Link stays up while idle; power-cycling one board shows `LINK LOST` within 3 s |
| 8 | Message round-trip | A 21-glyph message arrives exactly in both directions; the sender's draft clears on delivery |
| 9 | Non-blocking TX | UI and input respond while a message is transmitting |
| 10 | HUD | STA shows RSSI bars; AP signal cell is empty or shows relayed data, never a fake reading |
| 11 | Abort | GP12 BACK mid-send stops the message, leaves no partial text on the receiver, and keeps the draft on the sender |
| 12 | Recovery | The pair re-links after either board restarts |

**Record:** paste both serial logs, a photo of each OLED, and the UF2 hashes.
A check is not passed because the code looks right; it is passed because the
hardware did it.
