#pragma once

// In-browser editor for JSON layout widgets. Renders a pixel-accurate
// preview on a canvas using the same 5x7 GLCD font the device uses, and
// can push the layout to the device screen without saving it.

static const char EDITOR_HTML[] = R"html(
<!doctype html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Widget Editor</title>
<link rel="stylesheet" href="/style.css">
<style>
body{max-width:960px}
.cols{display:flex;gap:24px;flex-wrap:wrap;align-items:flex-start}
.col{flex:1 1 320px;min-width:0}
.row{display:flex;gap:8px}
.row>*{flex:1;min-width:0}
select{display:block;width:100%;margin:8px 0;padding:10px;font-size:16px;background:#222;color:#eee;border:1px solid #444;border-radius:6px;box-sizing:border-box}
textarea{display:block;width:100%;min-height:380px;margin:8px 0;padding:10px;font:13px/1.45 ui-monospace,SFMono-Regular,Menlo,Consolas,monospace;background:#222;color:#eee;border:1px solid #444;border-radius:6px;box-sizing:border-box;tab-size:2;white-space:pre;overflow-x:auto}
canvas{display:block;width:340px;max-width:100%;image-rendering:pixelated;image-rendering:crisp-edges;border:1px solid #333;border-radius:4px;background:#000}
.keys{display:flex;flex-wrap:wrap;gap:6px}
.keys button{display:inline-block;width:auto;margin:0;padding:4px 8px;font:12px ui-monospace,monospace;text-align:left}
.keys button span{color:#999;margin-left:6px}
.err{color:#f66;min-height:1.4em;font:13px ui-monospace,monospace;white-space:pre-wrap}
label.inline{display:flex;align-items:center;gap:10px;font-size:15px;margin:8px 0}
label.inline input{width:auto;display:inline;margin:0}
pre{font:12px/1.5 ui-monospace,monospace;color:#bbb;background:#1a1a1a;border:1px solid #333;border-radius:6px;padding:10px;overflow-x:auto}
.actions{display:flex;gap:8px}
.actions button{margin:8px 0}
</style>
</head>
<body>
<nav><a href="/setup">Setup</a><a href="/widgets">Widgets</a><a href="/editor">Editor</a></nav>
<h2>Widget Editor</h2>

<div class="cols">
<div class="col">
  <h3>Widget</h3>
  <div class="row">
    <select id="which"></select>
    <select id="tpl"><option value="">Insert template&hellip;</option></select>
  </div>
  <input id="id" placeholder="id (lowercase letters, digits, dashes)" autocapitalize="off" autocorrect="off" maxlength="24">
  <textarea id="src" spellcheck="false"></textarea>
  <p id="err" class="err"></p>
  <label class="inline"><input type="checkbox" id="live"> Live preview on the device while typing</label>
  <div class="actions">
    <button id="run">Show on device</button>
    <button id="save" class="primary">Save</button>
    <button id="del" class="danger">Delete</button>
  </div>
  <p id="msg"></p>
  <h3>API token</h3>
  <input id="token" placeholder="Shown on the device screen after it connects" autocapitalize="off" autocorrect="off">
</div>

<div class="col">
  <h3>Preview</h3>
  <canvas id="cv" width="340" height="640"></canvas>
  <p class="hint">Rendered in the browser with live values from the device. "Show on device" puts it on the real screen for 60 seconds.</p>
  <h3>Keys</h3>
  <p class="hint">Tap to insert at the cursor. <code>ping.N</code> also accepts the target name, e.g. <code>{ping.router.ms}</code>. <code>api.*</code> keys come from the data sources on the <a href="/setup">setup page</a>.</p>
  <div class="keys" id="keys"></div>
  <h3>Reference</h3>
<pre>screen 170 x 320, black background
size 1 = 6x8 px per char (28 cols)
size 2 = 14 cols, size 4 = 7 cols

text  x y size align color text
line  x y x2 y2 color
rect  x y w h color fill
bar   x y w h color value   (0-100)

align: left | center | right
color: bg text dim ok warn bad accent
       #rrggbb, or {ping.0.color}
text and value take {keys}
api.&lt;source&gt;.&lt;field&gt; plus .status
  .color .age .updated .error

math in braces, keys as variables:
  {round(api.weather.temp * 9/5 + 32, 1)}
  {min(ping.0.ms / 2, 100)}
  + - * / % ^ ( )  round(x,n) abs
  min max floor ceil sqrt clamp(x,lo,hi)</pre>
</div>
</div>

<script>
const $ = id => document.getElementById(id);
const esc = s => String(s).replace(/[&<>"]/g, c => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]));
const msg = t => { $('msg').textContent = t; };

let token = '';
try { token = localStorage.getItem('apiToken') || ''; } catch (e) {}
$('token').value = token;
$('token').addEventListener('change', () => {
  token = $('token').value.trim();
  try { localStorage.setItem('apiToken', token); } catch (e) {}
});

async function api(path, method, body) {
  const r = await fetch(path, {
    method: method || 'GET',
    headers: { 'Content-Type': 'application/json', 'Authorization': 'Bearer ' + token },
    body: body !== undefined ? JSON.stringify(body) : undefined
  });
  if (r.status === 401) throw new Error('Unauthorized. Enter the API token shown on the device.');
  const j = await r.json().catch(() => ({}));
  if (!r.ok) throw new Error(j.error || ('HTTP ' + r.status));
  return j;
}

// ---------- templates ----------
// Fetched from the device; this one is only the fallback for a new widget.
let TEMPLATES = [
  { id: 'blank', name: 'Blank', json: { name: 'My Widget', elements: [
    { type: 'text', x: 85, y: 150, size: 2, align: 'center', color: 'text', text: 'Hello' }
  ] } }
];

async function loadTemplates() {
  try {
    const r = await fetch('/api/layouts/templates');
    const list = await r.json();
    if (Array.isArray(list) && list.length) TEMPLATES = list.map(t => ({ id: t.id, name: t.name, preload: t.preload, json: t.layout }));
  } catch (e) {}
  const sel = $('tpl');
  sel.innerHTML = '<option value="">Insert template&hellip;</option>';
  TEMPLATES.forEach((t, i) => {
    const o = document.createElement('option');
    o.value = i; o.textContent = t.name + (t.preload ? ' (preloaded)' : '');
    sel.appendChild(o);
  });
}

// ---------- renderer ----------
// 5x7 GLCD font, printable ASCII 32..126, 5 column bytes per glyph, bit 0 = top row.
const FONT = Uint8Array.from(atob('AAAAAAAAAF8AAAAHAAcAFH8UfxQkKn8qEiMTCGRiNklWIFAACAcDAAAcIkEAAEEiHAAqHH8cKggIPggIAIBwMAAICAgICAAAYGAAIBAIBAI+UUlFPgBCf0AAcklJSUYhQUlNMxgUEn8QJ0VFRTk8SklJMUEhEQkHNklJSTZGSUkpHgAAFAAAAEA0AAAACBQiQRQUFBQUAEEiFAgCAVkJBj5BXVlOfBIREnx/SUlJNj5BQUEif0FBQT5/SUlJQX8JCQkBPkFBUXN/CAgIfwBBf0EAIEBBPwF/CBQiQX9AQEBAfwIcAn9/BAgQfz5BQUE+fwkJCQY+QVEhXn8JGSlGJklJSTIDAX8BAz9AQEA/HyBAIB8/QDhAP2MUCBRjAwR4BANhWUlNQwB/QUFBAgQIECAAQUFBfwQCAQIEQEBAQEAAAwcIACBUVHhAfyhERDg4REREKDhERCh/OFRUVBgACH4JAhikpJx4fwgEBHgARH1AACBAQD0AfxAoRAAAQX9AAHwEeAR4fAgEBHg4REREOPwYJCQYGCQkGPx8CAQECEhUVFQkBAQ/RCQ8QEAgfBwgQCAcPEAwQDxEKBAoREyQkJB8RGRUTEQACDZBAAAAdwAAAEE2CAACAQIEAg=='), c => c.charCodeAt(0));
const COLORS = { bg: '#000000', text: '#ffffff', dim: '#3a3d3a', ok: '#00ff00', warn: '#ffa600', bad: '#ff0000', accent: '#009eff' };
const S = 2;
const cv = $('cv');
const ctx = cv.getContext('2d');

let data = {};

function lookup(key, strict) {
  if (key in data) return data[key];
  const m = /^ping\.([^.]+)\.(\w+)$/.exec(key);
  if (m && !/^\d+$/.test(m[1])) {
    for (let i = 0; ; i++) {
      const n = data['ping.' + i + '.name'];
      if (n === undefined) break;
      if (n.toLowerCase() === m[1].toLowerCase()) return data['ping.' + i + '.' + m[2]] ?? '--';
    }
  }
  return strict ? undefined : '--';
}

// ---------- expressions ----------
// Mirrors LayoutExpr.cpp on the device: a brace body that is not a key is
// arithmetic with keys as variables. round(x, n) fixes the decimals.
function isExpr(b) { return /[()+\-*\/%^ ]/.test(b) || /^[0-9]/.test(b); }

function evalExpr(body) {
  let p = 0, err = false, depth = 0;
  const s = body;
  const skip = () => { while (s[p] === ' ' || s[p] === '\t') p++; };
  const identStart = c => /[A-Za-z_]/.test(c || '');
  const identChar = c => /[A-Za-z0-9_.\-]/.test(c || '');
  function variable() {
    const start = p;
    while (identChar(s[p])) p++;
    let len = p - start;
    for (;;) {
      if (len === 0) return null;
      const name = s.substr(start, len);
      const v = lookup(name, true);
      if (v !== undefined) {
        const m = /^\s*[-+]?(\d+\.?\d*|\.\d+)([eE][-+]?\d+)?/.exec(v);
        if (!m) return null;
        p = start + len;
        return { v: parseFloat(m[0]), d: -1 };
      }
      const cut = name.lastIndexOf('-');
      if (cut <= 0) return null;
      len = cut;
    }
  }
  function args() {
    const a = [];
    skip();
    if (s[p] === ')') { p++; return a; }
    for (;;) {
      if (a.length >= 4) { err = true; return a; }
      a.push(expr());
      if (err) return a;
      skip();
      if (s[p] === ',') { p++; continue; }
      if (s[p] === ')') { p++; return a; }
      err = true; return a;
    }
  }
  function call(name) {
    const a = args();
    if (err) return { v: NaN, d: -1 };
    const n = a.length;
    const bad = () => { err = true; return { v: NaN, d: -1 }; };
    switch (name) {
      case 'round': { if (n < 1 || n > 2) return bad(); let d = n === 2 ? Math.trunc(a[1].v) : 0; d = Math.max(0, Math.min(6, d)); const m = Math.pow(10, d); return { v: Math.round(a[0].v * m) / m, d: d }; }
      case 'abs':   return n === 1 ? { v: Math.abs(a[0].v), d: a[0].d } : bad();
      case 'floor': return n === 1 ? { v: Math.floor(a[0].v), d: 0 } : bad();
      case 'ceil':  return n === 1 ? { v: Math.ceil(a[0].v), d: 0 } : bad();
      case 'sqrt':  return n === 1 ? { v: Math.sqrt(a[0].v), d: -1 } : bad();
      case 'min': case 'max': { if (n < 1) return bad(); let r = a[0]; for (let i = 1; i < n; i++) if (name === 'min' ? a[i].v < r.v : a[i].v > r.v) r = a[i]; return r; }
      case 'clamp': { if (n !== 3) return bad(); return { v: Math.min(Math.max(a[0].v, a[1].v), a[2].v), d: a[0].d }; }
      default: return bad();
    }
  }
  function primary() {
    skip();
    if (++depth > 24) { err = true; return { v: NaN, d: -1 }; }
    let r = { v: NaN, d: -1 };
    if (s[p] === '(') {
      p++; r = expr(); skip();
      if (s[p] !== ')') err = true; else p++;
    } else if (/[0-9]/.test(s[p] || '') || (s[p] === '.' && /[0-9]/.test(s[p + 1] || ''))) {
      const m = /^(\d+\.?\d*|\.\d+)([eE][-+]?\d+)?/.exec(s.slice(p));
      r.v = parseFloat(m[0]); p += m[0].length;
    } else if (identStart(s[p])) {
      const start = p;
      while (/[A-Za-z0-9_]/.test(s[p] || '')) p++;
      const save = p;
      skip();
      if (s[p] === '(') { p++; r = call(s.substring(start, save)); }
      else { p = start; const v = variable(); if (!v) err = true; else r = v; }
    } else err = true;
    depth--;
    return r;
  }
  function unary() {
    skip();
    if (s[p] === '-') { p++; const r = unary(); r.v = -r.v; return r; }
    if (s[p] === '+') { p++; return unary(); }
    const r = primary();
    skip();
    if (s[p] === '^') { p++; const e = unary(); return { v: Math.pow(r.v, e.v), d: -1 }; }
    return r;
  }
  function term() {
    let r = unary();
    for (;;) {
      skip();
      const op = s[p];
      if (op !== '*' && op !== '/' && op !== '%') return r;
      p++;
      const b = unary();
      if (err) return r;
      if (op === '*') r.v = r.v * b.v; else if (op === '/') r.v = b.v === 0 ? NaN : r.v / b.v; else r.v = b.v === 0 ? NaN : r.v % b.v;
      if (b.d > r.d) r.d = b.d;
    }
  }
  function expr() {
    let r = term();
    for (;;) {
      skip();
      const op = s[p];
      if (op !== '+' && op !== '-') return r;
      p++;
      const b = term();
      if (err) return r;
      r.v = op === '+' ? r.v + b.v : r.v - b.v;
      if (b.d > r.d) r.d = b.d;
    }
  }
  const v = expr();
  skip();
  if (err || p !== s.length || !isFinite(v.v)) return '--';
  if (v.d >= 0) return v.v.toFixed(v.d);
  if (v.v === Math.floor(v.v) && Math.abs(v.v) < 1e15) return v.v.toFixed(0);
  return String(parseFloat(v.v.toFixed(2)));
}

function expandKey(k) {
  const v = lookup(k, true);
  if (v !== undefined) return v;
  return isExpr(k) ? evalExpr(k) : '--';
}

const expand = t => String(t ?? '').replace(/\{([^}]+)\}/g, (m, k) => expandKey(k));

function color(spec) {
  let n = String(spec ?? 'text');
  const templated = n.includes('{');
  if (templated) n = expand(n);
  if (COLORS[n]) return COLORS[n];
  if (/^#[0-9a-f]{6}$/i.test(n)) return n;
  return templated ? COLORS.dim : COLORS.text;   // unresolved template renders dim, like the device
}

function drawText(s, x, y, size, align, col) {
  const w = s.length * 6 * size;
  if (align === 'center') x -= Math.floor(w / 2);
  else if (align === 'right') x -= w;
  ctx.fillStyle = col;
  for (const ch of s) {
    let code = ch.charCodeAt(0);
    if (code < 32 || code > 126) code = 63;
    const off = (code - 32) * 5;
    for (let i = 0; i < 5; i++) {
      const b = FONT[off + i];
      for (let r = 0; r < 8; r++) {
        if (b >> r & 1) ctx.fillRect((x + i * size) * S, (y + r * size) * S, size * S, size * S);
      }
    }
    x += 6 * size;
  }
}

function drawLine(x1, y1, x2, y2, col) {
  ctx.fillStyle = col;
  let dx = Math.abs(x2 - x1), dy = -Math.abs(y2 - y1);
  let sx = x1 < x2 ? 1 : -1, sy = y1 < y2 ? 1 : -1, e = dx + dy;
  for (;;) {
    ctx.fillRect(x1 * S, y1 * S, S, S);
    if (x1 === x2 && y1 === y2) break;
    const e2 = 2 * e;
    if (e2 >= dy) { e += dy; x1 += sx; }
    if (e2 <= dx) { e += dx; y1 += sy; }
  }
}

function drawRect(x, y, w, h, col, fill) {
  ctx.fillStyle = col;
  if (fill) { ctx.fillRect(x * S, y * S, w * S, h * S); return; }
  ctx.fillRect(x * S, y * S, w * S, S);
  ctx.fillRect(x * S, (y + h - 1) * S, w * S, S);
  ctx.fillRect(x * S, y * S, S, h * S);
  ctx.fillRect((x + w - 1) * S, y * S, S, h * S);
}

function parse() {
  const j = JSON.parse($('src').value);
  if (!j || typeof j !== 'object' || Array.isArray(j)) throw new Error('layout must be a JSON object');
  if (!Array.isArray(j.elements)) throw new Error('"elements" must be an array');
  if (j.elements.length > 32) throw new Error('too many elements (max 32)');
  j.elements.forEach((e, i) => {
    if (!e || typeof e !== 'object') throw new Error('element ' + i + ': must be an object');
    if (!['text', 'line', 'rect', 'bar'].includes(e.type)) throw new Error('element ' + i + ': unknown type (use text, line, rect or bar)');
    const size = e.size ?? 1;
    if (!Number.isInteger(size) || size < 1 || size > 8) throw new Error('element ' + i + ': size must be 1-8');
    if (e.align && !['left', 'center', 'right'].includes(e.align)) throw new Error('element ' + i + ': align must be left, center or right');
    const c = String(e.color ?? 'text');
    if (!c.includes('{') && !COLORS[c] && !/^#[0-9a-f]{6}$/i.test(c)) throw new Error('element ' + i + ': unknown color');
    if (String(e.type === 'bar' ? (e.value ?? '') : (e.text ?? '')).length > 63) throw new Error('element ' + i + ': text longer than 63 characters');
  });
  return j;
}

function render() {
  ctx.fillStyle = '#000';
  ctx.fillRect(0, 0, cv.width, cv.height);
  let j;
  try { j = parse(); $('err').textContent = ''; }
  catch (e) { $('err').textContent = e.message; return null; }

  for (const e of j.elements) {
    const col = color(e.color);
    const x = e.x | 0, y = e.y | 0;
    if (e.type === 'text') drawText(expand(e.text), x, y, e.size ?? 1, e.align || 'left', col);
    else if (e.type === 'line') drawLine(x, y, e.x2 ?? x, e.y2 ?? y, col);
    else if (e.type === 'rect') drawRect(x, y, e.w | 0, e.h | 0, col, !!e.fill);
    else if (e.type === 'bar') {
      let v = parseInt(expand(e.value), 10); if (isNaN(v)) v = 0; v = Math.max(0, Math.min(100, v));
      const w = e.w | 0, h = e.h | 0;
      drawRect(x, y, w, h, COLORS.dim, false);
      const fw = Math.floor(Math.max(0, w - 2) * v / 100);
      if (fw > 0 && h > 2) drawRect(x + 1, y + 1, fw, h - 2, col, true);
    }
  }
  return j;
}

// ---------- editor state ----------
let layouts = [];
let current = '';        // id being edited, '' for a new widget
let renderTimer = 0, liveTimer = 0, keepAlive = 0;

function fmt(o) {
  const rest = Object.assign({}, o); delete rest.elements;
  const els = (o.elements || []).map(e => '    ' + JSON.stringify(e)).join(',\n');
  const head = Object.keys(rest).length ? JSON.stringify(rest, null, 2).replace(/\n}$/, '') + ',' : '{';
  return head + '\n  "elements": [\n' + els + '\n  ]\n}';
}

const slug = s => String(s).toLowerCase().replace(/[^a-z0-9]+/g, '-').replace(/^-+|-+$/g, '').slice(0, 24);

function setSource(obj) {
  $('src').value = fmt(obj);
  render();
}

function fillWhich() {
  const sel = $('which');
  sel.innerHTML = '<option value="">New widget</option>';
  layouts.forEach(l => {
    const o = document.createElement('option');
    o.value = l.id; o.textContent = l.name + ' (' + l.id + ')';
    sel.appendChild(o);
  });
  sel.value = current;
  $('del').disabled = !current;
}

async function loadList() {
  const j = await api('/api/layouts');
  layouts = j.layouts || [];
  fillWhich();
  return j;
}

async function open(id) {
  current = id;
  $('id').value = id;
  $('del').disabled = !id;
  if (!id) { setSource(TEMPLATES[0].json); return; }
  try {
    const r = await fetch('/api/layouts?id=' + encodeURIComponent(id));
    if (!r.ok) throw new Error('HTTP ' + r.status);
    setSource(await r.json());
  } catch (e) { msg('Failed to load widget: ' + e.message); }
}

async function pushPreview() {
  const j = render();
  if (!j) return false;
  try { await api('/api/layouts/preview', 'POST', j); return true; }
  catch (e) { msg(e.message); return false; }
}

async function stopPreview() {
  clearInterval(keepAlive); keepAlive = 0;
  try { await api('/api/layouts/preview', 'DELETE'); } catch (e) {}
}

async function save() {
  const j = render();
  if (!j) { msg('Fix the error above first.'); return; }
  let id = $('id').value.trim().toLowerCase();
  if (!id) { id = slug(j.name || 'widget'); $('id').value = id; }
  $('save').disabled = true;
  try {
    await api('/api/layouts?id=' + encodeURIComponent(id), 'PUT', j);
    current = id;
    await loadList();
    msg('Saved "' + (j.name || id) + '". It is now in the widget list.');
  } catch (e) { msg(e.message); }
  $('save').disabled = false;
}

let delArmed = false;
async function del() {
  if (!current) return;
  if (!delArmed) {
    delArmed = true;
    $('del').textContent = 'Tap again to delete';
    setTimeout(() => { delArmed = false; $('del').textContent = 'Delete'; }, 4000);
    return;
  }
  delArmed = false;
  $('del').textContent = 'Delete';
  try {
    await api('/api/layouts?id=' + encodeURIComponent(current), 'DELETE');
    msg('Deleted.');
    current = '';
    await loadList();
    open('');
  } catch (e) { msg(e.message); }
}

// ---------- keys panel ----------
let keySig = '';
function renderKeys() {
  const keys = Object.keys(data);
  const sig = keys.join('|');
  if (sig !== keySig) {
    keySig = sig;
    const box = $('keys');
    box.innerHTML = '';
    keys.forEach(k => {
      const b = document.createElement('button');
      b.innerHTML = esc(k) + '<span data-k="' + esc(k) + '"></span>';
      b.onclick = () => insert('{' + k + '}');
      box.appendChild(b);
    });
  }
  document.querySelectorAll('#keys span').forEach(s => { s.textContent = data[s.dataset.k]; });
}

function insert(text) {
  const t = $('src');
  t.setRangeText(text, t.selectionStart, t.selectionEnd, 'end');
  t.focus();
  t.dispatchEvent(new Event('input'));
}

async function pollData() {
  try {
    const r = await fetch('/api/layouts/data');
    data = await r.json();
    renderKeys();
    render();
  } catch (e) {}
}

// ---------- wiring ----------
$('src').addEventListener('input', () => {
  clearTimeout(renderTimer);
  renderTimer = setTimeout(render, 120);
  if ($('live').checked) {
    clearTimeout(liveTimer);
    liveTimer = setTimeout(pushPreview, 600);
  }
});

$('which').addEventListener('change', () => open($('which').value));

$('tpl').addEventListener('change', () => {
  const t = TEMPLATES[$('tpl').value];
  if (t) { setSource(t.json); if (!current && !$('id').value) $('id').value = t.id || slug(t.json.name); }
  $('tpl').value = '';
  if ($('live').checked) pushPreview();
});

$('live').addEventListener('change', async () => {
  if ($('live').checked) {
    if (await pushPreview()) keepAlive = setInterval(pushPreview, 30000);
    else $('live').checked = false;
  } else {
    stopPreview();
  }
});

$('run').onclick = async () => { if (await pushPreview()) msg('Showing on the device for 60 seconds.'); };
$('save').onclick = save;
$('del').onclick = del;
window.addEventListener('beforeunload', () => { if ($('live').checked) navigator.sendBeacon && stopPreview(); });

(async () => {
  await loadTemplates();
  await pollData();
  setInterval(pollData, 2000);
  try { await loadList(); } catch (e) { msg('Failed to load widget list: ' + e.message); }
  const want = new URLSearchParams(location.search).get('id') || '';
  open(layouts.some(l => l.id === want) ? want : '');
})();
</script>
</body>
</html>
)html";
