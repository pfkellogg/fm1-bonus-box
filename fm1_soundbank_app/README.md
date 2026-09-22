# fm1-soundbank

Lists the [M-VAVE FM-1](https://www.m-vave.com/products)'s 128 factory
presets with where each one actually came from, and lets you reorder them —
notably, move a piano patch to slot 001 so that's what plays when the FM-1
powers on.

## Does the FM-1 have a "boot patch" preference?

No. Checked the manual's Global (`GLO`) settings end to end — page 1 is
MIDI channels/pitch bend, page 2 is only `Reset Patches` and `Globe Save`
(which just persists global settings like MIDI channel, not a preset
choice). There's no startup-preset setting anywhere. The FM-1 has one flat
list of 128 presets and always powers on at preset 001, whatever that
happens to be. The only way to make it boot into piano is to physically put
a piano voice in slot 001 — which is what this tool's `move` command does.

## Different FM-1 units, different soundbanks

**The 128 voices documented here are one specific recovered snapshot, not a universal constant.** FM-1 units don't necessarily all ship with, or keep, the same factory content — different units, firmware versions, or a previous owner's imports can leave a given FM-1 with different voices in some or all of its 128 slots. This isn't hypothetical: the unit this project was built against booted with `BRASS` in slot 001, not this doc's `PIANO 1`, before any reordering was done — confirming its actual on-device content had already diverged from the set recovered here.

**There is no way to ask the FM-1 what it currently has loaded**, from this tool or anything else. It only *receives* SysEx voice data over MIDI — it never transmits its own voices back out, to a computer or to anything. That's the same reason recovering the factory set in the first place required intercepting M-VAVE's own restore-tool traffic (see below) rather than just reading it off the device directly.

Practical upshot: if your FM-1's current soundbank is better than what's documented here, **this tool can't extract it and neither can anything else** — there's no read path. What you *can* do is keep (or recreate) it as standard DX7 bank `.syx` files — from wherever it originally came from, a SysEx librarian's export, a backup taken before an import — and use those instead of/alongside this repo's `reference/banks/*.syx`. Back up voices you care about *before* running `export` + `send` against slots that hold them; a send overwrites the full 32-voice quarter, not just the slot you're rearranging.

If you've built the ESP32-S3 control box (`fm1_control_box/` in this same repo), it has a standalone way to hold a second ("CUSTOM") soundbank and push either it or the FACTORY set to the FM-1 without a computer at all — you load CUSTOM once via a phone/laptop browser over the box's own WiFi, then it's self-contained from the box afterward. See the [main README](../README.md), "Different FM-1 units, different soundbanks" and "Loading a CUSTOM bank over WiFi".

## Where the factory sounds came from

Recovered by KingParamount ([fm1-factory-presets](https://github.com/KingParamount/fm1-factory-presets))
by capturing the SysEx M-VAVE's own (Windows, Chinese-language) restore
tool sends, since the FM-1 never transmits its voice data itself. Short
version:

- 126 of the 128 voices are byte-identical to patches in BlackWinny's
  `Dexed_cart 1.0` compilation (a long-circulating deduplicated pool of DX7
  community patches), one more is a single byte off.
- 40 of those 128 also trace back to genuine Yamaha DX7 ROM/VRC cartridges
  (they ended up in the community pool too).
- The preset *names* mostly come from the folder each patch was found in,
  not the patch's own original name — e.g. FM-1's `GUITAR 3` is DX7 ROM1B's
  `BANJO`, and `WOODWIND 7` is ROM1B's `ACCORDION`.

Full per-voice sourcing is in [`reference/presets_provenance.json`](reference/presets_provenance.json)
(one entry per slot: current name, the source file/version in the Dexed
cart pool, the patch's original name there, and the matching Yamaha
cartridge voice when there is one). Run `list --provenance` to see it
inline.

`reference/banks/*.syx` are that repo's recovered factory bank dumps
(standard 32-voice DX7 bank SysEx, confirmed byte-for-byte against a fresh
`export` of the untouched factory order) — also your way back to stock if
`reset` + `export` + `send` isn't enough (e.g. after a firmware update
wipes things differently).

## Usage

```
pip install mido python-rtmidi   # only needed for `send`

python3 fm1_soundbank.py list                    # all 128 presets, current order
python3 fm1_soundbank.py list --provenance       # + where each one came from

python3 fm1_soundbank.py move "PIANO3" --to 1    # make PIANO3 the boot sound
python3 fm1_soundbank.py move 9 --to 1           # same thing, by preset number

python3 fm1_soundbank.py reorder my_order.txt    # full custom order (128 lines,
                                                  # names or numbers, one per line)

python3 fm1_soundbank.py reset                   # back to factory order

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
- `reference/presets_provenance.json` — all 128 factory voices' names + sourcing
- `reference/banks/FM-1_factory_bank[1-4].syx` — untouched factory banks (also your restore-to-stock files)
- `reference/FM-1_factory_128voices_packed.bin` — same 128 voices, no SysEx wrapper, used internally for fast slicing/reordering
- `state/current_order.json` — your working reorder (created on first `move`/`reorder`)
- `export/` — generated bank files, ready to `send` (created by `export`)
