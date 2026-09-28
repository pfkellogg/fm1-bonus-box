// web_page.h — the soundbank page fm1_control_box serves in WiFi mode.
//
// Everything interactive happens here in the browser: reading .syx files
// off the phone/laptop, loading a 32-voice bank into one of the box's four
// quarters, and drag-and-drop reordering (with multi-select) across all 128
// slots; also reading a .kar/.mid song and extracting its melody + lyrics
// for sing mode. The box itself only stores the final 16KB bank (/bank.bin,
// /bank), sends quarters to the FM-1 (/send?q=N), and stores the song
// (/song.bin, /song — see sing_mode.ino for the format), and plays the
// song's timed vocal track on the FM-1 (/play, /stop).

#pragma once
#include <Arduino.h>

const char WEB_PAGE[] PROGMEM = R"HTML(<!DOCTYPE html>
<html lang="en"><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>FM-1 Soundbank</title>
<style>
:root{--bg:#f6f5f2;--fg:#1d1d1f;--muted:#6b6b70;--card:#fff;--line:#dcdad4;--accent:#c2410c;--sel:#fde6d8;--selline:#c2410c;--ok:#15803d;--err:#b91c1c}
@media (prefers-color-scheme:dark){:root{--bg:#161615;--fg:#ecebe8;--muted:#9a9993;--card:#20201e;--line:#34332f;--accent:#fb923c;--sel:#3b2415;--selline:#fb923c;--ok:#4ade80;--err:#f87171}}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--fg);font:15px/1.4 -apple-system,system-ui,sans-serif}
main{max-width:560px;margin:0 auto;padding:12px 16px 120px}
h1{font-size:20px;margin:8px 0 4px}
h2{font-size:15px;margin:0 0 8px;text-transform:uppercase;letter-spacing:.04em;color:var(--muted)}
section{background:var(--card);border:1px solid var(--line);border-radius:10px;padding:12px;margin:12px 0}
button{font:inherit;padding:9px 14px;border-radius:8px;border:1px solid var(--line);background:var(--card);color:var(--fg);cursor:pointer}
button.primary{background:var(--accent);border-color:var(--accent);color:#fff;font-weight:600}
button:disabled{opacity:.45;cursor:default}
select{font:inherit;padding:8px;border-radius:8px;border:1px solid var(--line);background:var(--card);color:var(--fg)}
.row{display:flex;gap:8px;flex-wrap:wrap;align-items:center}
.muted{color:var(--muted);font-size:13px}
.msg{font-size:14px;margin-top:8px;min-height:1em}
.msg.ok{color:var(--ok)}.msg.err{color:var(--err)}
.found{list-style:none;margin:8px 0 0;padding:0}
.found li{border:1px solid var(--line);border-radius:8px;padding:8px;margin-bottom:6px}
.found label{display:flex;gap:8px;align-items:baseline;cursor:pointer}
.found .names{font-size:12px;color:var(--muted);margin:4px 0 0 24px;word-break:break-word}
#bank{list-style:none;margin:0;padding:0;position:relative;user-select:none;-webkit-user-select:none}
.qhead{font-weight:700;font-size:13px;padding:10px 4px 4px;color:var(--muted);display:flex;justify-content:space-between}
.slot{display:flex;align-items:center;gap:10px;padding:0 4px 0 0;border-bottom:1px solid var(--line);min-height:40px;cursor:pointer;-webkit-tap-highlight-color:transparent}
.slot.sel{background:var(--sel);box-shadow:inset 3px 0 var(--selline)}
.slot.moving{opacity:.35}
.slot .num{width:32px;text-align:right;font-variant-numeric:tabular-nums;color:var(--muted);font-size:13px}
.slot .name{flex:1;font-family:ui-monospace,Menlo,monospace;white-space:pre}
.slot .empty{color:var(--muted);font-style:italic;font-family:inherit}
.handle{width:44px;align-self:stretch;display:flex;align-items:center;justify-content:center;color:var(--muted);touch-action:none;cursor:grab;font-size:18px}
#dropline{position:absolute;left:0;right:0;height:3px;background:var(--accent);border-radius:2px;display:none;pointer-events:none;z-index:2}
#ghost{position:fixed;z-index:10;pointer-events:none;background:var(--accent);color:#fff;padding:6px 10px;border-radius:8px;font-size:13px;font-weight:600;display:none;box-shadow:0 4px 14px rgba(0,0,0,.25)}
#bar{position:fixed;left:0;right:0;bottom:0;background:var(--card);border-top:1px solid var(--line);padding:10px 16px calc(10px + env(safe-area-inset-bottom))}
#bar .inner{max-width:560px;margin:0 auto}
#sendbox{display:none;margin-top:8px;padding:10px;border-radius:8px;background:var(--sel)}
input[type=file]{display:none}
a{color:var(--accent)}
</style></head>
<body><main>
<h1>FM-1 Soundbank</h1>
<p class="muted">Pick DX7 bank files, load them into the box, drag voices into the order you want, then save and send to the FM-1. Upload a song to practice singing it in sing mode.</p>

<section>
<h2>Song to sing (sing mode)</h2>
<p class="muted" id="songOnBox">Checking the box&hellip;</p>
<div class="row"><button class="primary" id="songFindBtn">Upload song (.kar / .mid)&hellip;</button><button id="songRemoveBtn" disabled>Remove song from box</button></div>
<div class="row" style="margin-top:8px"><button id="playBtn" disabled>&#9654; Play vocal track on FM-1</button><button id="stopBtn" disabled>&#9632; Stop</button></div>
<input type="file" id="songInput">
<div id="songSetup" style="display:none;margin-top:10px">
<div class="row"><span>Melody</span><select id="partSel" style="max-width:100%"></select></div>
<div class="row" style="margin-top:8px"><span>Transpose</span><select id="transSel"></select><span class="muted">semitones</span></div>
<p class="muted" id="songPreview"></p>
<button class="primary" id="songSendBtn">Send song to box</button>
</div>
<div class="msg" id="songMsg"></div>
</section>

<section>
<h2>1 &middot; Find sound banks</h2>
<div class="row">
<button class="primary" id="findBtn">Find sound banks&hellip;</button>
<span class="muted">Standard DX7 32-voice .syx files. You can pick several at once.</span>
</div>
<input type="file" id="fileInput" multiple>
<div class="msg" id="findMsg"></div>
<ul class="found" id="found"></ul>
<details class="muted"><summary>Where to get banks</summary>
<p>Download .syx files on a normal internet connection first, then reconnect to <b>FM1-ControlBox</b> and pick them here. Some sources: the <a href="https://asb2m10.github.io/dexed/">Dexed</a> site's cartridge links, <a href="https://yamahablackboxes.com/collection/yamaha-dx7-synthesizer/patches/">Yamaha Black Boxes</a>, or any bank you've made or saved in Dexed.</p>
</details>
</section>

<section>
<h2>2 &middot; Load a sound bank</h2>
<div class="row">
<span>Into</span>
<select id="quarterSel">
<option value="0">Bank A (001&ndash;032)</option>
<option value="1">Bank B (033&ndash;064)</option>
<option value="2">Bank C (065&ndash;096)</option>
<option value="3">Bank D (097&ndash;128)</option>
</select>
<button class="primary" id="loadBtn" disabled>Load sound bank</button>
</div>
<div class="msg" id="loadMsg"></div>
</section>

<section>
<h2>3 &middot; Arrange</h2>
<p class="muted">Tap voices to select several (shift-click selects a range on a computer), then drag any selected voice by its &#8801; handle. They move together, in order.</p>
<div class="row"><button id="clearSelBtn" disabled>Select none</button><button id="emptyBtn" disabled>Empty selected slots</button><span class="muted" id="selCount"></span></div>
<div class="row" style="margin-top:8px"><button id="dooBtn" disabled>Put DOO voice in selected slot</button><span class="muted">Sing mode's reference sound. Select one slot first.</span></div>
<div class="msg" id="arrMsg"></div>
<ul id="bank"><div id="dropline"></div></ul>
</section>

</main>

<div id="ghost"></div>

<div id="bar"><div class="inner">
<div class="row">
<button class="primary" id="saveBtn" disabled>Save to box</button>
<button id="sendBtn">Send to FM-1&hellip;</button>
<span class="muted" id="dirtyNote"></span>
</div>
<div id="sendbox"><div id="sendText"></div><div class="row" style="margin-top:8px"><button class="primary" id="sendNext">Next</button><button id="sendStop">Stop</button></div></div>
<div class="msg" id="barMsg"></div>
</div></div>

<script>
const VLEN = 128;
const $ = id => document.getElementById(id);
let bank = new Array(128).fill(null);  // Uint8Array(128) per slot, or null = empty
let found = [];                         // [{file, voices:[32 Uint8Array]}]
let chosenFound = -1;
let sel = new Set();
let anchor = -1;
let dirty = false;

// The project's own DOO VOICE patch (tools/doo_voice.py, CC0).
const DOO_VOICE = new Uint8Array([99,99,99,99,99,99,99,0,39,0,0,0,57,8,0,2,0,72,50,30,55,99,95,90,0,39,0,0,0,65,8,70,2,0,99,99,99,99,99,99,99,0,39,0,0,0,57,8,0,2,0,72,50,30,55,99,95,90,0,39,0,0,0,57,8,62,4,0,80,35,20,50,99,80,75,0,39,0,0,0,57,12,58,2,0,72,50,30,55,99,95,90,0,39,0,0,0,57,8,99,2,0,99,99,99,99,50,50,50,50,4,8,34,45,4,0,56,24,68,79,79,32,86,79,73,67,69,32]);

function msg(el, text, kind) { el.textContent = text; el.className = 'msg' + (kind ? ' ' + kind : ''); }

function voiceName(v) {
  let s = '';
  for (let i = 118; i < 128; i++) { const c = v[i]; s += (c >= 32 && c <= 126) ? String.fromCharCode(c) : ' '; }
  return s.replace(/\s+$/, '');
}

function isEmptyVoice(v) { for (let i = 0; i < VLEN; i++) if (v[i]) return false; return true; }

// Finds every DX7 32-voice bulk dump (F0 43 0n 09 20 00 ... F7) in a file.
function parseSyx(buf) {
  const b = new Uint8Array(buf), banks = [];
  for (let i = 0; i + 4104 <= b.length; i++) {
    if (b[i] !== 0xF0 || b[i+1] !== 0x43 || (b[i+2] & 0xF0) !== 0 || b[i+3] !== 0x09 || b[i+4] !== 0x20 || b[i+5] !== 0x00) continue;
    if (b[i+4103] !== 0xF7) continue;
    const body = b.slice(i + 6, i + 6 + 4096);
    // Checksum deliberately not enforced: plenty of circulating banks have a wrong one,
    // and the box recomputes it when sending anyway.
    const voices = [];
    for (let k = 0; k < 32; k++) voices.push(body.slice(k * VLEN, (k + 1) * VLEN));
    banks.push(voices);
    i += 4103;
  }
  return banks;
}

function setDirty(d) {
  dirty = d;
  $('saveBtn').disabled = !d;
  $('dirtyNote').textContent = d ? 'Unsaved changes' : '';
}

// ---- 1. Find ----
$('findBtn').onclick = () => $('fileInput').click();
$('fileInput').onchange = async e => {
  const files = [...e.target.files];
  e.target.value = '';
  let added = 0, bad = [];
  for (const f of files) {
    const banks = parseSyx(await f.arrayBuffer());
    if (!banks.length) { bad.push(f.name); continue; }
    banks.forEach((voices, n) => found.push({ file: banks.length > 1 ? f.name + ' #' + (n + 1) : f.name, voices }));
    added += banks.length;
  }
  if (added && chosenFound < 0) chosenFound = found.length - added;
  renderFound();
  msg($('findMsg'), (added ? 'Found ' + added + ' bank' + (added > 1 ? 's' : '') + '. ' : '') + (bad.length ? 'Not a DX7 32-voice bank: ' + bad.join(', ') : ''), bad.length ? 'err' : 'ok');
};

function renderFound() {
  const ul = $('found');
  ul.innerHTML = '';
  found.forEach((fb, i) => {
    const li = document.createElement('li');
    li.innerHTML = '<label><input type="radio" name="fb"><b></b></label><div class="names"></div>';
    const r = li.querySelector('input');
    r.checked = i === chosenFound;
    r.onchange = () => { chosenFound = i; updateLoadBtn(); };
    li.querySelector('b').textContent = fb.file;
    li.querySelector('.names').textContent = fb.voices.map(voiceName).join(' · ');
    ul.appendChild(li);
  });
  updateLoadBtn();
}

function updateLoadBtn() { $('loadBtn').disabled = chosenFound < 0; }

// ---- 2. Load ----
$('loadBtn').onclick = () => {
  const q = +$('quarterSel').value, fb = found[chosenFound];
  if (!fb) return;
  const hadVoices = bank.slice(q * 32, q * 32 + 32).some(v => v);
  if (hadVoices && !confirmReplace(q)) return;
  for (let k = 0; k < 32; k++) bank[q * 32 + k] = fb.voices[k].slice();
  sel.clear();
  setDirty(true);
  renderBank();
  msg($('loadMsg'), 'Loaded ' + fb.file + ' into Bank ' + 'ABCD'[q] + '. Not saved to the box yet.', 'ok');
};

// Two-tap confirm instead of window.confirm(), which some phone browsers block on captive networks.
let pendingReplace = -1;
function confirmReplace(q) {
  if (pendingReplace === q) { pendingReplace = -1; return true; }
  pendingReplace = q;
  msg($('loadMsg'), 'Bank ' + 'ABCD'[q] + ' already has voices. Press Load again to replace them.', 'err');
  return false;
}
$('quarterSel').onchange = () => { pendingReplace = -1; msg($('loadMsg'), ''); };

// ---- 3. Arrange ----
function renderBank() {
  const ul = $('bank');
  const line = $('dropline');
  ul.innerHTML = '';
  ul.appendChild(line);
  for (let i = 0; i < 128; i++) {
    if (i % 32 === 0) {
      const h = document.createElement('li');
      h.className = 'qhead';
      const q = i / 32, full = bank.slice(i, i + 32).every(v => v);
      h.innerHTML = '<span>Bank ' + 'ABCD'[q] + '</span><span>' + (full ? 'ready to send' : 'has empty slots') + '</span>';
      ul.appendChild(h);
    }
    const li = document.createElement('li');
    li.className = 'slot' + (sel.has(i) ? ' sel' : '');
    li.dataset.i = i;
    const v = bank[i];
    li.innerHTML = '<span class="num"></span><span class="name"></span><span class="handle" aria-label="Drag">&#8801;</span>';
    li.querySelector('.num').textContent = String(i + 1).padStart(3, '0');
    const nm = li.querySelector('.name');
    if (v) nm.textContent = voiceName(v); else { nm.textContent = 'empty'; nm.classList.add('empty'); }
    ul.appendChild(li);
  }
  $('selCount').textContent = sel.size ? sel.size + ' selected' : '';
  $('clearSelBtn').disabled = $('emptyBtn').disabled = !sel.size;
  $('dooBtn').disabled = sel.size !== 1;
}

$('bank').addEventListener('click', e => {
  const li = e.target.closest('.slot');
  if (!li || e.target.closest('.handle')) return;
  const i = +li.dataset.i;
  if (e.shiftKey && anchor >= 0) {
    const [a, b] = anchor < i ? [anchor, i] : [i, anchor];
    for (let k = a; k <= b; k++) sel.add(k);
  } else {
    sel.has(i) ? sel.delete(i) : sel.add(i);
    anchor = i;
  }
  renderBank();
});

$('dooBtn').onclick = () => {
  const i = [...sel][0];
  bank[i] = DOO_VOICE.slice();
  setDirty(true);
  renderBank();
  msg($('arrMsg'), 'DOO VOICE is in slot ' + String(i + 1).padStart(3, '0') + '. Save, then send Bank ' + 'ABCD'[i >> 5] + ' so the FM-1 has it; sing mode picks it automatically.', 'ok');
};

$('clearSelBtn').onclick = () => { sel.clear(); renderBank(); };
$('emptyBtn').onclick = () => { for (const i of sel) bank[i] = null; sel.clear(); setDirty(true); renderBank(); };

// Pointer-based drag (works for mouse and touch alike, unlike HTML5 drag-and-drop on phones).
let drag = null;
$('bank').addEventListener('pointerdown', e => {
  const h = e.target.closest('.handle');
  if (!h) return;
  e.preventDefault();
  const i = +h.parentNode.dataset.i;
  if (!sel.has(i)) { sel.clear(); sel.add(i); anchor = i; renderBank(); }
  const items = [...sel].sort((a, b) => a - b);
  drag = { items, gap: -1, y: e.clientY, id: e.pointerId };
  document.querySelectorAll('.slot').forEach(el => { if (sel.has(+el.dataset.i)) el.classList.add('moving'); });
  const g = $('ghost');
  g.textContent = items.length === 1 ? 'Moving ' + (bank[items[0]] ? voiceName(bank[items[0]]) : 'empty slot') : 'Moving ' + items.length + ' voices';
  g.style.display = 'block';
  moveDrag(e.clientX, e.clientY);
  requestAnimationFrame(autoScroll);
});

window.addEventListener('pointermove', e => { if (drag && e.pointerId === drag.id) { e.preventDefault(); moveDrag(e.clientX, e.clientY); } }, { passive: false });
window.addEventListener('pointerup', e => { if (drag && e.pointerId === drag.id) endDrag(true); });
window.addEventListener('pointercancel', e => { if (drag && e.pointerId === drag.id) endDrag(false); });

function moveDrag(x, y) {
  drag.x = x; drag.y = y;
  const g = $('ghost');
  g.style.left = (x + 12) + 'px';
  g.style.top = (y - 34) + 'px';
  // Gap index 0..128: the new position sits before slot `gap`.
  const rows = document.querySelectorAll('.slot');
  let gap = 128, lineY;
  for (const el of rows) {
    const r = el.getBoundingClientRect();
    if (y < r.top + r.height / 2) { gap = +el.dataset.i; lineY = el.offsetTop; break; }
  }
  if (gap === 128) { const last = rows[rows.length - 1]; lineY = last.offsetTop + last.offsetHeight; }
  drag.gap = gap;
  const line = $('dropline');
  line.style.display = 'block';
  line.style.top = (lineY - 1) + 'px';
}

function autoScroll() {
  if (!drag) return;
  const edge = 70, h = window.innerHeight - $('bar').offsetHeight;
  let dy = 0;
  if (drag.y < edge) dy = -Math.ceil((edge - drag.y) / 5);
  else if (drag.y > h - edge) dy = Math.ceil((drag.y - (h - edge)) / 5);
  if (dy) { window.scrollBy(0, dy); moveDrag(drag.x, drag.y); }
  requestAnimationFrame(autoScroll);
}

function endDrag(commit) {
  const d = drag;
  drag = null;
  $('ghost').style.display = 'none';
  $('dropline').style.display = 'none';
  if (commit && d.gap >= 0) {
    const moving = d.items.map(i => bank[i]);
    const before = d.items.filter(i => i < d.gap).length;
    const rest = bank.filter((_, i) => !sel.has(i));
    const at = d.gap - before;
    const next = rest.slice(0, at).concat(moving, rest.slice(at));
    const changed = next.some((v, i) => v !== bank[i]);
    bank = next;
    sel = new Set(moving.map((_, k) => at + k));
    anchor = at;
    if (changed) setDirty(true);
  }
  renderBank();
}

// ---- Save / send ----
async function save() {
  const out = new Uint8Array(128 * VLEN);
  bank.forEach((v, i) => { if (v) out.set(v, i * VLEN); });
  const fd = new FormData();
  fd.append('bank', new Blob([out], { type: 'application/octet-stream' }), 'bank.bin');
  const r = await fetch('/bank', { method: 'POST', body: fd });
  const t = await r.text();
  if (!r.ok) throw new Error(t);
  setDirty(false);
  return t;
}

$('saveBtn').onclick = async () => {
  $('saveBtn').disabled = true;
  try { msg($('barMsg'), await save(), 'ok'); }
  catch (err) { msg($('barMsg'), 'Save failed: ' + err.message, 'err'); setDirty(true); }
};

let sendQueue = [];
$('sendBtn').onclick = async () => {
  sendQueue = [0, 1, 2, 3].filter(q => bank.slice(q * 32, q * 32 + 32).every(v => v));
  const skipped = [0, 1, 2, 3].filter(q => !sendQueue.includes(q)).map(q => 'ABCD'[q]);
  if (!sendQueue.length) { msg($('barMsg'), 'No bank is complete yet. Each bank needs all 32 slots filled before it can be sent.', 'err'); return; }
  if (dirty) {
    try { await save(); } catch (err) { msg($('barMsg'), 'Save failed, nothing sent: ' + err.message, 'err'); return; }
  }
  msg($('barMsg'), skipped.length ? 'Skipping bank ' + skipped.join(', ') + ' (has empty slots).' : '');
  $('sendBtn').disabled = true;
  $('sendbox').style.display = 'block';
  sendNext();
};

async function sendNext() {
  const q = sendQueue.shift();
  if (q === undefined) { stopSend('Done. Every complete bank has been sent.'); return; }
  const L = 'ABCD'[q];
  $('sendText').textContent = 'Sending bank ' + L + '…';
  $('sendNext').disabled = true;
  try {
    const r = await fetch('/send?q=' + q, { method: 'POST' });
    if (!r.ok) throw new Error(await r.text());
    $('sendText').innerHTML = '';
    $('sendText').append('Bank ' + L + ' sent. On the FM-1, turn ', Object.assign(document.createElement('b'), { textContent: 'Knob ' + (q + 1) }), ' to save it into bank ' + L + '. Then press ' + (sendQueue.length ? 'Next.' : 'Next to finish.'));
    $('sendNext').disabled = false;
  } catch (err) {
    stopSend('Send failed: ' + err.message, true);
  }
}

function stopSend(text, isErr) {
  sendQueue = [];
  $('sendbox').style.display = 'none';
  $('sendBtn').disabled = false;
  msg($('barMsg'), text, isErr ? 'err' : 'ok');
}
$('sendNext').onclick = sendNext;
$('sendStop').onclick = () => stopSend('Stopped.');

// ---- 4. Song ----
const NOTE_NAMES = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'];
const noteLabel = n => NOTE_NAMES[n % 12] + (Math.floor(n / 12) - 1);
const MAX_STEPS = 1000, LYRIC_LEN = 11;
let midi = null;   // {parts, lyrics, tol, title}

function readVar(b, p) {
  let v = 0, c;
  do { c = b[p.i++]; v = (v << 7) | (c & 0x7F); } while ((c & 0x80) && p.i < b.length);
  return v;
}

// Standard MIDI File -> note onsets per (track, channel) and text/lyric events.
function parseMidi(buf) {
  const b = new Uint8Array(buf), dv = new DataView(buf);
  const tag = o => String.fromCharCode(b[o], b[o + 1], b[o + 2], b[o + 3]);
  if (b.length < 14 || tag(0) !== 'MThd') throw new Error('not a MIDI file');
  const ntrks = dv.getUint16(10), division = dv.getUint16(12);
  if (division & 0x8000) throw new Error('SMPTE-timed MIDI files aren\'t supported');
  let pos = 8 + dv.getUint32(4);
  const notes = [], texts = [], tempos = [];
  for (let t = 0; t < ntrks && pos + 8 <= b.length; ) {
    const id = tag(pos), len = dv.getUint32(pos + 4);
    pos += 8;
    const end = Math.min(pos + len, b.length);
    if (id === 'MTrk') {
      const p = { i: pos };
      let tick = 0, status = 0;
      const open = new Map();  // "ch:note" -> notes still sounding
      while (p.i < end) {
        tick += readVar(b, p);
        let st = b[p.i];
        if (st & 0x80) { p.i++; if (st < 0xF0) status = st; } else st = status;  // running status
        if (st === 0xFF) {
          const type = b[p.i++], l = readVar(b, p);
          if (type === 0x01 || type === 0x05) texts.push({ tick, type, txt: String.fromCharCode(...b.subarray(p.i, p.i + l)) });
          if (type === 0x51 && l === 3) tempos.push({ tick, us: (b[p.i] << 16) | (b[p.i + 1] << 8) | b[p.i + 2] });
          p.i += l;
          if (type === 0x2F) break;
        } else if (st === 0xF0 || st === 0xF7) {
          p.i += readVar(b, p);
        } else {
          const hi = st & 0xF0, ch = st & 0x0F;
          const d1 = b[p.i++], d2 = (hi === 0xC0 || hi === 0xD0) ? 0 : b[p.i++];
          if (ch === 9) continue;  // skip GM drums
          const k = ch + ':' + d1;
          if (hi === 0x90 && d2 > 0) {
            const n = { track: t, ch, note: d1, tick, end: -1 };
            notes.push(n);
            if (!open.has(k)) open.set(k, []);
            open.get(k).push(n);
          } else if (hi === 0x80 || hi === 0x90) {
            const q = open.get(k);
            if (q && q.length) q.shift().end = tick;
          }
        }
      }
      t++;
    }
    pos = end;
  }
  tempos.sort((a, b) => a.tick - b.tick);
  return { notes, texts, division, tempos };
}

// Tick -> milliseconds, following the file's tempo changes (120 BPM until the first one).
function tickClock(tempos, division) {
  const segs = [{ tick: 0, ms: 0, us: 500000 }];
  for (const tp of tempos) {
    const s = segs[segs.length - 1];
    const ms = s.ms + (tp.tick - s.tick) * s.us / division / 1000;
    if (tp.tick === s.tick) s.us = tp.us; else segs.push({ tick: tp.tick, ms, us: tp.us });
  }
  return tick => {
    let s = segs[0];
    for (const x of segs) { if (x.tick <= tick) s = x; else break; }
    return s.ms + (tick - s.tick) * s.us / division / 1000;
  };
}

// First index in sorted `arr` with value >= x.
function lowerBound(arr, x) {
  let lo = 0, hi = arr.length;
  while (lo < hi) { const m = (lo + hi) >> 1; if (arr[m] < x) lo = m + 1; else hi = m; }
  return lo;
}

function analyzeMidi(buf, fileName) {
  const { notes, texts, division, tempos } = parseMidi(buf);
  if (!notes.length) throw new Error('no notes found');
  // .kar puts lyrics in text events (with @-prefixed headers); plain MIDI uses lyric events.
  const lyricEvents = texts.filter(e => e.type === 0x05);
  const karText = texts.filter(e => e.type === 0x01 && !e.txt.startsWith('@') && !e.txt.startsWith('%'));
  const lyrics = (lyricEvents.length >= karText.length ? lyricEvents : karText).map(e => ({ tick: e.tick, txt: e.txt.replace(/[\/\\]/g, ' ') }));
  const titleEv = texts.find(e => e.txt.startsWith('@T'));
  const title = (titleEv ? titleEv.txt.slice(2).trim() : '') || fileName.replace(/\.[^.]+$/, '');
  const tol = Math.max(1, division >> 3);

  const map = new Map();
  for (const n of notes) {
    const k = n.track + ':' + n.ch;
    if (!map.has(k)) map.set(k, { track: n.track, ch: n.ch, notes: [] });
    map.get(k).notes.push(n);
  }
  const lyricTicks = lyrics.map(l => l.tick);
  const parts = [...map.values()].map(pt => {
    pt.notes.sort((a, b) => a.tick - b.tick);
    const onsets = [...new Set(pt.notes.map(n => n.tick))];
    const pitches = pt.notes.map(n => n.note).sort((a, b) => a - b);
    pt.min = pitches[0];
    pt.max = pitches[pitches.length - 1];
    const median = pitches[pitches.length >> 1];
    let matched = 0;
    for (const lt of lyricTicks) {
      const k = lowerBound(onsets, lt - tol);
      if (k < onsets.length && onsets[k] <= lt + tol) matched++;
    }
    pt.lyricMatch = lyricTicks.length ? matched / lyricTicks.length : 0;
    const mono = onsets.length / pt.notes.length;  // 1 = one note at a time
    const vocal = median >= 52 && median <= 81 ? 1 : 0.4;
    pt.score = pt.lyricMatch * 1000 + Math.min(onsets.length, 400) * mono * vocal;
    return pt;
  }).sort((a, b) => b.score - a.score);
  return { parts, lyrics, tol, title, toMs: tickClock(tempos, division) };
}

// Melody of one part: highest note at each onset, lyrics attached,
// repeated pitches merged into one step (their lyrics joined).
// Also returns `play`: every onset unmerged, timed in ms, for playback.
function buildSteps(part, transpose) {
  const top = new Map();
  for (const n of part.notes) { const o = top.get(n.tick); if (!o || n.note > o.note) top.set(n.tick, n); }
  const ticks = [...top.keys()].sort((a, b) => a - b);
  const steps = ticks.map(t => ({ note: Math.min(127, Math.max(0, top.get(t).note + transpose)), lyric: '' }));
  for (const l of midi.lyrics) {
    const k = lowerBound(ticks, l.tick - midi.tol);
    if (k < steps.length) steps[k].lyric += l.txt;
  }
  const merged = [], play = [];
  steps.forEach((st, i) => {
    const last = merged[merged.length - 1];
    if (last && last.note === st.note) last.lyric += st.lyric; else merged.push({ ...st });
    const t = ticks[i], n = top.get(t), next = ticks[i + 1];
    let endTick = n.end > t ? n.end : (next ?? t + midi.tol * 8);
    if (next !== undefined) endTick = Math.min(endTick, next);  // one note at a time
    const start = Math.round(midi.toMs(t));
    play.push({ note: st.note, step: merged.length - 1, start, dur: Math.max(20, Math.min(65535, Math.round(midi.toMs(endTick)) - start)) });
  });
  for (const st of merged) st.lyric = st.lyric.replace(/\s+/g, ' ').trim();
  const t0 = play.length ? play[0].start : 0;  // skip the intro before the first sung note
  for (const p of play) p.start -= t0;
  return { steps: merged, play };
}

function asciiBytes(str, len) {
  const out = new Uint8Array(len);
  for (let i = 0; i < Math.min(len, str.length); i++) { const c = str.charCodeAt(i); out[i] = c >= 32 && c < 127 ? c : 63; }
  return out;
}

function songPreview() {
  const part = midi.parts[+$('partSel').value], { steps, play } = buildSteps(part, +$('transSel').value);
  const notes = steps.map(s => s.note), distinct = new Set(notes).size;
  const lo = Math.min(...notes), hi = Math.max(...notes);
  const start = steps.slice(0, 10).map(s => noteLabel(s.note) + (s.lyric ? ' ' + s.lyric : '')).join(' \u00b7 ');
  const secs = play.length ? Math.round((play[play.length - 1].start + play[play.length - 1].dur) / 1000) : 0;
  $('songPreview').textContent = steps.length + ' notes' + (steps.length > MAX_STEPS ? ' (only the first ' + MAX_STEPS + ' will be sent)' : '') +
    ', range ' + noteLabel(lo) + '\u2013' + noteLabel(hi) + ', ' + distinct + ' distinct pitches, sung part ' + Math.floor(secs / 60) + ':' + String(secs % 60).padStart(2, '0') + '. Starts: ' + start;
  return { steps: steps.slice(0, MAX_STEPS), play: play.filter(p => p.step < MAX_STEPS).slice(0, MAX_STEPS) };
}

$('songFindBtn').onclick = () => $('songInput').click();
$('songInput').onchange = async e => {
  const f = e.target.files[0];
  e.target.value = '';
  if (!f) return;
  try {
    midi = analyzeMidi(await f.arrayBuffer(), f.name);
  } catch (err) {
    $('songSetup').style.display = 'none';
    msg($('songMsg'), f.name + ': ' + err.message + '.', 'err');
    return;
  }
  $('partSel').innerHTML = '';
  midi.parts.forEach((pt, i) => {
    const o = document.createElement('option');
    o.value = i;
    o.textContent = 'Track ' + (pt.track + 1) + ', ch ' + (pt.ch + 1) + ': ' + pt.notes.length + ' notes, ' + noteLabel(pt.min) + '\u2013' + noteLabel(pt.max) +
      (midi.lyrics.length ? ', lyrics match ' + Math.round(pt.lyricMatch * 100) + '%' : '');
    $('partSel').appendChild(o);
  });
  $('transSel').innerHTML = '';
  for (let t = -12; t <= 12; t++) {
    const o = document.createElement('option');
    o.value = t;
    o.textContent = (t > 0 ? '+' : '') + t + (t === -12 ? ' (octave down)' : t === 12 ? ' (octave up)' : '');
    o.selected = t === 0;
    $('transSel').appendChild(o);
  }
  $('songSetup').style.display = 'block';
  songPreview();
  msg($('songMsg'), 'Read "' + midi.title + '". The melody guess is preselected; pick another part if it sounds wrong.', 'ok');
};
$('partSel').onchange = $('transSel').onchange = songPreview;

async function postSong(bytes) {
  const fd = new FormData();
  fd.append('song', new Blob([bytes], { type: 'application/octet-stream' }), 'song.bin');
  const r = await fetch('/song', { method: 'POST', body: fd });
  const t = await r.text();
  if (!r.ok) throw new Error(t);
  return t;
}

$('songSendBtn').onclick = async () => {
  const { steps, play } = songPreview();
  const pAt = 38 + steps.length * 12;
  const out = new Uint8Array(pAt + 6 + play.length * 9);
  out.set([70, 77, 49, 83], 0);  // "FM1S"
  out[4] = steps.length & 0xFF;
  out[5] = steps.length >> 8;
  out.set(asciiBytes(midi.title, 32), 6);
  steps.forEach((s, i) => { out[38 + i * 12] = s.note; out.set(asciiBytes(s.lyric, LYRIC_LEN), 39 + i * 12); });
  // Timed vocal track for playback: "FM1P", count, then note, step, start ms (u32), duration ms (u16).
  const dv = new DataView(out.buffer);
  out.set([70, 77, 49, 80], pAt);
  dv.setUint16(pAt + 4, play.length, true);
  play.forEach((p, i) => {
    const o = pAt + 6 + i * 9;
    out[o] = p.note;
    dv.setUint16(o + 1, p.step, true);
    dv.setUint32(o + 3, p.start, true);
    dv.setUint16(o + 7, p.dur, true);
  });
  $('songSendBtn').disabled = true;
  try { msg($('songMsg'), await postSong(out), 'ok'); showSongOnBox(midi.title, steps.length); }
  catch (err) { msg($('songMsg'), 'Sending the song failed: ' + err.message, 'err'); }
  $('songSendBtn').disabled = false;
};

$('songRemoveBtn').onclick = async () => {
  try { msg($('songMsg'), await postSong(new Uint8Array(0)), 'ok'); showSongOnBox('', 0); }
  catch (err) { msg($('songMsg'), 'Removing the song failed: ' + err.message, 'err'); }
};

function showSongOnBox(name, count, timed = count > 0) {
  $('songOnBox').textContent = count ? 'On the box: "' + name + '" (' + count + ' notes)' + (timed ? '.' : '. Send it again to be able to play it.') : 'No song on the box yet. Sing mode uses the chromatic FREE list until you add one.';
  $('songRemoveBtn').disabled = !count;
  $('playBtn').disabled = $('stopBtn').disabled = !timed;
}

async function postPlay(path, okText) {
  try {
    const r = await fetch(path, { method: 'POST' });
    const t = await r.text();
    if (!r.ok) throw new Error(t);
    msg($('songMsg'), okText, 'ok');
  } catch (err) { msg($('songMsg'), err.message, 'err'); }
}
$('playBtn').onclick = () => postPlay('/play', 'Playing the vocal track on the FM-1 in its DOO / voice preset. Sing along!');
$('stopBtn').onclick = () => postPlay('/stop', 'Stopped.');

fetch('/song.bin').then(r => r.arrayBuffer()).then(buf => {
  const b = new Uint8Array(buf);
  if (b.length >= 38 && String.fromCharCode(b[0], b[1], b[2], b[3]) === 'FM1S') {
    const count = b[4] | (b[5] << 8);
    showSongOnBox(String.fromCharCode(...b.subarray(6, 38)).replace(/\0.*$/, ''), count, b.length > 38 + count * 12);
  } else showSongOnBox('', 0);
}).catch(() => showSongOnBox('', 0));

// ---- Start: pull the box's current bank ----
fetch('/bank.bin').then(r => r.arrayBuffer()).then(buf => {
  const b = new Uint8Array(buf);
  if (b.length === 128 * VLEN) {
    for (let i = 0; i < 128; i++) { const v = b.slice(i * VLEN, (i + 1) * VLEN); bank[i] = isEmptyVoice(v) ? null : v; }
  }
  renderBank();
}).catch(() => { renderBank(); msg($('barMsg'), 'Couldn\'t read the box\'s bank.', 'err'); });
</script>
</body></html>
)HTML";
