#pragma once

// Login page: first visit creates the account with the API key from the
// device screen, later visits ask for the username and password, and a
// forgotten password is reset with the key after the device shows it again.

static const char LOGIN_HTML[] = R"html(
<!doctype html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Log in</title>
<link rel="stylesheet" href="/style.css">
<style>
form{display:none}
form.show{display:block}
input.key{font:20px ui-monospace,monospace;letter-spacing:.15em;text-align:center;text-transform:lowercase}
.linkbtn{background:none;border:0;color:#6cf;padding:0;margin:12px 0;width:auto;font-size:14px;text-decoration:underline}
</style>
</head>
<body>
<nav><a href="/setup">Setup</a><a href="/widgets">Widgets</a><a href="/editor">Editor</a><a class="gh" href="https://github.com/BeekrBonkr/DeskWiG" target="_blank" rel="noopener" title="Project page and README on GitHub">GitHub</a></nav>
<h2 id="title">Log in</h2>
<div class="card dim" id="status">Checking&hellip;</div>

<form id="setupForm" autocomplete="on">
  <h3>Create your account</h3>
  <p class="hint" id="setupHint">Enter the API key shown on the device screen, then choose a username and password. Nothing on the device can be changed until this is done.</p>
  <input id="setupKey" class="key" placeholder="API key" maxlength="8" autocapitalize="off" autocorrect="off" spellcheck="false" autocomplete="off">
  <input id="setupUser" placeholder="Username" maxlength="32" autocapitalize="off" autocorrect="off" autocomplete="username">
  <input id="setupPass" type="password" placeholder="Password (8+ characters)" maxlength="64" autocomplete="new-password">
  <input id="setupPass2" type="password" placeholder="Repeat password" maxlength="64" autocomplete="new-password">
  <button class="primary" type="submit">Create account</button>
</form>

<form id="loginForm" autocomplete="on">
  <input id="loginUser" placeholder="Username" maxlength="32" autocapitalize="off" autocorrect="off" autocomplete="username">
  <input id="loginPass" type="password" placeholder="Password" maxlength="64" autocomplete="current-password">
  <button class="primary" type="submit">Log in</button>
  <button class="linkbtn" type="button" id="forgot">Forgot your password?</button>
</form>

<form id="resetForm" autocomplete="on">
  <h3>Reset password</h3>
  <p class="hint">The device is showing its API key on screen for <span id="resetSecs">60</span> s. Type it here with a new username and password. The key changes once the reset goes through, so every script that uses it needs the new one.</p>
  <input id="resetKey" class="key" placeholder="API key" maxlength="8" autocapitalize="off" autocorrect="off" spellcheck="false" autocomplete="off">
  <input id="resetUser" placeholder="Username" maxlength="32" autocapitalize="off" autocorrect="off" autocomplete="username">
  <input id="resetPass" type="password" placeholder="New password (8+ characters)" maxlength="64" autocomplete="new-password">
  <input id="resetPass2" type="password" placeholder="Repeat password" maxlength="64" autocomplete="new-password">
  <button class="primary" type="submit">Reset password</button>
  <button class="linkbtn" type="button" id="backToLogin">Back to login</button>
</form>

<p id="msg"></p>

<script>
const $ = id => document.getElementById(id);
const msg = t => { $('msg').textContent = t; };
const next = (() => {
  const n = new URLSearchParams(location.search).get('next') || '/widgets';
  return /^\/[a-z]*$/.test(n) ? n : '/widgets';
})();

function show(form) {
  for (const f of ['setupForm', 'loginForm', 'resetForm']) $(f).classList.toggle('show', f === form);
  $('title').textContent = form === 'setupForm' ? 'Welcome' : form === 'resetForm' ? 'Reset password' : 'Log in';
  const first = $(form).querySelector('input');
  if (first) first.focus();
}

async function post(path, body) {
  const r = await fetch(path, { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(body) });
  const j = await r.json().catch(() => ({}));
  if (!r.ok) throw new Error(j.error || ('HTTP ' + r.status));
  return j;
}

async function init() {
  let a;
  try { a = await (await fetch('/api/auth')).json(); }
  catch (e) { $('status').textContent = 'Cannot reach the device.'; return; }
  if (a.loggedIn && a.configured) { location.replace(next); return; }
  if (a.hotspot) {
    $('status').textContent = 'Connected through the setup hotspot. The API key is optional here.';
  } else {
    $('status').style.display = 'none';
  }
  if (a.lockedFor) msg('Too many attempts. Try again in ' + a.lockedFor + ' s.');
  show(a.configured ? 'loginForm' : 'setupForm');
}

$('setupForm').onsubmit = async e => {
  e.preventDefault();
  if ($('setupPass').value !== $('setupPass2').value) { msg('Passwords do not match.'); return; }
  msg('');
  try {
    await post('/api/auth/setup', { key: $('setupKey').value.trim().toLowerCase(), user: $('setupUser').value.trim(), pass: $('setupPass').value });
    location.replace(next);
  } catch (err) { msg(err.message); }
};

$('loginForm').onsubmit = async e => {
  e.preventDefault();
  msg('');
  try {
    await post('/api/auth/login', { user: $('loginUser').value.trim(), pass: $('loginPass').value });
    location.replace(next);
  } catch (err) { msg(err.message); }
};

let resetTimer = 0;
$('forgot').onclick = async () => {
  msg('');
  try {
    const r = await post('/api/auth/reveal', {});
    let secs = r.seconds || 60;
    $('resetSecs').textContent = secs;
    clearInterval(resetTimer);
    resetTimer = setInterval(() => { secs = Math.max(0, secs - 1); $('resetSecs').textContent = secs; if (!secs) clearInterval(resetTimer); }, 1000);
    show('resetForm');
  } catch (err) { msg(err.message); }
};

$('backToLogin').onclick = () => { clearInterval(resetTimer); msg(''); show('loginForm'); };

$('resetForm').onsubmit = async e => {
  e.preventDefault();
  if ($('resetPass').value !== $('resetPass2').value) { msg('Passwords do not match.'); return; }
  msg('');
  try {
    await post('/api/auth/setup', { key: $('resetKey').value.trim().toLowerCase(), user: $('resetUser').value.trim(), pass: $('resetPass').value });
    location.replace(next);
  } catch (err) { msg(err.message); }
};

init();
</script>
</body>
</html>
)html";
