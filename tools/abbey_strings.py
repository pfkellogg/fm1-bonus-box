#!/usr/bin/env python3
"""
abbey_strings.py — the project's own "ABBEY STRS" DX7 patch for the FM-1: a
warm, bowed string ensemble in the spirit of the Beatles' "Eleanor Rigby"
string octet (a companion to abbey_brass.py). Written from scratch for this
project and released CC0, like doo_voice.py.

Aim: a section rather than one instrument — three slightly detuned bowed
voices, a gentle bow-in attack, a full sustain, a smooth release, warm (not
buzzy) tone that brightens when played harder, and a moderate vibrato that
fades in on held notes.

Algorithm 5 (three 2-operator stacks, all 1:1 for a bowed, sawtooth-ish tone):
  OP1 <- OP2   centre voice
  OP3 <- OP4   detuned up
  OP5 <- OP6   detuned down, light OP6 feedback for bow "rosin"

    python3 tools/abbey_strings.py        # prints the 128-byte packed voice
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


# Carriers: ~200 ms bow-in, full sustain, ~0.7 s release.
CARRIER_EG = ((48, 35, 25, 42), (99, 97, 95, 0))
# Modulators: tone opens with the bow, settles warm.
MOD_EG = ((45, 30, 20, 42), (92, 80, 76, 0))

op1 = op(*CARRIER_EG, out=93, coarse=1, detune=7)          # centre
op2 = op(*MOD_EG, out=70, coarse=1, kvs=3)                 # its tone
op3 = op(*CARRIER_EG, out=88, coarse=1, detune=9)          # up
op4 = op(*MOD_EG, out=68, coarse=1, kvs=3)
op5 = op(*CARRIER_EG, out=88, coarse=1, detune=5)          # down
op6 = op(*MOD_EG, out=64, coarse=1, kvs=3)                 # + light feedback

GLOBAL = [
    99, 99, 99, 99, 50, 50, 50, 50,  # pitch EG: neutral
    4,                                # algorithm 5 (0-based 4)
    (1 << 3) | 3,                     # osc key sync on, feedback 3 (on OP6)
    34, 50, 7, 0,                     # LFO speed (~5.5 Hz), delay (vibrato fades in), pitch mod depth, no amp mod
    (3 << 4) | (4 << 1) | 0,          # pitch mod sensitivity 3, sine LFO, no LFO key sync
    24,                               # transpose: none (C3)
]

voice = op6 + op5 + op4 + op3 + op2 + op1 + GLOBAL + list(b"ABBEY STRS")
assert len(voice) == 128 and all(0 <= x <= 127 for x in voice)

if __name__ == "__main__":
    print(",".join(map(str, voice)))
