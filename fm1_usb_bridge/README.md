# FM-1 USB Bridge (bare-bones)

Play the M-VAVE FM-1 from a USB-only MIDI keyboard (e.g. M-Audio Keystation 49 MK3) with **no tablet, no computer and no MIDI cable** — just an ESP32-S3 and a powered USB hub.

```
Keystation ──USB──┐
                  ├── powered USB hub ──USB── ESP32-S3 Super Mini
FM-1 ───────USB───┘                          (the hub's own cable)
```

The ESP32-S3's USB-C port runs as a **USB host**: it reads the keyboard and sends to the FM-1 over USB, the same thing the Android app does. Plug in power and play.

## Parts

- ESP32-S3 Super Mini (or any ESP32-S3 board whose USB-C goes to the chip's native USB)
- A **powered** USB hub (its own power adapter) — it powers the keyboard and the FM-1
- The hub's own upstream cable (the one that normally goes to a computer) connects it to the ESP32-S3's USB-C port. If that cable ends in USB-C, plug it straight in; if it ends in USB-A, add a small USB-A (female) to USB-C (male) adapter.

## Power

The ESP32-S3 can't power USB devices by itself, which is why the hub must have its own power adapter. The board also needs 5V:

- **Try first:** many powered hubs also send 5V back up their cable, which powers the ESP32-S3 by itself. If the board's LED lights when only the hub is plugged in, you're done.
- **If not:** feed 5V to the board's **5V** and **GND** pins from a USB phone charger (or a spare USB port on the hub).

## Status LED (onboard RGB)

| Color | Meaning |
|---|---|
| red | no MIDI devices found |
| yellow | only one found — keyboard or FM-1 missing |
| green | ready |
| blue blink | a note was forwarded |
| magenta | the USB host didn't start |

## What it forwards

Notes, sustain, pitch bend, mod wheel, program changes and aftertouch from the keyboard's main port, all moved to **MIDI channel 1** (the FM-1's default Note Channel). SysEx, clock, and the Keystation's transport-button port are dropped. Every MIDI device gets what every *other* one plays, so it doesn't matter which hub port is which.

## Flashing

Arduino IDE or `arduino-cli`, board **ESP32S3 Dev Module** (`esp32:esp32:esp32s3`), no extra libraries:

```
arduino-cli compile --fqbn esp32:esp32:esp32s3 fm1_usb_bridge
arduino-cli upload  --fqbn esp32:esp32:esp32s3 -p <port> fm1_usb_bridge
```

The first upload works normally. After that the USB-C port is a USB host, so the computer won't see the board: **hold BOOT while plugging it in** to upload again.

## Status

Compiles (26% flash / 7% RAM). **Not yet tested on hardware.** The USB host code is shared with `fm1_control_box/usb_midi_host.cpp` (also untested on hardware), plus OUT transfers for sending to the FM-1.
