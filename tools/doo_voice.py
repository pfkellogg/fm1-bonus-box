#!/usr/bin/env python3
"""
doo_voice.py — the project's own "DOO VOICE" DX7 patch, used as sing mode's
reference sound. Written from scratch for this project and released CC0, so
unlike third-party banks it can live in the repo.

Aim: a soft, rounded "doo" — mostly fundamental plus a little 2nd harmonic,
a quick-but-not-clicky attack, and gentle delayed vibrato. Algorithm 5
(three 2-operator stacks); only OP1 (fundamental, lightly brightened by OP2),
OP3 (2nd harmonic) and OP5 (fundamental, +1 detune for warmth) sound.

Not auditioned by ear yet. Tweak the numbers below, then:
    python3 tools/doo_voice.py
and paste the printed line over the DOO_VOICE line in
fm1_control_box/web_page.h.
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


CARRIER_EG = ((72, 50, 30, 55), (99, 95, 90, 0))
SILENT_EG = ((99, 99, 99, 99), (99, 99, 99, 0))

op1 = op(*CARRIER_EG, out=99, coarse=1)                               # fundamental
op2 = op((80, 35, 20, 50), (99, 80, 75, 0), out=58, coarse=1, kvs=3)  # soft "d" brightness on OP1
op3 = op(*CARRIER_EG, out=62, coarse=2)                               # 2nd harmonic body
op4 = op(*SILENT_EG, out=0, coarse=1)                                 # unused
op5 = op(*CARRIER_EG, out=70, coarse=1, detune=8)                     # +1 detune warmth
op6 = op(*SILENT_EG, out=0, coarse=1)                                 # unused

GLOBAL = [
    99, 99, 99, 99, 50, 50, 50, 50,  # pitch EG: neutral
    4,                                # algorithm 5 (0-based 4)
    (1 << 3) | 0,                     # osc key sync on, feedback 0
    34, 45, 4, 0,                     # LFO speed, delay, pitch mod depth, amp mod depth
    (3 << 4) | (4 << 1) | 0,          # pitch mod sensitivity 3, sine LFO, no LFO key sync
    24,                               # transpose: none (C3)
]

voice = op6 + op5 + op4 + op3 + op2 + op1 + GLOBAL + list(b"DOO VOICE ")
assert len(voice) == 128 and all(0 <= x <= 127 for x in voice)

if __name__ == "__main__":
    print("const DOO_VOICE = new Uint8Array([" + ",".join(map(str, voice)) + "]);")
