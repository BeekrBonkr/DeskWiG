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
<script src="/cm.js"></script>
<style>
body{max-width:1260px}
h2{margin:6px 0 10px}
/* Toolbar: what you are editing and what to do with it, always in one place. */
.top{display:flex;flex-wrap:wrap;gap:8px;align-items:center;margin:0 0 8px}
.top select,.top input{width:auto;margin:0;padding:8px 10px;font-size:14px}
.top #which{min-width:180px;max-width:280px}
.top #id{width:220px}
.top #tpl{max-width:200px}
.top button{width:auto;margin:0;padding:8px 12px;font-size:14px;white-space:nowrap}
.top .sep{flex:1 0 4px}
.top label.inline{margin:0;font-size:13px;color:#bbb}
#msg{margin:0 0 6px;min-height:1.4em}
/* Two columns: code on the left, preview and Design on the right. The right
   column sticks to the viewport so the preview stays in view while the
   code scrolls; it scrolls on its own when the inspector gets long. */
.cols{display:flex;gap:20px;align-items:flex-start}
.col.code{flex:1 1 460px;min-width:0}
.col.side{flex:0 0 400px;max-width:100%;position:sticky;top:8px;max-height:calc(100vh - 16px);overflow:auto;padding-right:4px}
.col.side>h3:first-child{margin-top:4px}
.preview{position:sticky;top:0;z-index:2;background:#111;padding-bottom:6px}
.preview .led{display:flex;align-items:center;gap:8px;font-size:13px;color:#999;margin:6px 0 0}
#ledDot{display:inline-block;width:12px;height:12px;border-radius:50%;background:#000;border:1px solid #444}
/* The code editor fills a good share of the screen and has a drag handle
   at its bottom-right corner; the height is remembered per browser. */
#cm{resize:vertical;overflow:hidden;height:62vh;min-height:160px;margin:8px 0}
#cm .cm-editor{height:100%}
#cm .cm-scroller{overflow:auto}
/* The preview is sized by the window height so the Design panel below it
   still gets room on a laptop screen; 170:320 is the panel's aspect. */
.cvwrap{position:relative;width:clamp(170px,calc((100vh - 330px) * 0.53125),340px);max-width:100%}
.cvwrap canvas{display:block;width:100%}
#ov{position:absolute;left:0;top:0;pointer-events:none;background:transparent;border-color:transparent}
#cv{touch-action:none;cursor:crosshair}
.dtools{display:flex;gap:6px;flex-wrap:wrap;align-items:center;margin:8px 0}
.dtools select{width:auto;margin:0;padding:6px 8px;font-size:14px}
.dtools button{width:auto;margin:0;padding:6px 10px;font-size:14px}
.dtools .gap{flex:1 0 4px}
.tree{background:#151515;border:1px solid #444;border-radius:6px;max-height:220px;overflow:auto;font:13px ui-monospace,monospace;user-select:none}
.trow{padding:4px 8px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis;border-top:1px solid #222;cursor:pointer}
.trow.root{color:#9cf;border-top:none}
.trow.sel{background:#1e3a52;color:#fff}
.trow:hover{background:#222}
.trow.dragging{opacity:.4}
.trow.drop-before{box-shadow:inset 0 2px 0 #3c3}
.trow.drop-after{box-shadow:inset 0 -2px 0 #3c3}
.trow.drop-into{outline:2px solid #3c3;outline-offset:-2px}
.insp{background:#151515;border:1px solid #444;border-radius:6px;padding:8px;margin-top:8px}
.ihead{font-size:12px;color:#999;text-transform:uppercase;letter-spacing:.08em;margin:6px 0 8px}
.ihead.small{margin-top:12px;color:#7a9}
.irow{display:grid;grid-template-columns:110px 1fr;gap:6px;align-items:center;margin:4px 0}
.irow label{font-size:13px;color:#bbb}
.irow input,.irow select{margin:0;padding:6px 8px;font-size:14px;width:100%}
.irow input[type=checkbox]{width:auto;justify-self:start}
.irow .ihint{grid-column:2;font-size:11px;color:#777}
.colorctl{display:flex;gap:6px}
.colorctl input[type=color]{width:40px;padding:0;height:34px;flex:0 0 auto}
.lrule{border-top:1px solid #333;padding-top:4px;margin-top:6px}
.lrule button{margin:6px 0 0;width:auto;padding:6px 10px;font-size:13px}
.insp>button{width:auto;padding:8px 12px;font-size:14px;margin:6px 6px 0 0;display:inline-block}
select{display:block;width:100%;margin:8px 0;padding:10px;font-size:16px;background:#222;color:#eee;border:1px solid #444;border-radius:6px;box-sizing:border-box}
textarea{display:block;width:100%;min-height:380px;margin:8px 0;padding:10px;font:13px/1.45 ui-monospace,SFMono-Regular,Menlo,Consolas,monospace;background:#222;color:#eee;border:1px solid #444;border-radius:6px;box-sizing:border-box;tab-size:2;white-space:pre;overflow-x:auto}
canvas{display:block;width:340px;max-width:100%;image-rendering:pixelated;image-rendering:crisp-edges;border:1px solid #333;border-radius:4px;background:#000}
.keys{display:flex;flex-wrap:wrap;gap:6px}
.keys button{display:inline-block;width:auto;margin:0;padding:4px 8px;font:12px ui-monospace,monospace;text-align:left}
.keys button span{color:#999;margin-left:6px}
.err{color:#f66;min-height:1.4em;font:13px ui-monospace,monospace;white-space:pre-wrap;margin:0 0 8px}
label.inline{display:flex;align-items:center;gap:8px;font-size:15px;margin:8px 0}
label.inline input{width:auto;display:inline;margin:0}
pre{font:12px/1.5 ui-monospace,monospace;color:#bbb;background:#1a1a1a;border:1px solid #333;border-radius:6px;padding:10px;overflow-x:auto;margin:6px 0}
/* Keys and Reference fold away under the code so they are there when needed. */
details{margin:10px 0;border-top:1px solid #333;padding-top:6px}
summary{cursor:pointer;font-size:12px;color:#999;text-transform:uppercase;letter-spacing:.08em;padding:4px 0;user-select:none;list-style:none}
summary::before{content:"\25B8";display:inline-block;width:14px;color:#666}
details[open]>summary::before{content:"\25BE"}
details>.hint{margin-top:0}
@media (max-width:900px){
  .cols{flex-wrap:wrap}
  .col.side{position:static;max-height:none;overflow:visible;flex:1 1 320px}
  .preview{position:static}
  .cvwrap{width:min(340px,100%)}
  #cm{height:50vh}
}
</style>
</head>
<body>
<nav><a href="/setup">Setup</a><a href="/widgets">Widgets</a><a href="/editor">Editor</a><a href="/terminal">Terminal</a><a class="gh" href="https://github.com/BeekrBonkr/DeskWiG" target="_blank" rel="noopener" title="Project page and README on GitHub">GitHub</a></nav>
<h2>Widget Editor</h2>

<div class="top">
  <select id="which" title="Which widget to edit"></select>
  <input id="id" placeholder="id (lowercase letters, digits, dashes)" autocapitalize="off" autocorrect="off" maxlength="24" title="Widget id: used in the file name and the widget list">
  <select id="tpl" title="Replace the code with a built-in template"><option value="">Insert template&hellip;</option></select>
  <span class="sep"></span>
  <button id="run" type="button" title="Show this layout on the device for 60 seconds without saving">Show on device</button>
  <button id="fmt" type="button" title="Reformat the JSON">Format</button>
  <button id="save" type="button" class="primary" title="Ctrl+S">Save</button>
  <button id="del" type="button" class="danger" title="Delete this widget from the device">Delete</button>
</div>
<div class="top">
  <label class="inline"><input type="checkbox" id="live"> Live preview on the device while typing</label>
  <span class="sep"></span>
  <span class="hint">Undo and redo work anywhere on the page: Ctrl+Z, Ctrl+Y or Ctrl+Shift+Z. Need a starting point? The <a href="https://github.com/BeekrBonkr/DeskWiG/tree/main/examples/widgets" target="_blank" rel="noopener">example widgets on GitHub</a> paste straight in.</span>
</div>
<p id="msg"></p>

<div class="cols">
<div class="col code">
  <div id="cm"></div>
  <textarea id="src" spellcheck="false"></textarea>
  <p id="err" class="err"></p>

  <details id="keysBox">
    <summary>Keys</summary>
    <p class="hint">Tap to insert at the cursor. <code>ping.N</code> also accepts the target name, e.g. <code>{ping.router.ms}</code>. <code>api.*</code> keys come from the data sources on the <a href="/setup">setup page</a>.</p>
    <div class="keys" id="keys"></div>
  </details>
  <details id="refBox">
    <summary>Reference</summary>
<pre>screen 170 x 320, black background
size 1 = 6x8 px per char (28 cols)
size 2 = 14 cols, size 4 = 7 cols

elements flow top to bottom like HTML;
give x AND y to place one absolutely.

text      text  (w h optional)
line      w h | x2 y2 (absolute)
rect      w h  fill | style.bg
bar       w h  value (0-100)
box       children[] + style:
          direction row|column, gap,
          padding, align, justify
circle    w (or r)    ellipse  w h
arc       w value start end thickness
triangle  points [[x,y] x3]
polygon   points [[x,y] ...max 8]
image     src (name or URL) w h
          style.fit contain|cover|stretch
          refresh (s) for URLs
box       style.image = background

style: {font, size, color, background,
  border, borderWidth, radius, align,
  fill, thickness, gap, padding,
  direction, justify, position}
font: sans | bold | emoji | uploaded;
  size is then pixels (6-160).
  no font = 6x8 bitmap, size 1-40;
  emoji work in both.
"styles": {"name": {...}} then
  "class": "name" on an element
align: left | center | right | stretch
color: bg text dim ok warn bad accent
       #rrggbb, or {ping.0.color}
text and value take {keys}
api.&lt;source&gt;.&lt;field&gt; plus .status
  .color .age .updated .error

math in braces, keys as variables:
  {round(api.weather.temp * 9/5 + 32, 1)}
  {min(ping.0.ms / 2, 100)}
  + - * / % ^ ( )  round(x,n) abs
  min max floor ceil sqrt clamp(x,lo,hi)
  &lt; &gt; &lt;= &gt;= == != &amp;&amp; || !  if(c,a,b)

"led": {"color":"{ping.0.color}",
  "mode":"solid|breathe|blink|pulse|
          rainbow|off", "speed":ms,
  "brightness":0-100, "rules":[
   {"when":"ping.0.ms > 100","mode":"blink"},
   {"key":"ping.0.status","is":"down",
    "color":"bad"}]}
no "led" = LED off for this widget</pre>
  </details>
</div>

<div class="col side">
  <div class="preview">
    <div class="cvwrap"><canvas id="cv" width="340" height="640"></canvas><canvas id="ov" width="340" height="640"></canvas></div>
    <p class="led"><span id="ledDot"></span><span id="ledText">LED: off</span><span class="sep"></span><span>Click an element to edit it; drag positioned ones.</span></p>
  </div>
  <h3>Design</h3>
  <div class="dtools">
    <select id="addType" title="Add an element after the selected one"><option value="">Add element…</option><option>text</option><option>box</option><option>line</option><option>rect</option><option>bar</option><option>circle</option><option>ellipse</option><option>arc</option><option>triangle</option><option>polygon</option><option>image</option></select>
    <button id="elDup" type="button" title="Duplicate the selected element">Duplicate</button>
    <button id="elUp" type="button" title="Move up">&uarr;</button>
    <button id="elDown" type="button" title="Move down">&darr;</button>
    <button id="elDel" type="button" class="danger" title="Delete the selected element">Delete</button>
    <span class="gap"></span>
    <button id="undo" type="button" title="Undo (Ctrl+Z)">Undo</button>
    <button id="redo" type="button" title="Redo (Ctrl+Y)">Redo</button>
  </div>
  <div class="tree" id="tree" title="Drag rows to reorder or drop them into a box"></div>
  <div class="insp" id="insp"></div>
</div>
</div>

<script>
const $ = id => document.getElementById(id);
const esc = s => String(s).replace(/[&<>"]/g, c => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]));
const msg = t => { $('msg').textContent = t; };

// Every page starts by checking the login; unauthenticated browsers go to /login.
const toLogin = () => { location.replace('/login?next=' + encodeURIComponent(location.pathname)); };
let auth = null;
async function requireLogin() {
  try { auth = await (await fetch('/api/auth')).json(); } catch (e) { return null; }
  if (!auth.loggedIn) { toLogin(); return null; }
  return auth;
}
requireLogin();

async function api(path, method, body) {
  const r = await fetch(path, {
    method: method || 'GET',
    headers: { 'Content-Type': 'application/json' },
    body: body !== undefined ? JSON.stringify(body) : undefined
  });
  if (r.status === 401) { toLogin(); throw new Error('Not logged in.'); }
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

loadFonts();
loadImageList();

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
function isExpr(b) { return /[()+\-*\/%^ <>=!&|]/.test(b) || /^[0-9]/.test(b); }
function evalNumber(body) { const r = evalExpr(body); return r === '--' ? null : parseFloat(r); }

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
      case 'if': { if (n !== 3) return bad(); return a[0].v !== 0 ? a[1] : a[2]; }
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
    if (s[p] === '!' && s[p + 1] !== '=') { p++; const r = unary(); return { v: r.v === 0 ? 1 : 0, d: 0 }; }
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
  function sum() {
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
  function cmp() {
    const r = sum();
    skip();
    const two = s.substr(p, 2), one = s[p];
    let op = null;
    if (['<=', '>=', '==', '!='].includes(two)) { op = two; p += 2; }
    else if (one === '<' || one === '>') { op = one; p += 1; }
    if (!op) return r;
    const b = sum();
    if (err) return r;
    const t = op === '<' ? r.v < b.v : op === '>' ? r.v > b.v : op === '<=' ? r.v <= b.v : op === '>=' ? r.v >= b.v : op === '==' ? r.v === b.v : r.v !== b.v;
    return { v: t ? 1 : 0, d: 0 };
  }
  function andExpr() {
    let r = cmp();
    for (;;) { skip(); if (s.substr(p, 2) !== '&&') return r; p += 2; const b = cmp(); if (err) return r; r = { v: (r.v !== 0 && b.v !== 0) ? 1 : 0, d: 0 }; }
  }
  function expr() {
    let r = andExpr();
    for (;;) { skip(); if (s.substr(p, 2) !== '||') return r; p += 2; const b = andExpr(); if (err) return r; r = { v: (r.v !== 0 || b.v !== 0) ? 1 : 0, d: 0 }; }
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

const AUTO = null;
const TYPES = ['text', 'line', 'rect', 'bar', 'box', 'circle', 'ellipse', 'arc', 'triangle', 'polygon', 'image'];

// ---------- images ----------
// Stored images come from the device; URLs load directly. Animated GIFs
// show their first frame in the preview.
let imageNames = [];
const imgCache = {};
async function loadImageList() {
  try { const r = await fetch('/api/images'); const j = await r.json(); imageNames = (j.images || []).map(i => i.name); } catch (e) {}
}
function getImage(src) {
  if (imgCache[src]) return imgCache[src];
  const im = new Image();
  im.onload = () => render();
  im.onerror = () => { im.failed = true; };
  im.src = /^https?:\/\//.test(src) ? src : '/img/' + src;
  imgCache[src] = im;
  return im;
}
function imageOk(src) { return /^https?:\/\//.test(src) ? src.length <= 159 : imageNames.includes(src); }
function fitRect(fit, iw, ih, w, h) {
  if (fit === 'stretch') return { x: 0, y: 0, w, h };
  const sx = w / iw, sy = h / ih;
  const sc = fit === 'contain' ? Math.min(sx, sy) : Math.max(sx, sy);
  const dw = Math.max(1, Math.round(iw * sc)), dh = Math.max(1, Math.round(ih * sc));
  return { x: Math.trunc((w - dw) / 2), y: Math.trunc((h - dh) / 2), w: dw, h: dh };
}
function drawImageFit(src, x, y, w, h, fit) {
  const im = getImage(src);
  if (!im.complete || im.failed || !im.naturalWidth) { if (w > 8 && h > 8) strokeRectR(x, y, w, h, 0, COLORS.dim); return; }
  const r = fitRect(fit, im.naturalWidth, im.naturalHeight, w, h);
  ctx.save();
  ctx.beginPath(); ctx.rect(x * S, y * S, w * S, h * S); ctx.clip();
  ctx.imageSmoothingEnabled = false;
  ctx.drawImage(im, (x + r.x) * S, (y + r.y) * S, r.w * S, r.h * S);
  ctx.restore();
}

// Returns the CSS colour for a spec. present=false when the spec is empty.
function color(spec, fallback) {
  const raw = String(spec ?? '');
  if (!raw) return { c: fallback, present: false };
  let n = raw;
  const templated = n.includes('{');
  let fb = fallback;
  if (templated) { n = expand(n); fb = COLORS.dim; }
  if (COLORS[n]) return { c: COLORS[n], present: true };
  if (/^#[0-9a-f]{6}$/i.test(n)) return { c: n, present: true };
  return { c: fb, present: true };
}

// Mirrors fail() on the device: element paths get an "element" prefix, styles, led and root do not.
const at = path => (/^(style|led|root)/.test(path) ? path : 'element ' + path);

function checkColor(spec, path, what) {
  const c = String(spec ?? '');
  if (c && !c.includes('{') && !COLORS[c] && !/^#[0-9a-f]{6}$/i.test(c)) throw new Error(at(path) + ': unknown ' + what + ' (use a role name or #rrggbb)');
}

function defaultStyle() {
  return { font: '', image: '', fit: 'contain', color: 'text', bg: '', border: '', size: 1, borderW: 0, radius: 0, pad: 0, gap: 0, thick: 0,
           align: 'stretch', justify: 'start', row: false, fill: false, absolute: false };
}

const ALIGNS = { left: 'start', start: 'start', top: 'start', center: 'center', right: 'end', end: 'end', bottom: 'end', stretch: 'stretch' };

function parseStyle(st, obj, path) {
  for (const k of Object.keys(obj)) {
    const v = obj[k];
    switch (k) {
      case 'color': checkColor(v, path, 'color'); st.color = String(v ?? 'text'); break;
      case 'background': case 'bg': checkColor(v, path, 'background'); st.bg = String(v ?? ''); break;
      case 'border': checkColor(v, path, 'border'); st.border = String(v ?? ''); if (st.border && !st.borderW) st.borderW = 1; break;
      case 'borderWidth': st.borderW = Math.max(0, Math.min(20, v | 0)); break;
      case 'radius': st.radius = Math.max(0, Math.min(80, v | 0)); break;
      case 'padding': st.pad = Math.max(0, Math.min(80, v | 0)); break;
      case 'gap': st.gap = Math.max(0, Math.min(200, v | 0)); break;
      case 'thickness': case 'width': st.thick = Math.max(1, Math.min(80, v | 0)); break;
      case 'size': if (!Number.isInteger(v) || v < 1 || v > 160) throw new Error(at(path) + ': size must be 1-40 (bitmap font) or 6-160 (TrueType font)'); st.size = v; break;
      case 'font': if (v && !fontNames.includes(v)) throw new Error(at(path) + ': unknown font (see the setup page for the list)'); st.font = String(v ?? ''); break;
      case 'align': if (!ALIGNS[v]) throw new Error(at(path) + ': align must be left/start, center, right/end or stretch'); st.align = ALIGNS[v]; break;
      case 'justify': if (!['start', 'center', 'end', 'between'].includes(v)) throw new Error(at(path) + ': justify must be start, center, end or between'); st.justify = v; break;
      case 'direction': if (v !== 'row' && v !== 'column') throw new Error(at(path) + ': direction must be column or row'); st.row = v === 'row'; break;
      case 'fill': st.fill = !!v; break;
      case 'position': st.absolute = v === 'absolute'; break;
      case 'fit': if (!['contain', 'cover', 'stretch'].includes(v)) throw new Error(at(path) + ': fit must be contain, cover or stretch'); st.fit = v; break;
      case 'image': if (v && !imageOk(String(v))) throw new Error(at(path) + ': unknown image (upload it on the setup page, or use an http(s) URL)'); st.image = String(v ?? ''); break;
      default: throw new Error(at(path) + ': unknown style property "' + String(k).slice(0, 20) + '"');
    }
  }
}

let nodeCount = 0;

function buildNode(e, path, styles, depth) {
  if (!e || typeof e !== 'object' || Array.isArray(e)) throw new Error(at(path) + ': must be an object');
  if (++nodeCount > 63) throw new Error('too many elements (max 63 including nested)');
  if (depth > 6) throw new Error(at(path) + ': nested too deep');
  if (!TYPES.includes(e.type)) throw new Error(at(path) + ': unknown type (' + TYPES.join(', ') + ')');
  const n = { type: e.type, x: AUTO, y: AUTO, w: AUTO, h: AUTO, x2: AUTO, y2: AUTO, a0: 0, a1: 360, pts: [], text: '', children: [], hasXY: false, el: e, path: path };
  const st = defaultStyle();
  const shape = ['circle', 'ellipse', 'triangle', 'polygon'].includes(e.type);
  if (shape) st.fill = true;
  if (e.type === 'arc') { st.thick = 8; st.bg = 'dim'; }
  if (e.type === 'line') st.thick = 1;

  const cls = e.class ?? (typeof e.style === 'string' ? e.style : '');
  if (cls) {
    if (!styles[cls]) throw new Error(at(path) + ': unknown class (define it in "styles")');
    const m = Object.assign({}, styles[cls]);
    m.fill = styles[cls].fill || st.fill;
    if (e.type === 'arc' && !m.thick) m.thick = 8;
    if (e.type === 'arc' && !m.bg) m.bg = 'dim';
    if (e.type === 'line' && !m.thick) m.thick = 1;
    Object.assign(st, m);
  }
  if (e.color !== undefined) { checkColor(e.color, path, 'color'); st.color = String(e.color); }
  if (e.size !== undefined) { if (!Number.isInteger(e.size) || e.size < 1 || e.size > 160) throw new Error(at(path) + ': size must be 1-40 (bitmap font) or 6-160 (TrueType font)'); st.size = e.size; }
  if (e.font !== undefined) { if (e.font && !fontNames.includes(e.font)) throw new Error(at(path) + ': unknown font (see the setup page for the list)'); st.font = String(e.font ?? ''); }
  if (e.align !== undefined) { if (!ALIGNS[e.align]) throw new Error(at(path) + ': align must be left, center or right'); st.align = ALIGNS[e.align]; }
  if (e.fill !== undefined) st.fill = !!e.fill;
  if (e.style && typeof e.style === 'object') parseStyle(st, e.style, path);
  if (e.type === 'text') {
    if (!st.font && st.size > 40) throw new Error(at(path) + ': size must be 1-40 with the bitmap font; set "font" for pixel sizes');
    if (st.font && st.size < 6) throw new Error(at(path) + ': size must be at least 6 with a TrueType font');
  }
  n.st = st;

  if (e.x !== undefined) n.x = e.x | 0;
  if (e.y !== undefined) n.y = e.y | 0;
  if (e.w !== undefined) n.w = e.w | 0;
  if (e.h !== undefined) n.h = e.h | 0;
  if (e.r !== undefined) n.w = n.h = (e.r | 0) * 2;
  if (e.x2 !== undefined) n.x2 = e.x2 | 0;
  if (e.y2 !== undefined) n.y2 = e.y2 | 0;
  n.hasXY = (n.x !== AUTO && n.y !== AUTO) || st.absolute;
  if (n.hasXY) { if (n.x === AUTO) n.x = 0; if (n.y === AUTO) n.y = 0; }
  if (e.type === 'arc') { n.a0 = e.start ?? 0; n.a1 = e.end ?? 360; }
  if (e.type === 'triangle' || e.type === 'polygon') {
    if (!Array.isArray(e.points)) throw new Error(at(path) + ': "points" must be an array of [x,y] pairs');
    if (e.points.length > 8) throw new Error(at(path) + ': too many points (max 8)');
    for (const p of e.points) {
      if (!Array.isArray(p) || p.length !== 2) throw new Error(at(path) + ': each point must be [x,y]');
      n.pts.push([p[0] | 0, p[1] | 0]);
    }
    if (e.type === 'triangle' && n.pts.length !== 3) throw new Error(at(path) + ': triangle needs exactly 3 points');
    if (n.pts.length < 3) throw new Error(at(path) + ': polygon needs at least 3 points');
  }
  if (e.type === 'image') {
    const src = String(e.src ?? '');
    if (!src) throw new Error(at(path) + ': image needs "src": an uploaded image name or an http(s) URL');
    if (src.length > 159) throw new Error(at(path) + ': image source too long');
    if (!imageOk(src)) throw new Error(at(path) + ': unknown image (upload it on the setup page, or use an http(s) URL)');
    n.src = src;
  }
  let text = '';
  if (e.type === 'bar' || e.type === 'arc') text = String(e.value ?? '0');
  else if (e.type === 'text') text = String(e.text ?? '');
  if (text.length > 63) throw new Error(at(path) + ': text longer than 63 characters');
  n.text = text;

  if (e.children !== undefined) {
    if (e.type !== 'box') throw new Error(at(path) + ': only a box can have children');
    if (!Array.isArray(e.children)) throw new Error(at(path) + ': "children" must be an array');
    e.children.forEach((c, i) => n.children.push(buildNode(c, path + '.' + i, styles, depth + 1)));
  }
  return n;
}

function parse() { return parseText(srcGet()); }
function parseText(text) { return validate(JSON.parse(text)); }

// Validates a layout object and attaches its render tree as a
// non-enumerable _root, so the object still serialises cleanly.
function validate(j) {
  if (!j || typeof j !== 'object' || Array.isArray(j)) throw new Error('layout must be a JSON object');
  if (!Array.isArray(j.elements)) throw new Error('"elements" must be an array');
  const styles = {};
  if (j.styles !== undefined) {
    if (!j.styles || typeof j.styles !== 'object' || Array.isArray(j.styles)) throw new Error('"styles" must be an object');
    const names = Object.keys(j.styles);
    if (names.length > 8) throw new Error('too many styles (max 8)');
    for (const name of names) {
      if (!j.styles[name] || typeof j.styles[name] !== 'object') throw new Error('style "' + name + '": must be an object');
      const st = defaultStyle();
      parseStyle(st, j.styles[name], 'style "' + name + '"');
      styles[name] = st;
    }
  }
  if (j.led !== undefined) parseLed(j.led);
  nodeCount = 0;
  const root = { type: 'box', x: 0, y: 0, w: 170, h: 320, x2: AUTO, y2: AUTO, a0: 0, a1: 360, pts: [], text: '', children: [], hasXY: true, st: defaultStyle(), el: j, path: 'root' };
  if (j.style && typeof j.style === 'object') parseStyle(root.st, j.style, 'root');
  j.elements.forEach((e, i) => root.children.push(buildNode(e, String(i), styles, 1)));
  Object.defineProperty(j, '_root', { value: root, enumerable: false, configurable: true, writable: true });
  return j;
}

// ---------- fonts ----------
// Device fonts are served from /fonts/<name>.ttf, so the preview draws
// with the same files. The device scales a font so ascent-descent equals
// the size; canvas sizes by em, so each font gets a correction ratio.
let fontNames = ['sans', 'bold', 'emoji'];
const fontRatio = {};
const mctx = document.createElement('canvas').getContext('2d');

async function loadFonts() {
  try {
    const r = await fetch('/api/fonts');
    const j = await r.json();
    if (Array.isArray(j.fonts) && j.fonts.length) fontNames = j.fonts.map(f => f.name);
  } catch (e) {}
  // One at a time: the device serves large files more reliably that way.
  for (const n of fontNames) {
    for (let attempt = 0; attempt < 2 && !fontRatio[n]; attempt++) {
      try {
        const ff = new FontFace('dw-' + n, 'url(/fonts/' + n + '.ttf)');
        await ff.load();
        document.fonts.add(ff);
        mctx.font = '100px "dw-' + n + '"';
        const m = mctx.measureText('Hg');
        const box = (m.fontBoundingBoxAscent || 80) + (m.fontBoundingBoxDescent || 20);
        fontRatio[n] = { r: 100 / box, asc: (m.fontBoundingBoxAscent || 80) / box };
        render();
      } catch (e) {}
    }
  }
}

function canvasFont(name, px) {
  const fr = fontRatio[name] || { r: 1, asc: 0.8 };
  return { font: Math.round(px * fr.r * 100) / 100 + 'px "dw-' + name + '", "dw-emoji", "dw-sans", sans-serif', ascent: Math.round(px * fr.asc) };
}

// The device draws emoji from the monochrome font. Chrome would swap in
// its colour emoji font for emoji-presentation characters, so the text
// presentation selector (U+FE0E) is appended to keep the preview honest.
function monoEmoji(s) {
  let out = '';
  for (const ch of s) {
    const cp = ch.codePointAt(0);
    if (cp === 0xFE0F) continue;
    out += ch;
    if (cp >= 0x2190) out += '\uFE0E';
  }
  return out;
}

function ttfWidth(name, s, px) {
  mctx.font = canvasFont(name, px).font;
  return Math.round(mctx.measureText(monoEmoji(s)).width);
}

// ---------- led (mirrors LayoutWidget::parseLed) ----------
const LED_MODES = ['off', 'solid', 'breathe', 'blink', 'pulse', 'rainbow'];
const LED_RGB = { ok: '#00ff00', warn: '#ff7800', bad: '#ff0000', accent: '#0078ff', text: '#ffffff', dim: '#282828', bg: '#000000' };
function parseLedRule(o, path) {
  if (!o || typeof o !== 'object' || Array.isArray(o)) throw new Error(path + ': must be an object');
  const r = {};
  for (const k of Object.keys(o)) {
    const v = o[k];
    switch (k) {
      case 'when': if (String(v).length > 63) throw new Error(path + ': "when" too long'); r.when = String(v); break;
      case 'key': if (String(v).length > 39) throw new Error(path + ': "key" too long'); r.key = String(v); break;
      case 'is': if (String(v).length > 23) throw new Error(path + ': "is" too long'); r.is = String(v); break;
      case 'color': checkColor(v, path, 'color'); r.color = String(v ?? ''); break;
      case 'mode': if (!LED_MODES.includes(v)) throw new Error(path + ': mode must be off, solid, breathe, blink, pulse or rainbow'); r.mode = v; break;
      case 'speed': r.speed = Math.max(100, Math.min(60000, v | 0)); break;
      case 'brightness': r.brightness = Math.max(0, Math.min(100, v | 0)); break;
      case 'rules': break;
      default: throw new Error(path + ': unknown led property "' + String(k).slice(0, 20) + '"');
    }
  }
  if (r.key && !r.is) throw new Error(path + ': "key" needs "is"');
  return r;
}
function parseLed(led) {
  if (!led || typeof led !== 'object' || Array.isArray(led)) throw new Error('"led" must be an object');
  const base = parseLedRule(led, 'led');
  if (!base.mode) base.mode = 'solid';
  if (!base.speed) base.speed = 2000;
  const rules = [];
  if (led.rules !== undefined) {
    if (!Array.isArray(led.rules)) throw new Error('led: "rules" must be an array');
    if (led.rules.length > 6) throw new Error('led: too many rules (max 6)');
    led.rules.forEach((rv, i) => {
      const r = parseLedRule(rv, 'led rule ' + i);
      if (!r.when && !r.key) throw new Error('led rule ' + i + ': needs "when" or "key"/"is"');
      if (r.color === undefined && !r.mode && !r.speed && r.brightness === undefined) throw new Error('led rule ' + i + ': sets nothing');
      rules.push(r);
    });
  }
  return { base, rules };
}
function ledState(cfg) {
  let pick = null;
  for (const r of cfg.rules) {
    let m = false;
    if (r.key) { const v = lookup(r.key, true); m = v !== undefined && String(v).toLowerCase() === r.is.toLowerCase(); }
    else { const v = evalNumber(r.when); m = v !== null && v !== 0; }
    if (m) { pick = r; break; }
  }
  const b = cfg.base;
  const color = pick && pick.color !== undefined ? pick.color : (b.color ?? '');
  const mode = pick && pick.mode ? pick.mode : b.mode;
  const speed = pick && pick.speed ? pick.speed : b.speed;
  let n = color.includes('{') ? expand(color) : color;
  let rgb = LED_RGB[n] || (/^#[0-9a-f]{6}$/i.test(n) ? n : null);
  if (mode === 'rainbow') return { rgb: '#ff40ff', mode, speed };
  return { rgb: rgb || '#000000', mode: rgb ? mode : 'off', speed };
}

// ---------- layout (mirrors LayoutWidget.cpp) ----------
const isAscii = ch => ch.charCodeAt(0) < 128;

// Bitmap font width; non-ASCII characters (emoji) come from the emoji font at the same line height.
function bitmapW(s, size) {
  let w = 0;
  for (const ch of s) w += isAscii(ch) ? 6 * size : ttfWidth('emoji', ch, 8 * size);
  return w;
}

const textW = (st, s) => st.font ? ttfWidth(st.font, s, st.size) : bitmapW(s, st.size);
const textH = st => st.font ? st.size : 8 * st.size;

function measure(n) {
  let w = AUTO, h = AUTO;
  switch (n.type) {
    case 'text': n.txt = expand(n.text); w = textW(n.st, n.txt); h = textH(n.st); break;
    case 'line':
      if (n.hasXY && n.x2 !== AUTO) { w = Math.abs(n.x2 - n.x) + 1; h = (n.y2 === AUTO ? 0 : Math.abs(n.y2 - n.y)) + 1; }
      else h = n.st.thick;
      break;
    case 'bar': n.txt = expand(n.text); h = 8; break;
    case 'arc': n.txt = expand(n.text); // fall through
    case 'circle':
      if (n.w !== AUTO && n.h === AUTO) h = n.w;
      if (n.h !== AUTO && n.w === AUTO) w = n.h;
      break;
    case 'image': {
      const im = getImage(n.src);
      if (im.complete && im.naturalWidth) {
        const iw = im.naturalWidth, ih = im.naturalHeight;
        if (n.w !== AUTO && n.h === AUTO) h = Math.trunc(n.w * ih / iw);
        else if (n.h !== AUTO && n.w === AUTO) w = Math.trunc(n.h * iw / ih);
        else { w = iw; h = ih; }
      }
      break;
    }
    case 'triangle': case 'polygon': {
      let mx = 0, my = 0;
      for (const p of n.pts) { mx = Math.max(mx, p[0]); my = Math.max(my, p[1]); }
      w = mx + 1; h = my + 1;
      break;
    }
    case 'box': {
      let main = 0, cross = 0, cnt = 0;
      for (const c of n.children) {
        measure(c);
        if (c.hasXY) continue;
        const cw = c.lw === AUTO ? 0 : c.lw, chh = c.lh === AUTO ? 0 : c.lh;
        main += n.st.row ? cw : chh;
        cross = Math.max(cross, n.st.row ? chh : cw);
        cnt++;
      }
      if (cnt > 1) main += n.st.gap * (cnt - 1);
      main += 2 * n.st.pad; cross += 2 * n.st.pad;
      w = n.st.row ? main : cross; h = n.st.row ? cross : main;
      break;
    }
  }
  n.lw = n.w !== AUTO ? n.w : w;
  n.lh = n.h !== AUTO ? n.h : h;
}

function place(n, x, y, w, h) {
  n.lx = x; n.ly = y; n.lw = w; n.lh = h;
  if (n.type !== 'box') return;
  const cx = x + n.st.pad, cy = y + n.st.pad;
  const cw = Math.max(0, w - 2 * n.st.pad), ch = Math.max(0, h - 2 * n.st.pad);
  const mainAvail = n.st.row ? cw : ch, crossAvail = n.st.row ? ch : cw;
  let total = 0, cnt = 0;
  for (const k of n.children) {
    if (k.hasXY) continue;
    const m = n.st.row ? k.lw : k.lh;
    total += m === AUTO ? 0 : m; cnt++;
  }
  let gap = n.st.gap;
  if (cnt > 1) total += gap * (cnt - 1);
  const free = mainAvail - total;
  let offset = 0;
  if (free > 0) {
    if (n.st.justify === 'center') offset = Math.trunc(free / 2);
    else if (n.st.justify === 'end') offset = free;
    else if (n.st.justify === 'between' && cnt > 1) gap += Math.trunc(free / (cnt - 1));
  }
  let cursor = offset;
  for (const k of n.children) {
    if (k.hasXY) {
      const kw = k.lw === AUTO ? 0 : k.lw, kh = k.lh === AUTO ? 0 : k.lh;
      let kx = cx + k.x;
      if (k.type === 'text' && k.w === AUTO) {
        if (k.st.align === 'center') kx -= Math.trunc(kw / 2);
        else if (k.st.align === 'end') kx -= kw;
      }
      place(k, kx, cy + k.y, kw, kh);
      continue;
    }
    let mainSize = n.st.row ? k.lw : k.lh;
    if (mainSize === AUTO) mainSize = 0;
    let crossSize = n.st.row ? k.lh : k.lw;
    const stretch = n.st.align === 'stretch';
    const crossExplicit = (n.st.row ? k.h : k.w) !== AUTO;
    if (crossSize === AUTO) crossSize = stretch ? crossAvail : 0;
    else if (stretch && !crossExplicit && ['text', 'bar', 'box', 'line'].includes(k.type)) crossSize = crossAvail;
    let crossOff = 0;
    if (n.st.align === 'center') crossOff = Math.trunc((crossAvail - crossSize) / 2);
    else if (n.st.align === 'end') crossOff = crossAvail - crossSize;
    if (n.st.row) place(k, cx + cursor, cy + crossOff, mainSize, crossSize);
    else place(k, cx + crossOff, cy + cursor, crossSize, mainSize);
    cursor += mainSize + gap;
  }
}

// ---------- drawing ----------
function drawTtf(name, s, x, y, px, col) {
  const f = canvasFont(name, px);
  ctx.font = f.font;
  ctx.fillStyle = col;
  ctx.textBaseline = 'alphabetic';
  ctx.textAlign = 'left';
  ctx.save();
  ctx.scale(S, S);
  ctx.fillText(monoEmoji(s), x, y + f.ascent);
  ctx.restore();
}

function drawText(st, s, x, y, col) {
  if (st.font) { drawTtf(st.font, s, x, y, st.size, col); return; }
  const size = st.size;
  ctx.fillStyle = col;
  for (const ch of s) {
    if (!isAscii(ch)) { drawTtf('emoji', ch, x, y, 8 * size, col); x += ttfWidth('emoji', ch, 8 * size); continue; }
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

function drawLine(x1, y1, x2, y2, col, thick) {
  if (thick > 1) {
    ctx.strokeStyle = col; ctx.lineWidth = thick * S; ctx.lineCap = 'round';
    ctx.beginPath(); ctx.moveTo((x1 + 0.5) * S, (y1 + 0.5) * S); ctx.lineTo((x2 + 0.5) * S, (y2 + 0.5) * S); ctx.stroke();
    return;
  }
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

function rrPath(x, y, w, h, r) {
  r = Math.min(r, w / 2, h / 2);
  ctx.beginPath();
  ctx.moveTo((x + r) * S, y * S);
  ctx.arcTo((x + w) * S, y * S, (x + w) * S, (y + h) * S, r * S);
  ctx.arcTo((x + w) * S, (y + h) * S, x * S, (y + h) * S, r * S);
  ctx.arcTo(x * S, (y + h) * S, x * S, y * S, r * S);
  ctx.arcTo(x * S, y * S, (x + w) * S, y * S, r * S);
  ctx.closePath();
}

function fillRectR(x, y, w, h, r, col) {
  ctx.fillStyle = col;
  if (w <= 0 || h <= 0) return;
  if (!r) { ctx.fillRect(x * S, y * S, w * S, h * S); return; }
  rrPath(x, y, w, h, r); ctx.fill();
}

function strokeRectR(x, y, w, h, r, col) {
  if (w <= 0 || h <= 0) return;
  if (!r) {
    ctx.fillStyle = col;
    ctx.fillRect(x * S, y * S, w * S, S); ctx.fillRect(x * S, (y + h - 1) * S, w * S, S);
    ctx.fillRect(x * S, y * S, S, h * S); ctx.fillRect((x + w - 1) * S, y * S, S, h * S);
    return;
  }
  ctx.strokeStyle = col; ctx.lineWidth = S;
  rrPath(x + 0.5, y + 0.5, w - 1, h - 1, r); ctx.stroke();
}

function drawNode(n) {
  let { lx: x, ly: y, lw: w, lh: h } = n;
  w = Math.max(0, w); h = Math.max(0, h);
  const col = color(n.st.color, COLORS.text).c;
  const bg = color(n.st.bg, COLORS.bg);
  const border = color(n.st.border, COLORS.dim);
  const bw = border.present ? (n.st.borderW || 1) : 0;

  switch (n.type) {
    case 'box': case 'rect': {
      const fillIt = bg.present || (n.type === 'rect' && n.st.fill);
      const fillColor = bg.present ? bg.c : col;
      const outline = bw > 0 || (n.type === 'rect' && !n.st.fill && !bg.present);
      const oc = bw > 0 ? border.c : col;
      const ow = bw > 0 ? bw : 1;
      if (fillIt) fillRectR(x, y, w, h, n.st.radius, fillColor);
      if (outline) for (let k = 0; k < ow && k * 2 < w && k * 2 < h; k++) strokeRectR(x + k, y + k, w - 2 * k, h - 2 * k, Math.max(0, n.st.radius - k), oc);
      if (n.type === 'box') {
        ctx.save();
        ctx.beginPath(); ctx.rect(x * S, y * S, w * S, h * S); ctx.clip();
        if (n.st.image) drawImageFit(n.st.image, x, y, w, h, 'cover');
        for (const c of n.children) drawNode(c);
        ctx.restore();
      }
      break;
    }
    case 'image': {
      if (bg.present) fillRectR(x, y, w, h, 0, bg.c);
      drawImageFit(n.src, x, y, w, h, n.st.fit);
      break;
    }
    case 'text': {
      const s = n.txt;
      const tw = textW(n.st, s);
      let tx = x;
      if (w > tw) {
        if (n.st.align === 'center') tx = x + Math.trunc((w - tw) / 2);
        else if (n.st.align === 'end') tx = x + w - tw;
      }
      if (bg.present) fillRectR(x, y, w, h, 0, bg.c);
      drawText(n.st, s, tx, y, col);
      break;
    }
    case 'line': {
      let x1, y1;
      if (n.hasXY && n.x2 !== AUTO) { x1 = x + (n.x2 - n.x); y1 = y + (n.y2 === AUTO ? 0 : n.y2 - n.y); }
      else { x1 = x + (w > 0 ? w - 1 : 0); y1 = y + (h > 0 ? h - 1 : 0); if (h <= n.st.thick) y1 = y; }
      drawLine(x, y, x1, y1, col, n.st.thick);
      break;
    }
    case 'bar': {
      let v = parseInt(n.txt, 10); if (isNaN(v)) v = 0; v = Math.max(0, Math.min(100, v));
      if (w <= 0 || h <= 0) break;
      strokeRectR(x, y, w, h, n.st.radius, bg.present ? bg.c : COLORS.dim);
      const fw = Math.floor(Math.max(0, w - 2) * v / 100);
      if (fw > 0 && h > 2) fillRectR(x + 1, y + 1, fw, h - 2, Math.max(0, n.st.radius - 1), col);
      break;
    }
    case 'circle': case 'ellipse': {
      let rx = Math.trunc(w / 2), ry = Math.trunc(h / 2);
      if (n.type === 'circle') rx = ry = Math.trunc(Math.min(w, h) / 2);
      const mx = x + Math.trunc(w / 2), my = y + Math.trunc(h / 2);
      if (rx <= 0 || ry <= 0) break;
      const fillIt = n.st.fill || bg.present;
      if (fillIt) { ctx.fillStyle = bg.present ? bg.c : col; ctx.beginPath(); ctx.ellipse((mx + 0.5) * S, (my + 0.5) * S, (rx + 0.5) * S, (ry + 0.5) * S, 0, 0, Math.PI * 2); ctx.fill(); }
      if (bw > 0 || !fillIt) {
        ctx.strokeStyle = bw > 0 ? border.c : col; ctx.lineWidth = (bw > 0 ? bw : 1) * S;
        const inset = (bw > 0 ? bw : 1) / 2;
        ctx.beginPath(); ctx.ellipse((mx + 0.5) * S, (my + 0.5) * S, Math.max(0.5, rx + 0.5 - inset) * S, Math.max(0.5, ry + 0.5 - inset) * S, 0, 0, Math.PI * 2); ctx.stroke();
      }
      break;
    }
    case 'arc': {
      const r = Math.trunc(Math.min(w, h) / 2);
      const mx = x + Math.trunc(w / 2), my = y + Math.trunc(h / 2);
      if (r <= 1) break;
      const t = Math.min(n.st.thick, r);
      let v = parseInt(n.txt, 10); if (isNaN(v)) v = 0; v = Math.max(0, Math.min(100, v));
      const rad = a => (a - 90) * Math.PI / 180;
      ctx.lineWidth = t * S; ctx.lineCap = 'butt';
      if (bg.present) { ctx.strokeStyle = bg.c; ctx.beginPath(); ctx.arc((mx + 0.5) * S, (my + 0.5) * S, (r - t / 2 + 0.5) * S, rad(n.a0), rad(n.a1)); ctx.stroke(); }
      if (v > 0) { ctx.strokeStyle = col; ctx.beginPath(); ctx.arc((mx + 0.5) * S, (my + 0.5) * S, (r - t / 2 + 0.5) * S, rad(n.a0), rad(n.a0 + (n.a1 - n.a0) * v / 100)); ctx.stroke(); }
      break;
    }
    case 'triangle': case 'polygon': {
      const pts = n.pts.map(p => [x + p[0], y + p[1]]);
      const fillIt = n.st.fill || bg.present;
      if (fillIt) {
        ctx.fillStyle = bg.present ? bg.c : col;
        ctx.beginPath(); pts.forEach((p, i) => i ? ctx.lineTo((p[0] + 0.5) * S, (p[1] + 0.5) * S) : ctx.moveTo((p[0] + 0.5) * S, (p[1] + 0.5) * S)); ctx.closePath(); ctx.fill();
      }
      if (bw > 0 || !fillIt) {
        const oc = bw > 0 ? border.c : col;
        for (let k = 0; k < pts.length; k++) { const j = (k + 1) % pts.length; drawLine(pts[k][0], pts[k][1], pts[j][0], pts[j][1], oc, bw > 1 ? bw : 1); }
      }
      break;
    }
  }
}

let lastRoot = null;   // render tree of the last successful render, for the designer

function render() {
  let j;
  try { j = parse(); $('err').textContent = ''; }
  catch (e) { $('err').textContent = e.message; if (window.designerOnError) designerOnError(e); return null; }
  renderObj(j, true);
  return j;
}

// Draws a validated layout object. fromText says the object came from the
// code editor, so the designer should adopt it as its model.
function renderObj(j, fromText) {
  ctx.fillStyle = '#000';
  ctx.fillRect(0, 0, cv.width, cv.height);
  const root = j._root;
  measure(root);
  place(root, 0, 0, 170, 320);
  ctx.save();
  ctx.beginPath(); ctx.rect(0, 0, 170 * S, 320 * S); ctx.clip();
  drawNode(root);
  ctx.restore();
  lastRoot = root;
  if (window.designerOnRender) designerOnRender(j, fromText);
  try {
    const st = j.led !== undefined ? ledState(parseLed(j.led)) : { rgb: '#000000', mode: 'off', speed: 0 };
    $('ledDot').style.background = st.mode === 'off' ? '#000' : st.rgb;
    $('ledDot').style.boxShadow = st.mode === 'off' ? 'none' : '0 0 8px ' + st.rgb;
    $('ledText').textContent = 'LED: ' + st.mode + (st.mode === 'off' ? '' : ' ' + st.rgb + (st.mode === 'solid' ? '' : ' ' + st.speed + ' ms'));
  } catch (e) {}
}

// ---------- editor state ----------
let layouts = [];
let current = '';        // id being edited, '' for a new widget
let renderTimer = 0, liveTimer = 0, keepAlive = 0;

// ---------- source editor ----------
// CodeMirror when the bundle loaded (served by the device at /cm.js),
// otherwise the plain textarea.
let editor = null;
function srcGet() { return editor ? editor.get() : $('src').value; }
function srcSet(t) { if (editor) editor.set(t); else $('src').value = t; }
function onSrcChange() {
  clearTimeout(renderTimer);
  renderTimer = setTimeout(render, 120);
  if ($('live').checked) {
    clearTimeout(liveTimer);
    liveTimer = setTimeout(pushPreview, 600);
  }
}

// Turns a validation error into an editor range: JSON syntax errors carry
// a position, layout errors name an element path, style or led rule.
function diagnostics(text) {
  let j;
  try { j = JSON.parse(text); }
  catch (e) {
    const m = /position (\d+)/.exec(e.message);
    const pos = m ? Math.min(parseInt(m[1], 10), Math.max(0, text.length - 1)) : 0;
    return [{ from: pos, to: Math.min(pos + 1, text.length), message: e.message }];
  }
  try { parseText(text); return []; }
  catch (e) {
    let path = null, m;
    if ((m = /^element ([0-9.]+):/.exec(e.message))) path = 'elements.' + m[1].split('.').join('.children.');
    else if ((m = /^style "([^"]+)"/.exec(e.message))) path = 'styles.' + m[1];
    else if ((m = /^led rule (\d+)/.exec(e.message))) path = 'led.rules.' + m[1];
    else if (/^led\b/.test(e.message) || /"led"/.test(e.message)) path = 'led';
    else if (/"styles"/.test(e.message)) path = 'styles';
    else if (/"elements"/.test(e.message)) path = 'elements';
    const r = path && editor ? editor.locate(path) : null;
    return [r ? { from: r.from, to: r.to, message: e.message } : { from: 0, to: Math.min(1, text.length), message: e.message }];
  }
}

if (window.CM) {
  try {
    editor = CM.create($('cm'), '', onSrcChange, diagnostics);
    $('src').style.display = 'none';
  } catch (e) { editor = null; }
}

function fmt(o) {
  if (editor) return JSON.stringify(o, null, 2);   // foldable, so full pretty print reads best
  const rest = Object.assign({}, o); delete rest.elements;
  const els = (o.elements || []).map(e => '    ' + JSON.stringify(e)).join(',\n');
  const head = Object.keys(rest).length ? JSON.stringify(rest, null, 2).replace(/\n}$/, '') + ',' : '{';
  return head + '\n  "elements": [\n' + els + '\n  ]\n}';
}

const slug = s => String(s).toLowerCase().replace(/[^a-z0-9]+/g, '-').replace(/^-+|-+$/g, '').slice(0, 24);

function setSource(obj) {
  srcSet(fmt(obj));
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
  if (editor) { editor.insert(text); onSrcChange(); return; }
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
$('src').addEventListener('input', onSrcChange);

$('fmt').onclick = () => {
  try { srcSet(fmt(JSON.parse(srcGet()))); render(); msg(''); }
  catch (e) { msg('Cannot format: ' + e.message); }
};

document.addEventListener('keydown', e => {
  const mod = e.ctrlKey || e.metaKey;
  if (!mod) return;
  const k = e.key.toLowerCase();
  if (k === 's') { e.preventDefault(); save(); return; }
  // Undo and redo anywhere on the page. The Design panel writes every
  // change into the code, so the editor's history is the one history.
  // Inside the editor itself CodeMirror already handles these keys.
  if (!editor || e.altKey) return;
  const inEditor = e.target && e.target.closest && e.target.closest('#cm');
  if (inEditor) return;
  if (k === 'z' && !e.shiftKey) { e.preventDefault(); if (window.designerFlush) designerFlush(); editor.undo(); }
  else if (k === 'y' || (k === 'z' && e.shiftKey)) { e.preventDefault(); if (window.designerFlush) designerFlush(); editor.redo(); }
});

// Per-browser conveniences: editor height and which panels are open.
(() => {
  const cm = $('cm');
  try {
    const h = parseInt(localStorage.getItem('editorHeight'), 10);
    if (h >= 160) cm.style.height = h + 'px';
    for (const id of ['keysBox', 'refBox']) if (localStorage.getItem(id) === 'open') $(id).open = true;
  } catch (e) {}
  let t = 0;
  new ResizeObserver(() => { clearTimeout(t); t = setTimeout(() => { try { localStorage.setItem('editorHeight', String(cm.offsetHeight)); } catch (e) {} }, 300); }).observe(cm);
  for (const id of ['keysBox', 'refBox']) $(id).addEventListener('toggle', () => { try { localStorage.setItem(id, $(id).open ? 'open' : 'closed'); } catch (e) {} });
})();

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
<script src="/designer.js"></script>
</body>
</html>
)html";
