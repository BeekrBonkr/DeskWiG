#include "Auth.h"

#include <Preferences.h>
#include <esp_random.h>
#include <mbedtls/sha256.h>
#include "Settings.h"
#include "../app/Log.h"

static const char* NVS_NS = "auth";

constexpr uint8_t  SALT_LEN         = 16;             // bytes
constexpr uint16_t HASH_ROUNDS      = 5000;
constexpr uint8_t  MAX_SESSIONS     = 4;
constexpr uint32_t SESSION_IDLE_MS  = 7UL * 24 * 3600 * 1000;
constexpr uint8_t  AUTH_MAX_FAILURES = 5;
constexpr uint32_t LOCKOUT_MS       = 60000;

static char    account[AUTH_USER_MAX + 1] = "";
static uint8_t salt[SALT_LEN];
static uint8_t hash[32];
static bool    configured = false;

struct Session {
  char sid[AUTH_SID_LEN + 1];
  uint32_t lastSeen;
  bool used;
};
static Session sessions[MAX_SESSIONS];

static uint8_t  failures = 0;
static uint32_t lockedUntil = 0;
static bool     locked = false;

static uint32_t keyScreenUntil = 0;

// =====================
// HELPERS
// =====================
static void randomBytes(uint8_t* out, size_t n) {
  for (size_t i = 0; i < n; i += 4) {
    uint32_t r = esp_random();
    for (uint8_t j = 0; j < 4 && i + j < n; j++) { out[i + j] = r & 0xFF; r >>= 8; }
  }
}

static void toHex(const uint8_t* in, size_t n, char* out) {
  static const char hex[] = "0123456789abcdef";
  for (size_t i = 0; i < n; i++) { out[i * 2] = hex[in[i] >> 4]; out[i * 2 + 1] = hex[in[i] & 0xF]; }
  out[n * 2] = '\0';
}

static bool fromHex(const char* in, uint8_t* out, size_t n) {
  if (strlen(in) != n * 2) return false;
  for (size_t i = 0; i < n * 2; i++) {
    char c = in[i];
    uint8_t v = (c >= '0' && c <= '9') ? c - '0' : (c >= 'a' && c <= 'f') ? c - 'a' + 10 : 0xFF;
    if (v == 0xFF) return false;
    if (i & 1) out[i / 2] |= v; else out[i / 2] = v << 4;
  }
  return true;
}

// Compares two buffers without leaking where they differ through timing.
static bool constEq(const void* a, const void* b, size_t n) {
  const uint8_t* x = (const uint8_t*)a;
  const uint8_t* y = (const uint8_t*)b;
  uint8_t d = 0;
  for (size_t i = 0; i < n; i++) d |= x[i] ^ y[i];
  return d == 0;
}

// Salted, iterated SHA-256. Not a memory-hard KDF, but with the lockout
// it is plenty for a password that only crosses the local network.
static void deriveHash(const uint8_t* s, const char* pass, uint8_t* out) {
  uint8_t buf[32 + SALT_LEN + AUTH_PASS_MAX];
  size_t plen = strlen(pass);
  memcpy(buf, s, SALT_LEN);
  memcpy(buf + SALT_LEN, pass, plen);
  mbedtls_sha256_ret(buf, SALT_LEN + plen, out, 0);
  for (uint16_t i = 1; i < HASH_ROUNDS; i++) {
    memcpy(buf, out, 32);
    memcpy(buf + 32, s, SALT_LEN);
    memcpy(buf + 32 + SALT_LEN, pass, plen);
    mbedtls_sha256_ret(buf, 32 + SALT_LEN + plen, out, 0);
  }
}

static bool validUser(const char* u) {
  size_t n = strlen(u);
  if (n < 1 || n > AUTH_USER_MAX) return false;
  for (size_t i = 0; i < n; i++) {
    char c = u[i];
    if (!(isalnum((unsigned char)c) || c == '.' || c == '_' || c == '-')) return false;
  }
  return true;
}

// =====================
// ACCOUNT
// =====================
void authBegin() {
  Preferences prefs;
  prefs.begin(NVS_NS, true);
  char saltHex[SALT_LEN * 2 + 1] = "", hashHex[65] = "";
  prefs.getString("user", account, sizeof(account));
  prefs.getString("salt", saltHex, sizeof(saltHex));
  prefs.getString("hash", hashHex, sizeof(hashHex));
  prefs.end();
  configured = validUser(account) && fromHex(saltHex, salt, SALT_LEN) && fromHex(hashHex, hash, 32);
  if (!configured) account[0] = '\0';
  Log.printf("[AUTH] %s\n", configured ? "Account loaded" : "No account yet");
}

bool authConfigured() { return configured; }
const char* authUsername() { return account; }

