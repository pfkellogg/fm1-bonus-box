#!/usr/bin/env python3
"""
oboe_voice.py — the project's own "OBOE" DX7 patch for the FM-1. Written from
scratch for this project and released CC0, like doo_voice.py.

Aim: a nasal, reedy double-reed tone — strong upper harmonics, a pronounced
"nasal" band around the 3rd harmonic, a quick (not clicky) attack, a steady
sustain, a short release, and light vibrato that fades in on held notes.

Algorithm 5 (three 2-operator stacks):
  OP1 (fundamental) <- OP2 (1:1, fairly strong: the reedy body; brighter when played harder)
  OP3 (3rd harmonic: the nasal formant) <- OP4 (1:1, light)
  OP5 (fundamental, slightly detuned) <- OP6 (2:1 with feedback: the buzzy reed edge)

    python3 tools/oboe_voice.py           # prints the 128-byte packed voice
"""


def op(rates, levels, out, coarse, fine=0, detune=7, rs=1, kvs=2, bp=39):
    """One operator in DX7 packed (VMEM) layout, 17 bytes."""
    left_curve, right_curve, ams, osc_mode = 0, 0, 0, 0
    b = list(rates) + list(levels) + [
        bp, 0, 0,                          # breakpoint, left/right depth (no key scaling)
        (right_curve << 2) | left_curve,
        (detune << 3) | rs,
        (kvs << 2) | ams,
        out,
        (coarse << 1) | osc_mode,
        fine,
    ]
    assert len(b) == 17 and all(0 <= x <= 127 for x in b)
    return b


# Quick ~40 ms attack, full sustain while held, short release.
CARRIER_EG = ((75, 45, 35, 60), (99, 96, 94, 0))
MOD_EG = ((70, 40, 30, 55), (99, 90, 86, 0))   # a touch of extra bite at the start

op1 = op(*CARRIER_EG, out=96, coarse=1)                        # fundamental
op2 = op(*MOD_EG, out=78, coarse=1, kvs=3)                     # reedy body
op3 = op(*CARRIER_EG, out=72, coarse=3)                        # nasal 3rd-harmonic band
op4 = op(*MOD_EG, out=58, coarse=1, kvs=2)                     # light colour on the nasal band
op5 = op(*CARRIER_EG, out=68, coarse=1, detune=8)              # body, +1 detune
op6 = op(*MOD_EG, out=56, coarse=2, kvs=3)                     # buzzy edge (with feedback)

GLOBAL = [
    99, 99, 99, 99, 50, 50, 50, 50,  # pitch EG: neutral
    4,                                # algorithm 5 (0-based 4)
    (1 << 3) | 5,                     # osc key sync on, feedback 5 (on OP6): reed buzz
    33, 60, 5, 0,                     # LFO speed (~5 Hz), delay (vibrato fades in), pitch mod depth, amp mod depth
    (3 << 4) | (4 << 1) | 0,          # pitch mod sensitivity 3, sine LFO, no LFO key sync
    24,                               # transpose: none (C3)
]

voice = op6 + op5 + op4 + op3 + op2 + op1 + GLOBAL + list(b"OBOE      ")
assert len(voice) == 128 and all(0 <= x <= 127 for x in voice)

if __name__ == "__main__":
    print(",".join(map(str, voice)))
