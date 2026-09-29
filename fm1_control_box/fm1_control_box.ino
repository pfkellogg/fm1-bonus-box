/*
 * fm1_control_box — ESP32-S3 Mini replacement for the Arduino Uno in this
 * project. Same sustain-pedal job as fm1_sustain_footswitch.ino, plus:
 *
 *   - A rotary encoder (KY-040, with a built-in pushbutton) browses the
 *     FM-1's 128 presets, with velocity-based acceleration (spin fast to
 *     cover ground, slow down for single-step precision). In LIVE mode
 *     every detent sends a MIDI Program Change, so you hear the change
 *     live — same as turning the FM-1's own PRESETS knob.
 *   - Tapping the encoder's button toggles LIVE / SILENT browse. SILENT
 *     stops sending Program Changes, so you can browse for an Assign
 *     target mid-performance without the FM-1's sound changing under you;
 *     the screen shows which preset is actually still playing. Going back
 *     to LIVE snaps the browse position back to that playing preset (no
 *     surprise sound change).
 *   - A 1.28" round GC9A01 color TFT shows the browsed preset, its name
 *     (read from the box's stored bank), and a background color per FM-1
 *     bank quarter (A/B/C/D).
 *   - A medium-length press (400ms-3s) "Assigns" the browsed preset to
 *     slot 1 of its quarter — the FM-1 always boots into whatever's in
 *     slot 001, so Assigning something from 001-032 is how you set the
 *     boot sound from the box itself.
 *   - A SING button toggles "sing on key" mode (see sing_mode.ino): the
 *     box plays target notes on the FM-1, listens through a microphone
 *     (a MAX9814 module's onboard mic), and charts how close you're singing,
 *     with EASY/MEDIUM/EXPERT tolerances and auto-advance on a hit. Notes
 *     come from a song (.kar/.mid) uploaded on the web page, or chromatic.
 *   - A long press (3s+) toggles WIFI MODE: the box becomes a WiFi access
 *     point serving the soundbank page (see WEB_PAGE below) where you pick
 *     DX7 .syx banks from your phone/laptop, load them into the box's
 *     128-voice bank, drag-and-drop to reorder (multi-select supported),
 *     save, send the result to the FM-1, and upload a song for sing mode —
 *     no computer software needed.
 *   - USB KEYBOARD IN: the box's USB-C port runs as a USB host, so a
 *     class-compliant USB MIDI keyboard (e.g. an M-Audio Keystation, which
 *     has no 5-pin MIDI OUT) plugs straight into the box and plays the FM-1
 *     through the box's MIDI OUT — no computer in between. Everything it
 *     sends on its main port is forwarded (notes, sustain, pitch bend, mod
 *     wheel, program changes), moved to the FM-1's channel and merged with
 *     the box's own pedal/knob/sing-mode MIDI. See usb_midi_host.cpp.
 *     Hold the knob button while powering on to skip USB host mode, so a
 *     computer can see the box for flashing.
 *
 * No voice data ships with this firmware. The box starts with an empty
 * bank; everything in it comes from .syx files you load yourself, so the
 * repository never has to carry anyone else's (unlicensed) patches.
 *
 * Why the box keeps its own copy of the bank:
 *   The FM-1 never sends its voice data back over MIDI, period — confirmed
 *   during the original factory-preset recovery (see
 *   https://github.com/KingParamount/fm1-factory-presets). It only
 *   receives SysEx. So there is NO way to ask the FM-1 what's in its
 *   memory; the box can only push. Its stored bank is what it believes the
 *   FM-1 should hold, and Assign/Send work from that copy.
 *
 * IMPORTANT limits on Assign / Send:
 *   - A DX7 bank dump is always 32 voices, so both rewrite a whole quarter
 *     (001-032, 033-064, 065-096, 097-128) of the FM-1 at once. They refuse
 *     to send a quarter that still has empty slots in the box's bank.
 *   - After each dump, the FM-1 shows its own A/B/C/D bank-slot picker and
 *     needs a physical knob turn on the FM-1 itself to commit — the box
 *     and web page prompt you, but can't press that knob for you.
 *
 * Hardware
 * --------
 * MCU: ESP32-S3 Mini ("Super Mini", ESP32-S3FH4R2). 3.3V logic — the MIDI
 * OUT circuit below is adapted from the original 5V Arduino Uno version;
 * confirm on the bench (see README "Not yet built/tested").
 *
 * Pin map (change to taste — avoid ESP32-S3 strapping pins 0/3/45/46):
 *   GPIO4  -> MIDI OUT (UART1 TX, through 220ohm to TRS tip)
 *   GPIO1  -> Mic in (MAX9814 OUT; ADC1 — ADC2 pins don't work with WiFi on)
 *   GPIO2  -> SING button (INPUT_PULLUP, other leg to GND)
 *   GPIO5  -> Sustain pedal/button (INPUT_PULLUP, shared node, same as v1)
 *   GPIO6  -> Encoder CLK
 *   GPIO7  -> Encoder DT
 *   GPIO15 -> Encoder SW (INPUT_PULLUP)
 *   GPIO10 -> TFT CS
 *   GPIO11 -> TFT DC (data/command)
 *   GPIO12 -> TFT RST
 *   GPIO13 -> TFT SCK
 *   GPIO14 -> TFT MOSI (SDA/DIN on the module)
 *   GPIO19/20 -> USB-C D-/D+ (native USB, used as USB HOST for a keyboard)
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
 * USB keyboard power: in host mode the box must SUPPLY 5V to the keyboard,
 * and the Super Mini can't switch its USB-C VBUS on by itself. Easiest: a
 * USB-C OTG adapter/hub with a charging (PD) input — the charger then
 * powers both the box (through VBUS, as usual) and the keyboard. See README.
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
 * box's bank won't persist across power cycles.
 */

