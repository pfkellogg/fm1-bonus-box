#!/usr/bin/env python3
"""
fm1_soundbank.py — list, reorder, and re-send a 128-voice soundbank for the
M-VAVE FM-1.

Background
----------
The FM-1 has one flat bank of 128 voices (presets 001-128). The PRESETS
encoder scrolls through them and there is no documented "startup preset"
preference anywhere in Global (GLO) settings — the manual's GLO page 2 only
has Reset Patches and Globe Save. In practice the FM-1 always powers on at
preset 001, whatever that currently is. So the only way to make it boot into
a piano sound is to make sure a piano voice actually occupies slot 001 — by
reordering the bank — not by setting a preference (there isn't one).

No voice data ships with this tool. Put the 4 DX7 32-voice bank .syx files
you want to work with in banks/ next to this script (gitignored, so they
never end up in the repo). Sorted by filename, they become banks A-D
(presets 001-032, 033-064, 065-096, 097-128). For the FM-1's own factory
set, see https://github.com/KingParamount/fm1-factory-presets —
reference/presets_provenance.json here documents where each of those
factory voices came from.

What this tool does
--------------------
- list      Print all 128 presets in current order.
- move      Move one preset to a new slot (e.g. move your favorite piano to
            slot 1 so it's what plays on power-on).
- reorder   Apply a full custom order from a file (one preset-number-per-line).
- reset     Restore the original order of the files in banks/.
- export    Write the current order out as 4 DX7 bank .syx files, ready to
            import into the FM-1.
- send      Send one exported bank .syx to the FM-1 over MIDI. You still
            have to pick the destination bank slot (A/B/C/D) on the FM-1's
            own screen when it prompts, per the manual — this tool can't do
            that part for you, it just puts the SysEx on the wire.

State (the current custom order) lives in state/current_order.json, as a
list of 128 original slot indices (0-127). Nothing here touches the FM-1
unless you run `send`.
"""

import argparse
import json
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
BANKS_DIR = HERE / "banks"
STATE_DIR = HERE / "state"
STATE_FILE = STATE_DIR / "current_order.json"
EXPORT_DIR = HERE / "export"

VOICE_LEN = 128  # bytes per packed DX7 voice
VOICES_PER_BANK = 32

DEFAULT_PORT_NAME = "USB Composite Device"  # how the FM-1 enumerates over USB


def load_packed_voices():
    """Return list of 128 packed 128-byte voice blobs, in banks/ file order."""
    files = sorted(BANKS_DIR.glob("*.syx")) if BANKS_DIR.is_dir() else []
    if len(files) != 4:
        raise SystemExit(
            f"Put exactly 4 DX7 32-voice bank .syx files in {BANKS_DIR} "
            f"(found {len(files)}). Sorted by filename they become banks A-D."
        )
    voices = []
    for path in files:
        raw = path.read_bytes()
        if len(raw) != 4104 or raw[:6] != bytes([0xF0, 0x43, 0x00, 0x09, 0x20, 0x00]) or raw[-1] != 0xF7:
            raise SystemExit(f"{path.name} isn't a standard DX7 32-voice bank dump (4104 bytes)")
        body = raw[6:6 + 4096]
        for i in range(VOICES_PER_BANK):
            voices.append(body[i * VOICE_LEN:(i + 1) * VOICE_LEN])
    assert len(voices) == 128
    return voices


def voice_names(voices):
    return [v[118:128].decode("ascii", "replace").rstrip() for v in voices]


def load_order():
    """Current order = list of 128 original slot indices. Defaults to banks/ (identity) order."""
    if STATE_FILE.exists():
        with open(STATE_FILE) as f:
            order = json.load(f)
        assert sorted(order) == list(range(128)), "state/current_order.json is corrupt"
        return order
    return list(range(128))


def save_order(order):
    STATE_DIR.mkdir(exist_ok=True)
    with open(STATE_FILE, "w") as f:
        json.dump(order, f)


def find_preset(names, order, query):
    """Find a preset by number (1-128) or case-insensitive name match against
    its current position. Returns the position (0-127) in the CURRENT order."""
    query = query.strip()
    if query.isdigit():
        n = int(query)
        if not (1 <= n <= 128):
            raise SystemExit(f"Preset number must be 1-128, got {n}")
        return n - 1
    q = query.lower()
    matches = [pos for pos, orig in enumerate(order) if q in names[orig].lower()]
    if not matches:
        raise SystemExit(f"No preset matches '{query}'")
    if len(matches) > 1:
        hits = [f"{pos + 1:03d} {names[order[pos]]}" for pos in matches]
        raise SystemExit("Multiple presets match '" + query + "':\n  " + "\n  ".join(hits))
    return matches[0]


def cmd_list(args):
    names = voice_names(load_packed_voices())
    order = load_order()
    is_original = order == list(range(128))
    print(f"FM-1 soundbank — {'banks/ order' if is_original else 'CUSTOM order (not yet sent to the FM-1 unless you ran `send`)'}\n")
    for pos, orig in enumerate(order):
        marker = " <- boots here on power-on" if pos == 0 else ""
        print(f"{pos + 1:03d}  {names[orig]:<12}{marker}")


