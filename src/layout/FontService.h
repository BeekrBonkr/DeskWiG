#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <LovyanGFX.hpp>

// TrueType fonts for layouts, rasterised on the device with stb_truetype.
//
// Three fonts are built into the firmware: "sans" and "bold" (Inter,
// Latin subset) and "emoji" (Noto Emoji, monochrome subset). Users can
// upload more .ttf files to /fonts on the filesystem; they are loaded
// into PSRAM the first time a layout uses them.
//
// Glyphs are rasterised once per (font, size, codepoint) into an alpha
// bitmap cache in PSRAM and blended onto the sprite. Any codepoint the
// chosen font lacks falls back to the emoji font, then to "sans".
//
// stb_truetype does no bounds checking on the font file, so only fonts
// from a trusted source should be uploaded.

constexpr uint8_t  MAX_FONTS       = 12;
constexpr uint8_t  FONT_NAME_LEN   = 23;
constexpr size_t   FONT_MAX_FILE   = 2 * 1024 * 1024;
constexpr size_t   FONT_MAX_LOADED = 4 * 1024 * 1024;   // total PSRAM for uploaded fonts
constexpr uint16_t FONT_MIN_PX     = 6;
constexpr uint16_t FONT_MAX_PX     = 160;

// Registers the built-in fonts and scans /fonts. Call after LittleFS is mounted.
void fontsBegin();
void fontsRescan();

// Lowercase letters, digits and dashes, 1-23 chars.
bool fontValidName(const char* name);

// Index of a font by name, or -1.
int fontFind(const char* name);

// True if the named font exists (built in or uploaded).
bool fontExists(const char* name);

// Path of an uploaded font file (no existence check).
String fontPath(const char* name);

// Checks that data is a TrueType file stb_truetype can open.
bool fontValidate(const uint8_t* data, size_t len);

// Same check for a file on the filesystem (read into PSRAM temporarily).
bool fontValidateFile(const char* path);

// Removes an uploaded font. Built-ins cannot be deleted.
bool fontDelete(const char* name, char* err, size_t errLen);

// Built-in font bytes for serving to the browser; nullptr if not built in.
const uint8_t* fontBuiltinData(const char* name, size_t* len);

// Every font with name, size and whether it is built in.
void fontsToJson(JsonArray arr);

// Width of a UTF-8 string at px pixel line height, in the named font
// (fallbacks included). Returns 0 if the font is missing.
int16_t fontTextWidth(const char* font, const char* utf8, uint16_t px);

// Draws a UTF-8 string with its line box top-left at (x, y).
void fontDrawText(lgfx::LGFX_Sprite& ui, const char* font, const char* utf8, uint16_t px,
                  int16_t x, int16_t y, uint16_t color565);

// Draws a single codepoint from the emoji/sans fallback chain at px line
// height, for emoji inside bitmap-font text. Returns the advance. With
// ui == nullptr it only measures.
int16_t fontDrawFallbackGlyph(lgfx::LGFX_Sprite* ui, uint32_t cp, uint16_t px,
                              int16_t x, int16_t y, uint16_t color565);

// Decodes one UTF-8 codepoint; advances *s. Returns 0 at end of string.
uint32_t utf8Next(const char** s);
