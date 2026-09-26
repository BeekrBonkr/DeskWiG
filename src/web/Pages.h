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
select{display:block;width:100%;margin:8px 0;padding:10px;font-size:16px;background:#222;color:#eee;border:1px solid #444;border-radius:6px;box-sizing:border-box}
label.inline{display:flex;align-items:center;gap:10px;font-size:15px;margin:8px 0}
label.inline input{width:auto;display:inline;margin:0}
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
<nav><a href="/setup">Setup</a><a href="/widgets">Widgets</a><a href="/editor">Editor</a></nav>
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

<h3>API token</h3>
<input id="token" placeholder="Shown on the device screen after it connects" autocapitalize="off" autocorrect="off">
<p class="hint">Not needed while you're connected through the setup hotspot.</p>

<script>
const $ = id => document.getElementById(id);
const sleep = ms => new Promise(r => setTimeout(r, ms));
const esc = s => String(s).replace(/[&<>"]/g, c => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]));
const bars = r => r > -55 ? '||||' : r > -65 ? '|||.' : r > -75 ? '||..' : '|...';
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
    body: body ? JSON.stringify(body) : undefined
  });
  if (r.status === 401) throw new Error('Unauthorized. Enter the API token shown on the device.');
  const j = await r.json().catch(() => ({}));
  if (!r.ok) throw new Error(j.error || ('HTTP ' + r.status));
  return j;
}

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
<nav><a href="/setup">Setup</a><a href="/widgets">Widgets</a><a href="/editor">Editor</a></nav>
<h2>Widget Selector</h2>
<div id="list"></div>
<p id="msg"></p>

<h3>API token</h3>
<input id="token" placeholder="Shown on the device screen after it connects" autocapitalize="off" autocorrect="off">

<script>
const $ = id => document.getElementById(id);
let token = '';
try { token = localStorage.getItem('apiToken') || ''; } catch (e) {}
$('token').value = token;
$('token').addEventListener('change', () => {
  token = $('token').value.trim();
  try { localStorage.setItem('apiToken', token); } catch (e) {}
});

async function load() {
  $('msg').textContent = '';
  try {
    const r = await fetch('/api/widgets');
    const j = await r.json();
    const list = $('list');
    list.innerHTML = '';
    j.widgets.forEach((w, i) => {
      const b = document.createElement('button');
      b.textContent = w + (i === j.active ? '  (active)' : '');
      if (i === j.active) b.className = 'active';
      b.onclick = () => activate(i);
      list.appendChild(b);
    });
  } catch (e) {
    $('msg').textContent = 'Failed to load widgets';
  }
}

async function activate(i) {
  const r = await fetch('/api/widgets', {
    method: 'POST',
    headers: {
      'Content-Type': 'application/x-www-form-urlencoded',
      'Authorization': 'Bearer ' + token
    },
    body: 'index=' + i
  });
  if (r.status === 401) {
    $('msg').textContent = 'Unauthorized. Enter the API token shown on the device.';
    return;
  }
  load();
}

load();
</script>
</body>
</html>
)html";
