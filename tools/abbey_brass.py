#!/usr/bin/env python3
"""
abbey_brass.py — the project's own "ABBEY BRSS" DX7 patch for the FM-1,
inspired by Arturia Synclavier V's "Abbey Road Brass" (bright 60s ensemble
synth brass, "dedicated to Maxwell's Silver Hammer"). Written from scratch for
this project and released CC0, like doo_voice.py.

Aim: three slightly detuned brass voices for an ensemble sound, each getting
brighter as the note swells in (the brass "blat" — modulator envelopes rise
with the attack), a bright held sustain, a medium release, and a tiny pitch
scoop at the start of each note.

Algorithm 5 (three 2-operator stacks, all 1:1 sawtooth-ish brass):
  OP1 <- OP2   centre voice
  OP3 <- OP4   detuned up
  OP5 <- OP6   detuned down, OP6 feedback adds buzz

    python3 tools/abbey_brass.py          # prints the 128-byte packed voice
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


# Carriers: ~100 ms swell, full sustain, ~0.4 s release.
CARRIER_EG = ((58, 40, 30, 50), (99, 96, 94, 0))
# Modulators: brightness swells in a little behind the volume, then settles bright.
MOD_EG = ((50, 35, 25, 50), (99, 88, 84, 0))

op1 = op(*CARRIER_EG, out=94, coarse=1, detune=7)          # centre
op2 = op(*MOD_EG, out=80, coarse=1, kvs=3)                 # its brightness
op3 = op(*CARRIER_EG, out=88, coarse=1, detune=10)         # up
op4 = op(*MOD_EG, out=78, coarse=1, kvs=3)
op5 = op(*CARRIER_EG, out=88, coarse=1, detune=4)          # down
op6 = op(*MOD_EG, out=72, coarse=1, kvs=3)                 # + feedback buzz

GLOBAL = [
    60, 99, 99, 99, 50, 50, 50, 48,  # pitch EG: start a hair flat (L4=48), rise to centre: brass scoop
    4,                                # algorithm 5 (0-based 4)
    (1 << 3) | 6,                     # osc key sync on, feedback 6 (on OP6)
    30, 70, 3, 0,                     # LFO speed, long delay, light pitch mod, no amp mod
    (2 << 4) | (4 << 1) | 0,          # pitch mod sensitivity 2, sine LFO, no LFO key sync
    24,                               # transpose: none (C3)
]

voice = op6 + op5 + op4 + op3 + op2 + op1 + GLOBAL + list(b"ABBEY BRSS")
assert len(voice) == 128 and all(0 <= x <= 127 for x in voice)

if __name__ == "__main__":
    print(",".join(map(str, voice)))
