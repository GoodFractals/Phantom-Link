# Model proofs vs reality — three kinds of verification

The user asked how car brakes can be trusted "a billion/billion" times and
which open-source tools prove determinism. Recorded alongside Lesson 005 so
future sessions do not overclaim what TLA+, Z3 or CBMC establish.

Key distinction the user now understands:

- TLA+ / TLC checks every state of a **model you write**. Wrong model, worthless proof.
- Z3 proves an **arithmetic statement**; a wrong statement yields a misleading proof.
- CBMC checks C **within bounds you choose**; beyond the bound is unchecked.
- None proves the physical board. Serial logs and hardware acceptance do that.

Implication for claims: say "session model checked, bounds proven, codec
harness staged, hardware verified" — never "formally verified codebase".

Evidence: user explicitly asked to /teach the tools and described wanting
"best, maxed out specs". Their instinct (cheap hardware = shared parameters)
is the right framing for why bounded proofs are valuable here.
