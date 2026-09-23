// pitch_detector.h — microphone pitch detection for sing mode.
//
// Samples the mic (MAX9814 module output) with the ESP32-S3's continuous
// ADC (DMA, no CPU per sample) and runs the YIN pitch algorithm on core 0,
// so the main loop on core 1 never waits on it. Results are published as a
// snapshot the main loop polls with pitchLatest().

#pragma once
#include <Arduino.h>

struct PitchReading {
  uint32_t seq;      // increments on every new analysis frame (~30 per second)
  float hz;          // detected fundamental, 0 = silence or unvoiced
  float clarity;     // 0-1, YIN confidence (1 - aperiodicity)
  float level;       // input RMS in ADC counts, after removing DC
};

// Starts sampling on `adcPin` (must be an ADC1 pin — ADC2 is unusable
// while WiFi is on). Returns false if the ADC driver couldn't start.
bool pitchBegin(int adcPin);

// Latest analysis result. Cheap; call as often as you like.
PitchReading pitchLatest();

// Measured sample rate (Hz). The S3's continuous ADC doesn't always run at
// exactly the configured rate, so pitch math uses this measured value.
float pitchSampleRate();