#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_GC9A01A.h>
#include <ESP32Encoder.h>
#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>

#include "web_page.h"
#include "pitch_detector.h"
#include "usb_midi_host.h"

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
static const int PIN_MIC = 1;
static const int PIN_SING_BTN = 2;

// ---- MIDI ----
static const uint32_t MIDI_BAUD = 31250;
static const uint8_t MIDI_CHANNEL = 0;  // channel 1 (0-indexed on the wire) — must match the FM-1's Note Channel, set in GLO mode
HardwareSerial MidiSerial(1);

// ---- Display ----
static const int TFT_SIZE = 240;
// Hardware SPI (pins remapped in setup) — sing mode's live chart redraws
// ~30 times a second, which software SPI is far too slow for. Lower
// TFT_SPI_HZ if the screen shows glitches over long breadboard wires.
static const uint32_t TFT_SPI_HZ = 40000000;
Adafruit_GC9A01A tft(&SPI, PIN_TFT_DC, PIN_TFT_CS, PIN_TFT_RST);

// Background color per FM-1 bank quarter (A/B/C/D), RGB565.
static const uint16_t QUARTER_COLOR565[4] = {
  0xFDA7,  // A rgb(255, 180, 60)
  0x3E56,  // B rgb(60, 200, 180)
  0x93DF,  // C rgb(150, 120, 255)
  0x6BD0,  // D rgb(110, 120, 135)
};

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

// ---- The box's soundbank ----
// 128 packed DX7 voices (128 bytes each, same layout as the body of a
// 32-voice bulk dump). An all-zero voice means "empty slot" — a real voice
// can never be all zeros, since its name bytes are printable ASCII.
static const int VOICE_LEN = 128;
static const int BANK_BYTES = 128 * VOICE_LEN;  // 16KB
static const char *BANK_PATH = "/bank.bin";
static uint8_t bankVoices[128][VOICE_LEN];

bool slotEmpty(int slot) {
  for (int i = 0; i < VOICE_LEN; i++) {
    if (bankVoices[slot][i]) return false;
  }
  return true;
}

