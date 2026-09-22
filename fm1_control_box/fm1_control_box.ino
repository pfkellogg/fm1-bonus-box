/*
 * fm1_control_box — ESP32-S3 Mini replacement for the Arduino Uno in this
 * project. Same sustain-pedal job as fm1_sustain_footswitch.ino, plus:
 *
 *   - A rotary encoder (KY-040, with a built-in pushbutton) browses the
 *     FM-1's 128 presets, with velocity-based acceleration (spin fast to
 *     cover ground, slow down for single-step precision). Every detent
 *     sends a MIDI Program Change, so you hear the change live — same as
 *     turning the FM-1's own PRESETS knob.
 *   - A 1.28" round GC9A01 color TFT shows the browsed preset, color-coded
 *     by category (piano/organ/brass/etc.) so you recognize where you are
 *     while spinning through it, not just by reading text.
 *   - Short-pressing the encoder's button toggles between two BANK
 *     SOURCES — FACTORY (the recovered-factory set embedded in flash, see
 *     fm1_soundbank_data.h) and CUSTOM (a second 128-voice set stored in
 *     the ESP32's own filesystem, which you load via WiFi — see below).
 *     This exists because different FM-1 units ship with, or accumulate,
 *     different soundbank content — the embedded FACTORY set is one
 *     specific recovered snapshot, not necessarily what's on your unit,
 *     and it may not be what you actually want as your "best" set. See
 *     the project README for the full explanation.
 *   - A medium-length press (400ms-3s) "Assigns" the currently browsed
 *     preset (from whichever source is active) to slot 001 — the FM-1
 *     always boots into whatever's in slot 001, so this is how you set
 *     the boot sound from the box itself.
 *   - A long press (3s+) toggles WIFI UPLOAD MODE: the box becomes a WiFi
 *     access point serving a small upload page, so you can push a new
 *     CUSTOM bank in from a phone or laptop's browser — no MIDI cable,
 *     no fm1_soundbank_app, no computer running special software. This is
 *     the "load in the best soundbank without a computer" path: one-time
 *     setup from any device with a browser, then the box uses it standalone
 *     from then on.
 *
 * IMPORTANT — why this can't just read your "better" bank off the FM-1:
 *   The FM-1 never sends its voice data back over MIDI, period — confirmed
 *   during the original factory-preset recovery (see
 *   https://github.com/KingParamount/fm1-factory-presets). It only
 *   receives SysEx. So there is NO way, from this box or any software, to
 *   ask the FM-1 "what's actually loaded in your memory right now" and
 *   get an answer. If your FM-1's current soundbank is better than the
 *   embedded FACTORY set, the only way to preserve and reuse it is to
 *   already have (or recreate) it as standard DX7 bank .syx files from
 *   wherever it originally came from (Dexed, another SysEx librarian, a
 *   backup you made before importing it, etc.) and upload those — this
 *   box cannot extract it from the FM-1 itself, and neither can anything
 *   else.
 *
 * IMPORTANT limits on Assign — read before relying on it:
 *   - Assign works from its own local copy of a soundbank (FACTORY in
 *     flash, or CUSTOM in the filesystem) — it does NOT read-modify-write
 *     whatever's really on the FM-1.
 *   - Because a DX7 bank dump is always 32 voices, Assign rewrites *all 32*
 *     presets in whichever quarter the target falls in (001-032 for
 *     anything in that range, 033-064 for the next, etc.) — with your
 *     chosen preset moved into slot 1 of that quarter. Anything currently
 *     sitting anywhere else in that same 32-preset range on the real FM-1
 *     gets overwritten back to whatever this box's active source has for
 *     that quarter.
 *   - After Assign sends the SysEx, the FM-1 still shows its own A/B/C/D
 *     bank-slot picker and needs a physical knob turn on the FM-1 itself
 *     to commit — this box can prompt you on the screen, but can't press
 *     that knob for you.
 *
 * Hardware
 * --------
 * MCU: ESP32-S3 Mini ("Super Mini", ESP32-S3FH4R2). 3.3V logic — the MIDI
 * OUT circuit below is adapted from the original 5V Arduino Uno version;
 * confirm on the bench (see README "Not yet built/tested").
 *
 * Pin map (change to taste — avoid ESP32-S3 strapping pins 0/3/45/46):
 *   GPIO4  -> MIDI OUT (UART1 TX, through 220ohm to TRS tip)
 *   GPIO5  -> Sustain pedal/button (INPUT_PULLUP, shared node, same as v1)
 *   GPIO6  -> Encoder CLK
 *   GPIO7  -> Encoder DT
 *   GPIO15 -> Encoder SW (INPUT_PULLUP)
 *   GPIO10 -> TFT CS
 *   GPIO11 -> TFT DC (data/command)
 *   GPIO12 -> TFT RST
 *   GPIO13 -> TFT SCK
 *   GPIO14 -> TFT MOSI (SDA/DIN on the module)
 *   TFT BLK (backlight) -> 3V3 directly, always-on (no GPIO, no PWM dimming
 *     yet — same choice already proven in this project's fm1-midi-voice-tuner)
 *
 * Encoder part: KY-040 module (e.g. WayinTop 360-degree rotary encoder,
 * amazon.com/dp/B07T3672VK) — 5-pin breakout (CLK, DT, SW, +, GND) with
 * onboard pull-ups already, 20 detents/revolution. + to 3V3, GND to GND,
 * CLK/DT/SW to the GPIOs above (the sketch's own INPUT_PULLUP on SW is
 * redundant with the module's onboard pull-up but harmless).
 *
 * Display part: 1.28" round GC9A01 color TFT, 240x240, 4-wire SPI (e.g.
 * D-FLIFE module, amazon.com/dp/B0C1G92F2B) — same part/library already
 * proven in this project's fm1-midi-voice-tuner. Listed "Driving voltage:
 * 3-5V" on that module, so wiring straight to the ESP32-S3's 3V3 rail is
 * fine, no level shifter needed (confirm your specific module's spec sheet
 * matches before assuming this).
 *
 * MIDI OUT (TRS, Type A — same convention as the rest of this project):
 *   GPIO4 (TX) --220ohm-- TRS tip
 *   3V3        --220ohm-- TRS ring
 *   GND                    TRS sleeve
 * Original Arduino version drove this from 5V; at 3.3V the opto in the
 * FM-1's MIDI IN gets less drive current (~2.5mA vs the MIDI-spec 5mA).
 * Widely reported to still work with modern high-gain optos, but this is
 * exactly the kind of thing to confirm on the bench before trusting it —
 * if the FM-1 doesn't respond, try 33ohm/10ohm in place of the 220ohm
 * pair, or add a transistor/inverter buffer.
 *
 * Sustain pedal/button node: same wiring as fm1_sustain_footswitch.ino
 * (pedal tip + panel button, both to this pin, ring/sleeve/other leg to
 * GND, INPUT_PULLUP so idle reads HIGH).
 *
 * Libraries (Library Manager):
 *   Adafruit GC9A01A + Adafruit GFX Library + Adafruit BusIO
 *   ESP32Encoder (madhephaestus/ESP32Encoder)
 * WiFi, WebServer, LittleFS are part of the esp32 core, no separate install.
 *
 * Board (Boards Manager): esp32 by Espressif Systems.
 * Select an "ESP32S3 Dev Module" board profile (or your specific Super
 * Mini's profile if the vendor publishes one), USB-C for both power and
 * flashing. **Partition scheme must include a LittleFS/SPIFFS partition**
 * (e.g. "Default 4MB with spiffs") — Tools > Partition Scheme — or the
 * CUSTOM bank storage will fail to mount at runtime.
 */

