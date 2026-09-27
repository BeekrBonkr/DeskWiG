#include "LayoutExpr.h"

#include <math.h>
#include "LayoutData.h"

namespace {

struct Value {
  double v;
  int8_t decimals;   // fixed by round(x, n); -1 = automatic
};

struct Parser {
  const char* p;
  bool err;
  int depth;

  void skip() { while (*p == ' ' || *p == '\t') p++; }

  bool isIdentStart(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }
  bool isIdentChar(char c)  { return isIdentStart(c) || (c >= '0' && c <= '9') || c == '.' || c == '-'; }

  // Reads a key such as ping.my-router.ms. Dashes are ambiguous with
  // minus, so if the greedy name is unknown, retry with the part before
  // each dash: "api.x.temp-3" becomes api.x.temp minus 3.
  bool variable(Value& out) {
    const char* start = p;
    while (isIdentChar(*p)) p++;
    size_t len = p - start;
    char name[64];
    char val[32];
    for (;;) {
      if (len == 0 || len >= sizeof(name)) return false;
      memcpy(name, start, len);
      name[len] = '\0';
      if (layoutResolveKey(name, val, sizeof(val))) {
        char* end;
        double d = strtod(val, &end);
        if (end == val) return false;   // no number in the value
        out.v = d;
        out.decimals = -1;
        p = start + len;
        return true;
      }
      size_t cut = len;
      while (cut > 0 && start[cut - 1] != '-') cut--;
      if (cut == 0) return false;
      len = cut - 1;
    }
  }

  bool args(Value* a, int maxArgs, int& n) {
    n = 0;
    skip();
    if (*p == ')') { p++; return true; }
    for (;;) {
      if (n >= maxArgs) return false;
      a[n] = expr();
      if (err) return false;
      n++;
      skip();
      if (*p == ',') { p++; continue; }
      if (*p == ')') { p++; return true; }
      return false;
    }
  }

  Value call(const char* name, size_t len) {
    Value a[4];
    int n;
    Value r = { NAN, -1 };
    if (!args(a, 4, n)) { err = true; return r; }
    auto is = [&](const char* s) { return strlen(s) == len && !strncmp(name, s, len); };

    if (is("round")) {
      if (n < 1 || n > 2) { err = true; return r; }
      int d = n == 2 ? (int)a[1].v : 0;
      if (d < 0) d = 0;
      if (d > 6) d = 6;
      double m = pow(10.0, d);
      r.v = round(a[0].v * m) / m;
      r.decimals = d;
    } else if (is("abs"))   { if (n != 1) { err = true; return r; } r.v = fabs(a[0].v); r.decimals = a[0].decimals; }
    else if (is("floor"))   { if (n != 1) { err = true; return r; } r.v = floor(a[0].v); r.decimals = 0; }
    else if (is("ceil"))    { if (n != 1) { err = true; return r; } r.v = ceil(a[0].v);  r.decimals = 0; }
    else if (is("sqrt"))    { if (n != 1) { err = true; return r; } r.v = sqrt(a[0].v); }
    else if (is("min") || is("max")) {
      if (n < 1) { err = true; return r; }
      bool mn = is("min");
      r = a[0];
      for (int i = 1; i < n; i++) if (mn ? a[i].v < r.v : a[i].v > r.v) r = a[i];
    } else if (is("clamp")) {
      if (n != 3) { err = true; return r; }
      r = a[0];
      if (r.v < a[1].v) r.v = a[1].v;
      if (r.v > a[2].v) r.v = a[2].v;
    } else {
      err = true;
    }
    return r;
  }

  Value primary() {
    Value r = { NAN, -1 };
    skip();
    if (++depth > 24) { err = true; return r; }

    if (*p == '(') {
      p++;
      r = expr();
      skip();
      if (*p != ')') err = true; else p++;
    } else if ((*p >= '0' && *p <= '9') || (*p == '.' && p[1] >= '0' && p[1] <= '9')) {
      char* end;
      r.v = strtod(p, &end);
      p = end;
    } else if (isIdentStart(*p)) {
      const char* start = p;
      while (isIdentStart(*p) || (*p >= '0' && *p <= '9')) p++;
      const char* save = p;
      skip();
      if (*p == '(') {
        p++;
        r = call(start, save - start);
      } else {
        p = start;
        if (!variable(r)) err = true;
      }
    } else {
      err = true;
    }
    depth--;
    return r;
  }

  Value unary() {
    skip();
    if (*p == '-') { p++; Value r = unary(); r.v = -r.v; return r; }
    if (*p == '+') { p++; return unary(); }
    Value r = primary();
    skip();
    if (*p == '^') { p++; Value e = unary(); r.v = pow(r.v, e.v); r.decimals = -1; }
    return r;
  }

  Value term() {
    Value r = unary();
    for (;;) {
      skip();
      char op = *p;
      if (op != '*' && op != '/' && op != '%') return r;
      p++;
      Value b = unary();
      if (err) return r;
      if (op == '*')      r.v = r.v * b.v;
      else if (op == '/') r.v = b.v == 0 ? NAN : r.v / b.v;
      else                r.v = b.v == 0 ? NAN : fmod(r.v, b.v);
      if (b.decimals > r.decimals) r.decimals = b.decimals;
    }
  }

  Value expr() {
    Value r = term();
    for (;;) {
      skip();
      char op = *p;
      if (op != '+' && op != '-') return r;
      p++;
      Value b = term();
      if (err) return r;
      r.v = op == '+' ? r.v + b.v : r.v - b.v;
      if (b.decimals > r.decimals) r.decimals = b.decimals;
    }
  }
};

void format(const Value& v, char* out, size_t n) {
  if (v.decimals >= 0) {
    snprintf(out, n, "%.*f", v.decimals, v.v);
    return;
  }
  if (v.v == floor(v.v) && fabs(v.v) < 1e15) {
    snprintf(out, n, "%.0f", v.v);
    return;
  }
  snprintf(out, n, "%.2f", v.v);
  // strip trailing zeros: 21.50 -> 21.5
  char* dot = strchr(out, '.');
  if (dot) {
    char* e = out + strlen(out) - 1;
    while (e > dot && *e == '0') *e-- = '\0';
    if (e == dot) *e = '\0';
  }
}

}  // namespace

// Only consulted after key lookup failed, so a dash can be treated as
// minus here even though key names may contain dashes.
bool layoutIsExpr(const char* body) {
  for (const char* c = body; *c; c++) {
    if (*c == '(' || *c == '+' || *c == '-' || *c == '*' || *c == '/' || *c == '%' || *c == '^' || *c == ' ') return true;
  }
  return body[0] >= '0' && body[0] <= '9';
}

bool layoutEval(const char* body, char* out, size_t n) {
  if (n == 0) return false;
  Parser ps = { body, false, 0 };
  Value v = ps.expr();
  ps.skip();
  if (ps.err || *ps.p != '\0' || isnan(v.v) || isinf(v.v)) {
    strlcpy(out, "--", n);
    return false;
  }
  format(v, out, n);
  return true;
}
