# Phantom Link

A two-party, offline messaging system built from two identical Pico 2 W rigs. This glossary fixes the vocabulary used in the plan, the tickets, and the code.

## Units and rigs

**Phantom Link**:
The whole system: two rigs, one private Wi-Fi link, one protocol.
_Avoid_: the app, the device

**Rig**:
One untethered assembly: a Pico 2 W, an SH1106 OLED, a joystick, three buttons, a breadboard, and a zip-tied power bank.
_Avoid_: node, device, unit

**Barebones**:
The AP-side rig. Host and lookout; also called Clyde.
_Avoid_: the server, the base station

**Jank Box 5000**:
The STA-side rig. Client and courier; also called Bonnie.
_Avoid_: the client, the handheld

**Bonnie**:
The STA role: initiator, courier, and driver of SFO bursts.
_Avoid_: client, sender

**Clyde**:
The AP role: host, lookout, and ACK echo.
_Avoid_: server, host

## Protocol

**Bonnie & Clyde protocol**:
The two-party session protocol: framing, sequence numbers, ACKs, replay rejection, encryption, and an optional SFO side channel.
_Avoid_: the radio protocol, the cipher

**Frame**:
One protocol datagram: header, payload, and authentication tag. The unit the receiver validates and the sender retransmits.
_Avoid_: packet, message (a message spans many frames)

**Session**:
An authenticated link between exactly two rigs, created by a handshake and ended by timeout or `BYE`.
_Avoid_: connection, channel

**Handshake**:
The `HELLO` / `HELLO_ACK` exchange that creates a session and proves both sides hold the same key.
_Avoid_: pairing, login

**Link up**:
The state where a session is established and authenticated, as shown on the OLED.
_Avoid_: connected, online

**Message**:
Text a person types on a rig: at most 21 glyphs, carried by many frames, delivered exactly once.
_Avoid_: packet, payload

**Glyph**:
One character drawn from the 49-symbol alphabet the UI can render.
_Avoid_: character, symbol

**Base-6 digit**:
One of the three digits that encode a glyph on the wire.
_Avoid_: symbol, code

## SFO

**SFO** (StegoFrame-order):
The side channel that encodes bits in the order datagrams arrive, inspired by StegoFrameOrder but implemented above the MAC layer.
_Avoid_: covert channel, steganography

**Burst**:
Four small datagrams sent back to back; their arrival order carries one nibble.
_Avoid_: packet group, train

**Nibble**:
The four bits a burst carries, mirrored from the message authentication tag so the receiver can verify the decode.
_Avoid_: code, symbol

**Telemetry-only**:
The v1 rule that SFO is measured and reported but never gates message delivery.
_Avoid_: best-effort, optional

## Security

**PSK** (pre-shared key):
The secret compiled into both rigs that proves they belong to the same pair.
_Avoid_: password, pairing code

**Key confirmation**:
The handshake step that proves both rigs derived the same keys before the UI claims link up.
_Avoid_: authentication, login

**Replay window**:
The sliding record of recently accepted sequence numbers; a frame older than the window, or already seen, is rejected.
_Avoid_: cache, history

**Membership**:
Who is allowed in the link. In v1 it means physical possession of a rig; there is no directory and no invite.
_Avoid_: account, user list

**Pairing**:
A future first-boot exchange of a short code shown on both screens. Not part of v1.
_Avoid_: provisioning, onboarding
