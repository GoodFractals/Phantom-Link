# Phantom Link v1

Two untethered Pico 2 W rigs exchange authenticated, encrypted messages over a private Wi-Fi link using the Bonnie & Clyde protocol, with an optional SFO telemetry channel.

`docs/GAMEPLAN.md` is the authoritative plan: frame layout, key schedule, SFO spec, test matrix, and phase gates. This directory holds the executable backlog derived from it. When the two disagree, the gameplan wins and the ticket is corrected.

## Scope

- Both rigs run the full OLED/joystick/button UI.
- One `main.c`, two build targets; `IS_AP` selects only the Wi-Fi role.
- Framing, sequence numbers, ACKs, replay rejection, and Speck128/128 encryption.
- SFO as a measured, telemetry-only side channel.
- Untethered power from a zip-tied 2000 mAh pack.

## Out of scope

- Group, mesh, or multi-peer sessions.
- MAC-layer SFO, monitor mode, or raw 802.11 frames.
- Forward secrecy, secure boot, or tamper-resistant key storage.
- Battery-sense hardware and enclosures.

## Working agreement

- Work the frontier: a ticket starts only when every ticket in its **Blocked by** line is done.
- A ticket is done when its acceptance criteria pass on hardware, not when it compiles.
- Every ticket lands as its own commit naming the ticket number.
- If a change breaks a baseline item in `docs/GAMEPLAN.md` §2, revert it rather than fixing it later.
- Hardware-only tickets are marked `ready-for-human`; code tickets are `ready-for-agent`.