bool quarterFull(int quarter) {
  for (int i = 0; i < 32; i++) {
    if (slotEmpty(quarter * 32 + i)) return false;
  }
  return true;
}

bool bankAllEmpty() {
  for (int i = 0; i < 128; i++) {
    if (!slotEmpty(i)) return false;
  }
  return true;
}

// DX7 packed voices carry their own 10-char name at byte offset 118.
void getVoiceName(int slot, char *outBuf, size_t bufLen) {
  if (slotEmpty(slot)) {
    strncpy(outBuf, "(empty)", bufLen - 1);
    outBuf[bufLen - 1] = 0;
    return;
  }
  char raw[11];
  for (int i = 0; i < 10; i++) {
    char c = (char)bankVoices[slot][118 + i];
    raw[i] = (c >= 32 && c <= 126) ? c : ' ';
  }
  raw[10] = 0;
  int end = 9;
  while (end >= 0 && raw[end] == ' ') end--;
  raw[end + 1] = 0;
  strncpy(outBuf, raw, bufLen - 1);
  outBuf[bufLen - 1] = 0;
}

bool fsMounted = false;

void loadBank() {
  memset(bankVoices, 0, sizeof(bankVoices));
  if (!fsMounted) return;
  File f = LittleFS.open(BANK_PATH, "r");
  if (f && f.size() == BANK_BYTES) f.read((uint8_t *)bankVoices, BANK_BYTES);
  if (f) f.close();
}

bool saveBank() {
  if (!fsMounted) return false;
  File f = LittleFS.open(BANK_PATH, "w");
  if (!f) return false;
  size_t n = f.write((uint8_t *)bankVoices, BANK_BYTES);
  f.close();
  return n == BANK_BYTES;
}

// ---- State ----
int browseIndex = 0;              // 0-127, preset shown on screen (what Assign acts on)
int playingIndex = 0;             // 0-127, last preset we actually sent a Program Change for
bool liveBrowse = true;           // LIVE: every detent sends a Program Change. SILENT: browse only.
long lastDetentCount = 0;         // encoder.getCount() / STEPS_PER_DETENT, at last processed detent
unsigned long lastDetentAt = 0;
bool sustainOn = false;

// Encoder button: evaluated once at RELEASE, not mid-hold, so nothing
// fires twice. Three brackets by how long it was held.
unsigned long encBtnPressedAt = 0;
bool encBtnDown = false;
static const unsigned long SHORT_PRESS_MAX_MS = 400;   // < this: toggle LIVE/SILENT
static const unsigned long WIFI_HOLD_MS = 3000;        // >= this: toggle WiFi mode
                                                         // in between: Assign
int heldHint = 0;

static const unsigned long SING_BTN_DEBOUNCE_MS = 30;
static const unsigned long SING_BTN_HOLD_MS = 600;  // SING held this long (in sing mode) = next note list  // 0=none, 1="release for Assign", 2="release for WiFi" — live feedback while held

unsigned long bannerUntil = 0;
String bannerLine1, bannerLine2;
bool bannerNeedsKnob = false;

// ---- WiFi mode ----
bool wifiModeActive = false;
WebServer server(80);
// Big enough for a bank (16KB) or the largest song file (~21KB: 1000 steps + 1000 timed notes).
static uint8_t uploadBuf[24576];
static size_t uploadLen = 0;
static const char *WIFI_AP_SSID = "FM1-ControlBox";
static const char *WIFI_AP_PASSWORD = "fm1setup1";  // 8+ chars required by softAP
static const char *SONG_PATH_WEB = "/song.bin";     // same file sing_mode.ino's SONG_PATH reads
String wifiStatusLine = "";

void drawScreen();

void showBanner(const String &line1, const String &line2, bool needsKnob) {
  bannerLine1 = line1;
  bannerLine2 = line2;
  bannerNeedsKnob = needsKnob;
  bannerUntil = millis() + 6000;
}

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

