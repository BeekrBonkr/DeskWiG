#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <LovyanGFX.hpp>

// Images for layouts: PNG, JPEG and animated GIF, either uploaded to /img
// on the filesystem or fetched from an http(s) URL.
//
// An image source is a name ("sun" -> /img/sun.png) or a URL. Files are
// read into PSRAM the first time they are used; URLs are downloaded by a
// background task on their refresh interval, and like data sources only
// while a layout showing them is on screen.
//
// Decoding happens once per (source, size, fit) into a 16-bit sprite in
// PSRAM that is pushed each frame. GIFs keep a decoder open and advance
// a frame when its delay has elapsed.

constexpr uint8_t  MAX_IMAGE_FILES   = 24;
constexpr uint8_t  IMAGE_NAME_LEN    = 23;
constexpr uint8_t  IMAGE_SRC_LEN     = 159;
constexpr size_t   IMAGE_MAX_FILE    = 512 * 1024;
constexpr uint8_t  IMAGE_RAW_SLOTS   = 8;     // files + URLs held in PSRAM at once
constexpr uint8_t  IMAGE_DECODED     = 8;     // decoded sprites cached
constexpr uint16_t IMAGE_DEFAULT_REFRESH_S = 600;
constexpr uint16_t IMAGE_MIN_REFRESH_S = 30;

enum class ImgFit : uint8_t { CONTAIN, COVER, STRETCH };
enum class ImgType : uint8_t { UNKNOWN, PNG, JPG, GIF };

// Scans /img and starts the fetch task. Call after LittleFS is mounted.
void imagesBegin();
void imagesRescan();

// Lowercase letters, digits and dashes, 1-23 chars.
bool imageValidName(const char* name);

// Extension for a stored image name ("png", "jpg", "gif"), or "" if none.
const char* imageExt(const char* name);

// Full path of a stored image, or "" if there is none with that name.
String imagePath(const char* name);

// True for a stored name or any http(s) URL.
bool imageSourceOk(const char* src);

// Type from the first bytes of a file.
ImgType imageSniff(const uint8_t* data, size_t len);

// Removes a stored image.
bool imageDelete(const char* name, char* err, size_t errLen);

// Renames a stored image. Layouts that use the old name are not touched.
bool imageRename(const char* from, const char* to, char* err, size_t errLen);

// Every stored image with name, type and size.
void imagesToJson(JsonArray arr);

// Marks a URL source as on screen so it is fetched on its interval.
void imageTouch(const char* src, uint16_t refreshS);

// Native size of an image, loading it if needed. False if unavailable.
bool imageSize(const char* src, int16_t& w, int16_t& h);

// Draws an image into the w x h box at (x, y). Returns false if the
// image is not available yet (nothing is drawn).
bool imageDraw(lgfx::LGFX_Sprite& ui, const char* src, int16_t x, int16_t y, int16_t w, int16_t h,
               ImgFit fit, uint32_t now);

// Every URL source currently known, with fetch state, for the API.
void imageRemotesToJson(JsonArray arr);

// Decoded images currently cached, with GIF frame timings, for the API.
void imageDecodedToJson(JsonArray arr);