#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_GC9A01A.h>
#include <ESP32Encoder.h>
#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>

#include "fm1_soundbank_data.h"

// ---- Pins ----
static const int PIN_MIDI_TX = 4;
static const int PIN_SUSTAIN = 5;
static const int PIN_ENC_CLK = 6;
static const int PIN_ENC_DT = 7;
static const int PIN_ENC_BTN = 15;  // KY-040's "SW" pin
static const int PIN_TFT_CS = 10;
static const int PIN_TFT_DC = 11;
static const int PIN_TFT_RST = 12;
static const int PIN_TFT_SCK = 13;
static const int PIN_TFT_MOSI = 14;

// ---- MIDI ----
static const uint32_t MIDI_BAUD = 31250;
static const uint8_t MIDI_CHANNEL = 0;  // channel 1 (0-indexed on the wire) — must match the FM-1's Note Channel, set in GLO mode
HardwareSerial MidiSerial(1);

// ---- Display ----
static const int TFT_SIZE = 240;
Adafruit_GC9A01A tft(PIN_TFT_CS, PIN_TFT_DC, PIN_TFT_MOSI, PIN_TFT_SCK, PIN_TFT_RST);

// ---- Encoder ----
ESP32Encoder encoder;
// attachHalfQuad() typically yields 1-2 raw counts per physical detent on a
// standard EC11/KY-040 (varies by module/library version) — confirm on the
// bench by turning one detent and checking browseIndex moved by exactly 1;
// raise this if it's skipping presets (e.g. to 2), 1 if it's already right.
static const int STEPS_PER_DETENT = 1;

