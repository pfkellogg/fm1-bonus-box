// sing_mode.ino — "Sing on key" mode: learn to sing in tune.
//
// The box plays a target note on the FM-1 (using a vocal "doo" voice when
// the bank has one), goes quiet, then listens through the mic and shows
// how close you are on a live pitch chart. Hold the note inside the
// difficulty's tolerance long enough and it counts as a hit, then
// auto-advances to the next note.
//
// Note lists (SING button hold cycles them):
//   SONG  — the uploaded song's melody in order (repeated notes merged)
//   PLAY  — the song's vocal track played in time on the FM-1, with
//           lyrics, to sing along with (knob tap = pause/play, turning
//           the knob jumps to a note and plays from there). No scoring:
//           the mic hears the FM-1 too — use headphones on the FM-1 for a
//           meaningful chart.
//   DRILL — each distinct pitch in the song once, low to high
//   FREE  — chromatic C2-C6, for practicing without a song
//
// Why the reference note stops before you sing: the mic would otherwise
// hear the FM-1's own speaker and "hit" every note for you. While the
// reference plays, the chart shows what the mic hears in gray.

enum SingList { LIST_SONG, LIST_PLAY, LIST_DRILL, LIST_FREE };
static const int NUM_LISTS = 4;
static const char *const SING_LIST_NAMES[NUM_LISTS] = {"SONG", "PLAY", "DRILL", "FREE"};

struct DifficultySpec {
  const char *name;  // short, to fit the round screen's top line
  float toleranceCents;
  unsigned long holdMs;
  bool anyOctave;    // accept the right note in any octave (e.g. a low voice singing a high melody)
  float chartCents;  // chart shows +/- this many cents
};
static const DifficultySpec DIFFICULTIES[3] = {
  {"EASY", 50, 300, true, 150},
  {"MED", 25, 500, true, 100},
  {"EXP", 10, 800, false, 50},
};

static const unsigned long REF_MS = 1000;       // how long the reference note plays
static const unsigned long REF_TAIL_MS = 300;   // ignore the mic this long after note-off (voice release)
static const unsigned long HIT_SHOW_MS = 700;   // "HIT!" shows this long before auto-advance
static const unsigned long DROPOUT_GRACE_MS = 120;  // brief gaps in detection don't reset the hold
static const uint8_t REF_VELOCITY = 100;
static const int FREE_LOW = 36, FREE_HIGH = 84;  // C2-C6

// ---- Song (uploaded from the web page as /song.bin) ----
// File: "FM1S", uint16 LE step count, 32-byte name, then per step: MIDI
// note (1 byte) + lyric (11 bytes, zero-padded).
// Optional timed vocal track after that (older song files don't have it):
// "FM1P", uint16 LE note count, then per note: MIDI note (1), step index
// it belongs to (uint16 LE), start ms from song start (uint32 LE),
// duration ms (uint16 LE).
static const char *SONG_PATH = "/song.bin";
static const int SONG_MAX_STEPS = 1000;
static const int LYRIC_LEN = 11;
static const int SONG_HEADER = 38;
static const int SONG_STEP_BYTES = 1 + LYRIC_LEN;
static const int PLAY_MAX_NOTES = 1000;
static const int PLAY_HEADER = 6;
static const int PLAY_NOTE_BYTES = 9;
static const unsigned long PLAY_LEAD_MS = 400;  // pause before the first note, so you're ready

struct SongStep {
  uint8_t note;
  char lyric[LYRIC_LEN + 1];
};
SongStep songSteps[SONG_MAX_STEPS];
int songCount = 0;
char songName[33] = "";
uint8_t drillNotes[128];
int drillCount = 0;

struct PlayNote {
  uint32_t startMs;
  uint16_t durMs;
  uint16_t step;
  uint8_t note;
};
PlayNote playNotes[PLAY_MAX_NOTES];
int playCount = 0;

