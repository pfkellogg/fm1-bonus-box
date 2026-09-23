// pitch_detector.cpp — see pitch_detector.h.

#include "pitch_detector.h"
#include "esp_adc/adc_continuous.h"

// Voice range is roughly 70 Hz (low bass) to 1100 Hz (high soprano). 16 kHz
// rather than 8 kHz because at 8 kHz a high note's period is only ~10
// samples, which cost ~10 cents of accuracy and caused octave errors above
// ~800 Hz in host tests.
static const uint32_t SAMPLE_HZ = 16000;
static const int FRAME = 1024;                  // analysis frame (samples, 64ms)
static const int YIN_W = 768;                   // YIN integration window
static const int HOP = 512;                     // new samples between analyses (~32ms)
static const float MIN_HZ = 70.0f;
static const float MAX_HZ = 1100.0f;
static const float YIN_THRESHOLD = 0.15f;       // standard YIN absolute threshold
static const float YIN_ACCEPT = 0.35f;          // fall back to the global minimum if it's at least this good

// Input RMS (12-bit ADC counts) below which we call it silence. With a
// MAX9814 at its default 60dB gain, normal singing a hand's width from the
// mic is typically hundreds of counts; raise this if room noise triggers it.
static const float GATE_RMS = 25.0f;

static const int TAU_MAX = (int)(SAMPLE_HZ / MIN_HZ) + 2;  // ~230
static_assert(YIN_W + TAU_MAX <= FRAME, "YIN window plus longest lag must fit in one frame");

static adc_continuous_handle_t adcHandle = nullptr;

static float ring[FRAME];
static int ringHead = 0;  // next write position

static portMUX_TYPE resultMux = portMUX_INITIALIZER_UNLOCKED;
static PitchReading result = {0, 0, 0, 0};
static volatile float measuredRate = SAMPLE_HZ;

static float frame[FRAME];
static float diff[TAU_MAX + 1];
static float rawDiff[TAU_MAX + 1];

// Returns the detected frequency or 0. `clarityOut` gets 1 - cmndf(tau).
static float yin(const float *x, float fs, float *clarityOut) {
  int tauMin = (int)(fs / MAX_HZ);
  int tauMax = (int)(fs / MIN_HZ);
  if (tauMax > TAU_MAX) tauMax = TAU_MAX;
  if (tauMin < 2) tauMin = 2;

  // Difference function.
  for (int tau = 1; tau <= tauMax; tau++) {
    float sum = 0;
    for (int j = 0; j < YIN_W; j++) {
      float d = x[j] - x[j + tau];
      sum += d * d;
    }
    diff[tau] = sum;
    rawDiff[tau] = sum;
  }

  // Cumulative mean normalized difference.
  diff[0] = 1;
  float running = 0;
  for (int tau = 1; tau <= tauMax; tau++) {
    running += diff[tau];
    diff[tau] = running > 0 ? diff[tau] * tau / running : 1;
  }

  // First dip under the threshold, walked down to its local minimum.
  int best = -1;
  for (int tau = tauMin; tau <= tauMax; tau++) {
    if (diff[tau] < YIN_THRESHOLD) {
      while (tau + 1 <= tauMax && diff[tau + 1] < diff[tau]) tau++;
      best = tau;
      break;
    }
  }
  if (best < 0) {
    best = tauMin;
    for (int tau = tauMin + 1; tau <= tauMax; tau++) {
      if (diff[tau] < diff[best]) best = tau;
    }
    if (diff[best] > YIN_ACCEPT) {
      *clarityOut = 1 - diff[best];
      return 0;
    }
  }

  // Parabolic interpolation around the minimum for sub-sample accuracy, on
  // the raw difference function (the normalized one is skewed at small tau).
  float t = best;
  if (best > 1 && best < tauMax) {
    float a = rawDiff[best - 1], b = rawDiff[best], c = rawDiff[best + 1];
    float denom = a - 2 * b + c;
    if (denom != 0) t = best + 0.5f * (a - c) / denom;
  }
  *clarityOut = 1 - diff[best];
  return fs / t;
}

static float median3(float a, float b, float c) {
  if (a > b) { float t = a; a = b; b = t; }
  if (b > c) { float t = b; b = c; c = t; }
  if (a > b) { float t = a; a = b; b = t; }
  return b;
}