// Velocity-based acceleration: faster spins jump further. Tune these on
// the bench to taste — thresholds are milliseconds between detents.
struct AccelStep { unsigned long maxIntervalMs; int step; };
static const AccelStep ACCEL_TABLE[] = {
  {80, 8},
  {150, 4},
  {300, 2},
  {ULONG_MAX, 1},
};

// ---- Bank sources ----
enum BankSource { SRC_FACTORY, SRC_CUSTOM };
BankSource activeSource = SRC_FACTORY;

static const char *CUSTOM_BANK_PATH = "/custom_voices.bin";
static uint8_t customVoices[128][VOICE_LEN];  // 16KB, fits RAM easily

// Which category (0-12) a slot belongs to, purely by position — same
// interleave convention the factory set uses (see
// tools/generate_soundbank_header.py's category_for_slot()). Applied to
// BOTH sources: for CUSTOM it's a display convenience, not a guarantee
// your uploaded voices are actually piano/organ/etc. in that order.
uint8_t categoryForSlot(int slot) {
  int bank = slot / 32;
  if (bank < 3) return bank * 4 + (slot % 32) % 4;
  return 12;  // PERC/FX
}

uint16_t categoryColor(uint8_t cat) {
  return pgm_read_word(&CATEGORY_COLOR565[cat]);
}

void getVoiceBytes(int slot, uint8_t out[VOICE_LEN]) {
  if (activeSource == SRC_CUSTOM) {
    memcpy(out, customVoices[slot], VOICE_LEN);
  } else {
    memcpy_P(out, FACTORY_VOICES[slot], VOICE_LEN);
  }
}

// DX7 packed voices carry their own 10-char name at byte offset 118 —
// used directly for CUSTOM so no separate name table is needed for
// whatever gets uploaded. FACTORY uses the curated PRESET_NAMES table
// instead (fixes a couple of known encoding quirks in the raw bytes, see
// fm1_soundbank_app/README.md).
void getVoiceName(int slot, char *outBuf, size_t bufLen) {
  if (activeSource == SRC_CUSTOM) {
    char raw[11];
    memcpy(raw, &customVoices[slot][118], 10);
    raw[10] = 0;
    int end = 9;
    while (end >= 0 && raw[end] == ' ') end--;
    raw[end + 1] = 0;
    strncpy(outBuf, raw, bufLen - 1);
    outBuf[bufLen - 1] = 0;
  } else {
    strncpy(outBuf, PRESET_NAMES[slot], bufLen - 1);
    outBuf[bufLen - 1] = 0;
  }
}

void loadOrInitCustomBank() {
  File f = LittleFS.open(CUSTOM_BANK_PATH, "r");
  if (f && f.size() == 128 * VOICE_LEN) {
    f.read((uint8_t *)customVoices, 128 * VOICE_LEN);
    f.close();
    return;
  }
  if (f) f.close();
  // No valid custom bank yet: start CUSTOM as a copy of FACTORY, so
  // toggling to it before uploading anything still gives valid, playable
  // voices rather than silence/garbage.
  for (int i = 0; i < 128; i++) memcpy_P(customVoices[i], FACTORY_VOICES[i], VOICE_LEN);
  File wf = LittleFS.open(CUSTOM_BANK_PATH, "w");
  if (wf) {
    wf.write((uint8_t *)customVoices, 128 * VOICE_LEN);
    wf.close();
  }
}