void midiNoteOn(uint8_t note, uint8_t velocity) {
  MidiSerial.write(0x90 | MIDI_CHANNEL);
  MidiSerial.write(note & 0x7F);
  MidiSerial.write(velocity & 0x7F);
}

void midiNoteOff(uint8_t note) {
  MidiSerial.write(0x80 | MIDI_CHANNEL);
  MidiSerial.write(note & 0x7F);
  MidiSerial.write(0);
}

void sendSustain(bool on) {
  midiControlChange(64, on ? 127 : 0);
}

// ---- USB keyboard -> FM-1 ----
bool usbHostOn = false;
int usbShownCount = -1;  // device count the screen last showed

// Forwards channel messages from a USB MIDI keyboard to the FM-1, moved to
// MIDI_CHANNEL so the keyboard's own channel setting doesn't matter. Each
// message goes out whole from this (the only) MIDI-writing thread, so it
// can't interleave with the box's own messages. Returns true if the screen
// should be redrawn (a Program Change moved the playing preset).
bool forwardUsbMidi() {
  bool presetMoved = false;
  uint8_t p[4];
  for (int n = 0; n < 32 && usbMidiHostRead(p); n++) {
    if (p[0] >> 4) continue;  // cable 0 only (Keystation's 2nd port is its transport buttons)
    uint8_t cin = p[0] & 0x0F;
    if (cin < 0x08 || cin > 0x0E) continue;  // SysEx, system common, clock/active sensing
    uint8_t type = p[1] & 0xF0;
    MidiSerial.write(type | MIDI_CHANNEL);
    MidiSerial.write(p[2] & 0x7F);
    if (type != 0xC0 && type != 0xD0) MidiSerial.write(p[3] & 0x7F);
    if (type == 0xC0) {  // keep the box's idea of what's playing in sync
      playingIndex = p[2] & 0x7F;
      if (liveBrowse) browseIndex = playingIndex;
      presetMoved = true;
    }
  }
  return presetMoved;
}

// Sends one quarter (0-3) of the box's bank as a standard DX7 32-voice
// bulk dump: F0 43 00 09 20 00 <4096 bytes> <checksum> F7.
void sendQuarter(int quarter) {
  const uint8_t *body = bankVoices[quarter * 32];
  uint32_t sum = 0;
  for (int i = 0; i < 4096; i++) sum += body[i];
  uint8_t checksum = (128 - (sum & 0x7F)) & 0x7F;

  static const uint8_t header[6] = {0xF0, 0x43, 0x00, 0x09, 0x20, 0x00};
  MidiSerial.write(header, sizeof(header));
  MidiSerial.write(body, 4096);
  MidiSerial.write(checksum);
  MidiSerial.write(0xF7);
  MidiSerial.flush();
}

// Moves `targetSlot` to position 0 of its quarter (everything before it
// shifts down one) in the box's own bank, saves, and sends that quarter —
// so the box's copy keeps matching what the FM-1 now holds.
void assignPresetToSlotOne(int targetSlot) {
  int quarter = targetSlot / 32;
  int bankStart = quarter * 32;
  char letter = 'A' + quarter;

  if (!quarterFull(quarter)) {
    showBanner("NOT SENT", String("Bank ") + letter + " has empty slots", false);
    return;
  }

  uint8_t moving[VOICE_LEN];
  memcpy(moving, bankVoices[targetSlot], VOICE_LEN);
  memmove(bankVoices[bankStart + 1], bankVoices[bankStart], (targetSlot - bankStart) * VOICE_LEN);
  memcpy(bankVoices[bankStart], moving, VOICE_LEN);
  saveBank();

  sendQuarter(quarter);
  browseIndex = bankStart;
  showBanner("ASSIGN SENT", String("Turn FM-1 Knob") + (quarter + 1) + " -> " + letter, true);
}

