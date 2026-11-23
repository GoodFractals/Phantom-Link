# True SFO vs Phantom Link's application-layer variant

The user understands textbook SFO at a basic level but not this project's
hardware-constrained adaptation. Recorded at the start of ticket 16 so future
sessions don't re-teach the distinction or conflate SFO with LPI shaping.

Implications: when teaching ticket 16/17, start from the CYW43439 limitation,
then the 4-datagram permutation, then the "telemetry only, never gates delivery"
rule. LPI shaping (padding/decoys/masking) is a separate layer.

Evidence: user explicitly asked for the fullest breakdown before writing TDD tests.
