---------------------------- MODULE session ----------------------------
\* Ticket 37 verification stack: abstract model of the Phantom Link
\* session/link state machine. This is a MODEL, not the firmware source.
\* It checks the safety properties the firmware claims:
\*   - LINK UP never happens without ECDH-derived keys
\*   - LINK UP never carries session id 0 (the "no session" sentinel)
\*   - keys are never ready without a nonzero session id
\* Finite domain (Ids = 0..3) makes the state graph exhaustively checkable.

EXTENDS Naturals, FiniteSets

VARIABLES link, sessionId, keysReady, pending, heard

vars == <<link, sessionId, keysReady, pending, heard>>

LinkStates == {"DOWN", "UP"}
Ids == {0, 1, 2, 3}

TypeOK == /\ link \in LinkStates
          /\ sessionId \in Ids
          /\ keysReady \in BOOLEAN
          /\ pending \in BOOLEAN
          /\ heard \in BOOLEAN

Init == /\ link = "DOWN"
        /\ sessionId = 0
        /\ keysReady = FALSE
        /\ pending = FALSE
        /\ heard = FALSE

\* HELLO from a peer with a new session id adopts it and defers ECDH.
Hello == /\ link = "DOWN"
         /\ sessionId = 0
         /\ \E n \in Ids \ {0} : sessionId' = n
         /\ pending' = TRUE
         /\ keysReady' = FALSE
         /\ heard' = FALSE
         /\ link' = "DOWN"

\* Main loop completes deferred ECDH before any LINK UP is possible.
Derive == /\ pending = TRUE
          /\ keysReady = FALSE
          /\ keysReady' = TRUE
          /\ pending' = FALSE
          /\ UNCHANGED <<link, sessionId, heard>>

LinkUp == /\ link = "DOWN"
          /\ keysReady = TRUE
          /\ pending = FALSE
          /\ link' = "UP"
          /\ heard' = TRUE
          /\ UNCHANGED <<sessionId, keysReady, pending>>

Heartbeat == /\ link = "UP"
             /\ heard' = TRUE
             /\ UNCHANGED <<link, sessionId, keysReady, pending>>

MissHeartbeat == /\ link = "UP"
                 /\ heard' = FALSE
                 /\ UNCHANGED <<link, sessionId, keysReady, pending>>

\* Timeout clears the session and keys; a later HELLO can start a new one.
Timeout == /\ link = "UP"
           /\ heard = FALSE
           /\ link' = "DOWN"
           /\ sessionId' = 0
           /\ keysReady' = FALSE
           /\ pending' = FALSE
           /\ heard' = FALSE

Stutter == UNCHANGED vars

Next == Hello \/ Derive \/ LinkUp \/ Heartbeat \/ MissHeartbeat \/ Timeout \/ Stutter

Spec == Init /\ [][Next]_vars

NoUpWithoutKeys == link = "UP" => keysReady = TRUE
NoZeroSessionWhenUp == link = "UP" => sessionId # 0
KeysNeedSession == keysReady = TRUE => sessionId # 0
NoPendingWithoutHello == pending = TRUE => sessionId # 0
============================================================================