// ---------------------------------------------------------------------
// WiFi mode — the soundbank page (web_page.h) plus a tiny API:
//   GET  /bank.bin   the box's 128-voice bank, raw 16384 bytes
//   POST /bank       multipart upload, field "bank": 16384 bytes, replaces + saves it
//   POST /send?q=N   sends quarter N (0-3) to the FM-1 over MIDI
//   GET  /song.bin, POST /song   the sing-mode song (see sing_mode.ino)
//   POST /play, /stop            play the song's vocal track on the FM-1
//   GET  /playstate              JSON: playing, note position, lyric (for the page's pads)
//   POST /step[?again=1]         play the song's next (or current) pitch for 1s; JSON of that step
// All .syx parsing, loading and reordering happens in the browser; the box
// only stores and sends the result.
// ---------------------------------------------------------------------

void handleRoot() {
  server.send_P(200, "text/html", WEB_PAGE);
}

void handleGetBank() {
  server.sendHeader("Cache-Control", "no-store");
  server.send_P(200, "application/octet-stream", (const char *)bankVoices, BANK_BYTES);
}

void handleUploadData() {
  HTTPUpload &upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    uploadLen = 0;
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (uploadLen != SIZE_MAX && uploadLen + upload.currentSize <= sizeof(uploadBuf)) {
      memcpy(uploadBuf + uploadLen, upload.buf, upload.currentSize);
      uploadLen += upload.currentSize;
    } else {
      uploadLen = SIZE_MAX;  // mark oversized, rejected below
    }
  }
}

void handleBankUploadDone() {
  if (uploadLen != BANK_BYTES) {
    server.send(400, "text/plain", "Bank must be exactly 16384 bytes.");
    return;
  }
  memcpy(bankVoices, uploadBuf, BANK_BYTES);
  if (!saveBank()) {
    server.send(500, "text/plain", "Couldn't write to flash (check the partition scheme includes LittleFS). Kept in memory until power-off.");
    return;
  }
  wifiStatusLine = "Bank saved";
  drawScreen();
  server.send(200, "text/plain", "Saved to the box.");
}

void handleSend() {
  int q = server.hasArg("q") ? server.arg("q").toInt() : -1;
  if (q < 0 || q > 3) {
    server.send(400, "text/plain", "Bad quarter.");
    return;
  }
  char letter = 'A' + q;
  if (!quarterFull(q)) {
    server.send(409, "text/plain", String("Bank ") + letter + " has empty slots, not sent.");
    return;
  }
  sendQuarter(q);
  wifiStatusLine = String("Sent ") + letter + ": turn FM-1 Knob" + (q + 1);
  drawScreen();
  server.send(200, "text/plain", "Sent.");
}

void handleGetSong() {
  server.sendHeader("Cache-Control", "no-store");
  File f = fsMounted ? LittleFS.open(SONG_PATH_WEB, "r") : File();
  if (!f) {
    server.send(204, "application/octet-stream", "");
    return;
  }
  server.streamFile(f, "application/octet-stream");
  f.close();
}

void handleSongUploadDone() {
  if (uploadLen == SIZE_MAX) {
    server.send(400, "text/plain", "Song file too large.");
    return;
  }
  if (uploadLen == 0) {  // empty upload = remove the song
    if (fsMounted) LittleFS.remove(SONG_PATH_WEB);
    loadSong();
    wifiStatusLine = "Song removed";
    drawScreen();
    server.send(200, "text/plain", "Song removed.");
    return;
  }
  const char *err = parseSong(uploadBuf, uploadLen);
  if (err) {  // parseSong validates before touching anything, so the old song is intact
    server.send(400, "text/plain", err);
    return;
  }
  bool saved = false;
  if (fsMounted) {
    File f = LittleFS.open(SONG_PATH_WEB, "w");
    if (f) {
      saved = f.write(uploadBuf, uploadLen) == uploadLen;
      f.close();
    }
  }
  wifiStatusLine = "Song saved";
  drawScreen();
  server.send(saved ? 200 : 500, "text/plain", saved ? "Song saved to the box." : "Song loaded, but couldn't write it to flash (lost at power-off).");
}