// ---- Song player (used by the PLAY list and the web page's Play button) ----
bool playing = false;
bool playFinished = false;
bool playRestoreProgram = false;  // started from the web page: go back to playingIndex when done
long playT0 = 0;                  // millis() at song time 0
int playNext = 0;                 // next note to start
int playPos = 0;                  // last note started
int playSounding = -1;            // MIDI note currently on, -1 = none
unsigned long playOffAt = 0;      // song time its note-off is due

// ---- Pitch stepper (web page's pad 3): one song step per press ----
int stepPos = -1;                 // last step played, -1 = none yet
int stepSounding = -1;            // MIDI note currently on, -1 = none
unsigned long stepOffAt = 0;      // millis() its note-off is due
unsigned long stepAt = 0;         // millis() of the last step

// ---- Lyric screen (WiFi mode, while the page's pads play) ----
static const unsigned long LYRIC_HOLD_MS = 4000;  // keep a stepped lyric up this long after the tap
int lyricShownStep = -1;          // step on screen, -1 = lyric screen not showing
int lyricShownNote = -1;          // playPos on screen while playing (the note line changes more often than the lyric)

// ---- Sing state ----
bool singMode = false;
SingList singList = LIST_FREE;
int singPos[NUM_LISTS] = {0, 0, 0, 60 - FREE_LOW};  // remembered position per list (FREE starts at middle C)
int difficulty = 1;                      // MEDIUM
int refSlot = -1;                        // program used for the reference note; -1 = keep the current preset
uint8_t targetNote = 60;
bool refNoteOn = false;
unsigned long refStartedAt = 0;
unsigned long listenFrom = 0;
unsigned long inTuneSince = 0;
unsigned long lastInTuneAt = 0;
unsigned long hitAt = 0;
int hitCount = 0;
bool songDone = false;

// What the mic hears right now, for the display.
float youHz = 0;
float youCents = 0;  // vs target, folded to the nearest octave when the difficulty allows
uint32_t lastPitchSeq = 0;

// Chart history: one entry per pitch analysis (~31/s), 100 entries = ~3s.
static const int HIST = 100;
static const int16_t HIST_NONE = INT16_MIN;
int16_t histCents[HIST];
uint8_t histKind[HIST];  // 0 = listening, 1 = reference playing
int histHead = 0;

GFXcanvas16 chartCanvas(200, 80);
GFXcanvas16 infoCanvas(200, 52);
static const int CHART_X = 20, CHART_Y = 90;
static const int INFO_X = 20, INFO_Y = 170;

static const uint16_t SING_BG = 0x0841;
static const uint16_t COL_GRAY = 0x7BEF;
static const uint16_t COL_BAND = 0x0320;
static const uint16_t COL_GOOD = 0x07E0;
static const uint16_t COL_NEAR = 0xFD20;
static const uint16_t COL_FAR = 0xF800;

// ---------------------------------------------------------------------
// Notes
// ---------------------------------------------------------------------

