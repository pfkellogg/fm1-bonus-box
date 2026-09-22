# FM-1 Bonus Box

A small companion board for the [M-VAVE FM-1](https://www.m-vave.com/products), talking to it over its 3.5mm TRS MIDI IN. Started as a sustain-pedal-to-CC64 converter on a spare Arduino Uno R3 (**v1**, below); as of 2026-09-22 it's being rebuilt around an **ESP32-S3 Mini** (**v2**, current) to add live preset browsing, a one-button way to set the FM-1's boot sound, and standalone soundbank management over WiFi — in the same small enclosure.

## v2 — ESP32-S3 Control Box (current)

Same sustain-pedal/button job as v1, plus:

- A rotary encoder (KY-040, with a built-in pushbutton) browses the FM-1's 128 presets, with **velocity-based acceleration** — spin fast to cover ground, slow down for single-step precision (see the `ACCEL_TABLE` in the sketch). Every step sends a MIDI Program Change, so you hear the change live — same as turning the FM-1's own PRESETS knob.
- A 1.28" round GC9A01 color TFT (240x240, SPI) shows the browsed preset, **color-coded by category** (piano/organ/synth lead/pad, guitar/bass, brass/woodwind/string/voice, percussion — see "Categories" below) so you recognize where you are in the list at a glance while spinning through it, not just by reading text — plus preset number/name, category name, active bank source, and sustain state.
- **Two bank sources, switchable from the box**: FACTORY (the recovered-factory set embedded in flash) and CUSTOM (your own set, stored in the ESP32's own filesystem — see "Different FM-1 units, different soundbanks" below for why this exists). Short-press the encoder to toggle between them.
- **A medium press (release between 400ms-3s) "Assigns"** the currently browsed preset — from whichever source is active — to slot 001. The FM-1 always powers on at whatever's in slot 001 (there's no separate boot-preset preference — confirmed by checking the manual's Global settings end to end and by testing directly on the device), so this is how you set the boot sound from the box itself, no computer needed.
- **A long press (3s+) opens WiFi Upload Mode**: the box becomes its own WiFi access point serving a small upload page, so you can push a new CUSTOM bank in from any phone or laptop browser — no MIDI cable, no `fm1_soundbank_app`, no computer running special software, ever, after the box itself is flashed.

**Compiles clean** against `esp32:esp32:esp32s3` (77% flash / 22% RAM — the WiFi stack is most of that jump) as of 2026-09-22. **Not yet built or bench-tested** — no physical board yet, same caveat as any new circuit in this project. **Requires a partition scheme with LittleFS/SPIFFS** (Tools > Partition Scheme, e.g. "Default 4MB with spiffs") for CUSTOM bank storage to work.

### Different FM-1 units, different soundbanks

Worth understanding before relying on any of this: **FM-1 units in the wild don't all have the same 128 factory voices.** The embedded FACTORY set here is one specific snapshot — [recovered by KingParamount](https://github.com/KingParamount/fm1-factory-presets) from one unit's restore data — and different units, firmware versions, or prior imports can leave a given FM-1 with different content. This was confirmed directly on this project's own unit: it booted with BRASS at slot 1, not the FACTORY set's PIANO 1, before any of this tooling touched it.

**The FM-1 can't be asked what it currently has loaded.** It only receives SysEx voice data over MIDI — it never transmits its own voices back, to this box, to a computer, or to anything else. That's not a software limitation to work around; it's how the hardware/firmware is built (same reason KingParamount had to intercept M-VAVE's own restore-tool traffic to recover the FACTORY set at all, rather than just asking the device). So if your FM-1's current soundbank is better than FACTORY, **this box cannot extract it from the device** — nothing can, over MIDI.

What it *can* do: hold a second full soundbank (CUSTOM) in its own storage, and push either source to the FM-1 on demand. Getting your preferred voices into CUSTOM means having them as standard DX7 bank `.syx` files from wherever they originally came from — Dexed, a SysEx librarian, a backup taken before an import, etc. — and uploading those via WiFi Upload Mode (below). If you don't already have `.syx` backups of whatever's better about your unit's current set, there unfortunately isn't a way to generate them after the fact; back up voices you care about *before* overwriting them, the same caution that applies to using `fm1_soundbank_app` from a computer.

### Controls

| Action | Result |
|---|---|
| Turn encoder | Browse presets (accelerates with speed), live MIDI Program Change |
| Short press, release < 400ms | Toggle bank source: FACTORY ↔ CUSTOM |
| Medium press, release 400ms-3s | Assign browsed preset to slot 001 (active source) |
| Long press, release ≥ 3s | Toggle WiFi Upload Mode on/off |
| Pedal / built-in button | Sustain (CC64) |

The screen shows a live hint ("release: ASSIGN" / "release: WIFI UPLOAD") once you've held past each threshold, so you don't have to count seconds.

### Loading a CUSTOM bank over WiFi

1. Hold the encoder button 3+ seconds. The box becomes a WiFi access point: SSID `FM1-ControlBox`, password `fm1setup1` (change both in the sketch before relying on this outside a trusted room — this is an open, unencrypted-beyond-WPA2-PSK local AP, not meant to be internet-facing).
2. Connect a phone or laptop to that network, browse to `http://192.168.4.1`.
3. Pick which quarter (Bank A/B/C/D, matching presets 001-032/033-064/065-096/097-128) and choose a `.syx` file — a standard 4104-byte DX7 32-voice bank dump, the same format `fm1_soundbank_app`'s `export` produces and any SysEx librarian (Dexed, PocketMIDI, SysEx Librarian...) can save. Upload.
4. Repeat for any other quarters you want to set. Unset quarters keep whatever they already had (a fresh CUSTOM bank starts as a full copy of FACTORY, so it's always fully playable even if you only ever upload one quarter).
5. Hold the encoder 3+ seconds again to close WiFi mode. Short-press to switch to CUSTOM if it wasn't already active, then browse/Assign as usual.

CUSTOM is saved to the ESP32's flash (LittleFS), so it survives power cycles and reflashing the sketch (reflashing the filesystem partition itself would wipe it, ordinary sketch uploads won't).

### Categories

The FM-1's factory 128 aren't a flat list — they're 4 categories interleaved within each 32-preset bank (confirmed against the actual factory names in `reference/presets_provenance.json`, below):

| Bank | Slots | Categories (8 each, interleaved) |
|---|---|---|
| A | 001-032 | PIANO, ORGAN, SYN LEAD, SYN PAD |
| B | 033-064 | GUITAR, DS GUITAR, BASS, SYN BASS |
| C | 065-096 | BRASS, WOODWIND, STRING, VOICE |
| D | 097-128 | Percussion & effects (bucketed as one PERC/FX category — not a 4-way split like A-C) |

`tools/generate_soundbank_header.py` derives each slot's category from its position (not stored per-voice) and bakes in a distinct color per category (`CATEGORY_COLOR565`) that the TFT fills the background with.

### Schematic and layout

| Schematic | Physical layout |
|---|---|
| [![schematic](schematics/fm1_control_box_schematic.png)](schematics/fm1_control_box_schematic.pdf) | [![layout](schematics/fm1_control_box_layout.png)](schematics/fm1_control_box_layout.pdf) |

PDFs (linked above) are the high-res versions. `schematics/generate_fm1_control_box_schematic.py` regenerates both from source (matplotlib; `pip install matplotlib` first).

### What "Assign" actually does, and its real limits

Per "Different FM-1 units, different soundbanks" above: the FM-1 never sends its voice data back over MIDI, so this box has no way to know what's actually loaded in the FM-1's memory right now. Assign works from its own local copy of a soundbank — FACTORY (`fm1_control_box/fm1_soundbank_data.h`, embedded in flash) or CUSTOM (`/custom_voices.bin`, in the ESP32's filesystem, loaded via WiFi Upload Mode), whichever is currently active — never a read-modify-write of the real device state.

Concretely:
- A DX7 bank dump is always 32 voices, so Assign rewrites **all 32 presets** in whichever quarter the target falls in (001-032, 033-064, 065-096, or 097-128) — with your chosen preset moved into slot 1 of that quarter. Anything currently sitting anywhere else in that same 32-preset range on the real FM-1 gets overwritten back to whatever the active source has for that quarter. This is exactly what was done by hand from the computer on 2026-09-22 (moving a piano to slot 001, using FACTORY) — the box just automates that same action, now for either source.
- After Assign sends the SysEx, **the FM-1 still shows its own A/B/C/D bank-slot picker** on its screen and needs a physical knob turn on the FM-1 itself (Knob 1 for slot A, matching presets 001-032) to commit. The box prompts on the TFT but can't press that knob for you — confirmed this step is required, not optional, when doing this from a computer earlier the same day.

### Parts

- ESP32-S3 Mini ("Super Mini", ESP32-S3FH4R2 — 3.3V logic, unlike the Uno's 5V)
- [WayinTop 360-degree rotary encoder module](https://www.amazon.com/dp/B07T3672VK) (KY-040, 5-pin breakout)
- [1.28" round GC9A01 color TFT](https://www.amazon.com/dp/B0C1G92F2B) (e.g. D-FLIFE, 240x240, 4-wire SPI) — same part + library already proven in this project's [fm1-midi-voice-tuner](https://github.com/pfkellogg/fm1-midi-voice-tuner)
- 1/8" (3.5mm) TRS panel-mount jack, wired to the sustain pedal (reused from v1)
- 1/8" (3.5mm) TRS panel-mount jack, for MIDI out (reused from v1)
- Momentary panel pushbutton (normally-open SPST), for sustain with no pedal plugged in (reused from v1)
- 2x 220ohm resistors (MIDI out circuit)
- 3.5mm TRS-to-TRS cable, to reach the FM-1's MIDI IN

### Hardware

MCU: **ESP32-S3 Mini** ("Super Mini", ESP32-S3FH4R2 — 3.3V logic, unlike the Uno's 5V).

Pin map (avoid ESP32-S3's strapping pins 0/3/45/46):

| Pin | Function |
|---|---|
| GPIO4 | MIDI OUT (UART1 TX, through 220ohm to TRS tip) |
| GPIO5 | Sustain pedal/button (`INPUT_PULLUP`) — same node design as v1 |
| GPIO6 | Encoder CLK |
| GPIO7 | Encoder DT |
| GPIO15 | Encoder SW (pushbutton, `INPUT_PULLUP`) |
| GPIO10 | TFT CS |
| GPIO11 | TFT DC |
| GPIO12 | TFT RES (reset) |
| GPIO13 | TFT SCL (SPI clock) |
| GPIO14 | TFT SDA (SPI MOSI) |

TFT's `VCC` and `BLK` (backlight) both tie straight to 3V3 — no GPIO needed, backlight always-on (no PWM dimming built yet), same choice already proven on this exact display in fm1-midi-voice-tuner.

**MIDI OUT (TRS, Type A — same convention as v1):**
```
GPIO4 (TX) -> 220ohm -> TRS tip
3V3        -> 220ohm -> TRS ring
GND        ->          TRS sleeve
```
v1 drove this from 5V; at 3.3V the opto in the FM-1's MIDI IN gets less drive current (~2.5mA vs. the MIDI-spec 5mA). Widely reported to still work fine with modern high-gain optos, but **this needs bench confirmation** before trusting it — if the FM-1 doesn't respond, try 33ohm/10ohm in place of the 220ohm pair, or add a transistor/inverter buffer stage.

**Sustain pedal/button:** identical wiring to v1's D2 node (see below), just on GPIO5 instead.

**Encoder:** [WayinTop 360-degree rotary encoder module](https://www.amazon.com/dp/B07T3672VK) (KY-040 — 5-pin breakout, onboard pull-ups, 20 detents/revolution). `+` to 3V3, `GND` to GND, `CLK`/`DT`/`SW` to GPIO6/7/15.

**Round TFT (1.28" GC9A01, SPI):** [D-FLIFE or similar](https://www.amazon.com/dp/B0C1G92F2B) — `CS`/`DC`/`RES`/`SCL`/`SDA` to GPIO10/11/12/13/14, `VCC` and `BLK` both to 3V3, `GND` to GND. Listed "Driving voltage: 3-5V" on the linked module, so wiring straight to 3V3 is safe without a level shifter — **confirm your specific module's spec sheet matches** before assuming that; some bare GC9A01 panels floating around are 3.3V-only or need a level shifter, per the same caution already noted in fm1-midi-voice-tuner's README.

**Live browsing needs FM-1 firmware v14+.** Program Change support for preset selection was added to the FM-1 in firmware v14 — on older firmware the encoder's live-audition (turn = hear the change) won't do anything, though Assign (which sends a SysEx bank dump, not Program Change) doesn't depend on this and should still work. Check/update firmware via M-VAVE's tool if browsing seems dead.

### Firmware

`fm1_control_box/fm1_control_box.ino`. Libraries (Library Manager): `Adafruit GC9A01A` + `Adafruit GFX Library` + `Adafruit BusIO`, `ESP32Encoder` (madhephaestus). `WiFi`, `WebServer`, and `LittleFS` are part of the `esp32` core itself, nothing extra to install. Board: `esp32` core (Espressif), profile `ESP32S3 Dev Module` (`esp32:esp32:esp32s3`) unless your specific Super Mini clone publishes its own board profile — **and a Partition Scheme that includes LittleFS/SPIFFS** (Tools menu), or CUSTOM bank storage silently falls back to RAM-only (works until power-off, then reverts to a copy of FACTORY).

**Encoder scaling is untuned** — `STEPS_PER_DETENT` in the sketch is set to 1, but KY-040/EC11 modules commonly report 2 raw quadrature counts per physical detent with `attachHalfQuad()` depending on the exact module/library version. First thing to check on the bench: turn the knob exactly one detent (slowly, well outside the acceleration thresholds) and confirm the display's preset number moved by exactly 1. If it jumps by 2 (or 4), raise `STEPS_PER_DETENT` to match.

**Encoder acceleration is likewise untuned** — the `ACCEL_TABLE` thresholds (80/150/300ms between steps → 8/4/2/1 presets per step) are a starting guess, not bench-measured. Adjust to taste once you can actually feel how fast a real spin registers.

`fm1_soundbank_data.h` (128 preset names, all 128 packed DX7 voices, and per-slot category index + colors, ~16KB) is generated by `tools/generate_soundbank_header.py` from `reference/` (below). Re-run that script if the reference data ever changes.

**WiFi Upload Mode is compiled and internally consistent but entirely unbench-tested** — no physical upload has been attempted yet (no board built). The upload parsing (multipart form handling, DX7 header/checksum validation) follows the standard ESP32 `WebServer` HTTPUpload pattern, but hasn't been exercised against a real `.syx` file over a real HTTP request. Test with a known-good bank file (e.g. `reference/banks/FM-1_factory_bank1.syx`, included in this repo) before trusting it with anything irreplaceable.

### Flashing

USB-C into the ESP32-S3 Mini for both power and flashing. **Disconnect the MIDI TX line (GPIO4) before uploading** if it's wired up — same class of gotcha as v1's Arduino, though the ESP32-S3's native USB-CDC serial is less likely to collide with GPIO4 than the Uno's shared pins 0/1 were.

---

## v1 — Arduino Uno, sustain only (superseded by v2, kept for reference)

Confirmed working end-to-end 2026-08: physical pedal → Arduino → TRS MIDI (Type A) → FM-1 sustain. `fm1_sustain_footswitch.ino` at the repo root is this version; superseded by `fm1_control_box/` above but left in place/working as a simpler fallback if the ESP32-S3 build isn't wanted.

### Schematic and layout

| Schematic | Physical layout |
|---|---|
| [![schematic](schematics/fm1_footswitch_schematic.png)](schematics/fm1_footswitch_schematic.pdf) | [![layout](schematics/fm1_footswitch_layout.png)](schematics/fm1_footswitch_layout.pdf) |

PDFs (linked above) are the high-res versions. `schematics/generate_fm1_footswitch_schematic.py` regenerates both from source (matplotlib; `pip install matplotlib` first). This is v1's Arduino Uno wiring — see v2 above for the current ESP32-S3 diagrams.

### Parts

- Arduino Uno R3
- 1/8" (3.5mm) TRS panel-mount jack, wired to the sustain pedal
- 1/8" (3.5mm) TRS panel-mount jack, for MIDI out
- Momentary panel pushbutton (normally-open SPST), for sustain with no pedal plugged in
- 2x 220ohm resistors (MIDI out circuit)
- 3.5mm TRS-to-TRS cable, to reach the FM-1's MIDI IN

### Wiring

**Pedal input (TRS jack):**
```
pedal jack tip           -> Arduino D2
pedal jack ring + sleeve -> Arduino GND (tied together)
```
D2 is `INPUT_PULLUP`, so the pedal just needs to short tip to ring/sleeve when pressed. Ring and sleeve are tied together so a plain mono (TS) pedal plug still grounds correctly when inserted into the TRS jack. This covers the vast majority of 1/8" sustain pedals (simple momentary SPST, normally open).

**Built-in button (optional, for no pedal):**
```
button leg 1 -> Arduino D2   (same node as pedal jack tip)
button leg 2 -> Arduino GND  (same node as pedal jack ring/sleeve)
```
Wired straight in parallel with the pedal jack — no separate pin, no code changes. Either the button or the pedal alone can pull D2 low, so pressing the button works whether or not a pedal is plugged in. If a pedal *is* plugged in and its footswitch is a normally-closed momentary (rare), it'll hold D2 low all the time, which would also hold the panel button's effect at "always on" — not an issue for the standard normally-open pedals this is designed for.

**MIDI output (direct to TRS, Type A):**
```
Arduino TX (pin 1) -> 220ohm resistor -> TRS jack tip
Arduino 5V         -> 220ohm resistor -> TRS jack ring
Arduino GND        -> TRS jack sleeve
```
Then a plain 3.5mm TRS-to-TRS cable into the FM-1's MIDI IN.

### Flashing

Upload `fm1_sustain_footswitch.ino` as usual. **Disconnect the TRS output jack (or at least the TX line) from the Arduino before uploading** — pins 0/1 are shared with the USB serial the IDE uses to program the board, and the resistor circuit sitting on TX can interfere with the upload.

### If it comes up backwards

Two independent polarity unknowns here, each with a one-line fix — don't chase the other one first:

- **Pedal polarity** (sustain reads "on" at idle, "off" when pressed): flip `kInvert` to `true` in the sketch and re-upload.
- **TRS MIDI type** (FM-1 doesn't respond at all): this board is wired Type A (tip = signal, ring = +5V, sleeve = GND). If the FM-1 turns out to expect Type B, swap tip and ring at the jack (or re-wire): tip = GND, ring = signal, sleeve = GND. Applies to v2 as well.

## History

A pitch-strip input (linear softpot) was planned for v1 but never got past "part not sourced" before the ESP32-S3 rebuild started — dropped rather than ported to v2 for now; revisit if wanted later.

An OLED display, a mode switch (to flip the OLED between sustain status and a live pitch readout), and an audio-input pitch detector (reading the FM-1's own headphone output) all previously lived on an earlier version of this board. All three were removed 2026-08-09 — the OLED moved permanently to a separate project, [fm1-midi-voice-tuner](https://github.com/pfkellogg/fm1-midi-voice-tuner), which reads the FM-1's MIDI output instead of its audio output. See git history on this repo if the mode switch or audio pitch detector are ever needed again. (v2's round TFT, above, is a new, unrelated addition built for preset browsing, not a return of this one.)

## Files

```
fm1_control_box/                       v2 — ESP32-S3 firmware
  fm1_control_box.ino
  fm1_soundbank_data.h                 generated, see tools/ below
fm1_sustain_footswitch.ino             v1 — Arduino Uno firmware
schematics/
  generate_fm1_control_box_schematic.py / fm1_control_box_*.{pdf,png,svg}   v2 diagrams
  generate_fm1_footswitch_schematic.py / fm1_footswitch_*.{pdf,png,svg}     v1 diagrams
tools/
  generate_soundbank_header.py         regenerates fm1_soundbank_data.h from reference/
reference/                             vendored soundbank data (see below)
  presets_provenance.json
  banks/FM-1_factory_bank[1-4].syx     factory bank dumps, also your restore-to-stock files
  banks/FM-1_factory_128voices_packed.bin
```

`reference/` is vendored from [KingParamount/fm1-factory-presets](https://github.com/KingParamount/fm1-factory-presets), so this repo's build scripts are self-contained and don't depend on a separate project:
- `banks/*.syx` and `banks/FM-1_factory_128voices_packed.bin` are that repo's own SysEx capture/decode work, released [CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/) (public domain) — the voice parameters themselves originate from Yamaha ROM/VRC cartridges and the community Dexed_cart 1.0 compilation, as selected/renamed by M-VAVE; see that repo's LICENCE.md and docs/protocol-and-provenance.md for the full per-voice trace and individual patch-author credits.
- `presets_provenance.json` was generated this session from that repo's `docs/protocol-and-provenance.md` (CC BY-SA 4.0) — attributed here accordingly.
