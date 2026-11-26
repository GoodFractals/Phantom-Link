# Ticket 22 — untethered power rig measurement plan

**Status:** template, ready to execute. No runtime or current numbers yet — do
not claim "all-day battery" until the table below is filled in from a real run.

## What we are measuring

Two Pico 2 W rigs with OLED, joystick, buttons and CYW43 radio, powered from
their own packs with **no PC attached**. We need three things:

1. A current table (idle / RX / TX / radio power-save on-off).
2. At least one hour of idle time with LINK UP and no pack auto-cutoff.
3. A measured runtime, compared with the pack model within 20%.

The payload is a phone-charge power bank, so it may auto-shutoff at low current.
That behaviour is itself a result — record it, do not design around it silently.

## Preparation

- Use the final shipping build (`PHANTOM_DATA_PROBE=OFF`, Speck default,
  `PHANTOM_STATUS_SAVE=ON`). Flash both boards from BOOTSEL before the run.
- Mark each pack so charge state is readable without unzipping the rig.
- USB cable: strain-relieved so a tug cannot pull the board. The user already
  made this easy to disconnect by design — verify it once before starting.
- Have a USB current meter inline for the current table, then remove it for the
  untethered hour so no PC/meter can prop up the claim.

## Current table (fill in)

| Mode | AP mA | STA mA | Notes |
|---|---|---|---|
| Idle, LINK UP, no traffic | | | decoy chatter still active |
| RX burst (SFO + PING) | | | |
| TX DATA frame | | | 64-byte padded payload |
| Radio power-save on | | | if exposed by the SDK build |
| Radio power-save off | | | default CYW43 behaviour |

If the SDK exposes no CYW43 TX-power or power-save control (ticket 29 already
found no TX-power API), write "not exposed" rather than guessing a number.

## Untethered run

1. Both boards on packs, no USB to a PC, no serial console attached during the
   run. Start both and confirm LINK UP from a brief pre-run serial check.
2. Let them idle with LINK UP for **at least one hour**. Note the wall clock.
3. Confirm neither pack auto-cut off, and both boards still show LINK UP after
   the hour (brief serial reattach is acceptable at the end).
4. Record elapsed runtime until first pack dropout, and the pack capacity used.

| Item | Value |
|---|---|
| Pack model / capacity | |
| AP start time / end time | |
| STA start time / end time | |
| Idle hour completed (yes/no) | |
| Auto-cutoff observed | |
| Measured runtime | |
| Projected runtime | |
| Within 20% (yes/no) | |

## Reporting rules

- If the pack auto-cuts off at idle, the honest claim is "idle hour failed on
  this pack", not "the boards are efficient".
- If runtime is 6 h, say 6 h. Do not extrapolate to "all day".
- Ticket 23 (untethered boot/link and 21-glyph both ways) is a separate gate and
  is not satisfied by this run alone.
- Save raw readings in `logs/`; the committed table in this file is the durable
  evidence because `logs/` is gitignored.