static const char *const NOTE_NAMES[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

float noteHz(float midi) {
  return 440.0f * powf(2.0f, (midi - 69.0f) / 12.0f);
}

// "A4" style, middle C = C4.
void noteLabel(int midi, char *out, size_t len) {
  snprintf(out, len, "%s%d", NOTE_NAMES[midi % 12], midi / 12 - 1);
}

// ---------------------------------------------------------------------
// Song storage
// ---------------------------------------------------------------------

void buildDrillList() {
  bool seen[128] = {false};
  for (int i = 0; i < songCount; i++) seen[songSteps[i].note & 0x7F] = true;
  drillCount = 0;
  for (int n = 0; n < 128; n++) {
    if (seen[n]) drillNotes[drillCount++] = n;
  }
}

// Parses a song file image. Returns nullptr on success, or an error message.
const char *parseSong(const uint8_t *buf, size_t len) {
  if (len < SONG_HEADER || memcmp(buf, "FM1S", 4) != 0) return "Not a song file.";
  int count = buf[4] | (buf[5] << 8);
  if (count > SONG_MAX_STEPS) return "Song has too many notes (max 1000).";
  size_t stepsEnd = SONG_HEADER + count * SONG_STEP_BYTES;
  int pCount = 0;
  const uint8_t *pb = buf + stepsEnd + PLAY_HEADER;
  if (len != stepsEnd) {
    if (len < stepsEnd + PLAY_HEADER || memcmp(buf + stepsEnd, "FM1P", 4) != 0) return "Song file size doesn't match its note count.";
    pCount = buf[stepsEnd + 4] | (buf[stepsEnd + 5] << 8);
    if (pCount > PLAY_MAX_NOTES) return "Song's vocal track is too long (max 1000 notes).";
    if (len != stepsEnd + PLAY_HEADER + pCount * PLAY_NOTE_BYTES) return "Song file's vocal track size doesn't match its note count.";
    for (int i = 0; i < pCount; i++) {
      const uint8_t *p = pb + i * PLAY_NOTE_BYTES;
      if ((p[1] | (p[2] << 8)) >= count) return "Song file's vocal track points past its last step.";
    }
  }

  songPlayStop();  // the old song's notes are about to be overwritten
  songStepOff();
  stepPos = -1;
  playCount = pCount;
  for (int i = 0; i < pCount; i++) {
    const uint8_t *p = pb + i * PLAY_NOTE_BYTES;
    playNotes[i].note = p[0] & 0x7F;
    playNotes[i].step = p[1] | (p[2] << 8);
    playNotes[i].startMs = (uint32_t)p[3] | ((uint32_t)p[4] << 8) | ((uint32_t)p[5] << 16) | ((uint32_t)p[6] << 24);
    playNotes[i].durMs = p[7] | (p[8] << 8);
  }
  singPos[LIST_PLAY] = 0;

  memcpy(songName, buf + 6, 32);
  songName[32] = 0;
  for (int i = 0; i < count; i++) {
    const uint8_t *p = buf + SONG_HEADER + i * SONG_STEP_BYTES;
    songSteps[i].note = p[0] & 0x7F;
    memcpy(songSteps[i].lyric, p + 1, LYRIC_LEN);
    songSteps[i].lyric[LYRIC_LEN] = 0;
  }
  songCount = count;
  singPos[LIST_SONG] = 0;
  singPos[LIST_DRILL] = 0;
  buildDrillList();
  return nullptr;
}

void loadSong() {
  songPlayStop();
  songStepOff();
  stepPos = -1;
  songCount = 0;
  playCount = 0;
  drillCount = 0;
  songName[0] = 0;
  if (!fsMounted) return;
  File f = LittleFS.open(SONG_PATH, "r");
  if (!f) return;
  size_t len = f.size();
  if (len <= sizeof(uploadBuf)) {
    f.read(uploadBuf, len);
    parseSong(uploadBuf, len);
  }
  f.close();
}

// ---------------------------------------------------------------------
// Target list
// ---------------------------------------------------------------------

int listLength(int list) {
  if (list == LIST_SONG) return songCount;
  if (list == LIST_PLAY) return playCount;
  if (list == LIST_DRILL) return drillCount;
  return FREE_HIGH - FREE_LOW + 1;
}

uint8_t listNote(int list, int pos) {
  if (list == LIST_SONG) return songSteps[pos].note;
  if (list == LIST_PLAY) return playNotes[pos].note;
  if (list == LIST_DRILL) return drillNotes[pos];
  return FREE_LOW + pos;
}

const char *currentLyric() {
  if (singList == LIST_SONG) return songSteps[singPos[LIST_SONG]].lyric;
  if (singList == LIST_PLAY) return songSteps[playNotes[singPos[LIST_PLAY]].step].lyric;
  return "";
}

// Picks the voice in the box's bank that sounds most like a sung "doo",
// by name. The project's own DOO VOICE patch (insert it from the web
// page) wins; otherwise any vocal-sounding name. -1 = none found.
int findReferenceVoice() {
  static const char *const KEYWORDS[] = {"DOO", "OOH", "VOX", "VOICE", "CHOIR", "AAH", "HUM", "SING"};
  char name[16];
  for (const char *kw : KEYWORDS) {
    for (int slot = 0; slot < 128; slot++) {
      if (slotEmpty(slot)) continue;
      getVoiceName(slot, name, sizeof(name));
      for (char *c = name; *c; c++) *c = toupper(*c);
      if (strstr(name, kw)) return slot;
    }
  }
  return -1;
}

// ---------------------------------------------------------------------
// Song player — the timed vocal track, one note at a time
// ---------------------------------------------------------------------

void playNoteOff() {
  if (playSounding >= 0) {
    midiNoteOff((uint8_t)playSounding);
    playSounding = -1;
  }
}

// Starts the vocal track from note `pos`. fromWeb: the web page's Play
// button — switches to the reference voice and restores the browsed preset
// when done (sing mode handles the program itself).
void songPlayStart(int pos, bool fromWeb) {
  songPlayStop();
  songStepOff();
  if (playCount == 0) return;
  pos = constrain(pos, 0, playCount - 1);
  if (fromWeb) {
    int ref = findReferenceVoice();
    if (ref >= 0) midiProgramChange((uint8_t)ref);
    playRestoreProgram = true;
  }
  midiControlChange(64, 0);  // a held sustain pedal would smear the melody
  playNext = pos;
  playPos = pos;
  playT0 = (long)millis() + (long)PLAY_LEAD_MS - (long)playNotes[pos].startMs;
  playFinished = false;
  playing = true;
}

void songPlayStop() {
  bool was = playing;
  playing = false;
  playNoteOff();
  if (was && playRestoreProgram) midiProgramChange((uint8_t)playingIndex);
  playRestoreProgram = false;
}

void songStepOff() {
  if (stepSounding < 0) return;
  midiNoteOff((uint8_t)stepSounding);
  stepSounding = -1;
  midiProgramChange((uint8_t)playingIndex);
}

// Plays song step `pos` (one pitch, repeats merged) for REF_MS in the
// reference voice. Stops playback.
void songStepPlay(int pos) {
  songPlayStop();
  songStepOff();
  if (songCount == 0) return;
  stepPos = ((pos % songCount) + songCount) % songCount;
  int ref = findReferenceVoice();
  if (ref >= 0) midiProgramChange((uint8_t)ref);
  midiControlChange(64, 0);
  stepSounding = songSteps[stepPos].note;
  midiNoteOn((uint8_t)stepSounding, REF_VELOCITY);
  stepAt = millis();
  stepOffAt = stepAt + REF_MS;
}

// Pad 3: the next pitch, wrapping to the first after the last.
void songStepNext() { songStepPlay(stepPos + 1); }

// Pad 4: the current pitch again — the one playing now if the song is
// playing (so pad 3 continues from there), else the last one stepped to.
void songStepReplay() {
  songStepPlay(playing ? playNotes[playPos].step : max(stepPos, 0));
}

// {"pos":4,"count":151,"note":64,"lyric":"sa"} — the step just played.
String songStepJson() {
  if (stepPos < 0) return "{}";
  String j = String("{\"pos\":") + stepPos + ",\"count\":" + songCount + ",\"note\":" + songSteps[stepPos].note + ",\"lyric\":\"";
  for (const char *c = songSteps[stepPos].lyric; *c; c++) {
    if (*c == '"' || *c == '\\') j += '\\';
    j += *c;
  }
  return j + "\"}";
}

// ---------------------------------------------------------------------
// Lyric screen — shown in WiFi mode while pad 1 plays or pad 3 steps
// ---------------------------------------------------------------------

// Step whose lyric belongs on screen now, -1 = none.
int lyricStepNow() {
  if (playing) return playNotes[playPos].step;
  if (stepPos >= 0 && millis() - stepAt < LYRIC_HOLD_MS) return stepPos;
  return -1;
}

// Many steps carry no lyric (a syllable held over several pitches), so
// "current" is the last lyric at or before the step; prev/next skip blanks.
int lyricAtOrBefore(int step) {
  for (int i = step; i >= 0; i--) if (songSteps[i].lyric[0]) return i;
  return -1;
}
int lyricAfter(int step) {
  for (int i = step + 1; i < songCount; i++) if (songSteps[i].lyric[0]) return i;
  return -1;
}

// Top lines: position + note name.
void drawLyricNote(int step) {
  lyricShownNote = playing ? playPos : -1;
  tft.fillRect(40, 30, 160, 44, SING_BG);
  char line[32], note[8];
  uint8_t n = playing ? playNotes[playPos].note : songSteps[step].note;
  if (playing) snprintf(line, sizeof(line), "NOTE %d/%d", playPos + 1, playCount);
  else snprintf(line, sizeof(line), "PITCH %d/%d", step + 1, songCount);
  printCentered(40, line, 1, COL_GRAY);
  noteLabel(n, note, sizeof(note));
  printCentered(62, note, 2, GC9A01A_YELLOW);
}

void drawLyricScreen(int step) {
  lyricShownStep = step;
  tft.fillScreen(SING_BG);
  drawLyricNote(step);

  char line[32];
  int cur = lyricAtOrBefore(step);
  int prev = cur > 0 ? lyricAtOrBefore(cur - 1) : -1;
  int next = lyricAfter(step);
  if (prev >= 0) printCentered(92, songSteps[prev].lyric, 2, COL_GRAY);
  printCentered(125, cur >= 0 ? songSteps[cur].lyric : "~", 3, GC9A01A_WHITE);
  if (next >= 0) printCentered(160, songSteps[next].lyric, 2, COL_GRAY);

  snprintf(line, sizeof(line), "%.28s", songName);
  printCentered(192, line, 1, COL_GRAY);
}

// For drawScreen() in WiFi mode: draws the lyric screen and returns true
// if one belongs up now, else false (draw the normal WiFi screen).
bool lyricScreenDraw() {
  int s = lyricStepNow();
  if (s < 0) {
    lyricShownStep = -1;
    return false;
  }
  drawLyricScreen(s);
  return true;
}

// WiFi mode: redraw when the lyric changes, or when it should go away.
void songLyricLoop() {
  int s = lyricStepNow();
  if (s != lyricShownStep) drawScreen();
  else if (s >= 0 && playing && playPos != lyricShownNote) drawLyricNote(s);
}

// Called every loop() pass, in any mode.
void songPlayLoop() {
  if (stepSounding >= 0 && (long)(millis() - stepOffAt) >= 0) songStepOff();
  if (!playing) return;
  long t = (long)millis() - playT0;
  if (playSounding >= 0 && t >= (long)playOffAt) playNoteOff();
  if (playNext < playCount && t >= (long)playNotes[playNext].startMs) {
    const PlayNote &n = playNotes[playNext];
    playNoteOff();
    midiNoteOn(n.note, REF_VELOCITY);
    playSounding = n.note;
    playOffAt = n.startMs + n.durMs;
    playPos = playNext++;
  }
  if (playNext >= playCount && playSounding < 0) {
    songPlayStop();
    playFinished = true;
  }
}

bool songPlaying() { return playing; }
bool songHasVocalTrack() { return playCount > 0; }

// {"playing":true,"pos":12,"count":230,"note":65,"lyric":"me"} for the web page's pads.
String songPlayStateJson() {
  String j = String("{\"playing\":") + (playing ? "true" : "false");
  if (playing) {
    j += String(",\"pos\":") + playPos + ",\"count\":" + playCount + ",\"note\":" + playNotes[playPos].note + ",\"lyric\":\"";
    for (const char *c = songSteps[playNotes[playPos].step].lyric; *c; c++) {
      if (*c == '"' || *c == '\\') j += '\\';
      j += *c;
    }
    j += '"';
  }
  return j + "}";
}

// ---------------------------------------------------------------------
// Reference note + scoring
// ---------------------------------------------------------------------

void stopReference() {
  if (refNoteOn) {
    midiNoteOff(targetNote);
    refNoteOn = false;
  }
}

void playReference() {
  stopReference();
  midiNoteOn(targetNote, REF_VELOCITY);
  refNoteOn = true;
  refStartedAt = millis();
  listenFrom = refStartedAt + REF_MS + REF_TAIL_MS;
  inTuneSince = 0;
  hitAt = 0;
}

void setSingPos(int pos) {
  int len = listLength(singList);
  if (len == 0) return;
  pos = ((pos % len) + len) % len;
  singPos[singList] = pos;
  stopReference();  // note-off must use the old target
  songPlayStop();
  targetNote = listNote(singList, pos);
  songDone = false;
  if (singList == LIST_PLAY) songPlayStart(pos, false);
  else playReference();
  drawSingScreen();
}

void autoAdvance() {
  int len = listLength(singList);
  int pos = singPos[singList];
  if (singList == LIST_SONG && pos == len - 1) {
    songDone = true;  // stay on the last note; turning the knob moves on
    hitAt = 0;
    drawSingLive();
    return;
  }
  setSingPos(pos + 1);
}

void pushHistory(int16_t cents, uint8_t kind) {
  histCents[histHead] = cents;
  histKind[histHead] = kind;
  histHead = (histHead + 1) % HIST;
}

void processPitch(const PitchReading &r) {
  unsigned long now = millis();
  const DifficultySpec &d = DIFFICULTIES[difficulty];
  bool referencePhase = now < listenFrom;

  if (r.hz <= 0) {
    youHz = 0;
    pushHistory(HIST_NONE, referencePhase);
  } else {
    youHz = r.hz;
    float cents = 1200.0f * log2f(r.hz / noteHz(targetNote));
    if (d.anyOctave) cents -= 1200.0f * roundf(cents / 1200.0f);
    youCents = cents;
    pushHistory((int16_t)constrain(cents, -3000.0f, 3000.0f), referencePhase);

    if (!referencePhase && !hitAt && !songDone && singList != LIST_PLAY && fabsf(cents) <= d.toleranceCents) {
      if (!inTuneSince) inTuneSince = now;
      lastInTuneAt = now;
      if (now - inTuneSince >= d.holdMs) {
        hitAt = now;
        hitCount++;
      }
    }
  }
  if (inTuneSince && now - lastInTuneAt > DROPOUT_GRACE_MS) inTuneSince = 0;
}

// ---------------------------------------------------------------------
// Display
// ---------------------------------------------------------------------

void canvasCentered(GFXcanvas16 &c, int cy, const char *text, int size, uint16_t color) {
  int w = strlen(text) * 6 * size;
  c.setTextSize(size);
  c.setTextColor(color);
  c.setCursor((c.width() - w) / 2, cy - 4 * size);
  c.print(text);
}

void drawSingHeader() {
  tft.fillRect(0, 0, TFT_SIZE, CHART_Y, SING_BG);

  char line[32];
  int len = listLength(singList);
  if (len) {
    snprintf(line, sizeof(line), "SING %s %d/%d %s", SING_LIST_NAMES[singList], singPos[singList] + 1, len, DIFFICULTIES[difficulty].name);
  } else {
    snprintf(line, sizeof(line), "SING %s %s", SING_LIST_NAMES[singList], DIFFICULTIES[difficulty].name);
  }
  printCentered(28, line, 1, GC9A01A_WHITE);

  char note[8];
  noteLabel(targetNote, note, sizeof(note));
  printCentered(50, note, 3, GC9A01A_YELLOW);

  const char *lyric = currentLyric();
  if (lyric[0]) {
    snprintf(line, sizeof(line), "%.1f Hz  \"%s\"", noteHz(targetNote), lyric);
  } else {
    snprintf(line, sizeof(line), "%.1f Hz", noteHz(targetNote));
  }
  printCentered(76, line, 1, GC9A01A_WHITE);
}

void drawSingChart() {
  const DifficultySpec &d = DIFFICULTIES[difficulty];
  GFXcanvas16 &c = chartCanvas;
  int h = c.height(), mid = h / 2;
  float pxPerCent = (mid - 2) / d.chartCents;

  c.fillScreen(GC9A01A_BLACK);
  int band = max(1, (int)(d.toleranceCents * pxPerCent));
  c.fillRect(0, mid - band, c.width(), band * 2 + 1, COL_BAND);
  c.drawFastHLine(0, mid, c.width(), COL_GRAY);

  for (int i = 0; i < HIST; i++) {
    int idx = (histHead + i) % HIST;  // oldest first, left to right
    int16_t cents = histCents[idx];
    if (cents == HIST_NONE) continue;
    int y = mid - (int)(cents * pxPerCent);
    uint16_t color;
    if (histKind[idx]) {
      color = COL_GRAY;
    } else if (abs(cents) <= d.toleranceCents) {
      color = COL_GOOD;
    } else if (abs(cents) <= d.toleranceCents * 3) {
      color = COL_NEAR;
    } else {
      color = COL_FAR;
    }
    y = constrain(y, 0, h - 3);
    c.fillRect(i * 2, y - 1, 2, 3, color);
  }

  char lbl[8];
  snprintf(lbl, sizeof(lbl), "+%d", (int)d.chartCents);
  c.setTextSize(1);
  c.setTextColor(COL_GRAY);
  c.setCursor(14, 1);
  c.print(lbl);
  lbl[0] = '-';
  c.setCursor(14, h - 9);
  c.print(lbl);

  tft.drawRGBBitmap(CHART_X, CHART_Y, c.getBuffer(), c.width(), c.height());
}

void drawSingInfo() {
  const DifficultySpec &d = DIFFICULTIES[difficulty];
  GFXcanvas16 &c = infoCanvas;
  unsigned long now = millis();
  char line[32];

  if (singList == LIST_PLAY && songDone) {
    c.fillScreen(0x0400);
    canvasCentered(c, 14, "SONG DONE", 2, GC9A01A_WHITE);
    canvasCentered(c, 38, "tap knob: play again", 1, GC9A01A_WHITE);
  } else if (hitAt) {
    c.fillScreen(0x0400);
    canvasCentered(c, 14, "HIT!", 3, GC9A01A_WHITE);
    snprintf(line, sizeof(line), "hits: %d", hitCount);
    canvasCentered(c, 38, line, 1, GC9A01A_WHITE);
  } else if (songDone) {
    c.fillScreen(0x0400);
    canvasCentered(c, 14, "SONG DONE", 2, GC9A01A_WHITE);
    snprintf(line, sizeof(line), "hits: %d  turn knob", hitCount);
    canvasCentered(c, 38, line, 1, GC9A01A_WHITE);
  } else {
    c.fillScreen(SING_BG);
    if (youHz > 0) {
      char note[8];
      int sung = (int)lroundf(69.0f + 12.0f * log2f(youHz / 440.0f));
      noteLabel(constrain(sung, 0, 127), note, sizeof(note));
      snprintf(line, sizeof(line), "%s %+dc", note, (int)lroundf(youCents));
      uint16_t col = fabsf(youCents) <= d.toleranceCents ? COL_GOOD : fabsf(youCents) <= d.toleranceCents * 3 ? COL_NEAR : COL_FAR;
      canvasCentered(c, 10, line, 2, col);
      snprintf(line, sizeof(line), "%.1f Hz", youHz);
      canvasCentered(c, 27, line, 1, GC9A01A_WHITE);
    } else {
      canvasCentered(c, 10, now < listenFrom ? "listen..." : "sing!", 2, now < listenFrom ? COL_GRAY : GC9A01A_WHITE);
    }

    // Hold progress bar.
    int barW = 120, barX = (c.width() - barW) / 2;
    c.drawRect(barX, 35, barW, 5, COL_GRAY);
    if (inTuneSince) {
      float p = min(1.0f, (now - inTuneSince) / (float)d.holdMs);
      c.fillRect(barX + 1, 36, (int)((barW - 2) * p), 3, COL_GOOD);
    }

    if (singList == LIST_PLAY) {
      canvasCentered(c, 47, playing ? "playing - tap knob: pause" : "paused - tap knob: play", 1, COL_GRAY);
    } else {
      char name[16] = "current voice";
      if (refSlot >= 0) getVoiceName(refSlot, name, sizeof(name));
      snprintf(line, sizeof(line), "ref: %s", name);
      canvasCentered(c, 47, line, 1, COL_GRAY);
    }
  }
  tft.drawRGBBitmap(INFO_X, INFO_Y, c.getBuffer(), c.width(), c.height());
}

void drawSingLive() {
  drawSingChart();
  drawSingInfo();
}

void drawSingScreen() {
  tft.fillScreen(SING_BG);
  drawSingHeader();
  drawSingLive();
}

// ---------------------------------------------------------------------
// Mode control (called from the main loop)
// ---------------------------------------------------------------------

void enterSingMode() {
  singMode = true;
  if (listLength(singList) == 0) singList = LIST_FREE;
  for (int i = 0; i < HIST; i++) histCents[i] = HIST_NONE;
  hitCount = 0;
  refSlot = findReferenceVoice();
  if (refSlot >= 0) midiProgramChange((uint8_t)refSlot);
  setSingPos(singPos[singList]);
}

void exitSingMode() {
  stopReference();
  songPlayStop();
  singMode = false;
  midiProgramChange((uint8_t)playingIndex);  // back to the preset you were on
  drawScreen();
}

void singCycleList() {
  do {
    singList = (SingList)((singList + 1) % NUM_LISTS);
  } while (listLength(singList) == 0);
  setSingPos(singPos[singList]);
}

void singCycleDifficulty() {
  difficulty = (difficulty + 1) % 3;
  inTuneSince = 0;
  drawSingScreen();
}

void singStep(int delta) {
  setSingPos(singPos[singList] + delta);
}

void singReplay() {
  if (singList == LIST_PLAY) {  // pause / play (from the top once the song is done)
    if (playing) songPlayStop();
    else songPlayStart(songDone ? 0 : singPos[LIST_PLAY], false);
    songDone = false;
    drawSingScreen();
    return;
  }
  songDone = false;
  playReference();
  drawSingLive();
}

void singLoop() {
  unsigned long now = millis();
  if (refNoteOn && now - refStartedAt >= REF_MS) stopReference();

  // PLAY list: follow the player (songPlayLoop runs from the main loop).
  if (singList == LIST_PLAY) {
    if (playPos != singPos[LIST_PLAY]) {
      singPos[LIST_PLAY] = playPos;
      targetNote = playNotes[playPos].note;
      drawSingHeader();
    }
    if (playFinished && !songDone) {
      playFinished = false;
      songDone = true;
      drawSingLive();
    }
  }

  PitchReading r = pitchLatest();
  if (r.seq != lastPitchSeq) {
    lastPitchSeq = r.seq;
    processPitch(r);
    drawSingLive();
  }

  if (hitAt && now - hitAt >= HIT_SHOW_MS) autoAdvance();
}

// Accessors for fm1_control_box.ino, which is compiled before this file
// and so can't see its globals directly.
bool singActive() { return singMode; }
int songStepCount() { return songCount; }
const char *songTitle() { return songName; }