def cmd_move(args):
    names = voice_names(load_packed_voices())
    order = load_order()
    src_pos = find_preset(names, order, args.preset)
    dst_pos = args.to - 1
    if not (0 <= dst_pos <= 127):
        raise SystemExit("--to must be 1-128")
    name = names[order[src_pos]]
    orig = order.pop(src_pos)
    order.insert(dst_pos, orig)
    save_order(order)
    print(f"Moved {name} to slot {dst_pos + 1:03d}.")
    if dst_pos == 0:
        print(f"{name} will now be what plays when the FM-1 powers on.")
    print("Run `export` (and then `send`) to actually push this to the FM-1.")


def cmd_reorder(args):
    names = voice_names(load_packed_voices())
    order = load_order()
    with open(args.file) as f:
        wanted_names_or_nums = [line.strip() for line in f if line.strip()]
    if len(wanted_names_or_nums) != 128:
        raise SystemExit(f"{args.file} must list all 128 presets, one per line; found {len(wanted_names_or_nums)}")
    new_order = []
    for entry in wanted_names_or_nums:
        pos = find_preset(names, order, entry)
        new_order.append(order[pos])
    if sorted(new_order) != list(range(128)):
        raise SystemExit("That list doesn't contain each of the 128 presets exactly once.")
    save_order(new_order)
    print(f"Applied custom order from {args.file}.")


def cmd_reset(args):
    save_order(list(range(128)))
    print("Order reset to the banks/ file order. Run `export` + `send` to push this back to the FM-1.")


def cmd_export(args):
    voices = load_packed_voices()
    order = load_order()
    EXPORT_DIR.mkdir(exist_ok=True)
    for bank_idx in range(4):
        bank_voices = [voices[order[bank_idx * 32 + i]] for i in range(32)]
        body = b"".join(bank_voices)
        assert len(body) == 4096
        checksum = (128 - (sum(body) & 0x7F)) & 0x7F
        sysex = bytes([0xF0, 0x43, 0x00, 0x09, 0x20, 0x00]) + body + bytes([checksum, 0xF7])
        out_path = EXPORT_DIR / f"bank{bank_idx + 1}.syx"
        out_path.write_bytes(sysex)
        print(f"Wrote {out_path} ({len(sysex)} bytes) — presets {bank_idx * 32 + 1:03d}-{bank_idx * 32 + 32:03d}")
    print("\nThese are standard 32-voice DX7 bank dumps. Send each with `send`, or")
    print("with any SysEx tool (Dexed, PocketMIDI, SysEx Librarian, ...) — on the")
    print("FM-1 you'll be prompted to pick the destination bank slot (A/B/C/D) on")
    print("its own screen for each one; slot A holds presets 001-032, B 033-064,")
    print("C 065-096, D 097-128, matching bank1.syx..bank4.syx above.")


def cmd_send(args):
    import mido

    names = mido.get_output_names()
    port_name = args.port
    if port_name is None:
        port_name = DEFAULT_PORT_NAME if DEFAULT_PORT_NAME in names else None
    if port_name is None or port_name not in names:
        print("Available MIDI output ports:")
        for i, n in enumerate(names):
            print(f"  [{i}] {n}")
        idx = input("Enter the port number for the FM-1: ")
        port_name = names[int(idx)]

    sysex_path = Path(args.bank_file)
    data = sysex_path.read_bytes()
    if not (data[0] == 0xF0 and data[-1] == 0xF7):
        raise SystemExit(f"{sysex_path} doesn't look like a SysEx dump (missing F0/F7 framing)")

    msg = mido.Message.from_bytes(data)
    with mido.open_output(port_name) as outport:
        outport.send(msg)
    print(f"Sent {sysex_path.name} ({len(data)} bytes) to '{port_name}'.")
    print("Watch the FM-1's screen — it should now be prompting for a bank slot")
    print("(A/B/C/D). Turn Knob 1-4 there to pick it, per the manual.")


def build_parser():
    p = argparse.ArgumentParser(description="List, reorder, and re-send a 128-voice soundbank for the FM-1.")
    sub = p.add_subparsers(dest="command", required=True)

    p_list = sub.add_parser("list", help="Print all 128 presets in current order")
    p_list.set_defaults(func=cmd_list)

    p_move = sub.add_parser("move", help="Move one preset to a new slot number")
    p_move.add_argument("preset", help="Preset number (1-128) or name/substring to move")
    p_move.add_argument("--to", type=int, required=True, help="Destination slot number (1-128); use 1 to make it the boot sound")
    p_move.set_defaults(func=cmd_move)

    p_reorder = sub.add_parser("reorder", help="Apply a full custom order from a text file (128 lines, names or numbers)")
    p_reorder.add_argument("file", help="Path to a text file listing all 128 presets in the desired order")
    p_reorder.set_defaults(func=cmd_reorder)

    p_reset = sub.add_parser("reset", help="Reset the working order back to the banks/ file order")
    p_reset.set_defaults(func=cmd_reset)

    p_export = sub.add_parser("export", help="Write the current order out as 4 DX7 bank .syx files")
    p_export.set_defaults(func=cmd_export)

    p_send = sub.add_parser("send", help="Send one exported bank .syx file to the FM-1 over MIDI")
    p_send.add_argument("bank_file", help="Path to a .syx file, e.g. export/bank1.syx")
    p_send.add_argument("--port", help=f"MIDI output port name (default: autodetect '{DEFAULT_PORT_NAME}')")
    p_send.set_defaults(func=cmd_send)

    return p


def main():
    parser = build_parser()
    args = parser.parse_args()
    args.func(args)


if __name__ == "__main__":
    main()
