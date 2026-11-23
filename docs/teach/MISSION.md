# Mission: Phantom Link's application-layer SFO

## Why
The user understands textbook SFO but not the tailored, hardware-constrained
variant used in Phantom Link. They need to reason about ticket 16/17 honestly —
knowing what the channel carries, what it cannot do, and why it never affects
message delivery — so the project doesn't overclaim "covert" behavior.

## Success looks like
- Explain in one paragraph how true SFO differs from Phantom Link's burst channel.
- State what one 4-datagram burst encodes and why 8 of 24 permutations are rejected.
- Explain why a failed SFO burst produces `SFO degraded` and never a message failure.
- Distinguish SFO telemetry from LPI traffic shaping (decoys, jitter, padding, masking).

## Constraints
- User is new to agentic firmware development and TDD.
- No token/verbosity budget; they explicitly want full explanations.
- Work happens inside the Phantom Link repo; teaching artifacts live in `docs/teach/`.

## Out of scope
- MAC-layer frame injection or CYW43439 driver hacking (impossible on this hardware).
- Claiming the link is covert (forbidden until ticket 14 is green).
- UI visual design for the telemetry display (parked UI tickets).
