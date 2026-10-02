#!/usr/bin/env python3
"""
wendy_flute.py — the project's own "WENDY FLUT" DX7 patch for the FM-1,
inspired by Arturia Mini V4's "Wendy's Flute" (a Moog flute in the style of
Wendy Carlos' Switched-On Bach: soft, mellow, hollow, vibrato on held notes).
Written from scratch for this project and released CC0, like doo_voice.py.

Aim: a pure, slightly hollow tone — fundamental plus a little 2nd and 3rd
harmonic, a soft breathy-ish swell rather than a click, a full sustain, a
short natural release, and vibrato that fades in after the note starts (the
Mini V4 preset puts vibrato on aftertouch, which the Keystation doesn't have).

Algorithm 5 (three 2-operator stacks):
  OP1 (fundamental) <- OP2 (gentle 1:1 modulation: soft reedy edge, more when played harder)
  OP3 (2nd harmonic, quiet)
  OP5 (3rd harmonic, very quiet — the "hollow", clarinet-ish colour of a Moog)
  OP4, OP6 silent

    python3 tools/wendy_flute.py           # prints the 128-byte packed voice
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


# Soft ~80 ms swell, full sustain while held, ~0.3 s release.
CARRIER_EG = ((62, 40, 30, 52), (99, 97, 95, 0))
SILENT_EG = ((99, 99, 99, 99), (99, 99, 99, 0))

op1 = op(*CARRIER_EG, out=99, coarse=1)                               # fundamental
op2 = op((55, 30, 20, 50), (85, 70, 65, 0), out=52, coarse=1, kvs=4)  # soft edge, follows velocity
op3 = op(*CARRIER_EG, out=60, coarse=2, detune=8)                     # 2nd harmonic
op4 = op(*SILENT_EG, out=0, coarse=1)                                 # unused
op5 = op(*CARRIER_EG, out=52, coarse=3, detune=6)                     # 3rd harmonic: hollow colour
op6 = op(*SILENT_EG, out=0, coarse=1)                                 # unused

GLOBAL = [
    99, 99, 99, 99, 50, 50, 50, 50,  # pitch EG: neutral
    4,                                # algorithm 5 (0-based 4)
    (1 << 3) | 0,                     # osc key sync on, feedback 0
    35, 55, 6, 0,                     # LFO speed (~5.5 Hz), delay (vibrato fades in), pitch mod depth, amp mod depth
    (3 << 4) | (4 << 1) | 0,          # pitch mod sensitivity 3, sine LFO, no LFO key sync
    24,                               # transpose: none (C3)
]

voice = op6 + op5 + op4 + op3 + op2 + op1 + GLOBAL + list(b"WENDY FLUT")
assert len(voice) == 128 and all(0 <= x <= 127 for x in voice)

if __name__ == "__main__":
    print(",".join(map(str, voice)))
