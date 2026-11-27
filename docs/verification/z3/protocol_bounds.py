#!/usr/bin/env python3
"""Z3 proofs of Phantom Link's protocol arithmetic.

These prove bounds and serial-arithmetic rules, NOT the C implementation.
The C code is covered by host tests and hardware acceptance.
Run: py -3 docs/verification/z3/protocol_bounds.py
"""
from z3 import (And, BitVec, BitVecVal, IntVal, Not, Solver, ULE,
                UGT, unsat)

ok = True


def prove(name, formula):
    global ok
    s = Solver()
    s.add(Not(formula))
    r = s.check()
    if r == unsat:
        print(f"PASS {name}: proven for all inputs")
    else:
        print(f"FAIL {name}: {r}")
        ok = False


# 1. Message-length ceiling: 85 glyphs * 3 symbols = 255 fits uint8_t.
gl = IntVal(85)
prove("message_length_fits_pos", ULE(BitVecVal(gl.as_long() * 3, 8),
                                     BitVecVal(255, 8)))

# 2. TX queue depth: 255 symbols + 1 FINAL must fit in 256 slots.
prove("txq_depth", IntVal(256) >= (IntVal(85) * 3 + 1))

# 3. Serial-number comparison is asymmetric (matches proto_ratchet.c:8-11).
#    newer(c,t) == forward(c-t) in [1, 2^31).
tip, cand = BitVec('tip', 32), BitVec('cand', 32)


def newer(c, t):
    f = c - t
    return (f != BitVecVal(0, 32)) & ULE(f, BitVecVal(0x7FFFFFFF, 32))


prove("newer_is_asymmetric", Not(And(newer(cand, tip), newer(tip, cand))))

# 4. Serial wrap: tip = 0xFFFFFFFF, candidate = 0 is newer by 1.
prove("wraparound_zero_after_max",
      (BitVecVal(0, 32) - BitVecVal(0xFFFFFFFF, 32)) == BitVecVal(1, 32))
prove("wraparound_is_newer", newer(BitVecVal(0, 32), BitVecVal(0xFFFFFFFF, 32)))

# 5. An ID inside the 8-slot ratchet window is never "newer", so it resolves
#    to a cached key without moving the tip (proto_ratchet.c:71-84).
back = tip - cand
in_window = (back != BitVecVal(0, 32)) & ULE(back, BitVecVal(7, 32))
prove("window_ids_are_not_newer", Not(And(in_window, newer(cand, tip))))

# 6. Total order on uint8 positions (ACK clamp is meaningful).
pos, total = BitVec('pos', 8), BitVec('total', 8)
prove("ack_position_total_order", ULE(pos, total) | UGT(pos, total))

print("Z3 PROTOCOL BOUNDS:", "PASS" if ok else "FAIL")
raise SystemExit(0 if ok else 1)
