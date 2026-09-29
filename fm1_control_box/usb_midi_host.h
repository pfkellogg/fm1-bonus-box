#pragma once
// USB MIDI host on the ESP32-S3's USB-C port: a class-compliant USB MIDI
// keyboard (e.g. M-Audio Keystation) plugged into the box, directly or
// through a hub, gets read here so the main loop can forward it to the FM-1.

#include <stdint.h>

// Installs the ESP-IDF USB host stack and starts its tasks. After this the
// USB-C port is a HOST: the box no longer shows up on a computer as a serial
// port, so flashing needs the BOOT button (see README). False on failure.
bool usbMidiHostBegin();

// Next 4-byte USB-MIDI event packet from any connected MIDI device
// (byte 0 = cable number << 4 | code index, bytes 1-3 = the MIDI message).
// Non-blocking; false when there's nothing waiting.
bool usbMidiHostRead(uint8_t packet[4]);

// Number of MIDI devices currently connected, and the first one's product
// name ("" if none or it has no name) — for the screen.
int usbMidiDeviceCount();
const char *usbMidiDeviceName();
