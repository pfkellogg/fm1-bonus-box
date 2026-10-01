#pragma once
// USB MIDI host bridge for the ESP32-S3: every USB MIDI device on the
// USB-C port (through a hub) gets what every OTHER one plays. With a
// keyboard and the FM-1 on the hub, that's keyboard -> FM-1.

#include <stdint.h>

// Installs the ESP-IDF USB host stack and starts its tasks. After this the
// USB-C port is a HOST, so a computer no longer sees the board — re-flash by
// holding BOOT while plugging it in. False on failure.
bool bridgeBegin();

// For the status LED.
int bridgeDeviceCount();       // MIDI devices currently connected
uint32_t bridgeNoteCount();    // note-ons forwarded so far