// POST /play[?from=N]: plays the stored song's vocal track on the FM-1 from
// note N, in the reference ("doo") voice. POST /stop stops it.
void handlePlay() {
  if (!songHasVocalTrack()) {
    server.send(409, "text/plain", "The song on the box has no timed vocal track. Send it again from this page.");
    return;
  }
  songPlayStart(server.hasArg("from") ? server.arg("from").toInt() : 0, true);
  wifiStatusLine = "Playing song";
  drawScreen();
  server.send(200, "text/plain", "Playing on the FM-1.");
}

void handleStep() {
  if (!songStepCount()) {
    server.send(409, "text/plain", "No song on the box.");
    return;
  }
  if (server.hasArg("again")) songStepReplay();
  else songStepNext();
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", songStepJson());
}

void handleStop() {
  songPlayStop();
  songStepOff();
  wifiStatusLine = "Stopped";
  drawScreen();
  server.send(200, "text/plain", "Stopped.");
}

void handlePlayState() {
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", songPlayStateJson());
}

void enterWifiMode() {
  wifiModeActive = true;
  wifiStatusLine = "";
  WiFi.mode(WIFI_AP);
  WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASSWORD);
  server.on("/", HTTP_GET, handleRoot);
  server.on("/bank.bin", HTTP_GET, handleGetBank);
  server.on("/bank", HTTP_POST, handleBankUploadDone, handleUploadData);
  server.on("/send", HTTP_POST, handleSend);
  server.on("/song.bin", HTTP_GET, handleGetSong);
  server.on("/song", HTTP_POST, handleSongUploadDone, handleUploadData);
  server.on("/play", HTTP_POST, handlePlay);
  server.on("/stop", HTTP_POST, handleStop);
  server.on("/playstate", HTTP_GET, handlePlayState);
  server.on("/step", HTTP_POST, handleStep);
  server.begin();
  drawScreen();
}

void exitWifiMode() {
  songPlayStop();
  songStepOff();
  server.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
  wifiModeActive = false;
  drawScreen();
}

// ---------------------------------------------------------------------
// Display — 240x240 round TFT, background color by bank quarter
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
  printCentered(40, "WIFI MODE", 2, GC9A01A_WHITE);
  printCentered(75, WIFI_AP_SSID, 2, GC9A01A_WHITE);
  printCentered(100, WIFI_AP_PASSWORD, 1, GC9A01A_WHITE);
  printCentered(125, "http://192.168.4.1", 1, GC9A01A_WHITE);

  char line[24];
  for (int q = 0; q < 4; q++) {
    bool full = quarterFull(q);
    snprintf(line, sizeof(line), "Bank %c: %s", 'A' + q, full ? "ready" : "incomplete");
    printCentered(145 + q * 13, line, 1, full ? GC9A01A_GREEN : GC9A01A_WHITE);
  }

  if (wifiStatusLine.length()) {
    printCentered(203, wifiStatusLine.c_str(), 1, GC9A01A_YELLOW);
  } else if (songStepCount()) {
    snprintf(line, sizeof(line), "Song: %d notes", songStepCount());
    printCentered(203, line, 1, GC9A01A_WHITE);
  }
  printCentered(220, "Hold knob 3s to exit", 1, GC9A01A_WHITE);
}

