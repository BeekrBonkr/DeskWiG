#pragma once

// Static pages served by the device. Kept in a separate header so
// WebServer.cpp stays readable.

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
input[type=range]{padding:0;background:none;border:0;accent-color:#3c3}
textarea{display:block;width:100%;margin:8px 0;padding:10px;font:13px/1.45 ui-monospace,SFMono-Regular,Menlo,Consolas,monospace;background:#222;color:#eee;border:1px solid #444;border-radius:6px;box-sizing:border-box;min-height:180px;resize:vertical;white-space:pre;overflow-wrap:normal;overflow-x:auto}
.card{background:#1a1a1a;border:1px solid #333;border-radius:8px;padding:12px}
.dim{color:#999}
.hint{color:#999;font-size:13px}
#msg,.msg{color:#fc6;min-height:1.4em;margin:6px 0}
.msg:empty{display:none}
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
.thumb .btns{margin-left:auto;display:flex;gap:6px}
.dkeys{display:flex;flex-wrap:wrap;gap:6px;margin:8px 0}
.dkeys button{display:inline-block;width:auto;margin:0;padding:4px 8px;font:12px ui-monospace,monospace;text-align:left}
.dkeys button span{color:#999;margin-left:6px}
.dkeys button.added{border-color:#3c3}
.wrow{display:flex;gap:6px;margin:8px 0;align-items:stretch}
.wrow button{margin:0}
.wrow .pick{flex:1}
.wrow .del{width:auto;padding:12px 14px}
.wrow .grip{display:flex;align-items:center;padding:0 10px;color:#777;border:1px solid #444;border-radius:6px;background:#222;cursor:grab;touch-action:none;user-select:none;-webkit-user-select:none}
.wrow.dragging{opacity:.4}
body.dragging{user-select:none;-webkit-user-select:none;cursor:grabbing}
.term{background:#000;color:#ddd;border:1px solid #333;border-radius:8px;padding:10px;font:13px ui-monospace,monospace;white-space:pre-wrap;word-break:break-all;height:70vh;overflow-y:auto;margin:8px 0}
.bar{display:flex;gap:6px;align-items:center;margin:8px 0}
.bar button{width:auto;margin:0;padding:8px 12px;font-size:14px}
.bar label.inline{margin:0 0 0 auto}
.res{display:grid;grid-template-columns:repeat(auto-fit,minmax(140px,1fr));gap:8px;margin:8px 0}
.res .card{padding:8px 10px}
.res .k{font-size:12px;color:#999;text-transform:uppercase;letter-spacing:.06em}
.res .v{font-size:15px;margin:2px 0}
.res .m{height:4px;background:#333;border-radius:2px;overflow:hidden}
.res .m i{display:block;height:100%;background:#3c3}
.res .m i.warn{background:#fc6}.res .m i.bad{background:#f66}
details.sec{background:#1a1a1a;border:1px solid #333;border-radius:8px;margin:8px 0}
details.sec>summary{display:flex;align-items:center;gap:10px;padding:12px;font-size:16px;cursor:pointer;list-style:none}
details.sec>summary::-webkit-details-marker{display:none}
details.sec>summary::after{content:"\203A";margin-left:auto;color:#999;font-size:20px;transition:transform .15s}
details.sec[open]>summary::after{transform:rotate(90deg)}
details.sec>summary .sub{color:#999;font-size:13px;overflow:hidden;text-overflow:ellipsis;white-space:nowrap;min-width:0}
details.sec>.body{padding:0 12px 12px;border-top:1px solid #2a2a2a}
details.sec>.body>h3:first-child{margin-top:12px}
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
<p id="msg"></p>

<details class="sec" id="sec-wifi">
<summary><span>WiFi network</span><span class="sub" id="sub-wifi"></span></summary>
<div class="body">
<button id="scan">Scan for networks</button>
<div id="nets"></div>
<input id="ssid" placeholder="Network name" autocapitalize="off" autocorrect="off">
<input id="pass" type="password" placeholder="Password">
<button id="join" class="primary">Join</button>
<button id="forget" class="danger">Forget saved network</button>
<p class="msg" id="msg-wifi"></p>
</div>
</details>

<details class="sec" id="sec-name">
<summary><span>Device name</span><span class="sub" id="sub-name"></span></summary>
<div class="body">
<input id="host" placeholder="deskwig" autocapitalize="off" autocorrect="off" maxlength="32">
<button id="saveHost">Save name</button>
<p class="msg" id="msg-name"></p>
<p class="hint">Reachable at http://<span id="hostPreview">deskwig</span>.local once connected. Letters, digits and dashes only.</p>
</div>
</details>

<details class="sec" id="sec-display">
<summary><span>Display</span><span class="sub" id="sub-display"></span></summary>
<div class="body">
<label class="inline" for="bright">Brightness <b id="brightVal"></b></label>
<input type="range" id="bright" min="1" max="100" value="100">
<label class="inline"><input type="checkbox" id="flip"> Upside down</label>
<p class="msg" id="msg-display"></p>
<p class="hint">Brightness changes as you drag and is saved when you let go. Upside down turns the picture 180 degrees for a display mounted the other way round.</p>
</div>
</details>

<details class="sec" id="sec-clock">
<summary><span>Clock</span><span class="sub" id="sub-clock"></span></summary>
<div class="body">
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
<p class="msg" id="msg-clock"></p>
<p class="hint">"Router" asks your WiFi gateway for the time, which works on networks without internet access if the router runs an NTP server (most do). The timezone converts NTP's UTC to local time and handles daylight saving.</p>
</div>
</details>

<details class="sec" id="sec-fonts">
<summary><span>Fonts</span><span class="sub" id="sub-fonts"></span></summary>
<div class="body">
<div id="fontList" class="dim">Loading&hellip;</div>
<div class="row">
  <input type="file" id="fontFile" accept=".ttf,font/ttf" class="p">
  <button id="fontUpload" type="button">Upload</button>
</div>
<p class="msg" id="msg-fonts"></p>
<p class="hint">Upload a .ttf (up to 2 MB) and use it in a layout with <code>"font":"name"</code>; <code>size</code> is then the line height in pixels. <b>sans</b>, <b>bold</b> and <b>emoji</b> are built in. Only upload fonts you trust: the on-device rasterizer does no bounds checking.</p>
</div>
</details>

<details class="sec" id="sec-images">
<summary><span>Images</span><span class="sub" id="sub-images"></span></summary>
<div class="body">
<div id="imgList" class="dim">Loading&hellip;</div>
<div class="row">
  <input type="file" id="imgFile" accept=".png,.jpg,.jpeg,.gif,image/png,image/jpeg,image/gif" class="p">
  <button id="imgUpload" type="button">Upload</button>
</div>
<p class="msg" id="msg-images"></p>
<p class="hint">PNG, JPEG or animated GIF up to 512 KB. The screen is 170 &times; 320, so resize images before uploading: <a href="https://ezgif.com/resize" target="_blank" rel="noopener">ezgif.com/resize</a> shrinks any image or GIF to the size you need, and <a href="https://ezgif.com/optimize" target="_blank" rel="noopener">ezgif.com/optimize</a> squeezes it under the limit. Use one with <code>{"type":"image","src":"name","w":64}</code> or as a box background with <code>"style":{"image":"name"}</code>. <code>src</code> can also be an http(s) URL, fetched while the widget is on screen.</p>
</div>
</details>

<details class="sec" id="sec-sources">
<summary><span>Data sources</span><span class="sub" id="sub-sources"></span></summary>
<div class="body">
<div id="srcList" class="dim">Loading&hellip;</div>
<button id="srcAdd">Add data source</button>
<button id="srcPaste">Paste data sources as text</button>
<div class="card" id="srcImport" style="display:none">
  <p class="hint">One or more sources, in the text form below or as the JSON the API takes. A source that already exists is replaced; an empty header value keeps the stored one. The example widgets' README lists ready-made blocks to paste.</p>
  <textarea id="srcText" spellcheck="false" placeholder="id:       weather
url:      https://api.open-meteo.com/v1/forecast?latitude=51.48&amp;longitude=0.00&amp;current=temperature_2m,relative_humidity_2m
interval: 600
header:   Authorization = Bearer abc123   (optional)
fields:   temp = current.temperature_2m (decimals 0)
          humidity = current.relative_humidity_2m"></textarea>
  <button id="srcImportGo" class="primary">Import</button>
  <button id="srcFill" type="button">Fill with the current sources</button>
  <button id="srcImportCancel">Cancel</button>
</div>
<div class="card" id="srcForm" style="display:none">
  <input id="srcId" placeholder="Name used in layouts, e.g. weather" autocapitalize="off" autocorrect="off" maxlength="16">
  <input id="srcUrl" placeholder="https://api.example.com/data?key=..." autocapitalize="off" autocorrect="off" maxlength="511">
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
<p class="msg" id="msg-sources"></p>
<p class="hint">A source is polled only while a widget that uses it is on screen, so quotas are not spent on screens nobody is looking at. In a layout, use <code>{api.weather.temp}</code> for a field, plus <code>{api.weather.status}</code>, <code>.color</code>, <code>.age</code> and <code>.updated</code>. HTTPS is encrypted but the server certificate is not verified.</p>
</div>
</details>

<details class="sec" id="sec-history">
<summary><span>History</span><span class="sub" id="sub-history"></span></summary>
<div class="body">
<div id="serList" class="dim">Loading&hellip;</div>
<div class="card">
  <input id="serKey" placeholder="Key to sample, e.g. api.weather.temp or ping.0.ms" autocapitalize="off" autocorrect="off" maxlength="47">
  <div class="row">
    <input id="serEvery" class="n" type="number" min="5" placeholder="Every N seconds (60)">
    <input id="serKeep" class="n" type="number" min="10" placeholder="Keep N seconds (3600)">
    <button id="serSave" type="button" class="primary">Add</button>
  </div>
</div>
<p class="msg" id="msg-history"></p>
<p class="hint">A key sampled over time. Layouts draw it with <code>{"type":"chart","series":"api.weather.temp","h":60}</code> and read <code>{api.weather.temp.min}</code>, <code>.max</code>, <code>.avg</code>, <code>.first</code>, <code>.last</code>, <code>.delta</code>, <code>.count</code> and <code>.span</code>. A data source or ping target named here keeps updating whatever is on screen. At most 720 samples per key, so an hour at every 5 s or a day at every 2 min; samples are lost on reboot.</p>
</div>
</details>

<details class="sec" id="sec-account">
<summary><span>Account</span><span class="sub" id="sub-account"></span></summary>
<div class="body">
<div class="card" id="acct">Loading&hellip;</div>
<button id="showKey" type="button">Show API key</button>
<pre id="keyBox" style="display:none"></pre>
<p class="hint">Scripts can use the key instead of logging in: send it as <code>Authorization: Bearer &lt;key&gt;</code>. Forgot your password? Log out and use the link on the login page: the device shows the key on its screen.</p>
<input id="pwCurrent" type="password" placeholder="Current password" maxlength="64" autocomplete="current-password">
<input id="pwNew" type="password" placeholder="New password (8+ characters)" maxlength="64" autocomplete="new-password">
<button id="pwChange" type="button">Change password</button>
<p class="msg" id="msg-account"></p>
<button id="logout" type="button">Log out</button>
</div>
</details>

<details class="sec" id="sec-firmware">
<summary><span>Firmware</span><span class="sub" id="sub-firmware"></span></summary>
<div class="body">
<div class="card" id="fwInfo">Loading&hellip;</div>
<div class="row">
  <input type="file" id="fwFile" accept=".bin" class="p">
  <button id="fwUpload" type="button">Update</button>
</div>
<p class="msg" id="msg-firmware"></p>
<p class="hint">Upload <code>firmware.bin</code> from a PlatformIO build (<code>.pio/build/esp32-s3-devkitc-1/firmware.bin</code>). The device writes it to its spare app slot and reboots; settings, layouts, fonts and images are kept.</p>
</div>
</details>

<script>
const $ = id => document.getElementById(id);
const sleep = ms => new Promise(r => setTimeout(r, ms));
const esc = s => String(s).replace(/[&<>"]/g, c => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]));
const bars = r => r > -55 ? '||||' : r > -65 ? '|||.' : r > -75 ? '||..' : '|...';
// Status text goes next to the buttons of the open section (msg-<name>),
// so it is beside whatever was just pressed; with no section open it
// falls back to the line under the status card.
const SECTIONS = ['wifi', 'name', 'display', 'clock', 'fonts', 'images', 'sources', 'history', 'account', 'firmware'];
function msg(t, sec) {
  const target = sec || SECTIONS.find(n => $('sec-' + n).open);
  SECTIONS.forEach(n => { if (n !== target) $('msg-' + n).textContent = ''; });
  $('msg').textContent = target ? '' : t;
  if (target) $('msg-' + target).textContent = t;
}

// Every page starts by checking the login; unauthenticated browsers go to /login.
const toLogin = () => { location.replace('/login?next=' + encodeURIComponent(location.pathname)); };
let auth = null;
async function requireLogin() {
  try { auth = await (await fetch('/api/auth')).json(); } catch (e) { return null; }
  if (!auth.loggedIn) { toLogin(); return null; }
  return auth;
}
const loginReady = requireLogin();

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

// ---------- sections ----------
// One panel open at a time; the choice lives in the URL hash so a reload
// or a link (/setup#firmware) lands on the right panel. Each summary shows
// a one-line summary of that section filled in by its loader.
function openSection(name, scroll) {
  SECTIONS.forEach(n => { $('sec-' + n).open = n === name; });
  if (name) {
    if (location.hash !== '#' + name) history.replaceState(null, '', '#' + name);
    if (scroll) $('sec-' + name).scrollIntoView({ behavior: 'smooth', block: 'start' });
  } else if (location.hash) {
    history.replaceState(null, '', location.pathname);
  }
}
function sub(name, text) { $('sub-' + name).textContent = text || ''; }
// The device has little RAM to spare for requests nobody is looking at:
// a section is only re-polled while it is open and the tab is visible,
// and it is refreshed once when it opens.
let started = false;
const live = n => $('sec-' + n).open && !document.hidden;
const onOpen = { display: () => loadDisplay(), clock: () => loadClock(), sources: () => { if (sourcesIdle()) loadSources(); }, history: () => loadSeries() };
SECTIONS.forEach(n => {
  $('sec-' + n).addEventListener('toggle', e => {
    if (e.target.open) { openSection(n, false); if (started && onOpen[n]) onOpen[n](); }
    else if (location.hash === '#' + n) history.replaceState(null, '', location.pathname);
  });
});
window.addEventListener('hashchange', () => { const h = location.hash.slice(1); if (SECTIONS.includes(h)) openSection(h, true); });
{
  const h = location.hash.slice(1);
  if (SECTIONS.includes(h)) openSection(h, false);
}

// ---------- account ----------
async function loadAccount() {
  const a = auth || await loginReady;
  if (!a) return;
  $('acct').innerHTML = a.hotspot && !a.configured
    ? 'No account yet. Create one on the <a href="/login">login page</a> once the device is on your network.'
    : 'Logged in as <b>' + esc(a.user || '') + '</b>';
  sub('account', a.configured ? (a.user || '') : 'no account yet');
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
  sub('wifi', s.state === 'connected' ? s.ssid : s.state === 'connecting' ? 'connecting…' : 'setup hotspot');
  if (s.hostname && !$('host').value) {
    $('host').value = s.hostname;
    $('hostPreview').textContent = s.hostname;
  }
  if (s.hostname) sub('name', s.hostname + '.local');
  // With no panel chosen, land on WiFi until the device is on a network.
  if (!location.hash && s.state !== 'connected' && !SECTIONS.some(n => $('sec-' + n).open)) openSection('wifi', false);
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
    sub('name', c.hostname + '.local');
    msg('Saved. Reachable at http://' + c.hostname + '.local');
  } catch (e) { msg(e.message); }
}

// ---------- display ----------
// Dragging previews the level without saving, one request in flight at a
// time with only the latest value queued; letting go saves it.
let brightBusy = false, brightNext = -1;
function showDisplay(d) {
  $('bright').value = d.brightness;
  $('brightVal').textContent = d.brightness + '%';
  $('flip').checked = !!d.flip;
  sub('display', d.brightness + '%' + (d.flip ? ', upside down' : ''));
}
async function previewBright(pct) {
  if (brightBusy) { brightNext = pct; return; }
  brightBusy = true;
  try {
    await fetch('/api/display/preview', { method: 'POST', headers: { 'Content-Type': 'application/x-www-form-urlencoded' }, body: 'brightness=' + pct });
  } catch (e) {}
  brightBusy = false;
  if (brightNext >= 0) { const n = brightNext; brightNext = -1; if (n !== pct) previewBright(n); }
}
async function saveBright(pct) {
  try {
    const c = await api('/api/config', 'PUT', { display: { brightness: pct } });
    showDisplay(c.display);
    msg('Brightness saved.', 'display');
  } catch (e) { msg(e.message, 'display'); }
}
$('flip').onchange = async () => {
  try {
    const c = await api('/api/config', 'PUT', { display: { flip: $('flip').checked } });
    showDisplay(c.display);
    msg(c.display.flip ? 'Display turned upside down.' : 'Display the normal way up.', 'display');
  } catch (e) { msg(e.message, 'display'); }
};
$('bright').oninput = () => { const v = +$('bright').value; $('brightVal').textContent = v + '%'; previewBright(v); };
$('bright').onchange = () => saveBright(+$('bright').value);
async function loadDisplay() {
  try { const c = await api('/api/config'); showDisplay(c.display); }
  catch (e) { msg(e.message, 'display'); }
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
    sub('clock', t.synced ? t.local : 'waiting for time');
    $('clockStatus').innerHTML = t.synced
      ? 'Device time <b>' + esc(t.local) + '</b><br><span class="dim">from ' + esc(t.server) + ' (' + esc(t.source) + ')</span>'
      : 'Waiting for time from ' + esc(t.server) + ' (' + esc(t.source) + ')&hellip;';
  }
}

async function loadClock() {
  try {
    const c = await api('/api/config');
    if (c.display) showDisplay(c.display);
    const s = await api('/api/status');
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
setInterval(async () => { if (!live('clock')) return; try { showClock(null, (await api('/api/status')).time); } catch (e) {} }, 10000);

// ---------- firmware ----------
async function loadFw() {
  try { const st = await api('/api/status'); $('fwInfo').innerHTML = 'Running firmware <b>' + esc(st.firmware) + '</b> &middot; up ' + Math.round(st.uptimeMs / 60000) + ' min'; sub('firmware', 'v' + st.firmware); }
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
    sub('fonts', (r.fonts || []).length + ' fonts');
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

// ---------- images ----------
let imgDelPending = null;
let imgRenaming = null;        // image whose rename form is open
let imgRenamePrompt = null;    // {from, to, refs} while asking about widgets that use it

async function loadImages() {
  try {
    const r = await api('/api/images');
    const box = $('imgList');
    box.className = '';
    box.innerHTML = '';
    if (!(r.images || []).length) box.innerHTML = '<div class="dim">No images yet.</div>';
    sub('images', (r.images || []).length ? (r.images.length + ' images') : 'none yet');
    (r.images || []).forEach(im => {
      const d = document.createElement('div');
      // No inline thumbnails: the page would open every image at once,
      // and each is a file streamed from the device, which ran it out of
      // RAM with a long list. "View" fetches one on demand instead.
      d.className = 'card src thumb';
      d.innerHTML = '<div><b>' + esc(im.name) + '</b><br><span class="dim">' + esc(im.type) + ' &middot; ' + fmtBytes(im.size) + '</span></div>' +
        '<div class="btns"><button class="view">View</button><button class="ren">Rename</button><button class="danger del">' + (imgDelPending === im.name ? 'Tap again to delete' : 'Delete') + '</button></div>';
      d.querySelector('.view').onclick = () => window.open('/img/' + encodeURIComponent(im.name) + '.' + im.type, '_blank');
      d.querySelector('.ren').onclick = () => { imgRenaming = imgRenaming === im.name ? null : im.name; imgRenamePrompt = null; loadImages(); };
      d.querySelector('.del').onclick = () => deleteImage(im.name);
      box.appendChild(d);
      if (imgRenaming === im.name) box.appendChild(renameForm(im));
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

// The rename form under an image's card. Once the widgets that use the
// image are known it turns into the question of whether to update them.
function renameForm(im) {
  const f = document.createElement('div');
  f.className = 'card src';
  const close = () => { imgRenaming = null; imgRenamePrompt = null; loadImages(); };
  const p = imgRenamePrompt && imgRenamePrompt.from === im.name ? imgRenamePrompt : null;
  if (p) {
    const n = p.refs.length;
    f.innerHTML = '<div>Renaming <b>' + esc(p.from) + '</b> breaks ' + n + ' widget' + (n === 1 ? '' : 's') + ' that use' + (n === 1 ? 's' : '') +
      ' it: <b>' + p.refs.map(r => esc(r.name)).join('</b>, <b>') + '</b>. Update ' + (n === 1 ? 'it' : 'them') + ' to <b>' + esc(p.to) + '</b> and save?</div>' +
      '<div class="btns"><button class="primary upd">Rename and update</button><button class="only">Rename only</button><button class="cancel">Cancel</button></div>';
    f.querySelector('.upd').onclick = () => doRename(p.from, p.to, p.refs);
    f.querySelector('.only').onclick = () => doRename(p.from, p.to, []);
    f.querySelector('.cancel').onclick = close;
    return f;
  }
  f.innerHTML = '<div class="row"><input class="p" maxlength="23" value="' + esc(im.name) + '" placeholder="new name"><button class="primary ok">Save</button><button class="cancel">Cancel</button></div>' +
    '<div class="hint">Widgets that use this image are checked first, and can be updated to the new name for you.</div>';
  const inp = f.querySelector('input');
  f.querySelector('.ok').onclick = () => renameImage(im.name, inp.value);
  f.querySelector('.cancel').onclick = close;
  inp.onkeydown = e => { if (e.key === 'Enter') renameImage(im.name, inp.value); else if (e.key === 'Escape') close(); };
  setTimeout(() => { inp.focus(); inp.select(); }, 0);
  return f;
}

// Layouts use an image as "src" on an image element or "image" in a style
// (an element's, a named style's or the root's). Counts the uses, and
// rewrites them when `to` is given.
function imageUses(node, name, to) {
  let hits = 0;
  if (Array.isArray(node)) { node.forEach(n => { hits += imageUses(n, name, to); }); return hits; }
  if (!node || typeof node !== 'object') return 0;
  for (const k of Object.keys(node)) {
    const v = node[k];
    if ((k === 'src' || k === 'image') && v === name) { hits++; if (to) node[k] = to; }
    else if (v && typeof v === 'object') hits += imageUses(v, name, to);
  }
  return hits;
}

// Every stored layout that uses the image, with its JSON so it can be rewritten.
async function imageRefs(name) {
  const list = await api('/api/layouts');
  const out = [];
  for (const l of (list.layouts || [])) {
    const doc = await api('/api/layouts?id=' + encodeURIComponent(l.id));
    if (imageUses(doc, name)) out.push({ id: l.id, name: doc.name || l.name || l.id, doc });
  }
  return out;
}

async function renameImage(from, to) {
  to = to.trim().toLowerCase();
  if (!/^[a-z0-9](?:[a-z0-9-]{0,21}[a-z0-9])?$/.test(to)) { msg('Names are 1-23 lowercase letters, digits and dashes.'); return; }
  if (to === from) { imgRenaming = null; loadImages(); return; }
  msg('Checking widgets for "' + from + '"\u2026');
  let refs;
  try { refs = await imageRefs(from); } catch (e) { msg(e.message); return; }
  if (!refs.length) { doRename(from, to, []); return; }
  msg('');
  imgRenamePrompt = { from, to, refs };
  loadImages();
}

// Renames on the device, then saves each layout in refs with the new name.
async function doRename(from, to, refs) {
  try {
    await api('/api/images/rename?name=' + encodeURIComponent(from) + '&to=' + encodeURIComponent(to), 'POST');
  } catch (e) { msg(e.message); return; }
  imgRenaming = null;
  imgRenamePrompt = null;
  let updated = 0;
  const failed = [];
  for (const r of refs) {
    imageUses(r.doc, from, to);
    try { await api('/api/layouts?id=' + encodeURIComponent(r.id), 'PUT', r.doc); updated++; }
    catch (e) { failed.push(r.name + ' (' + e.message + ')'); }
  }
  let t = 'Renamed image "' + from + '" to "' + to + '".';
  if (updated) t += ' Updated ' + updated + ' widget' + (updated === 1 ? '' : 's') + '.';
  if (failed.length) t += ' Could not update ' + failed.join(', ') + '.';
  msg(t);
  loadImages();
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
  $('srcPaste').style.display = 'none';
  $('srcForm').scrollIntoView({ behavior: 'smooth', block: 'nearest' });
}

function closeForm() {
  clearTimeout(discTimer);
  $('srcKeys').innerHTML = '';
  $('srcKeys').className = '';
  $('srcForm').style.display = 'none';
  $('srcAdd').style.display = '';
  $('srcPaste').style.display = '';
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
    sub('sources', sources.length ? sources.map(x => x.id).join(', ') : 'none yet');
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

// ---------- paste import ----------
// Takes the JSON the API accepts (one source, an array, {"sources":[...]} or
// {"<id>":{...}}) or the text form used in examples/widgets/README.md:
//   id:       weather
//   url:      https://...
//   interval: 600
//   header:   Authorization = Bearer abc
//   fields:   temp = current.temperature_2m (decimals 0)
//             humidity = current.relative_humidity_2m
// A **bold** id line from the README also names the source(s) that follow.
// "name = path (decimals N)", with several pairs per line allowed.
function parseFields(s) {
  const out = [];
  for (const m of s.matchAll(/([^\s=()]+)\s*=\s*([^\s=()]*)(?:\s*\(([^)]*)\))?/g)) {
    const f = { name: m[1], path: m[2] };
    const d = m[3] && /dec/i.test(m[3]) ? m[3].match(/\d+/) : null;
    if (d) f.decimals = parseInt(d[0], 10);
    out.push(f);
  }
  return out;
}
function parseField(s) { return parseFields(s)[0] || null; }
function normalizeJsonSources(j) {
  let list;
  if (Array.isArray(j)) list = j;
  else if (j && Array.isArray(j.sources)) list = j.sources;
  else if (j && typeof j === 'object' && !j.url) list = Object.keys(j).map(k => Object.assign({ id: k }, j[k]));
  else list = [j];
  return list.map(x => {
    const o = { id: String(x.id || x.name || ''), url: String(x.url || ''), fields: [] };
    const iv = x.intervalS !== undefined ? x.intervalS : x.interval;
    if (iv !== undefined) o.intervalS = parseInt(iv, 10);
    if (x.header && x.header.name) o.header = { name: x.header.name, value: x.header.value || '' };
    const fs = x.fields || [];
    if (Array.isArray(fs)) fs.forEach(f => o.fields.push(typeof f === 'string' ? parseField(f) : { name: f.name, path: f.path || '', decimals: f.decimals }));
    else Object.keys(fs).forEach(k => o.fields.push(typeof fs[k] === 'object' ? Object.assign({ name: k }, fs[k]) : { name: k, path: String(fs[k]) }));
    o.fields = o.fields.filter(Boolean);
    return o;
  });
}
function parseSourceText(text) {
  const t = text.trim();
  if (!t) throw new Error('Paste one or more sources first.');
  if (t[0] === '{' || t[0] === '[') {
    try { return normalizeJsonSources(JSON.parse(t)); } catch (e) { throw new Error('Not valid JSON: ' + e.message); }
  }
  const out = [];
  let cur = null, inFields = false, pending = [];
  const start = id => { cur = { id: id || pending.shift() || '', fields: [] }; out.push(cur); inFields = false; };
  for (const raw of t.split(/\r?\n/)) {
    const line = raw.trim();
    if (!line) continue;
    if (line.startsWith('```')) { inFields = false; continue; }
    let m;
    if (inFields && line.indexOf('=') >= 0 && !/^(id|source|url|interval|intervals|refresh|every|header|fields?)\s*:/i.test(line)) { cur.fields.push(...parseFields(line)); continue; }
    inFields = false;
    const bold = [...line.matchAll(/\*\*([a-z0-9-]{1,16})\*\*/g)].map(x => x[1]);
    if (bold.length) { pending = bold; cur = null; continue; }
    if ((m = line.match(/^(?:id|source)\s*[:=]\s*(\S+)/i))) {
      if (!cur || cur.id || cur.url) start(m[1]); else cur.id = m[1];
      continue;
    }
    if ((m = line.match(/^url\s*[:=]\s*(\S+)/i))) {
      if (!cur || cur.url) start();
      cur.url = m[1];
      continue;
    }
    if ((m = line.match(/^([a-z0-9-]{1,16})(?:\s*\(.*\))?$/)) && (!cur || cur.url)) { start(m[1]); continue; }
    const setting = /^(?:interval|intervals|refresh|every|header|fields?)\s*[:=]/i.test(line);
    if (setting && !cur) throw new Error('"' + line.slice(0, 40) + '": an id: or url: line must come first.');
    if ((m = line.match(/^(?:interval|intervals|refresh|every)\s*[:=]\s*(\d+)/i))) { cur.intervalS = parseInt(m[1], 10); continue; }
    if ((m = line.match(/^header\s*[:=]\s*(.*)$/i))) {
      let h = m[1].replace(/\s*\((?:optional|[^)]*)\)\s*$/i, '').trim();
      if (!h) continue;
      let sep = h.indexOf('=');
      if (sep < 0) sep = h.indexOf(':');
      cur.header = sep < 0 ? { name: h, value: '' } : { name: h.slice(0, sep).trim(), value: h.slice(sep + 1).trim() };
      continue;
    }
    if ((m = line.match(/^fields?\s*[:=]\s*(.*)$/i))) {
      inFields = true;
      cur.fields.push(...parseFields(m[1]));
      continue;
    }
    if (/^[a-z][a-z0-9_-]{0,15}\s*[:=]\s*\S/i.test(line) && !/\s/.test(line.split(/[:=]/)[0].trim())) throw new Error('"' + line.slice(0, 30) + '": unknown setting. Use id, url, interval, header or fields.');
    // anything else is prose between blocks
  }
  return out;
}
function sourcesToText(list) {
  const pad = k => (k + ':').padEnd(10);
  return list.map(x => {
    const lines = [pad('id') + x.id, pad('url') + x.url, pad('interval') + x.intervalS];
    if (x.header && x.header.name) lines.push(pad('header') + x.header.name + ' = ');
    (x.fields || []).forEach((f, i) => lines.push((i ? ' '.repeat(10) : pad('fields')) + f.name + ' = ' + f.path + (f.decimals !== undefined ? ' (decimals ' + f.decimals + ')' : '')));
    return lines.join('\n');
  }).join('\n\n') + '\n';
}
// The same rules the device applies, checked here so the message can name the field.
function checkSource(x) {
  const id = (x.id || '').trim().toLowerCase();
  if (!/^[a-z0-9-]{1,16}$/.test(id)) return 'id "' + id + '" must be 1-16 lowercase letters, digits or dashes';
  if (!/^https?:\/\/\S+$/.test(x.url || '') || x.url.length > 511) return id + ': the url must start with http:// or https://';
  if (!x.fields.length) return id + ': add at least one field';
  if (x.fields.length > 24) return id + ': too many fields (max 24)';
  const seen = {};
  for (const f of x.fields) {
    if (!/^[A-Za-z0-9_]{1,16}$/.test(f.name) || ['status', 'color', 'age', 'updated', 'error'].includes(f.name)) return id + ': field name "' + f.name + '" must be 1-16 letters, digits or _ and not a status key';
    if (!/^[A-Za-z0-9_.\-\[\]]{0,63}$/.test(f.path)) return id + ': field ' + f.name + ' has an invalid path "' + f.path.slice(0, 40) + '"' + (f.path.indexOf(' ') >= 0 ? ' (one field per line)' : '');
    if (seen[f.name]) return id + ': field name "' + f.name + '" is used twice';
    seen[f.name] = true;
  }
  if (x.header && (!/^[A-Za-z0-9_-]{1,31}$/.test(x.header.name) || !/^[ -~]{0,127}$/.test(x.header.value))) return id + ': invalid header';
  return '';
}
function closeImport() { $('srcImport').style.display = 'none'; $('srcPaste').style.display = ''; }
async function importSources() {
  let list;
  try { list = parseSourceText($('srcText').value); } catch (e) { msg(e.message); return; }
  if (!list.length) { msg('No sources found in the text.'); return; }
  const done = [], failed = [];
  for (const x of list) {
    const id = x.id.trim().toLowerCase();
    if (!id) { failed.push((x.url || '(no url)').slice(0, 40) + ': no id'); continue; }
    const bad = checkSource(x);
    if (bad) { failed.push(bad); continue; }
    const body = { url: x.url, fields: x.fields };
    if (x.intervalS !== undefined && !isNaN(x.intervalS)) body.intervalS = x.intervalS;
    if (x.header) body.header = x.header;
    try {
      const r = await api('/api/sources?id=' + encodeURIComponent(id), 'PUT', body);
      sources = r.sources || [];
      done.push(id);
    } catch (e) { failed.push(id + ': ' + e.message); }
  }
  renderSources();
  if (!failed.length) { closeImport(); $('srcText').value = ''; }
  msg((done.length ? 'Imported ' + done.join(', ') + '. ' : '') + (failed.length ? 'Not imported: ' + failed.join('; ') : 'Fetching now…'));
  setTimeout(loadSources, 2500);
  setTimeout(loadSources, 8000);
}
$('srcPaste').onclick = () => { $('srcImport').style.display = ''; $('srcPaste').style.display = 'none'; $('srcText').focus(); };
$('srcImportCancel').onclick = closeImport;
$('srcImportGo').onclick = importSources;
$('srcFill').onclick = () => { $('srcText').value = sourcesToText(sources); };

// ---------- history (series) ----------
let series = [];
let serDelPending = null;

function spanText(s) {
  if (s < 60) return s + 's';
  if (s < 3600) return Math.floor(s / 60) + 'm';
  if (s < 86400) return Math.floor(s / 3600) + 'h ' + Math.floor((s % 3600) / 60) + 'm';
  return Math.floor(s / 86400) + 'd ' + Math.floor((s % 86400) / 3600) + 'h';
}

function renderSeries() {
  const box = $('serList');
  box.className = '';
  if (!series.length) { box.innerHTML = '<div class="dim">Nothing is sampled yet.</div>'; return; }
  box.innerHTML = '';
  series.forEach(se => {
    const d = document.createElement('div');
    d.className = 'card src';
    const st = se.count ? '<span class="ok">' + se.count + ' samples</span> <span class="dim">over ' + spanText(se.span) + ', last ' + esc(String(se.last)) + '</span>'
                        : '<span class="dim">waiting for the first sample</span>';
    d.innerHTML = '<b>' + esc(se.key) + '</b> &middot; ' + st +
      '<div class="dim" style="font-size:13px">every ' + se.every + ' s, keeps ' + spanText(se.keep) + ' (' + se.cap + ' samples)' +
      (se.count ? ' &middot; min ' + esc(String(se.min)) + ', max ' + esc(String(se.max)) : '') + '</div>' +
      '<div class="btns"><button class="edit">Edit</button><button class="danger del">' + (serDelPending === se.key ? 'Tap again to delete' : 'Delete') + '</button></div>';
    d.querySelector('.edit').onclick = () => { $('serKey').value = se.key; $('serEvery').value = se.every; $('serKeep').value = se.keep; $('serSave').textContent = 'Save'; $('serKey').focus(); };
    d.querySelector('.del').onclick = () => deleteSeries(se.key);
    box.appendChild(d);
  });
}

async function loadSeries() {
  try {
    const r = await api('/api/series');
    series = r.series || [];
    renderSeries();
    sub('history', series.length ? series.map(x => x.key).join(', ') : 'nothing sampled');
    $('serSave').disabled = r.free === 0 && !series.some(x => x.key === $('serKey').value.trim());
  } catch (e) { $('serList').textContent = e.message; }
}

async function saveSeries() {
  const key = $('serKey').value.trim();
  if (!key) { msg('Enter the key to sample, as you would write it in a layout without the braces.'); return; }
  const body = {};
  const ev = parseInt($('serEvery').value, 10), kp = parseInt($('serKeep').value, 10);
  if (!isNaN(ev)) body.every = ev;
  if (!isNaN(kp)) body.keep = kp;
  try {
    const r = await api('/api/series?key=' + encodeURIComponent(key), 'PUT', body);
    series = r.series || [];
    $('serKey').value = ''; $('serEvery').value = ''; $('serKeep').value = '';
    $('serSave').textContent = 'Add';
    renderSeries();
    sub('history', series.map(x => x.key).join(', '));
    msg('Sampling ' + key + '. Use {"type":"chart","series":"' + key + '"} in a layout.');
  } catch (e) { msg(e.message); }
}

async function deleteSeries(key) {
  if (serDelPending !== key) {
    serDelPending = key;
    renderSeries();
    setTimeout(() => { if (serDelPending === key) { serDelPending = null; renderSeries(); } }, 4000);
    return;
  }
  serDelPending = null;
  try {
    const r = await api('/api/series?key=' + encodeURIComponent(key), 'DELETE');
    series = r.series || [];
    renderSeries();
    sub('history', series.length ? series.map(x => x.key).join(', ') : 'nothing sampled');
    msg('Stopped sampling ' + key + '.');
  } catch (e) { msg(e.message); }
}

$('serSave').onclick = saveSeries;
$('serKey').onkeydown = e => { if (e.key === 'Enter') saveSeries(); };
setInterval(() => { if (live('history')) loadSeries(); }, 10000);

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
function sourcesIdle() { return !editingId && $('srcForm').style.display === 'none'; }
setInterval(() => { if (live('sources') && sourcesIdle()) loadSources(); }, 10000);

$('host').addEventListener('input', () => { $('hostPreview').textContent = $('host').value || 'deskwig'; });
$('scan').onclick = scan;
$('join').onclick = join;
$('forget').onclick = forget;
$('saveHost').onclick = saveHost;

// First load: one request at a time, so opening the page costs the device
// one connection's worth of memory instead of nine at once. Every loader
// also fills the one-line summary on its section's header.
(async () => {
  await loginReady;
  for (const load of [refresh, loadAccount, loadClock, loadFw, loadFonts, loadImages, loadSources, loadSeries]) {
    try { await load(); } catch (e) {}
  }
  started = true;
})();
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
    g.draggable = false;
    g.onmousedown = e => e.preventDefault();   // Firefox selects text on pointerdown alone
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
// row is moved in the DOM as the pointer crosses its neighbors' midlines,
// and the new order is sent when the pointer is released.
function startDrag(e, row) {
  if (e.button !== undefined && e.button !== 0) return;
  e.preventDefault();
  const list = $('list');
  row.classList.add('dragging');
  document.body.classList.add('dragging');
  const move = ev => {
    ev.preventDefault();
    const rows = [...list.children].filter(r => r !== row && r.classList.contains('wrow'));
    for (const r of rows) {
      const b = r.getBoundingClientRect();
      const mid = b.top + b.height / 2;
      if (ev.clientY < mid) { if (row.nextSibling !== r) list.insertBefore(row, r); return; }
    }
    if (list.lastElementChild !== row) list.appendChild(row);
  };
  const up = async () => {
    document.removeEventListener('pointermove', move);
    document.removeEventListener('pointerup', up);
    document.removeEventListener('pointercancel', up);
    row.classList.remove('dragging');
    document.body.classList.remove('dragging');
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
  // Listen on the document so the drag survives the pointer leaving the handle.
  document.addEventListener('pointermove', move);
  document.addEventListener('pointerup', up);
  document.addEventListener('pointercancel', up);
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
<div class="res" id="res"></div>
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
  if (paused || polling || document.hidden) return;
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

// ---------- resources ----------
const kb = n => n >= 1048576 ? (n / 1048576).toFixed(2) + ' MB' : Math.round(n / 1024) + ' KB';
function tile(k, v, used, total, note) {
  const pct = total ? Math.min(100, Math.round(100 * used / total)) : 0;
  const cls = pct >= 90 ? 'bad' : pct >= 75 ? 'warn' : '';
  return '<div class="card"><div class="k">' + k + '</div><div class="v">' + v + '</div>' +
    '<div class="m"><i class="' + cls + '" style="width:' + pct + '%"></i></div>' +
    (note ? '<div class="k" style="text-transform:none;margin-top:4px">' + note + '</div>' : '') + '</div>';
}
async function loadResources() {
  if (document.hidden) return;
  try {
    const r = await fetch('/api/status');
    if (!r.ok) return;
    const m = (await r.json()).memory;
    if (!m) return;
    $('res').innerHTML =
      tile('RAM', kb(m.heapFree) + ' free of ' + kb(m.heapTotal), m.heapTotal - m.heapFree, m.heapTotal,
           'lowest ' + kb(m.heapMin) + ' \u00b7 largest block ' + kb(m.heapBlock)) +
      tile('PSRAM', kb(m.psramFree) + ' free of ' + kb(m.psramTotal), m.psramTotal - m.psramFree, m.psramTotal) +
      tile('Storage', kb(m.fsUsed) + ' used of ' + kb(m.fsTotal), m.fsUsed, m.fsTotal,
           kb(m.fsTotal - m.fsUsed) + ' free for fonts, images and layouts');
  } catch (e) {}
}
setInterval(loadResources, 5000);
loadResources();

setInterval(poll, 1000);
poll();
</script>
</body>
</html>
)html";
