# Use Speck128/128 in CTR mode with CMAC

We need authenticated encryption small enough to audit and fast enough on an RP2350, with published test vectors, and we want the option to swap in a standard AEAD later. We chose Speck128/128 in CTR mode for confidentiality plus CMAC truncated to 8 bytes for authentication, behind a cipher-agnostic `proto_crypto` interface. The alternative was ChaCha20-Poly1305 through mbedTLS: better engineering, but more code and a larger dependency for a two-board demo.

## Consequences

- We own the key schedule, nonce construction, and encrypt-then-MAC ordering, so published vectors and host tests are mandatory.
- The interface must keep an AEAD slot so ChaCha20-Poly1305 can replace Speck without touching the session layer.
- The PSK lives in flash. Physical access defeats it; that is a documented limitation, not a bug.