void saveCustomBank() {
  File f = LittleFS.open(CUSTOM_BANK_PATH, "w");
  if (f) {
    f.write((uint8_t *)customVoices, 128 * VOICE_LEN);
    f.close();
  }
}

// ---- State ----
int browseIndex = 0;              // 0-127, last preset number we sent a Program Change for
long lastDetentCount = 0;         // encoder.getCount() / STEPS_PER_DETENT, at last processed detent
unsigned long lastDetentAt = 0;
bool sustainOn = false;

// Encoder button: evaluated once at RELEASE, not mid-hold, so nothing
// fires twice. Three brackets by how long it was held.
unsigned long encBtnPressedAt = 0;
bool encBtnDown = false;
static const unsigned long SHORT_PRESS_MAX_MS = 400;   // < this: toggle bank source
static const unsigned long WIFI_HOLD_MS = 3000;        // >= this: toggle WiFi upload mode
                                                         // in between: Assign
int heldHint = 0;  // 0=none, 1="release for Assign", 2="release for WiFi" — live feedback while held

unsigned long assignMessageUntil = 0;
String assignMessageLine1, assignMessageLine2;

// ---- WiFi upload mode ----
bool wifiModeActive = false;
WebServer server(80);
static uint8_t uploadBuf[4104];
static size_t uploadLen = 0;
bool quarterUploadedThisSession[4] = {false, false, false, false};
static const char *WIFI_AP_SSID = "FM1-ControlBox";
static const char *WIFI_AP_PASSWORD = "fm1setup1";  // 8+ chars required by softAP

// ---------------------------------------------------------------------
// MIDI helpers
// ---------------------------------------------------------------------

void midiControlChange(uint8_t cc, uint8_t value) {
  MidiSerial.write(0xB0 | MIDI_CHANNEL);
  MidiSerial.write(cc & 0x7F);
  MidiSerial.write(value & 0x7F);
}

void midiProgramChange(uint8_t program) {
  MidiSerial.write(0xC0 | MIDI_CHANNEL);
  MidiSerial.write(program & 0x7F);
}

void sendSustain(bool on) {
  midiControlChange(64, on ? 127 : 0);
}

// Builds and sends a 32-voice DX7 bank dump (same format/checksum as
// fm1_soundbank_app's `export`), with `targetSlot` (0-127) moved to
// position 0 of its bank-of-32 and everything else shifted down. Pulls
// voice bytes from whichever source (FACTORY/CUSTOM) is currently active.
void assignPresetToSlotOne(int targetSlot) {
  int bankStart = (targetSlot / 32) * 32;  // 0, 32, 64, or 96

  int order[32];
  int w = 0;
  order[w++] = targetSlot;
  for (int i = 0; i < 32; i++) {
    int orig = bankStart + i;
    if (orig != targetSlot) order[w++] = orig;
  }

  static uint8_t body[4096];
  for (int i = 0; i < 32; i++) {
    uint8_t voiceBuf[VOICE_LEN];
    getVoiceBytes(order[i], voiceBuf);
    memcpy(&body[i * VOICE_LEN], voiceBuf, VOICE_LEN);
  }

  uint32_t sum = 0;
  for (int i = 0; i < 4096; i++) sum += body[i];
  uint8_t checksum = (128 - (sum & 0x7F)) & 0x7F;

  // F0 43 00 09 20 00 <4096 bytes> <checksum> F7 — standard DX7 32-voice bulk dump
  MidiSerial.write(0xF0);
  MidiSerial.write(0x43);
  MidiSerial.write(0x00);
  MidiSerial.write(0x09);
  MidiSerial.write(0x20);
  MidiSerial.write(0x00);
  MidiSerial.write(body, 4096);
  MidiSerial.write(checksum);
  MidiSerial.write(0xF7);

  char destLetter = 'A' + (bankStart / 32);
  assignMessageLine1 = "ASSIGN SENT";
  assignMessageLine2 = String("Turn FM-1 Knob1 -> ") + destLetter;
  assignMessageUntil = millis() + 6000;
}