void drawScreen() {
  if (wifiModeActive) {
    if (!lyricScreenDraw()) drawWifiScreen();
    return;
  }
  if (singActive()) {
    drawSingScreen();
    return;
  }

  if (millis() < bannerUntil) {
    tft.fillScreen(GC9A01A_ORANGE);
    printCentered(95, bannerLine1.c_str(), 3, GC9A01A_BLACK);
    printCentered(135, bannerLine2.c_str(), bannerLine2.length() > 20 ? 1 : 2, GC9A01A_BLACK);
    if (bannerNeedsKnob) {
      printCentered(170, "(can't press it", 1, GC9A01A_BLACK);
      printCentered(185, " for you)", 1, GC9A01A_BLACK);
    }
    return;
  }

  int quarter = browseIndex / 32;
  tft.fillScreen(QUARTER_COLOR565[quarter]);

  char numBuf[5];
  snprintf(numBuf, sizeof(numBuf), "%03d", browseIndex + 1);
  printCentered(42, numBuf, 2, GC9A01A_WHITE);

  if (bankAllEmpty()) {
    printCentered(90, "NO BANK", 3, GC9A01A_WHITE);
    printCentered(122, "hold knob 3s", 1, GC9A01A_BLACK);
    printCentered(136, "to load via WiFi", 1, GC9A01A_BLACK);
  } else {
    char nameBuf[16];
    getVoiceName(browseIndex, nameBuf, sizeof(nameBuf));
    printCentered(90, nameBuf, 3, GC9A01A_WHITE);
    char bankBuf[8];
    snprintf(bankBuf, sizeof(bankBuf), "BANK %c", 'A' + quarter);
    printCentered(128, bankBuf, 2, GC9A01A_BLACK);
  }

  printCentered(150, liveBrowse ? "REGULAR MODE  LIVE" : "REGULAR MODE  SILENT", 1, GC9A01A_BLACK);

  // What's loaded: voices per bank, e.g. "A32 B32 C0 D32".
  char loadedBuf[24];
  int counts[4] = {0, 0, 0, 0};
  for (int i = 0; i < 128; i++) {
    if (!slotEmpty(i)) counts[i / 32]++;
  }
  snprintf(loadedBuf, sizeof(loadedBuf), "A%d B%d C%d D%d", counts[0], counts[1], counts[2], counts[3]);
  printCentered(164, loadedBuf, 1, GC9A01A_WHITE);

  if (!liveBrowse) {
    char playBuf[24];
    snprintf(playBuf, sizeof(playBuf), "FM-1 playing: %03d", playingIndex + 1);
    printCentered(178, playBuf, 1, GC9A01A_WHITE);
  }

  char sustBuf[16];
  snprintf(sustBuf, sizeof(sustBuf), "Sustain: %s", sustainOn ? "ON" : "off");
  printCentered(193, sustBuf, 1, GC9A01A_WHITE);

  if (encBtnDown && heldHint == 1) {
    printCentered(208, "release: ASSIGN", 1, GC9A01A_WHITE);
  } else if (encBtnDown && heldHint == 2) {
    printCentered(208, "release: WIFI MODE", 1, GC9A01A_WHITE);
  } else {
    printCentered(208, "tap:live hold:assign/wifi", 1, GC9A01A_WHITE);
  }

  if (usbHostOn && usbMidiDeviceCount() > 0) {
    char usbBuf[21];
    const char *name = usbMidiDeviceName();
    snprintf(usbBuf, sizeof(usbBuf), "%s", name[0] ? name : "USB keys");
    printCentered(222, usbBuf, 1, GC9A01A_BLACK);
  }
}

// ---------------------------------------------------------------------
// Setup / loop
// ---------------------------------------------------------------------

