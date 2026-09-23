# fm1-soundbank

Lists, reorders, and sends a 128-voice soundbank to the
[M-VAVE FM-1](https://www.m-vave.com/products) over USB MIDI — notably, move
a piano patch to slot 001 so that's what plays when the FM-1 powers on.

**No voice data is included.** Put the 4 DX7 32-voice bank `.syx` files you
want to work with in `banks/` (gitignored). Sorted by filename, they become
banks A-D (presets 001-032, 033-064, 065-096, 097-128). The tool reads voice
names straight from the files.

## Does the FM-1 have a "boot patch" preference?

No. Checked the manual's Global (`GLO`) settings end to end — page 1 is
MIDI channels/pitch bend, page 2 is only `Reset Patches` and `Globe Save`
(which just persists global settings like MIDI channel, not a preset
choice). There's no startup-preset setting anywhere. The FM-1 has one flat
list of 128 presets and always powers on at preset 001, whatever that
happens to be. The only way to make it boot into piano is to physically put
a piano voice in slot 001 — which is what this tool's `move` command does.

## The FM-1 can't be read back

**There is no way to ask the FM-1 what it currently has loaded**, from this tool or anything else. It only *receives* SysEx voice data over MIDI — it never transmits its own voices back out. So if you care about what's on your unit now, make sure you have it as `.syx` files *before* sending anything; a send overwrites a full 32-voice bank, not just the slot you rearranged. Units also differ: this project's own FM-1 booted with `BRASS` in slot 001, not the recovered factory set's `PIANO 1`.

If you've built the ESP32-S3 control box (`fm1_control_box/` in this same repo), it does the same job without a computer: load, reorder, and send banks from a phone browser over the box's own WiFi. See the [main README](../README.md), "The soundbank page".

## The FM-1's factory set

Recovered by KingParamount ([fm1-factory-presets](https://github.com/KingParamount/fm1-factory-presets))
by capturing the SysEx M-VAVE's own restore tool sends. The voices
themselves come from Yamaha ROM/VRC cartridges and the community
`Dexed_cart 1.0` compilation, which aren't openly licensed, so they aren't
included here — get the bank files from that repo if you want them.
[`reference/presets_provenance.json`](reference/presets_provenance.json)
documents where each of the 128 factory voices came from (names and sources
only, CC BY-SA 4.0 from that repo's docs).

## Usage

```
pip install mido python-rtmidi   # only needed for `send`

python3 fm1_soundbank.py list                    # all 128 presets, current order

python3 fm1_soundbank.py move "PIANO3" --to 1    # make PIANO3 the boot sound
python3 fm1_soundbank.py move 9 --to 1           # same thing, by preset number

python3 fm1_soundbank.py reorder my_order.txt    # full custom order (128 lines,
                                                  # names or numbers, one per line)

python3 fm1_soundbank.py reset                   # back to the banks/ file order

python3 fm1_soundbank.py export                  # writes export/bank1-4.syx
python3 fm1_soundbank.py send export/bank1.syx   # sends bank1.syx to the FM-1
```

`move`/`reorder`/`reset` only change local state
(`state/current_order.json`) — nothing touches the FM-1 until you `export`
then `send`.

## Actually applying a reorder on the FM-1

1. `export` — writes `export/bank1.syx` (presets 001-032) through
   `bank4.syx` (097-128).
2. `send export/bankN.syx` (or use any SysEx tool — Dexed, PocketMIDI,
   SysEx Librarian — they're plain DX7 bank dumps).
3. Per the manual, the FM-1 will show a bank-slot picker (A/B/C/D) on its
   own screen when it receives a bank. Turn Knob 1-4 there to choose the
   slot — slot A = presets 001-032, B = 033-064, C = 065-096, D = 097-128,
   matching `bank1.syx`..`bank4.syx`. You have to do this part on the
   hardware; MIDI SysEx alone doesn't pick the destination.
4. If you only reordered *within* one 32-voice range (e.g. moved a piano
   that was already in slots 001-032 to slot 001), you only need to send
   that one bank file.

## Files

- `fm1_soundbank.py` — the CLI tool
- `banks/` — your 4 bank `.syx` files (gitignored, you create it)
- `reference/presets_provenance.json` — names + sourcing of the FM-1's 128 factory voices (no voice data)
- `state/current_order.json` — your working reorder (created on first `move`/`reorder`)
- `export/` — generated bank files, ready to `send` (created by `export`)