// ---------------------------------------------------------------------
// WiFi upload mode — push a CUSTOM bank in from a phone/laptop browser,
// no MIDI cable and no fm1_soundbank_app needed.
// ---------------------------------------------------------------------

const char UPLOAD_PAGE[] PROGMEM = R"HTML(<!DOCTYPE html><html><head><meta name="viewport" content="width=device-width,initial-scale=1"></head>
<body style="font-family:sans-serif;max-width:480px;margin:1em auto;padding:0 1em">
<h2>FM-1 Control Box &mdash; Custom Soundbank</h2>
<p>Upload a standard DX7 32-voice bank dump (.syx, exactly 4104 bytes) for each quarter you want to set. Quarters you don't upload keep whatever's already stored here (starts as a copy of the factory set).</p>
<form method="POST" action="/upload" enctype="multipart/form-data">
<p>
<label><input type="radio" name="quarter" value="0" checked> Bank A &mdash; presets 001-032</label><br>
<label><input type="radio" name="quarter" value="1"> Bank B &mdash; presets 033-064</label><br>
<label><input type="radio" name="quarter" value="2"> Bank C &mdash; presets 065-096</label><br>
<label><input type="radio" name="quarter" value="3"> Bank D &mdash; presets 097-128</label>
</p>
<input type="file" name="bankfile" accept=".syx"><br><br>
<input type="submit" value="Upload this quarter">
</form>
<p><small>Get .syx bank files from Dexed, SysEx Librarian, PocketMIDI, or any DX7 patch source. This box can't read your FM-1's current voices back out over MIDI &mdash; it only receives what you upload here.</small></p>
</body></html>
)HTML";

void handleUploadForm() {
  server.send_P(200, "text/html", UPLOAD_PAGE);
}

void handleUploadData() {
  HTTPUpload &upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    uploadLen = 0;
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (uploadLen + upload.currentSize <= sizeof(uploadBuf)) {
      memcpy(uploadBuf + uploadLen, upload.buf, upload.currentSize);
      uploadLen += upload.currentSize;
    } else {
      uploadLen = SIZE_MAX;  // mark oversized, rejected in handleUploadComplete
    }
  }
}

void handleUploadComplete() {
  int quarter = server.hasArg("quarter") ? server.arg("quarter").toInt() : -1;
  String msg;

  if (uploadLen == SIZE_MAX) {
    msg = "Error: file too large — expected exactly 4104 bytes (a standard DX7 32-voice bank dump).";
  } else if (uploadLen != 4104) {
    msg = "Error: file must be exactly 4104 bytes. Got " + String((unsigned)uploadLen) + " bytes.";
  } else if (uploadBuf[0] != 0xF0 || uploadBuf[4103] != 0xF7 || uploadBuf[1] != 0x43 || uploadBuf[3] != 0x09 || uploadBuf[4] != 0x20) {
    msg = "Error: doesn't look like a DX7 32-voice bank dump (bad SysEx header).";
  } else if (quarter < 0 || quarter > 3) {
    msg = "Error: no quarter selected.";
  } else {
    uint32_t sum = 0;
    for (int i = 0; i < 4096; i++) sum += uploadBuf[6 + i];
    uint8_t checksum = (128 - (sum & 0x7F)) & 0x7F;
    if (checksum != uploadBuf[6 + 4096]) {
      msg = "Error: checksum mismatch — file may be corrupt or truncated.";
    } else {
      memcpy(&customVoices[quarter * 32], &uploadBuf[6], 4096);
      saveCustomBank();
      quarterUploadedThisSession[quarter] = true;
      msg = String("Loaded into Bank ") + (char)('A' + quarter) + ". Upload the next quarter if you have one, or close this page and use the box (short-press the knob to switch to CUSTOM if it isn't already active).";
      drawScreen();
    }
  }

  server.send(200, "text/html",
    "<html><body style='font-family:sans-serif;max-width:480px;margin:1em auto;padding:0 1em'>"
    "<p>" + msg + "</p><p><a href=\"/\">Back</a></p></body></html>");
}