void setup() {
  pinMode(PIN_SUSTAIN, INPUT_PULLUP);
  pinMode(PIN_ENC_BTN, INPUT_PULLUP);
  pinMode(PIN_SING_BTN, INPUT_PULLUP);

  MidiSerial.begin(MIDI_BAUD, SERIAL_8N1, -1, PIN_MIDI_TX);

  // Knob button held at power-on = leave the USB-C port as a normal device
  // port (for flashing); otherwise it becomes a host for a USB keyboard.
  if (digitalRead(PIN_ENC_BTN) == HIGH) usbHostOn = usbMidiHostBegin();

  SPI.begin(PIN_TFT_SCK, -1, PIN_TFT_MOSI, PIN_TFT_CS);
  tft.begin(TFT_SPI_HZ);
  tft.setRotation(0);
  tft.fillScreen(GC9A01A_BLACK);

  // If the mount fails the bank still works from RAM, it just won't
  // survive a power cycle.
  fsMounted = LittleFS.begin(true);
  loadBank();
  loadSong();

  // If the mic ADC can't start, everything else still works; sing mode
  // just never hears anything.
  pitchBegin(PIN_MIC);

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
  songPlayLoop();
  if (wifiModeActive) songLyricLoop();

  // --- USB keyboard -> FM-1 ---
  if (usbHostOn) {
    bool moved = forwardUsbMidi();
    int count = usbMidiDeviceCount();
    if ((moved || count != usbShownCount) && !wifiModeActive && !singActive()) drawScreen();
    usbShownCount = count;
  }

  // --- Sustain pedal/button ---
  bool pressed = (digitalRead(PIN_SUSTAIN) == LOW);
  if (pressed != sustainOn) {
    sustainOn = pressed;
    sendSustain(sustainOn);
    if (!wifiModeActive && !singActive()) drawScreen();
  }

  // --- SING button: tap toggles sing mode; hold (in sing mode) cycles SONG/DRILL/FREE ---
  static bool singBtnDown = false;
  static unsigned long singBtnChangedAt = 0;
  static unsigned long singBtnPressedAt = 0;
  bool singRaw = (digitalRead(PIN_SING_BTN) == LOW);
  if (singRaw != singBtnDown && millis() - singBtnChangedAt >= SING_BTN_DEBOUNCE_MS) {
    singBtnDown = singRaw;
    singBtnChangedAt = millis();
    if (singBtnDown) {
      singBtnPressedAt = millis();
    } else if (!wifiModeActive) {
      bool held = millis() - singBtnPressedAt >= SING_BTN_HOLD_MS;
      if (!singActive()) {
        enterSingMode();
      } else if (held) {
        singCycleList();
      } else {
        exitSingMode();
      }
    }
  }

  if (singActive() && !wifiModeActive) singLoop();

  // --- Encoder rotation -> browse (with acceleration) + Program Change in LIVE ---
  // (Still tracked in WiFi mode so nothing's lost, but only acted on/drawn
  // when not in WiFi mode, to keep that screen showing its status.)
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

    if (singActive() && !wifiModeActive) {
      singStep((int)deltaDetents);  // one note per detent, no acceleration
    } else if (!wifiModeActive) {
      int newIndex = browseIndex + (int)deltaDetents * step;
      newIndex = ((newIndex % 128) + 128) % 128;  // wrap 0-127 regardless of sign
      browseIndex = newIndex;
      if (liveBrowse) {
        playingIndex = browseIndex;
        midiProgramChange((uint8_t)playingIndex);
      }
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
    if (newHint != heldHint && !wifiModeActive && !singActive()) {
      heldHint = newHint;
      drawScreen();
    }
  } else if (!btnDown && encBtnDown) {
    encBtnDown = false;
    unsigned long heldMs = millis() - encBtnPressedAt;
    heldHint = 0;

    if (wifiModeActive) {
      exitWifiMode();
    } else if (singActive() && heldMs < SHORT_PRESS_MAX_MS) {
      singReplay();
    } else if (singActive() && heldMs < WIFI_HOLD_MS) {
      singCycleDifficulty();
    } else if (heldMs < SHORT_PRESS_MAX_MS) {
      liveBrowse = !liveBrowse;
      if (liveBrowse) browseIndex = playingIndex;  // snap back to what's actually sounding
      drawScreen();
    } else if (heldMs < WIFI_HOLD_MS) {
      assignPresetToSlotOne(browseIndex);
      drawScreen();
    } else {
      if (singActive()) exitSingMode();
      enterWifiMode();
    }
  }

  // Keep the Assign banner visible for its duration, then fall back to the
  // normal screen automatically.
  static bool showingBanner = false;
  bool bannerNow = (millis() < bannerUntil) && !wifiModeActive && !singActive();
  if (bannerNow != showingBanner) {
    showingBanner = bannerNow;
    drawScreen();
  }

  delay(2);
}
