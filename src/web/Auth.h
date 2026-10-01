#pragma once
#include <Arduino.h>

// Browser login: one account (username + salted password hash in NVS),
// a handful of cookie sessions in RAM, and a lockout after repeated
// failures. The short API key on the device screen is what lets the user
// create the account, reset a forgotten password, and drive the API from
// scripts with a bearer header.

constexpr uint8_t AUTH_USER_MAX  = 32;
constexpr uint8_t AUTH_PASS_MIN  = 8;
constexpr uint8_t AUTH_PASS_MAX  = 64;
constexpr uint8_t AUTH_SID_LEN   = 32;

// Loads the account from NVS. Call once after loadSettings().
void authBegin();

// True once a username and password have been set.
bool authConfigured();
const char* authUsername();

// Validates and stores an account. Fails (with a reason in err) on bad
// lengths or characters. Existing sessions are dropped.
bool authSetAccount(const char* user, const char* pass, char* err, size_t errLen);

// The stored account as hex strings, for a backup file. False if there
// is no account. Buffers: user AUTH_USER_MAX+1, salt 33, hash 65.
bool authExport(char* user, size_t userLen, char* saltHex, size_t saltLen, char* hashHex, size_t hashLen);

// Stores an account from a backup without knowing the password. Fails on
// a malformed username, salt or hash. Existing sessions are dropped.
bool authImport(const char* user, const char* saltHex, const char* hashHex, char* err, size_t errLen);

// Constant-time check of a login attempt. Does not touch the lockout.
bool authCheckPassword(const char* user, const char* pass);

// Constant-time check of the API key.
bool authCheckKey(const char* key);

// Lockout: after AUTH_MAX_FAILURES wrong attempts every attempt is
// rejected until the window passes. Returns seconds left, 0 = open.
uint32_t authLockedFor();
void authNoteFailure();
void authNoteSuccess();

// Sessions. Create returns a new id (valid until authSessionsClear or
// idle expiry); valid() also refreshes the idle timer.
const char* authSessionCreate();
bool authSessionValid(const char* sid);
void authSessionDrop(const char* sid);
void authSessionsClear();

// Pulls the sid out of a Cookie header value. Returns false if absent.
bool authSidFromCookie(const String& cookie, char* out, size_t outLen);

// Show the API key on the device screen for a while (forgot-password flow).
void authRevealKey(uint32_t ms);
bool authKeyScreenActive();
uint32_t authKeyScreenSecondsLeft();

// Erase the account (factory reset).
void authErase();
