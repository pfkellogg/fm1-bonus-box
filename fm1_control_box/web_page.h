// web_page.h — the soundbank page fm1_control_box serves in WiFi mode.
//
// Everything interactive happens here in the browser: reading .syx files
// off the phone/laptop, loading a 32-voice bank into one of the box's four
// quarters, and drag-and-drop reordering (with multi-select) across all 128
// slots. The box itself only stores the final 16KB bank (/bank.bin, /bank)
// and sends quarters to the FM-1 (/send?q=N).

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
<p class="muted">Pick DX7 bank files, load them into the box, drag voices into the order you want, then save and send to the FM-1.</p>

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