bool authSetAccount(const char* user, const char* pass, char* err, size_t errLen) {
  if (!user || !validUser(user)) { snprintf(err, errLen, "username: 1-%u letters, digits, dots, dashes or underscores", AUTH_USER_MAX); return false; }
  size_t plen = pass ? strlen(pass) : 0;
  if (plen < AUTH_PASS_MIN || plen > AUTH_PASS_MAX) { snprintf(err, errLen, "password: %u-%u characters", AUTH_PASS_MIN, AUTH_PASS_MAX); return false; }

  uint8_t newSalt[SALT_LEN], newHash[32];
  randomBytes(newSalt, SALT_LEN);
  deriveHash(newSalt, pass, newHash);

  char saltHex[SALT_LEN * 2 + 1], hashHex[65];
  toHex(newSalt, SALT_LEN, saltHex);
  toHex(newHash, 32, hashHex);

  Preferences prefs;
  if (!prefs.begin(NVS_NS, false)) { snprintf(err, errLen, "storage error"); return false; }
  bool ok = prefs.putString("user", user) && prefs.putString("salt", saltHex) && prefs.putString("hash", hashHex);
  prefs.end();
  if (!ok) { snprintf(err, errLen, "failed to save account"); return false; }

  strlcpy(account, user, sizeof(account));
  memcpy(salt, newSalt, SALT_LEN);
  memcpy(hash, newHash, 32);
  configured = true;
  authSessionsClear();
  Log.printf("[AUTH] Account set for %s\n", account);
  return true;
}

bool authCheckPassword(const char* user, const char* pass) {
  if (!configured || !user || !pass) return false;
  size_t plen = strlen(pass);
  if (plen < 1 || plen > AUTH_PASS_MAX) return false;
  uint8_t h[32];
  deriveHash(salt, pass, h);
  // Compare the hash regardless of the username so a wrong name costs the same time.
  bool hashOk = constEq(h, hash, 32);
  bool userOk = strlen(user) == strlen(account) && constEq(user, account, strlen(account));
  return hashOk && userOk;
}

bool authCheckKey(const char* key) {
  if (!key) return false;
  size_t n = strlen(settings.apiToken);
  if (n == 0 || strlen(key) != n) return false;
  return constEq(key, settings.apiToken, n);
}

void authErase() {
  Preferences prefs;
  prefs.begin(NVS_NS, false);
  prefs.clear();
  prefs.end();
  configured = false;
  account[0] = '\0';
  authSessionsClear();
}

// =====================
// LOCKOUT
// =====================
uint32_t authLockedFor() {
  if (!locked) return 0;
  int32_t left = (int32_t)(lockedUntil - millis());
  if (left <= 0) { locked = false; failures = 0; return 0; }
  return (left + 999) / 1000;
}

void authNoteFailure() {
  if (++failures >= AUTH_MAX_FAILURES) {
    locked = true;
    lockedUntil = millis() + LOCKOUT_MS;
    Log.println("[AUTH] Too many failed attempts, locked");
  }
}

void authNoteSuccess() { failures = 0; locked = false; }

// =====================
// SESSIONS
// =====================
static Session* findSession(const char* sid) {
  if (!sid || strlen(sid) != AUTH_SID_LEN) return nullptr;
  uint32_t now = millis();
  for (uint8_t i = 0; i < MAX_SESSIONS; i++) {
    Session& s = sessions[i];
    if (!s.used) continue;
    if ((uint32_t)(now - s.lastSeen) > SESSION_IDLE_MS) { s.used = false; continue; }
    if (constEq(s.sid, sid, AUTH_SID_LEN)) return &s;
  }
  return nullptr;
}

const char* authSessionCreate() {
  uint32_t now = millis();
  // Reuse a free or expired slot, else evict the least recently seen.
  Session* pick = nullptr;
  for (uint8_t i = 0; i < MAX_SESSIONS; i++) {
    Session& s = sessions[i];
    if (!s.used || (uint32_t)(now - s.lastSeen) > SESSION_IDLE_MS) { pick = &s; break; }
    if (!pick || (int32_t)(s.lastSeen - pick->lastSeen) < 0) pick = &s;
  }
  uint8_t raw[AUTH_SID_LEN / 2];
  randomBytes(raw, sizeof(raw));
  toHex(raw, sizeof(raw), pick->sid);
  pick->lastSeen = now;
  pick->used = true;
  return pick->sid;
}

bool authSessionValid(const char* sid) {
  Session* s = findSession(sid);
  if (!s) return false;
  s->lastSeen = millis();
  return true;
}

void authSessionDrop(const char* sid) {
  Session* s = findSession(sid);
  if (s) s->used = false;
}

void authSessionsClear() {
  for (uint8_t i = 0; i < MAX_SESSIONS; i++) sessions[i].used = false;
}

bool authSidFromCookie(const String& cookie, char* out, size_t outLen) {
  int at = 0;
  while (at >= 0 && at < (int)cookie.length()) {
    int end = cookie.indexOf(';', at);
    String part = end < 0 ? cookie.substring(at) : cookie.substring(at, end);
    part.trim();
    if (part.startsWith("sid=")) {
      strlcpy(out, part.c_str() + 4, outLen);
      return strlen(out) == AUTH_SID_LEN;
    }
    at = end < 0 ? -1 : end + 1;
  }
  return false;
}

// =====================
// KEY ON SCREEN
// =====================
void authRevealKey(uint32_t ms) { keyScreenUntil = millis() + ms; }

bool authKeyScreenActive() { return (int32_t)(keyScreenUntil - millis()) > 0; }

uint32_t authKeyScreenSecondsLeft() {
  int32_t left = (int32_t)(keyScreenUntil - millis());
  return left > 0 ? (left + 999) / 1000 : 0;
}