void enterWifiMode() {
  wifiModeActive = true;
  for (int i = 0; i < 4; i++) quarterUploadedThisSession[i] = false;
  WiFi.mode(WIFI_AP);
  WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASSWORD);
  server.on("/", HTTP_GET, handleUploadForm);
  server.on("/upload", HTTP_POST, handleUploadComplete, handleUploadData);
  server.begin();
  drawScreen();
}

void exitWifiMode() {
  server.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
  wifiModeActive = false;
  drawScreen();
}

// ---------------------------------------------------------------------
// Display — 240x240 round TFT, category-colored background
// ---------------------------------------------------------------------

// Adafruit GFX's default font is 6px wide / 8px tall per textsize unit.
void printCentered(int cy, const char *text, int textSize, uint16_t color) {
  int w = strlen(text) * 6 * textSize;
  int x = (TFT_SIZE - w) / 2;
  int y = cy - (8 * textSize) / 2;
  tft.setTextSize(textSize);
  tft.setTextColor(color);
  tft.setCursor(x, y);
  tft.print(text);
}

void drawWifiScreen() {
  tft.fillScreen(GC9A01A_BLUE);
  printCentered(30, "WIFI UPLOAD MODE", 1, GC9A01A_WHITE);
  printCentered(55, WIFI_AP_SSID, 2, GC9A01A_WHITE);
  printCentered(80, WIFI_AP_PASSWORD, 1, GC9A01A_WHITE);
  printCentered(105, "http://192.168.4.1", 1, GC9A01A_WHITE);

  char line[24];
  for (int i = 0; i < 4; i++) {
    bool has = quarterUploadedThisSession[i];
    snprintf(line, sizeof(line), "Bank %c: %s", 'A' + i, has ? "loaded" : "-");
    printCentered(135 + i * 20, line, 1, has ? GC9A01A_GREEN : GC9A01A_WHITE);
  }

  printCentered(220, "Hold knob 3s to exit", 1, GC9A01A_WHITE);
}

void drawScreen() {
  if (wifiModeActive) {
    drawWifiScreen();
    return;
  }

  if (millis() < assignMessageUntil) {
    tft.fillScreen(GC9A01A_ORANGE);
    printCentered(95, assignMessageLine1.c_str(), 3, GC9A01A_BLACK);
    printCentered(135, assignMessageLine2.c_str(), 2, GC9A01A_BLACK);
    printCentered(170, "(can't press it", 1, GC9A01A_BLACK);
    printCentered(185, " for you)", 1, GC9A01A_BLACK);
    return;
  }

  uint8_t cat = categoryForSlot(browseIndex);
  uint16_t bg = categoryColor(cat);
  tft.fillScreen(bg);

  char numBuf[5];
  snprintf(numBuf, sizeof(numBuf), "%03d", browseIndex + 1);
  printCentered(42, numBuf, 2, GC9A01A_WHITE);

  char nameBuf[16];
  getVoiceName(browseIndex, nameBuf, sizeof(nameBuf));
  printCentered(90, nameBuf, 3, GC9A01A_WHITE);

  printCentered(128, CATEGORY_NAMES[cat], 2, GC9A01A_BLACK);

  printCentered(155, activeSource == SRC_CUSTOM ? "[ CUSTOM ]" : "[ FACTORY ]", 1, GC9A01A_BLACK);

  char sustBuf[16];
  snprintf(sustBuf, sizeof(sustBuf), "Sustain: %s", sustainOn ? "ON" : "off");
  printCentered(190, sustBuf, 1, GC9A01A_WHITE);

  if (encBtnDown && heldHint == 1) {
    printCentered(212, "release: ASSIGN", 1, GC9A01A_WHITE);
  } else if (encBtnDown && heldHint == 2) {
    printCentered(212, "release: WIFI UPLOAD", 1, GC9A01A_WHITE);
  } else {
    printCentered(212, "tap:src  hold:assign/wifi", 1, GC9A01A_WHITE);
  }
}

// ---------------------------------------------------------------------
// Setup / loop
// ---------------------------------------------------------------------