static void pitchTask(void *) {
  static uint8_t raw[256];
  static adc_continuous_data_t parsed[64];
  int sinceAnalysis = 0;
  uint32_t rateSamples = 0;
  uint32_t rateStartUs = micros();
  float recent[3] = {0, 0, 0};  // last voiced estimates, for a median filter against octave glitches

  for (;;) {
    uint32_t got = 0;
    if (adc_continuous_read(adcHandle, raw, sizeof(raw), &got, 100) != ESP_OK) continue;
    uint32_t n = 0;
    if (adc_continuous_parse_data(adcHandle, raw, got, parsed, &n) != ESP_OK) continue;

    for (uint32_t i = 0; i < n; i++) {
      if (!parsed[i].valid) continue;
      ring[ringHead] = (float)parsed[i].raw_data;
      ringHead = (ringHead + 1) % FRAME;
      sinceAnalysis++;
      rateSamples++;
    }

    uint32_t nowUs = micros();
    if (nowUs - rateStartUs >= 2000000) {
      measuredRate = rateSamples * 1e6f / (nowUs - rateStartUs);
      rateSamples = 0;
      rateStartUs = nowUs;
    }

    if (sinceAnalysis < HOP) continue;
    sinceAnalysis = 0;

    // Unroll the ring into a linear frame, oldest first, and remove DC
    // (the MAX9814 output sits at ~1.25V).
    float mean = 0;
    for (int i = 0; i < FRAME; i++) {
      frame[i] = ring[(ringHead + i) % FRAME];
      mean += frame[i];
    }
    mean /= FRAME;
    float energy = 0;
    for (int i = 0; i < FRAME; i++) {
      frame[i] -= mean;
      energy += frame[i] * frame[i];
    }
    float rms = sqrtf(energy / FRAME);

    float hz = 0, clarity = 0;
    if (rms >= GATE_RMS) hz = yin(frame, measuredRate, &clarity);

    float out = 0;
    if (hz > 0) {
      recent[0] = recent[1];
      recent[1] = recent[2];
      recent[2] = hz;
      out = (recent[0] > 0 && recent[1] > 0) ? median3(recent[0], recent[1], recent[2]) : hz;
    } else {
      recent[0] = recent[1] = recent[2] = 0;
    }

    portENTER_CRITICAL(&resultMux);
    result.seq++;
    result.hz = out;
    result.clarity = clarity;
    result.level = rms;
    portEXIT_CRITICAL(&resultMux);
  }
}

bool pitchBegin(int adcPin) {
  adc_continuous_handle_cfg_t handleCfg = {};
  handleCfg.max_store_buf_size = 4096;
  handleCfg.conv_frame_size = 256;
  handleCfg.flags.flush_pool = 1;
  if (adc_continuous_new_handle(&handleCfg, &adcHandle) != ESP_OK) return false;

  adc_unit_t unit;
  adc_channel_t channel;
  if (adc_continuous_io_to_channel(adcPin, &unit, &channel) != ESP_OK || unit != ADC_UNIT_1) return false;

  adc_digi_pattern_config_t pattern = {};
  pattern.atten = ADC_ATTEN_DB_12;
  pattern.channel = channel;
  pattern.unit = unit;
  pattern.bit_width = SOC_ADC_DIGI_MAX_BITWIDTH;

  adc_continuous_config_t cfg = {};
  cfg.pattern_num = 1;
  cfg.adc_pattern = &pattern;
  cfg.sample_freq_hz = SAMPLE_HZ;
  cfg.conv_mode = ADC_CONV_SINGLE_UNIT_1;
  cfg.format = ADC_DIGI_OUTPUT_FORMAT_TYPE2;
  if (adc_continuous_config(adcHandle, &cfg) != ESP_OK) return false;
  if (adc_continuous_start(adcHandle) != ESP_OK) return false;

  // Core 0, below WiFi's priority; the Arduino loop runs on core 1.
  return xTaskCreatePinnedToCore(pitchTask, "pitch", 4096, nullptr, 1, nullptr, 0) == pdPASS;
}

PitchReading pitchLatest() {
  portENTER_CRITICAL(&resultMux);
  PitchReading r = result;
  portEXIT_CRITICAL(&resultMux);
  return r;
}

float pitchSampleRate() {
  return measuredRate;
}
