#pragma once
#include <Arduino.h>

// Arithmetic inside layout braces. A brace body that is not a plain key
// is evaluated as an expression in which keys are variables:
//
//   {round(api.weather.temp * 9/5 + 32, 1)}
//   {min(ping.0.ms / 2, 100)}
//
// Operators: + - * / % ^ and parentheses, comparisons < > <= >= == !=
// (giving 1 or 0), && || and !. Functions: round(x[, n]), abs, min, max,
// floor, ceil, sqrt, clamp(x, lo, hi), if(cond, a, b), lerp(a, b, t).
// Colour functions give a number 0xRRGGBB that colour properties accept:
// rgb(r, g, b), hsv(h, s, v) and mix(c1, c2, t).
//
// Key values are read as numbers (a leading number is enough, so "12s"
// is 12). A key that is unknown or has no number makes the whole
// expression "--", the same as an unknown key.
//
// Results print as integers when whole, otherwise with up to two
// decimals; round(x, n) fixes the number of decimals.

// True if the brace body looks like an expression rather than a key.
bool layoutIsExpr(const char* body);

// Evaluates body into out. Returns false (out = "--") on any error.
bool layoutEval(const char* body, char* out, size_t outLen);

// Evaluates body as a number. Returns false on any error.
bool layoutEvalNumber(const char* body, double& out);
