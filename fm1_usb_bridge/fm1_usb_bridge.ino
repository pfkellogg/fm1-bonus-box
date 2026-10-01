/*
 * fm1_usb_bridge — bare-bones "play the FM-1 from a USB keyboard" box.
 *
 * No tablet, no computer, no MIDI cable: an ESP32-S3 acts as the USB host
 * for a USB-only MIDI keyboard (e.g. M-Audio Keystation 49 MK3) AND the
 * FM-1, both plugged into a powered USB hub, and forwards what the keyboard
 * plays to the FM-1 over USB.
 *
 *   Keystation ──USB──┐
 *                     ├── powered USB hub ──USB── ESP32-S3 (this sketch)
 *   FM-1 ───────USB───┘
 *
 * What it forwards: channel messages (notes, sustain, pitch bend, mod wheel,
 * program change, aftertouch) from the keyboard's main port, all moved to
 * MIDI channel 1 (the FM-1's default). SysEx, clock and the keyboard's
 * transport-button port are dropped. Every MIDI device gets what every other
 * one plays, so it doesn't matter which hub port is which.
 *
 * Status LED (the Super Mini's onboard RGB LED, GPIO48):
 *   red    = no MIDI devices found
 *   yellow = only one MIDI device found (keyboard or FM-1 missing)
 *   green  = ready (two or more)
 *   blue blink = a note was forwarded
 *   solid magenta = the USB host failed to start
 *
 * Board: ESP32-S3 Super Mini, "ESP32S3 Dev Module" profile
 * (esp32:esp32:esp32s3). No extra libraries.
 *
 * Flashing: the first upload works normally. After that the USB-C port is
 * a USB host, so the computer won't see the board — hold BOOT while
 * plugging it into the computer to upload again.
 *
 * Power: see README.md (the hub has to power both the keyboard and the FM-1).
 */

#include <Arduino.h>
#include "usb_midi_bridge.h"

static const int PIN_RGB = 48;          // Super Mini onboard WS2812
static const uint8_t LED_LEVEL = 24;    // 0-255; the LED is very bright
static const unsigned long BLINK_MS = 40;

static bool hostOk = false;

void setLed(uint8_t r, uint8_t g, uint8_t b) {
  rgbLedWrite(PIN_RGB, r, g, b);
}

void setup() {
  setLed(LED_LEVEL, 0, 0);
  hostOk = bridgeBegin();
}

void loop() {
  if (!hostOk) {
    setLed(LED_LEVEL, 0, LED_LEVEL);
    delay(1000);
    return;
  }

  static uint32_t seenNotes = 0;
  static unsigned long blinkUntil = 0;
  static int shown = -1;  // last LED state: 0 red, 1 yellow, 2 green, 3 blue

  uint32_t notes = bridgeNoteCount();
  if (notes != seenNotes) {
    seenNotes = notes;
    blinkUntil = millis() + BLINK_MS;
  }

  int count = bridgeDeviceCount();
  int want = millis() < blinkUntil ? 3 : count >= 2 ? 2 : count == 1 ? 1 : 0;
  if (want != shown) {
    shown = want;
    switch (want) {
      case 0: setLed(LED_LEVEL, 0, 0); break;
      case 1: setLed(LED_LEVEL, LED_LEVEL, 0); break;
      case 2: setLed(0, LED_LEVEL, 0); break;
      case 3: setLed(0, 0, LED_LEVEL); break;
    }
  }
  delay(5);
}
