#pragma once

// Static pages served by the device. Kept in a separate header so
// WebServer.cpp stays readable. Phase 3 replaces these with a proper SPA.

static const char STYLE_CSS[] = R"css(
:root{color-scheme:dark}
body{font-family:system-ui,-apple-system,sans-serif;background:#111;color:#eee;margin:0 auto;padding:16px;max-width:480px;line-height:1.4}
h2{margin:8px 0 16px;font-size:22px}
h3{margin:24px 0 8px;font-size:12px;color:#999;text-transform:uppercase;letter-spacing:.08em}
button{display:block;width:100%;margin:8px 0;padding:12px;font-size:16px;background:#222;color:#eee;border:1px solid #444;border-radius:6px;text-align:left;cursor:pointer}
button.primary{border-color:#3c3}
button.danger{border-color:#844;color:#f99}
button.active{border-color:#3c3}
button:disabled{opacity:.5;cursor:default}
button.net{display:flex;justify-content:space-between}
input{display:block;width:100%;margin:8px 0;padding:10px;font-size:16px;background:#222;color:#eee;border:1px solid #444;border-radius:6px;box-sizing:border-box}
.card{background:#1a1a1a;border:1px solid #333;border-radius:8px;padding:12px}
.dim{color:#999}
.hint{color:#999;font-size:13px}
#msg{color:#fc6;min-height:1.4em}
a{color:#6cf}
nav{display:flex;gap:16px;margin-bottom:8px;font-size:14px}
nav a.gh{margin-left:auto;color:#999}
select{display:block;width:100%;margin:8px 0;padding:10px;font-size:16px;background:#222;color:#eee;border:1px solid #444;border-radius:6px;box-sizing:border-box}
label.inline{display:flex;align-items:center;gap:10px;font-size:15px;margin:8px 0}
label.inline input{width:auto;display:inline;margin:0}
.row{display:flex;gap:6px;margin:8px 0}
.row input{margin:0;min-width:0}
.row input.n{flex:2}.row input.p{flex:4}.row input.d{flex:1}
.row button{width:auto;margin:0;padding:10px}
.src{margin:8px 0}
.src .vals{font:13px ui-monospace,monospace;color:#ccc;margin:6px 0;word-break:break-all}
.src .btns{display:flex;gap:6px}
.src .btns button{margin:0;padding:8px;font-size:14px;text-align:center}
.ok{color:#3c3}.bad{color:#f66}.warn{color:#fc6}
.thumb{display:flex;align-items:center;gap:10px}
.thumb img{width:48px;height:48px;object-fit:contain;background:#000;border:1px solid #333;border-radius:4px}
.thumb .btns{margin-left:auto}
.dkeys{display:flex;flex-wrap:wrap;gap:6px;margin:8px 0}
.dkeys button{display:inline-block;width:auto;margin:0;padding:4px 8px;font:12px ui-monospace,monospace;text-align:left}
.dkeys button span{color:#999;margin-left:6px}
.dkeys button.added{border-color:#3c3}
.wrow{display:flex;gap:6px;margin:8px 0;align-items:stretch}
.wrow button{margin:0}
.wrow .pick{flex:1}
.wrow .del{width:auto;padding:12px 14px}
.wrow .grip{display:flex;align-items:center;padding:0 10px;color:#777;border:1px solid #444;border-radius:6px;background:#222;cursor:grab;touch-action:none;user-select:none}
.wrow.dragging{opacity:.4}
.term{background:#000;color:#ddd;border:1px solid #333;border-radius:8px;padding:10px;font:13px ui-monospace,monospace;white-space:pre-wrap;word-break:break-all;height:70vh;overflow-y:auto;margin:8px 0}
.bar{display:flex;gap:6px;align-items:center;margin:8px 0}
.bar button{width:auto;margin:0;padding:8px 12px;font-size:14px}
.bar label.inline{margin:0 0 0 auto}
)css";

static const char SETUP_HTML[] = R"html(
<!doctype html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Device Setup</title>
<link rel="stylesheet" href="/style.css">
</head>
<body>
<nav><a href="/setup">Setup</a><a href="/widgets">Widgets</a><a href="/editor">Editor</a><a href="/terminal">Terminal</a><a class="gh" href="https://github.com/BeekrBonkr/DeskWiG" target="_blank" rel="noopener" title="Project page and README on GitHub">GitHub</a></nav>
<h2>Device Setup</h2>
<div class="card" id="status">Loading&hellip;</div>

<h3>WiFi network</h3>
<button id="scan">Scan for networks</button>
<div id="nets"></div>
<input id="ssid" placeholder="Network name" autocapitalize="off" autocorrect="off">
<input id="pass" type="password" placeholder="Password">
<button id="join" class="primary">Join</button>
<button id="forget" class="danger">Forget saved network</button>
<p id="msg"></p>

<h3>Device name</h3>
<input id="host" placeholder="deskwig" autocapitalize="off" autocorrect="off" maxlength="32">
<button id="saveHost">Save name</button>
<p class="hint">Reachable at http://<span id="hostPreview">deskwig</span>.local once connected. Letters, digits and dashes only.</p>

<h3>Clock</h3>
<div class="card" id="clockStatus">Loading&hellip;</div>
<select id="tz"></select>
<input id="tzCustom" placeholder="POSIX TZ string, e.g. EST5EDT,M3.2.0,M11.1.0" autocapitalize="off" autocorrect="off" maxlength="63" style="display:none">
<select id="ntpSource">
  <option value="pool">Time from the internet (pool.ntp.org)</option>
  <option value="router">Time from the router (WiFi gateway)</option>
  <option value="custom">Time from a custom NTP server</option>
</select>
<input id="ntpServer" placeholder="NTP server hostname or IP" autocapitalize="off" autocorrect="off" maxlength="63" style="display:none">
<label class="inline"><input type="checkbox" id="h24"> 24-hour clock</label>
<button id="saveClock">Save clock settings</button>
<p class="hint">"Router" asks your WiFi gateway for the time, which works on networks without internet access if the router runs an NTP server (most do). The timezone converts NTP's UTC to local time and handles daylight saving.</p>

<h3>Fonts</h3>
<div id="fontList" class="dim">Loading&hellip;</div>
<div class="row">
  <input type="file" id="fontFile" accept=".ttf,font/ttf" class="p">
  <button id="fontUpload" type="button">Upload</button>
</div>
<p class="hint">Upload a .ttf (up to 2 MB) and use it in a layout with <code>"font":"name"</code>; <code>size</code> is then the line height in pixels. <b>sans</b>, <b>bold</b> and <b>emoji</b> are built in. Only upload fonts you trust: the on-device rasteriser does no bounds checking.</p>

<h3>Images</h3>
<div id="imgList" class="dim">Loading&hellip;</div>
<div class="row">
  <input type="file" id="imgFile" accept=".png,.jpg,.jpeg,.gif,image/png,image/jpeg,image/gif" class="p">
  <button id="imgUpload" type="button">Upload</button>
</div>
<p class="hint">PNG, JPEG or animated GIF up to 512 KB. The screen is 170 &times; 320, so resize images before uploading: <a href="https://ezgif.com/resize" target="_blank" rel="noopener">ezgif.com/resize</a> shrinks any image or GIF to the size you need, and <a href="https://ezgif.com/optimize" target="_blank" rel="noopener">ezgif.com/optimize</a> squeezes it under the limit. Use one with <code>{"type":"image","src":"name","w":64}</code> or as a box background with <code>"style":{"image":"name"}</code>. <code>src</code> can also be an http(s) URL, fetched while the widget is on screen.</p>

<h3>Data sources</h3>
<div id="srcList" class="dim">Loading&hellip;</div>
<button id="srcAdd">Add data source</button>
<div class="card" id="srcForm" style="display:none">
  <input id="srcId" placeholder="Name used in layouts, e.g. weather" autocapitalize="off" autocorrect="off" maxlength="16">
  <input id="srcUrl" placeholder="https://api.example.com/data?key=..." autocapitalize="off" autocorrect="off" maxlength="191">
  <input id="srcInterval" type="number" min="10" placeholder="Refresh every N seconds (default 300)">
  <div class="row">
    <input id="srcHdrName" class="n" placeholder="Header (optional)" autocapitalize="off" autocorrect="off" maxlength="31">
    <input id="srcHdrValue" class="p" placeholder="Header value, e.g. Bearer abc123" autocapitalize="off" autocorrect="off" maxlength="127">
  </div>
  <button id="srcDiscover" type="button">Discover keys from this URL</button>
  <div id="srcKeys"></div>
  <p class="hint">Fields: a name for the layout key and the JSON path to read, e.g. <code>current.temperature_2m</code> or <code>items[0].price</code>. Leave the path empty to use the whole response as text. Decimals rounds numbers.</p>
  <div id="srcFields"></div>
  <button id="srcFieldAdd">Add field</button>
  <button id="srcSave" class="primary">Save data source</button>
  <button id="srcCancel">Cancel</button>
</div>
<p class="hint">A source is polled only while a widget that uses it is on screen, so quotas are not spent on screens nobody is looking at. In a layout, use <code>{api.weather.temp}</code> for a field, plus <code>{api.weather.status}</code>, <code>.color</code>, <code>.age</code> and <code>.updated</code>. HTTPS is encrypted but the server certificate is not verified.</p>

<h3>Account</h3>
<div class="card" id="acct">Loading&hellip;</div>
<button id="showKey" type="button">Show API key</button>
<pre id="keyBox" style="display:none"></pre>
<p class="hint">Scripts can use the key instead of logging in: send it as <code>Authorization: Bearer &lt;key&gt;</code>. Forgot your password? Log out and use the link on the login page: the device shows the key on its screen.</p>
<input id="pwCurrent" type="password" placeholder="Current password" maxlength="64" autocomplete="current-password">
<input id="pwNew" type="password" placeholder="New password (8+ characters)" maxlength="64" autocomplete="new-password">
<button id="pwChange" type="button">Change password</button>
<button id="logout" type="button">Log out</button>

<h3>Firmware</h3>
<div class="card" id="fwInfo">Loading&hellip;</div>
<div class="row">
  <input type="file" id="fwFile" accept=".bin" class="p">
  <button id="fwUpload" type="button">Update</button>
</div>
<p class="hint">Upload <code>firmware.bin</code> from a PlatformIO build (<code>.pio/build/esp32-s3-devkitc-1/firmware.bin</code>). The device writes it to its spare app slot and reboots; settings, layouts, fonts and images are kept.</p>

<script>
const $ = id => document.getElementById(id);
const sleep = ms => new Promise(r => setTimeout(r, ms));
const esc = s => String(s).replace(/[&<>"]/g, c => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]));
const bars = r => r > -55 ? '||||' : r > -65 ? '|||.' : r > -75 ? '||..' : '|...';
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
    body: body ? JSON.stringify(body) : undefined
  });
  if (r.status === 401) { toLogin(); throw new Error('Not logged in.'); }
  const j = await r.json().catch(() => ({}));
  if (!r.ok) throw new Error(j.error || ('HTTP ' + r.status));
  return j;
}

// ---------- account ----------
async function loadAccount() {
  const a = auth || await requireLogin();
  if (!a) return;
  $('acct').innerHTML = a.hotspot && !a.configured
    ? 'No account yet. Create one on the <a href="/login">login page</a> once the device is on your network.'
    : 'Logged in as <b>' + esc(a.user || '') + '</b>';
}
$('showKey').onclick = async () => {
  try {
    const r = await api('/api/auth/key');
    const box = $('keyBox');
    box.textContent = r.key + '\n\ncurl -H "Authorization: Bearer ' + r.key + '" http://' + location.host + '/api/status';
    box.style.display = '';
  } catch (e) { msg(e.message); }
};
$('pwChange').onclick = async () => {
  try {
    await api('/api/auth/password', 'PUT', { current: $('pwCurrent').value, pass: $('pwNew').value });
    $('pwCurrent').value = ''; $('pwNew').value = '';
    msg('Password changed.');
  } catch (e) { msg(e.message); }
};
$('logout').onclick = async () => {
  try { await api('/api/auth/logout', 'POST'); } catch (e) {}
  location.href = '/login';
};
setTimeout(loadAccount, 0);

function showStatus(s) {
  let t;
  if (s.state === 'connected') {
    t = 'Connected to <b>' + esc(s.ssid) + '</b><br>' +
        '<span class="dim">http://' + esc(s.hostname) + '.local &middot; ' + esc(s.ip) + '</span>';
  } else if (s.state === 'connecting') {
    t = 'Connecting to <b>' + esc(s.ssid) + '</b>&hellip;';
  } else {
    const why = {
      no_credentials: 'No network saved yet.',
      connect_failed: "Couldn't connect to <b>" + esc(s.ssid) + '</b>.',
      forced: 'Recovery jumper is set.'
    }[s.apReason] || '';
    t = 'Setup hotspot active. ' + why;
  }
  $('status').innerHTML = t;
  if (s.hostname && !$('host').value) {
    $('host').value = s.hostname;
    $('hostPreview').textContent = s.hostname;
  }
}

async function refresh() {
  try { showStatus(await api('/api/wifi')); } catch (e) { $('status').textContent = e.message; }
}

function renderNets(nets) {
  const box = $('nets');
  box.innerHTML = '';
  if (!nets.length) { box.innerHTML = '<p class="dim">No networks found.</p>'; return; }
  nets.forEach(n => {
    const b = document.createElement('button');
    b.className = 'net';
    b.innerHTML = '<span>' + esc(n.ssid) + '</span><span class="dim">' + bars(n.rssi) + (n.secure ? ' &#128274;' : '') + '</span>';
    b.onclick = () => { $('ssid').value = n.ssid; $('pass').focus(); };
    box.appendChild(b);
  });
}

async function scan() {
  $('scan').disabled = true;
  $('nets').innerHTML = '<p class="dim">Scanning&hellip;</p>';
  try {
    for (let i = 0; i < 15; i++) {
      const j = await api('/api/wifi/scan');
      if (j.status === 'done') { renderNets(j.networks); break; }
      await sleep(1000);
    }
  } catch (e) { $('nets').innerHTML = '<p class="dim">' + esc(e.message) + '</p>'; }
  $('scan').disabled = false;
}

async function join() {
  const ssid = $('ssid').value.trim();
  if (!ssid) { msg('Enter a network name.'); return; }
  $('join').disabled = true;
  try {
    await api('/api/wifi/join', 'POST', { ssid: ssid, pass: $('pass').value });
  } catch (e) { msg(e.message); $('join').disabled = false; return; }

  msg('Connecting. This can take up to 20 seconds.');
  for (let i = 0; i < 20; i++) {
    await sleep(1500);
    let s;
    try { s = await api('/api/wifi'); } catch (e) { continue; }
    showStatus(s);
    if (s.state === 'connected') {
      msg('Connected. From your normal WiFi, open http://' + s.hostname + '.local or http://' + s.ip + '. The hotspot turns off shortly.');
      $('join').disabled = false;
      return;
    }
    if (s.state === 'ap') {
      msg("Couldn't connect. Check the password and try again.");
      $('join').disabled = false;
      return;
    }
  }
  msg('Still trying. If the device joins your network, this hotspot will disappear.');
  $('join').disabled = false;
}

let forgetArmed = false;
async function forget() {
  if (!forgetArmed) {
    forgetArmed = true;
    $('forget').textContent = 'Tap again to confirm';
    setTimeout(() => { forgetArmed = false; $('forget').textContent = 'Forget saved network'; }, 4000);
    return;
  }
  forgetArmed = false;
  $('forget').textContent = 'Forget saved network';
  try { await api('/api/wifi/forget', 'POST', {}); msg('Network forgotten. Setup hotspot is starting.'); }
  catch (e) { msg(e.message); }
  refresh();
}

async function saveHost() {
  const hostname = $('host').value.trim().toLowerCase();
  try {
    const c = await api('/api/config', 'PUT', { hostname: hostname });
    $('host').value = c.hostname;
    $('hostPreview').textContent = c.hostname;
    msg('Saved. Reachable at http://' + c.hostname + '.local');
  } catch (e) { msg(e.message); }
}

// ---------- clock ----------
const ZONES = [
  ['UTC', 'UTC0'],
  ['US Eastern', 'EST5EDT,M3.2.0,M11.1.0'],
  ['US Central', 'CST6CDT,M3.2.0,M11.1.0'],
  ['US Mountain', 'MST7MDT,M3.2.0,M11.1.0'],
  ['US Arizona', 'MST7'],
  ['US Pacific', 'PST8PDT,M3.2.0,M11.1.0'],
  ['US Alaska', 'AKST9AKDT,M3.2.0,M11.1.0'],
  ['US Hawaii', 'HST10'],
  ['Canada Atlantic', 'AST4ADT,M3.2.0,M11.1.0'],
  ['Brazil (Sao Paulo)', '<-03>3'],
  ['UK / Ireland', 'GMT0BST,M3.5.0/1,M10.5.0'],
  ['Central Europe', 'CET-1CEST,M3.5.0,M10.5.0/3'],
  ['Eastern Europe', 'EET-2EEST,M3.5.0/3,M10.5.0/4'],
  ['Moscow', 'MSK-3'],
  ['India', 'IST-5:30'],
  ['China / Singapore', 'CST-8'],
  ['Japan / Korea', 'JST-9'],
  ['Australia East', 'AEST-10AEDT,M10.1.0,M4.1.0/3'],
  ['Australia West', 'AWST-8'],
  ['New Zealand', 'NZST-12NZDT,M9.5.0,M4.1.0/3'],
  ['Custom\u2026', 'custom']
];
ZONES.forEach(z => { const o = document.createElement('option'); o.value = z[1]; o.textContent = z[0]; $('tz').appendChild(o); });

function syncClockInputs() {
  $('tzCustom').style.display = $('tz').value === 'custom' ? '' : 'none';
  $('ntpServer').style.display = $('ntpSource').value === 'custom' ? '' : 'none';
}
$('tz').addEventListener('change', syncClockInputs);
$('ntpSource').addEventListener('change', syncClockInputs);

function showClock(c, t) {
  if (c) {
    const known = ZONES.some(z => z[1] === c.tz);
    $('tz').value = known ? c.tz : 'custom';
    $('tzCustom').value = known ? '' : c.tz;
    $('ntpSource').value = c.ntpSource || 'pool';
    $('ntpServer').value = c.ntpServer || '';
    $('h24').checked = !!c['24h'];
    syncClockInputs();
  }
  if (t) {
    $('clockStatus').innerHTML = t.synced
      ? 'Device time <b>' + esc(t.local) + '</b><br><span class="dim">from ' + esc(t.server) + ' (' + esc(t.source) + ')</span>'
      : 'Waiting for time from ' + esc(t.server) + ' (' + esc(t.source) + ')&hellip;';
  }
}

async function loadClock() {
  try {
    const [c, s] = await Promise.all([api('/api/config'), api('/api/status')]);
    showClock(c.clock, s.time);
  } catch (e) { $('clockStatus').textContent = e.message; }
}

async function saveClock() {
  const tz = $('tz').value === 'custom' ? $('tzCustom').value.trim() : $('tz').value;
  if (!tz) { msg('Enter a timezone string.'); return; }
  const body = { clock: { tz: tz, ntpSource: $('ntpSource').value, '24h': $('h24').checked } };
  if ($('ntpSource').value === 'custom') {
    const h = $('ntpServer').value.trim();
    if (!h) { msg('Enter the NTP server.'); return; }
    body.clock.ntpServer = h;
  }
  try {
    const c = await api('/api/config', 'PUT', body);
    showClock(c.clock, null);
    msg('Clock settings saved. The time updates within a few seconds.');
    setTimeout(loadClock, 3000);
  } catch (e) { msg(e.message); }
}

$('saveClock').onclick = saveClock;
setInterval(async () => { try { showClock(null, (await api('/api/status')).time); } catch (e) {} }, 10000);
loadClock();

// ---------- firmware ----------
async function loadFw() {
  try { const st = await api('/api/status'); $('fwInfo').innerHTML = 'Running firmware <b>' + esc(st.firmware) + '</b> &middot; up ' + Math.round(st.uptimeMs / 60000) + ' min'; }
  catch (e) { $('fwInfo').textContent = e.message; }
}
function uploadFw() {
  const f = $('fwFile').files[0];
  if (!f) { msg('Choose firmware.bin first.'); return; }
  if (!/\.bin$/i.test(f.name)) { msg('That is not a .bin file.'); return; }
  const fd = new FormData();
  fd.append('file', f, f.name);
  const xhr = new XMLHttpRequest();
  xhr.open('POST', '/api/system/update');
  $('fwUpload').disabled = true;
  xhr.upload.onprogress = e => { if (e.lengthComputable) msg('Uploading firmware\u2026 ' + Math.round(100 * e.loaded / e.total) + '%'); };
  xhr.onerror = () => { msg('Upload failed (connection lost).'); $('fwUpload').disabled = false; };
  xhr.onload = async () => {
    let j = {}; try { j = JSON.parse(xhr.responseText); } catch (e) {}
    if (xhr.status !== 200) { msg(j.error || ('HTTP ' + xhr.status)); $('fwUpload').disabled = false; return; }
    msg('Firmware written. Rebooting\u2026');
    // Wait for the device to come back on the new firmware.
    for (let i = 0; i < 30; i++) {
      await new Promise(r => setTimeout(r, 2000));
      try { const st = await api('/api/status'); if (st.uptimeMs < 60000) { msg('Back on firmware ' + st.firmware + '.'); loadFw(); break; } } catch (e) {}
    }
    $('fwUpload').disabled = false;
  };
  xhr.send(fd);
}
$('fwUpload').onclick = uploadFw;
loadFw();

// ---------- fonts ----------
let fontDelPending = null;
function fmtBytes(n) { return n >= 1048576 ? (n / 1048576).toFixed(1) + ' MB' : Math.round(n / 1024) + ' KB'; }

async function loadFonts() {
  try {
    const r = await api('/api/fonts');
    const box = $('fontList');
    box.className = '';
    box.innerHTML = '';
    (r.fonts || []).forEach(f => {
      const d = document.createElement('div');
      d.className = 'card src';
      d.innerHTML = '<b>' + esc(f.name) + '</b> <span class="dim">' + fmtBytes(f.size) + (f.builtin ? ' &middot; built in' : '') + '</span>' +
        (f.builtin ? '' : '<div class="btns"><button class="danger del">' + (fontDelPending === f.name ? 'Tap again to delete' : 'Delete') + '</button></div>');
      const del = d.querySelector('.del');
      if (del) del.onclick = () => deleteFont(f.name);
      box.appendChild(d);
    });
    const free = document.createElement('div');
    free.className = 'hint';
    free.textContent = fmtBytes(r.fsFree) + ' free on the device for fonts, images and layouts.';
    box.appendChild(free);
  } catch (e) { $('fontList').textContent = e.message; }
}

async function uploadFont() {
  const f = $('fontFile').files[0];
  if (!f) { msg('Choose a .ttf file first.'); return; }
  const name = f.name.replace(/\.ttf$/i, '').toLowerCase().replace(/[^a-z0-9-]+/g, '-').replace(/^-+|-+$/g, '').slice(0, 23);
  if (!name) { msg('The file name gives no usable font name.'); return; }
  const fd = new FormData();
  fd.append('file', f, f.name);
  $('fontUpload').disabled = true;
  msg('Uploading ' + name + '\u2026');
  try {
    const r = await fetch('/api/fonts?name=' + encodeURIComponent(name), { method: 'POST', body: fd });
    const j = await r.json().catch(() => ({}));
    if (!r.ok) throw new Error(j.error || ('HTTP ' + r.status));
    msg('Font "' + name + '" uploaded. Use "font":"' + name + '" in a layout.');
    $('fontFile').value = '';
    loadFonts();
  } catch (e) { msg(e.message); }
  $('fontUpload').disabled = false;
}

async function deleteFont(name) {
  if (fontDelPending !== name) {
    fontDelPending = name;
    loadFonts();
    setTimeout(() => { if (fontDelPending === name) { fontDelPending = null; loadFonts(); } }, 4000);
    return;
  }
  fontDelPending = null;
  try {
    await api('/api/fonts?name=' + encodeURIComponent(name), 'DELETE');
    msg('Deleted font ' + name + '.');
    loadFonts();
  } catch (e) { msg(e.message); }
}

$('fontUpload').onclick = uploadFont;
loadFonts();

// ---------- images ----------
let imgDelPending = null;

async function loadImages() {
  try {
    const r = await api('/api/images');
    const box = $('imgList');
    box.className = '';
    box.innerHTML = '';
    if (!(r.images || []).length) box.innerHTML = '<div class="dim">No images yet.</div>';
    (r.images || []).forEach(im => {
      const d = document.createElement('div');
      d.className = 'card src thumb';
      d.innerHTML = '<img src="/img/' + esc(im.name) + '.' + esc(im.type) + '?t=' + Date.now() + '" alt="">' +
        '<div><b>' + esc(im.name) + '</b><br><span class="dim">' + esc(im.type) + ' &middot; ' + fmtBytes(im.size) + '</span></div>' +
        '<div class="btns"><button class="danger del">' + (imgDelPending === im.name ? 'Tap again to delete' : 'Delete') + '</button></div>';
      d.querySelector('.del').onclick = () => deleteImage(im.name);
      box.appendChild(d);
    });
  } catch (e) { $('imgList').textContent = e.message; }
}

async function uploadImage() {
  const f = $('imgFile').files[0];
  if (!f) { msg('Choose an image first.'); return; }
  const name = f.name.replace(/\.[a-z0-9]+$/i, '').toLowerCase().replace(/[^a-z0-9-]+/g, '-').replace(/^-+|-+$/g, '').slice(0, 23);
  if (!name) { msg('The file name gives no usable image name.'); return; }
  if (f.size > 512 * 1024) { msg('That image is larger than 512 KB. Resize it for the 170 x 320 screen first.'); return; }
  const fd = new FormData();
  fd.append('file', f, f.name);
  $('imgUpload').disabled = true;
  msg('Uploading ' + name + '\u2026');
  try {
    const r = await fetch('/api/images?name=' + encodeURIComponent(name), { method: 'POST', body: fd });
    const j = await r.json().catch(() => ({}));
    if (!r.ok) throw new Error(j.error || ('HTTP ' + r.status));
    msg('Image "' + name + '" uploaded. Use "src":"' + name + '" in a layout.');
    $('imgFile').value = '';
    loadImages();
  } catch (e) { msg(e.message); }
  $('imgUpload').disabled = false;
}

async function deleteImage(name) {
  if (imgDelPending !== name) {
    imgDelPending = name;
    loadImages();
    setTimeout(() => { if (imgDelPending === name) { imgDelPending = null; loadImages(); } }, 4000);
    return;
  }
  imgDelPending = null;
  try {
    await api('/api/images?name=' + encodeURIComponent(name), 'DELETE');
    msg('Deleted image ' + name + '.');
    loadImages();
  } catch (e) { msg(e.message); }
}

$('imgUpload').onclick = uploadImage;
loadImages();

// ---------- data sources ----------
let sources = [];
let editingId = null;
let pendingDelete = null;

function fieldRow(f) {
  const d = document.createElement('div');
  d.className = 'row';
  d.innerHTML = '<input class="n" placeholder="name" maxlength="16" autocapitalize="off" autocorrect="off">' +
                '<input class="p" placeholder="json.path" maxlength="63" autocapitalize="off" autocorrect="off">' +
                '<input class="d" type="number" min="0" max="6" placeholder="dec">' +
                '<button type="button" title="Remove">&times;</button>';
  d.children[0].value = f ? f.name : '';
  d.children[1].value = f ? f.path : '';
  d.children[2].value = (f && f.decimals !== undefined) ? f.decimals : '';
  d.children[3].onclick = () => d.remove();
  return d;
}

function openForm(src) {
  editingId = src ? src.id : null;
  $('srcId').value = src ? src.id : '';
  $('srcId').disabled = !!src;
  $('srcUrl').value = src ? src.url : '';
  $('srcInterval').value = src ? src.intervalS : '';
  $('srcHdrName').value = (src && src.header) ? src.header.name : '';
  $('srcHdrValue').value = '';
  $('srcHdrValue').placeholder = (src && src.header && src.header.set) ? '(unchanged, enter to replace)' : 'Header value, e.g. Bearer abc123';
  const box = $('srcFields');
  box.innerHTML = '';
  (src ? src.fields : [{ name: '', path: '' }]).forEach(f => box.appendChild(fieldRow(f)));
  clearTimeout(discTimer);
  $('srcKeys').innerHTML = '';
  $('srcKeys').className = '';
  $('srcForm').style.display = '';
  $('srcAdd').style.display = 'none';
  $('srcForm').scrollIntoView({ behavior: 'smooth', block: 'nearest' });
}

function closeForm() {
  clearTimeout(discTimer);
  $('srcKeys').innerHTML = '';
  $('srcKeys').className = '';
  $('srcForm').style.display = 'none';
  $('srcAdd').style.display = '';
  editingId = null;
}

function ageText(s) {
  if (s === undefined) return 'never fetched';
  if (s < 60) return s + 's ago';
  if (s < 3600) return Math.floor(s / 60) + 'm ago';
  return Math.floor(s / 3600) + 'h ago';
}

function renderSources() {
  const box = $('srcList');
  box.className = '';
  if (!sources.length) { box.innerHTML = '<div class="dim">No data sources yet.</div>'; return; }
  box.innerHTML = '';
  sources.forEach(src => {
    const d = document.createElement('div');
    d.className = 'card src';
    let st;
    if (src.state === 'fetching') st = '<span class="dim">fetching&hellip;</span>';
    else if (src.state === 'error') st = '<span class="bad">' + esc(src.error || 'error') + '</span>' + (src.everOk ? ' <span class="dim">(showing old values)</span>' : '');
    else if (src.everOk) st = '<span class="ok">ok</span> <span class="dim">' + ageText(src.ageS) + '</span>';
    else st = '<span class="dim">waiting for first fetch</span>';
    const vals = Object.keys(src.values || {}).map(k => '{api.' + esc(src.id) + '.' + esc(k) + '} = ' + esc(src.values[k])).join('<br>');
    d.innerHTML = '<b>' + esc(src.id) + '</b> &middot; ' + st +
      '<div class="dim" style="font-size:13px;word-break:break-all">' + esc(src.url) + '</div>' +
      '<div class="vals">' + vals + '</div>' +
      '<div class="btns"><button class="edit">Edit</button><button class="test">Test</button><button class="danger del">' +
      (pendingDelete === src.id ? 'Tap again to delete' : 'Delete') + '</button></div>';
    d.querySelector('.edit').onclick = () => openForm(src);
    d.querySelector('.test').onclick = () => testSource(src.id);
    d.querySelector('.del').onclick = () => deleteSource(src.id);
    box.appendChild(d);
  });
}

async function loadSources() {
  try {
    const r = await api('/api/sources');
    sources = r.sources || [];
    renderSources();
    $('srcAdd').disabled = r.free === 0;
    $('srcAdd').textContent = r.free === 0 ? 'All ' + r.max + ' source slots used' : 'Add data source';
  } catch (e) { $('srcList').textContent = e.message; }
}

async function saveSource() {
  const id = (editingId || $('srcId').value.trim().toLowerCase());
  if (!id) { msg('Enter a name for the source.'); return; }
  const url = $('srcUrl').value.trim();
  if (!url) { msg('Enter the URL.'); return; }
  const body = { url: url, fields: [] };
  const iv = parseInt($('srcInterval').value, 10);
  if (!isNaN(iv)) body.intervalS = iv;
  const hn = $('srcHdrName').value.trim();
  if (hn) body.header = { name: hn, value: $('srcHdrValue').value };
  for (const row of $('srcFields').children) {
    const name = row.children[0].value.trim();
    const path = row.children[1].value.trim();
    if (!name && !path) continue;
    const f = { name: name, path: path };
    const dec = parseInt(row.children[2].value, 10);
    if (!isNaN(dec)) f.decimals = dec;
    body.fields.push(f);
  }
  if (!body.fields.length) { msg('Add at least one field.'); return; }
  try {
    const r = await api('/api/sources?id=' + encodeURIComponent(id), 'PUT', body);
    sources = r.sources || [];
    closeForm();
    renderSources();
    msg('Saved. Fetching now…');
    setTimeout(loadSources, 2500);
    setTimeout(loadSources, 8000);
  } catch (e) { msg(e.message); }
}

async function testSource(id) {
  try {
    await api('/api/sources/test?id=' + encodeURIComponent(id), 'POST');
    msg('Fetching ' + id + '…');
    setTimeout(loadSources, 2500);
    setTimeout(loadSources, 8000);
  } catch (e) { msg(e.message); }
}

async function deleteSource(id) {
  if (pendingDelete !== id) {
    pendingDelete = id;
    renderSources();
    setTimeout(() => { if (pendingDelete === id) { pendingDelete = null; renderSources(); } }, 4000);
    return;
  }
  pendingDelete = null;
  try {
    const r = await api('/api/sources?id=' + encodeURIComponent(id), 'DELETE');
    sources = r.sources || [];
    renderSources();
    msg('Deleted ' + id + '.');
  } catch (e) { msg(e.message); }
}

$('srcAdd').onclick = () => openForm(null);
$('srcCancel').onclick = closeForm;
$('srcSave').onclick = saveSource;
$('srcFieldAdd').onclick = () => $('srcFields').appendChild(fieldRow(null));

// ---------- key discovery ----------
// The device fetches the URL (with the header, so private APIs work too)
// and lists every JSON path with a sample value. Tap a path to add it.
let discTimer = 0;
function fieldNameFor(path) {
  const seg = path.replace(/\[\d+\]/g, '').split('.').filter(Boolean).pop() || 'value';
  let name = seg.replace(/[^A-Za-z0-9_]/g, '').slice(0, 16) || 'value';
  if (/^\d/.test(name)) name = 'v' + name.slice(0, 15);
  if (['status', 'color', 'age', 'updated', 'error'].includes(name)) name += '2';
  const taken = [...$('srcFields').querySelectorAll('input.n')].map(i => i.value);
  let out = name, n = 2;
  while (taken.includes(out)) out = (name.slice(0, 14) + n++);
  return out;
}
function addDiscoveredField(path, btn) {
  const rows = [...$('srcFields').children];
  const empty = rows.find(r => !r.children[0].value && !r.children[1].value);
  const row = empty || fieldRow(null);
  row.children[0].value = fieldNameFor(path);
  row.children[1].value = path;
  if (!empty) $('srcFields').appendChild(row);
  btn.classList.add('added');
}
function renderDiscovery(d) {
  const box = $('srcKeys');
  box.className = 'dkeys';
  box.innerHTML = '';
  if (d.state === 'fetching') { box.className = 'dim'; box.textContent = 'Fetching ' + d.url + '…'; return; }
  if (d.state === 'error') { box.className = 'bad'; box.textContent = 'Could not read ' + d.url + ': ' + (d.error || 'unknown error'); return; }
  const r = d.result || {};
  if (!r.json) {
    box.className = 'dim';
    box.textContent = 'The reply is not JSON. Leave the path empty to use the whole text: ' + JSON.stringify(r.text || '');
    return;
  }
  if (!(r.keys || []).length) { box.className = 'dim'; box.textContent = 'No values found in the reply.'; return; }
  const present = [...$('srcFields').querySelectorAll('input.p')].map(i => i.value);
  for (const k of r.keys) {
    const b = document.createElement('button');
    b.type = 'button';
    b.innerHTML = esc(k.path) + '<span>' + esc(k.value) + '</span>';
    if (present.includes(k.path)) b.classList.add('added');
    b.onclick = () => addDiscoveredField(k.path, b);
    box.appendChild(b);
  }
  if (r.truncated) { const h = document.createElement('div'); h.className = 'hint'; h.textContent = 'Showing the first 200 values.'; box.appendChild(h); }
}
async function pollDiscovery(tries) {
  try {
    const d = await api('/api/sources/discover');
    renderDiscovery(d);
    if (d.state === 'fetching' && tries > 0) discTimer = setTimeout(() => pollDiscovery(tries - 1), 1000);
    else if (d.state === 'fetching') { $('srcKeys').className = 'bad'; $('srcKeys').textContent = 'Timed out waiting for the reply.'; }
  } catch (e) { $('srcKeys').className = 'bad'; $('srcKeys').textContent = e.message; }
}
$('srcDiscover').onclick = async () => {
  const url = $('srcUrl').value.trim();
  if (!url) { msg('Enter the URL first.'); return; }
  clearTimeout(discTimer);
  const body = { url, header: { name: $('srcHdrName').value.trim(), value: $('srcHdrValue').value } };
  if (editingId) body.id = editingId;
  try {
    await api('/api/sources/discover', 'POST', body);
    renderDiscovery({ state: 'fetching', url });
    discTimer = setTimeout(() => pollDiscovery(30), 800);
  } catch (e) { msg(e.message); }
};
setInterval(() => { if (!editingId && $('srcForm').style.display === 'none') loadSources(); }, 10000);
loadSources();

$('host').addEventListener('input', () => { $('hostPreview').textContent = $('host').value || 'deskwig'; });
$('scan').onclick = scan;
$('join').onclick = join;
$('forget').onclick = forget;
$('saveHost').onclick = saveHost;
refresh();
</script>
</body>
</html>
)html";

static const char WIDGETS_HTML[] = R"html(
<!doctype html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Widget Selector</title>
<link rel="stylesheet" href="/style.css">
</head>
<body>
<nav><a href="/setup">Setup</a><a href="/widgets">Widgets</a><a href="/editor">Editor</a><a href="/terminal">Terminal</a><a class="gh" href="https://github.com/BeekrBonkr/DeskWiG" target="_blank" rel="noopener" title="Project page and README on GitHub">GitHub</a></nav>
<h2>Widget Selector</h2>
<div id="list"></div>
<p id="msg"></p>
<p class="hint">Drag the handle to change the order the encoder knob steps through.</p>
<div id="hidden"></div>

<script>
const $ = id => document.getElementById(id);
// Every page starts by checking the login; unauthenticated browsers go to /login.
const toLogin = () => { location.replace('/login?next=' + encodeURIComponent(location.pathname)); };
let auth = null;
async function requireLogin() {
  try { auth = await (await fetch('/api/auth')).json(); } catch (e) { return null; }
  if (!auth.loggedIn) { toLogin(); return null; }
  return auth;
}
requireLogin();

let delPending = null;

function render(j) {
  const list = $('list');
  list.innerHTML = '';
  if (!j.widgets.length) list.innerHTML = '<p class="dim">No widgets. Restore a built-in below or make one in the editor.</p>';
  j.widgets.forEach((w, i) => {
    const row = document.createElement('div');
    row.className = 'wrow';
    row.dataset.key = w.key;
    const g = document.createElement('span');
    g.className = 'grip';
    g.innerHTML = '&#9776;';
    g.title = 'Drag to reorder';
    g.onpointerdown = e => startDrag(e, row);
    row.appendChild(g);
    const b = document.createElement('button');
    b.className = 'pick' + (i === j.active ? ' active' : '');
    b.textContent = w.name + (i === j.active ? '  (active)' : '');
    b.onclick = () => activate(i);
    const d = document.createElement('button');
    d.className = 'del danger';
    d.textContent = delPending === w.key ? 'Tap again' : 'Delete';
    d.title = w.builtin ? 'Remove this built-in widget (it can be restored)' : 'Delete this layout from the device';
    d.onclick = () => remove(w.key);
    row.appendChild(b);
    row.appendChild(d);
    list.appendChild(row);
  });
  const h = $('hidden');
  h.innerHTML = '';
  if (j.hidden.length) {
    const p = document.createElement('p');
    p.className = 'hint';
    p.textContent = 'Deleted built-in widgets:';
    h.appendChild(p);
    j.hidden.forEach(k => {
      const b = document.createElement('button');
      b.textContent = 'Restore ' + k;
      b.onclick = () => restore(k);
      h.appendChild(b);
    });
  }
}

async function load() {
  $('msg').textContent = '';
  try {
    const r = await fetch('/api/widgets');
    render(await r.json());
  } catch (e) {
    $('msg').textContent = 'Failed to load widgets';
  }
}

// ---------- drag to reorder ----------
// Pointer events so the same code serves mouse and touch. The dragged
// row is moved in the DOM as the pointer crosses its neighbours' midlines,
// and the new order is sent when the pointer is released.
function startDrag(e, row) {
  e.preventDefault();
  const list = $('list');
  const grip = e.currentTarget;
  grip.setPointerCapture(e.pointerId);
  row.classList.add('dragging');
  const move = ev => {
    const rows = [...list.children].filter(r => r !== row);
    for (const r of rows) {
      const b = r.getBoundingClientRect();
      const mid = b.top + b.height / 2;
      if (ev.clientY < mid) { list.insertBefore(row, r); return; }
    }
    list.appendChild(row);
  };
  const up = async () => {
    grip.removeEventListener('pointermove', move);
    grip.removeEventListener('pointerup', up);
    grip.removeEventListener('pointercancel', up);
    row.classList.remove('dragging');
    const order = [...list.children].map(r => r.dataset.key);
    const r = await fetch('/api/widgets/order', {
      method: 'PUT',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ order })
    });
    if (r.status === 401) { toLogin(); return; }
    const j = await r.json().catch(() => ({}));
    if (!r.ok) { $('msg').textContent = j.error || ('HTTP ' + r.status); load(); return; }
    render(j);
  };
  grip.addEventListener('pointermove', move);
  grip.addEventListener('pointerup', up);
  grip.addEventListener('pointercancel', up);
}

async function remove(key) {
  if (delPending !== key) {
    delPending = key;
    load();
    setTimeout(() => { if (delPending === key) { delPending = null; load(); } }, 4000);
    return;
  }
  delPending = null;
  const r = await fetch('/api/widgets?key=' + encodeURIComponent(key), { method: 'DELETE' });
  if (r.status === 401) { toLogin(); return; }
  const j = await r.json().catch(() => ({}));
  if (!r.ok) { $('msg').textContent = j.error || ('HTTP ' + r.status); load(); return; }
  render(j);
}

async function restore(key) {
  const r = await fetch('/api/widgets/restore', {
    method: 'POST',
    headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
    body: 'key=' + encodeURIComponent(key)
  });
  if (r.status === 401) { toLogin(); return; }
  const j = await r.json().catch(() => ({}));
  if (!r.ok) { $('msg').textContent = j.error || ('HTTP ' + r.status); return; }
  render(j);
}

async function activate(i) {
  const r = await fetch('/api/widgets', {
    method: 'POST',
    headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
    body: 'index=' + i
  });
  if (r.status === 401) { toLogin(); return; }
  load();
}

load();
</script>
</body>
</html>
)html";

static const char TERMINAL_HTML[] = R"html(
<!doctype html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Terminal</title>
<link rel="stylesheet" href="/style.css">
</head>
<body>
<nav><a href="/setup">Setup</a><a href="/widgets">Widgets</a><a href="/editor">Editor</a><a href="/terminal">Terminal</a><a class="gh" href="https://github.com/BeekrBonkr/DeskWiG" target="_blank" rel="noopener" title="Project page and README on GitHub">GitHub</a></nav>
<h2>Terminal</h2>
<p class="hint">Everything the device prints to its USB serial port, live. The device keeps the last 32 KB, so the log starts where the buffer does.</p>
<div class="bar">
  <button id="pause" type="button">Pause</button>
  <button id="clear" type="button">Clear</button>
  <label class="inline"><input type="checkbox" id="follow" checked> Follow</label>
</div>
<div class="term" id="out"></div>
<p id="msg"></p>

<script>
const $ = id => document.getElementById(id);
const toLogin = () => { location.replace('/login?next=' + encodeURIComponent(location.pathname)); };
let auth = null;
async function requireLogin() {
  try { auth = await (await fetch('/api/auth')).json(); } catch (e) { return null; }
  if (!auth.loggedIn) { toLogin(); return null; }
  return auth;
}
requireLogin();

let since = 0;      // byte number of the next output we have not seen
let paused = false;
let polling = false;

async function poll() {
  if (paused || polling) return;
  polling = true;
  try {
    const r = await fetch('/api/log?since=' + since);
    if (r.status === 401) { toLogin(); return; }
    if (!r.ok) throw new Error('HTTP ' + r.status);
    const next = parseInt(r.headers.get('X-Log-Seq') || '0', 10);
    const text = await r.text();
    if (text.length) {
      const out = $('out');
      out.appendChild(document.createTextNode(text));
      // Keep the page responsive: trim to the last ~64 KB of text.
      while (out.textContent.length > 65536 && out.firstChild) out.removeChild(out.firstChild);
      if ($('follow').checked) out.scrollTop = out.scrollHeight;
    }
    since = next;
    $('msg').textContent = '';
  } catch (e) { $('msg').textContent = 'Connection lost, retrying…'; }
  polling = false;
}

$('pause').onclick = () => {
  paused = !paused;
  $('pause').textContent = paused ? 'Resume' : 'Pause';
  if (!paused) poll();
};
$('clear').onclick = () => { $('out').textContent = ''; };
// Scrolling up turns Follow off, so the reader is not yanked back down.
$('out').addEventListener('scroll', () => {
  const o = $('out');
  const atBottom = o.scrollHeight - o.scrollTop - o.clientHeight < 4;
  if (!atBottom) $('follow').checked = false;
});

setInterval(poll, 1000);
poll();
</script>
</body>
</html>
)html";