void setup() {
  pinMode(PIN_SUSTAIN, INPUT_PULLUP);
  pinMode(PIN_ENC_BTN, INPUT_PULLUP);

  MidiSerial.begin(MIDI_BAUD, SERIAL_8N1, -1, PIN_MIDI_TX);

  tft.begin();
  tft.setRotation(0);
  tft.fillScreen(GC9A01A_BLACK);

  if (!LittleFS.begin(true)) {
    // Filesystem mount/format failed — CUSTOM bank storage won't work.
    // FACTORY still works fine; this just gets stuck initializing
    // customVoices from FACTORY in RAM each boot without persisting.
    for (int i = 0; i < 128; i++) memcpy_P(customVoices[i], FACTORY_VOICES[i], VOICE_LEN);
  } else {
    loadOrInitCustomBank();
  }

  ESP32Encoder::useInternalWeakPullResistors = puType::up;
  encoder.attachHalfQuad(PIN_ENC_CLK, PIN_ENC_DT);
  encoder.setCount(0);
  lastDetentAt = millis();

  drawScreen();
}

void loop() {
  if (wifiModeActive) {
    server.handleClient();
  }

  // --- Sustain pedal/button ---
  bool pressed = (digitalRead(PIN_SUSTAIN) == LOW);
  if (pressed != sustainOn) {
    sustainOn = pressed;
    sendSustain(sustainOn);
    if (!wifiModeActive) drawScreen();
  }

  // --- Encoder rotation -> browse (with acceleration) + live Program Change ---
  // (Still tracked in WiFi mode so nothing's lost, but only acted on/drawn
  // when not in WiFi mode, to keep that screen showing upload status.)
  long detentCount = encoder.getCount() / STEPS_PER_DETENT;
  long deltaDetents = detentCount - lastDetentCount;
  if (deltaDetents != 0) {
    unsigned long now = millis();
    unsigned long interval = now - lastDetentAt;
    int step = 1;
    for (const AccelStep &a : ACCEL_TABLE) {
      if (interval <= a.maxIntervalMs) { step = a.step; break; }
    }
    lastDetentCount = detentCount;
    lastDetentAt = now;

    if (!wifiModeActive) {
      int newIndex = browseIndex + (int)deltaDetents * step;
      newIndex = ((newIndex % 128) + 128) % 128;  // wrap 0-127 regardless of sign
      browseIndex = newIndex;
      midiProgramChange((uint8_t)browseIndex);
      drawScreen();
    }
  }

  // --- Encoder button: evaluated at release (see brackets above) ---
  bool btnDown = (digitalRead(PIN_ENC_BTN) == LOW);
  if (btnDown && !encBtnDown) {
    encBtnDown = true;
    encBtnPressedAt = millis();
    heldHint = 0;
  } else if (btnDown && encBtnDown) {
    unsigned long heldMs = millis() - encBtnPressedAt;
    int newHint = (heldMs >= WIFI_HOLD_MS) ? 2 : (heldMs >= SHORT_PRESS_MAX_MS) ? 1 : 0;
    if (newHint != heldHint && !wifiModeActive) {
      heldHint = newHint;
      drawScreen();
    }
  } else if (!btnDown && encBtnDown) {
    encBtnDown = false;
    unsigned long heldMs = millis() - encBtnPressedAt;
    heldHint = 0;

    if (wifiModeActive) {
      exitWifiMode();
    } else if (heldMs < SHORT_PRESS_MAX_MS) {
      activeSource = (activeSource == SRC_FACTORY) ? SRC_CUSTOM : SRC_FACTORY;
      drawScreen();
    } else if (heldMs < WIFI_HOLD_MS) {
      assignPresetToSlotOne(browseIndex);
      drawScreen();
    } else {
      enterWifiMode();
    }
  }

  // Keep the "Assign sent" banner visible for its duration, then fall back
  // to the normal screen automatically.
  static bool showingBanner = false;
  bool bannerNow = (millis() < assignMessageUntil) && !wifiModeActive;
  if (bannerNow != showingBanner) {
    showingBanner = bannerNow;
    drawScreen();
  }

  delay(2);
}
