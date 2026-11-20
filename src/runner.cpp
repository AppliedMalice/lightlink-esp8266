// Content engine: playlist of text/FSEQ items, temporary messages, test patterns.
#include "runner.h"
#include <ESP8266WiFi.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <LittleFS.h>
#include <time.h>
#include <vector>
#include "display.h"
#include "fseq.h"
#include "live.h"
#include "settings.h"

namespace {
struct FontDef {
  const GFXfont* f;
  uint8_t size;
  int8_t y;  // top row for built-in font, baseline for GFX fonts
  bool twoLine;
  const char* name;
};
const FontDef FONTS[] = {
    {nullptr, 1, 0, true, "Small 5x7 (two lines)"},
    {nullptr, 2, 1, false, "Block 10x14"},
    {&FreeSans9pt7b, 1, 13, false, "Sans 9pt"},
    {&FreeSansBold9pt7b, 1, 13, false, "Sans Bold 9pt"},
    {&FreeMonoBold9pt7b, 1, 12, false, "Mono Bold 9pt"},
};
const uint8_t NFONTS = sizeof(FONTS) / sizeof(FONTS[0]);
const char* PLAYLIST = "/playlist.json";
const size_t MAX_ITEMS = 40;

struct Show {
  Item item;
  bool active = false;
  uint32_t start = 0, lastRender = 0, lastStep = 0, lastExpand = 0;
  String lines[2];
  int nLines = 1;
  int16_t widths[2] = {0, 0};
  int16_t maxW = 0;
  float x = SIGN_W;
  uint16_t passes = 0;
  uint16_t loops = 0;
};

SignCanvas cv;
std::vector<Item> playlist;
int curIdx = -1;
int failures = 0;
bool idle = false;
Show cur;  // current playlist item (or idle banner)
Show msg;  // temporary message
FseqPlayer player;
String testPattern;
uint32_t testStart = 0, testLast = 0;
String lastError;

// ---------- text helpers ----------
String expandTokens(const String& in) {
  if (in.indexOf('{') < 0) return in;
  String s = in;
  time_t now = time(nullptr);
  bool ok = now > 1600000000;
  struct tm t;
  localtime_r(&now, &t);
  char b[40];
  if (s.indexOf("{time}") >= 0) {
    if (ok) strftime(b, sizeof b, "%H:%M", &t); else strcpy(b, "--:--");
    s.replace("{time}", b);
  }
  if (s.indexOf("{time12}") >= 0) {
    if (ok) {
      strftime(b, sizeof b, "%I:%M %p", &t);
      if (b[0] == '0') memmove(b, b + 1, strlen(b));
    } else strcpy(b, "--:--");
    s.replace("{time12}", b);
  }
  if (s.indexOf("{date}") >= 0) {
    if (ok) strftime(b, sizeof b, "%a %b %d", &t); else strcpy(b, "");
    s.replace("{date}", b);
  }
  if (s.indexOf("{ip}") >= 0) {
    IPAddress ip = WiFi.status() == WL_CONNECTED ? WiFi.localIP() : WiFi.softAPIP();
    s.replace("{ip}", ip.toString());
  }
  if (s.indexOf("{host}") >= 0) s.replace("{host}", settings.hostname + ".local");
  return s;
}

const FontDef& fontOf(const Item& it) { return FONTS[it.font < NFONTS ? it.font : 0]; }

int16_t advanceWidth(const FontDef& fd, const String& s) {
  if (!fd.f) return s.length() ? (int16_t)(s.length() * 6 * fd.size - fd.size) : 0;
  uint16_t first = pgm_read_word(&fd.f->first), last = pgm_read_word(&fd.f->last);
  const GFXglyph* glyphs = (const GFXglyph*)pgm_read_ptr(&fd.f->glyph);
  int16_t w = 0;
  for (size_t i = 0; i < s.length(); i++) {
    uint8_t c = s[i];
    if (c < first || c > last) continue;
    w += pgm_read_byte(&glyphs[c - first].xAdvance);
  }
  return w;
}

void prepare(Show& s) {
  const FontDef& fd = fontOf(s.item);
  String t = expandTokens(s.item.text);
  t.replace("\r", "");
  int sep = t.indexOf('\n');
  if (sep < 0) sep = t.indexOf('|');
  if (fd.twoLine && sep >= 0) {
    s.lines[0] = t.substring(0, sep);
    s.lines[1] = t.substring(sep + 1);
    s.lines[1].replace("\n", " ");
    s.lines[1].replace("|", " ");
    s.nLines = 2;
  } else {
    t.replace("\n", " ");
    t.replace("|", " ");
    s.lines[0] = t;
    s.lines[1] = "";
    s.nLines = 1;
  }
  s.maxW = 0;
  for (int l = 0; l < s.nLines; l++) {
    s.widths[l] = advanceWidth(fd, s.lines[l]);
    s.maxW = max(s.maxW, s.widths[l]);
  }
}

void startText(Show& s, const Item& it) {
  uint32_t now = millis();
  s.item = it;
  s.start = s.lastStep = s.lastExpand = now;
  s.lastRender = 0;
  s.passes = 0;
  s.x = SIGN_W;
  s.active = true;
  prepare(s);
}

uint16_t baseColor(uint8_t c) {
  switch (c) {
    case COL_RED: return C_RED;
    case COL_GREEN: return C_GREEN;
    default: return C_ORANGE;
  }
}

void drawLine(const FontDef& fd, const String& s, int16_t x, int16_t y, uint8_t color, uint32_t now) {
  static const uint16_t cyc[3] = {C_RED, C_ORANGE, C_GREEN};
  cv.setFont(fd.f);
  cv.setTextSize(fd.size);
  cv.setTextWrap(false);
  cv.setCursor(x, y);
  uint16_t base = baseColor(color);
  for (size_t i = 0; i < s.length(); i++) {
    if (cv.getCursorX() >= SIGN_W) break;
    cv.setTextColor(color == COL_CYCLE ? cyc[(i + now / 400) % 3] : base);
    cv.write((uint8_t)s[i]);
  }
}

// returns true when the item has finished
bool renderText(Show& s, uint32_t now) {
  const FontDef& fd = fontOf(s.item);
  uint32_t el = now - s.start;
  bool autoScroll = s.item.effect != FX_SCROLL && s.maxW > SIGN_W;
  bool scroll = s.item.effect == FX_SCROLL || autoScroll;
  bool done = false;

  if (!scroll && now - s.lastExpand >= 1000) {  // keep {time} etc. current
    prepare(s);
    s.lastExpand = now;
  }
  if (scroll) {
    float dt = (now - s.lastStep) / 1000.0f;
    s.lastStep = now;
    s.x -= dt * max<uint16_t>(s.item.speed, 1);
    if (s.x < -s.maxW) {
      s.passes++;
      if (autoScroll) {
        if (s.item.duration && el >= s.item.duration * 1000UL) done = true;
      } else if (s.item.duration && s.passes >= s.item.duration) {
        done = true;
      }
      prepare(s);
      s.x = SIGN_W;
    }
  } else if (s.item.duration && el >= s.item.duration * 1000UL) {
    done = true;
  }

  cv.clear();
  bool visible = s.item.effect != FX_BLINK || ((el / 500) % 2) == 0;
  if (visible) {
    for (int l = 0; l < s.nLines; l++) {
      int16_t x;
      if (scroll) x = (int16_t)s.x;
      else if (s.item.align == AL_LEFT) x = 0;
      else if (s.item.align == AL_RIGHT) x = SIGN_W - s.widths[l];
      else x = (SIGN_W - s.widths[l]) / 2;
      int16_t y = fd.twoLine ? (s.nLines == 2 ? l * 8 : 4) : fd.y;
      drawLine(fd, s.lines[l], x, y, s.item.color, now);
    }
  }
  return done;
}

// ---------- playlist ----------
void startIdle() {
  Item it;
  it.text = "{host}  {ip}";
  it.font = 2;
  it.color = COL_GREEN;
  it.effect = FX_SCROLL;
  it.speed = 35;
  it.duration = 0;
  if (lastError.length()) it.text = "Error: " + lastError + "   {ip}";
  idle = true;
  startText(cur, it);
}

void startItem(int idx) {
  player.close();
  cur.active = false;
  idle = false;
  if (playlist.empty()) { curIdx = -1; startIdle(); return; }
  curIdx = ((idx % (int)playlist.size()) + playlist.size()) % playlist.size();
  const Item& it = playlist[curIdx];
  if (it.type == IT_FSEQ) {
    if (!player.open("/seq/" + it.file)) {
      lastError = it.file + ": " + player.info().error;
      failures++;
      return;  // loop() advances
    }
    cur.item = it;
    cur.loops = 0;
    cur.active = true;
    cv.clear();
  } else {
    startText(cur, it);
  }
  failures = 0;
}

void advance() {
  if (failures >= (int)max<size_t>(playlist.size(), 1)) {  // everything is failing
    failures = 0;
    startIdle();
    cur.item.duration = 2;  // passes, then try the playlist again
    return;
  }
  startItem(curIdx + 1);
}

void restartCurrent() {
  if (cur.active && cur.item.type == IT_FSEQ && !idle) player.rewind();
  else if (cur.active) startText(cur, cur.item);
}

// ---------- test patterns ----------
void renderTest(uint32_t now) {
  uint32_t el = now - testStart;
  cv.clear();
  if (testPattern == "red") cv.fillScreen(C_RED);
  else if (testPattern == "green") cv.fillScreen(C_GREEN);
  else if (testPattern == "orange") cv.fillScreen(C_ORANGE);
  else if (testPattern == "rows") {
    int r = (el / 400) % SIGN_H;
    for (int x = 0; x < SIGN_W; x++) cv.setPx(x, r, PX_ORANGE);
  } else if (testPattern == "cols") {
    int c = (el / 25) % SIGN_W;
    for (int y = 0; y < SIGN_H; y++) cv.setPx(c, y, PX_ORANGE);
  } else if (testPattern == "checker") {
    int ph = (el / 500) & 1;
    for (int y = 0; y < SIGN_H; y++)
      for (int x = 0; x < SIGN_W; x++) cv.setPx(x, y, ((x + y + ph) & 1) ? PX_RED : PX_GREEN);
  } else if (testPattern == "panels") {
    for (int y = 0; y < SIGN_H; y++)
      for (int x = 0; x < SIGN_W; x++) cv.setPx(x, y, (((x / 40) + (y / 8)) & 1) ? PX_RED : PX_GREEN);
  } else {  // "ident": T1..T5 across the top band, B1..B5 across the bottom
    const FontDef& fd = FONTS[0];
    for (int p = 0; p < 5; p++) {
      drawLine(fd, "T" + String(p + 1) + ">", p * 40 + 2, 0, COL_GREEN, now);
      drawLine(fd, "B" + String(p + 1) + ">", p * 40 + 2, 8, COL_RED, now);
    }
    for (int y = 0; y < SIGN_H; y++) cv.setPx(0, y, PX_ORANGE);  // left edge marker
  }
}
}  // namespace

namespace Runner {

const char* fontName(uint8_t i) { return i < NFONTS ? FONTS[i].name : ""; }
uint8_t fontCount() { return NFONTS; }

bool itemFromJson(JsonVariantConst v, Item& it, String& err) {
  String type = v["type"] | "text";
  it = Item();
  if (type == "fseq") {
    it.type = IT_FSEQ;
    it.file = v["file"] | "";
    it.duration = v["duration"] | 1;
    if (it.file.indexOf('/') >= 0 || it.file.length() == 0) { err = "bad fseq file name"; return false; }
    return true;
  }
  it.type = IT_TEXT;
  it.text = v["text"] | "";
  it.font = v["font"] | 2;
  it.color = v["color"] | (uint8_t)COL_GREEN;
  it.effect = v["effect"] | (uint8_t)FX_SCROLL;
  it.align = v["align"] | (uint8_t)AL_CENTER;
  it.speed = v["speed"] | 40;
  it.duration = v["duration"] | 1;
  if (it.text.length() == 0) { err = "empty text"; return false; }
  if (it.text.length() > 400) it.text = it.text.substring(0, 400);
  if (it.font >= NFONTS) it.font = 2;
  if (it.color > COL_CYCLE) it.color = COL_GREEN;
  if (it.effect > FX_BLINK) it.effect = FX_SCROLL;
  if (it.align > AL_RIGHT) it.align = AL_CENTER;
  return true;
}

static void itemToJson(const Item& it, JsonObject o) {
  if (it.type == IT_FSEQ) {
    o["type"] = "fseq";
    o["file"] = it.file;
    o["duration"] = it.duration;
    return;
  }
  o["type"] = "text";
  o["text"] = it.text;
  o["font"] = it.font;
  o["color"] = it.color;
  o["effect"] = it.effect;
  o["align"] = it.align;
  o["speed"] = it.speed;
  o["duration"] = it.duration;
}

void playlistToJson(JsonDocument& d) {
  JsonArray a = d["items"].to<JsonArray>();
  for (auto& it : playlist) itemToJson(it, a.add<JsonObject>());
  JsonArray f = d["fonts"].to<JsonArray>();
  for (uint8_t i = 0; i < NFONTS; i++) f.add(FONTS[i].name);
}

bool loadPlaylist() {
  playlist.clear();
  File f = LittleFS.open(PLAYLIST, "r");
  if (f) {
    JsonDocument d;
    if (!deserializeJson(d, f)) {
      for (JsonVariantConst v : d["items"].as<JsonArrayConst>()) {
        Item it;
        String e;
        if (itemFromJson(v, it, e) && playlist.size() < MAX_ITEMS) playlist.push_back(it);
      }
    }
    f.close();
  }
  return !playlist.empty();
}

bool savePlaylist(JsonVariantConst items, String& err) {
  std::vector<Item> next;
  int i = 0;
  for (JsonVariantConst v : items.as<JsonArrayConst>()) {
    Item it;
    String e;
    i++;
    if (!itemFromJson(v, it, e)) { err = "item " + String(i) + ": " + e; return false; }
    if (next.size() >= MAX_ITEMS) { err = "too many items"; return false; }
    next.push_back(it);
  }
  playlist = next;
  JsonDocument d;
  playlistToJson(d);
  d.remove("fonts");
  File f = LittleFS.open(PLAYLIST, "w");
  if (!f) { err = "cannot write playlist"; return false; }
  serializeJson(d, f);
  f.close();
  lastError = "";
  failures = 0;
  startItem(0);
  return true;
}

void begin() {
  loadPlaylist();
  // Like the original controller: announce the address once at power-up
  Item hello;
  hello.text = "{host}  {ip}";
  hello.font = 2;
  hello.color = COL_ORANGE;
  hello.effect = FX_SCROLL;
  hello.speed = 45;
  hello.duration = 1;
  startItem(0);
  showMessage(hello);
}

void showMessage(const Item& it) {
  startText(msg, it);
  if (cur.item.type == IT_FSEQ) player.rewind();
}

void clearMessage() {
  msg.active = false;
  restartCurrent();
}

void setTest(const String& p) {
  testPattern = p;
  testStart = millis();
  if (!p.length()) restartCurrent();
}

void skip() {
  if (msg.active) { clearMessage(); return; }
  startItem(curIdx + 1);
}

void loop() {
  uint32_t now = millis();
  if (testPattern.length()) {
    if (now - testLast >= 25) {
      testLast = now;
      renderTest(now);
      Display::show(cv);
    }
    return;
  }
  if (Live::active()) return;  // FPP/xLights owns the sign

  if (msg.active) {
    if (now - msg.lastRender >= 20) {
      msg.lastRender = now;
      bool done = renderText(msg, now);
      Display::show(cv);
      if (done) clearMessage();
    }
    return;
  }

  if (!cur.active) { advance(); return; }

  if (cur.item.type == IT_FSEQ && !idle) {
    bool ended;
    if (player.update(cv, ended)) Display::show(cv);
    if (ended) {
      cur.loops++;
      if (cur.item.duration && cur.loops >= cur.item.duration) advance();
      else player.rewind();
    }
  } else if (now - cur.lastRender >= 20) {
    cur.lastRender = now;
    bool done = renderText(cur, now);
    Display::show(cv);
    if (done) {
      if (idle && playlist.empty()) startIdle();
      else advance();
    }
  }
}

void status(JsonDocument& d) {
  const char* mode = testPattern.length() ? "test"
                     : Live::active()     ? "live"
                     : msg.active         ? "message"
                     : idle               ? "idle"
                                          : "playlist";
  d["mode"] = mode;
  d["test"] = testPattern;
  d["index"] = curIdx;
  d["count"] = playlist.size();
  if (msg.active) d["now"] = msg.lines[0];
  else if (cur.active && cur.item.type == IT_FSEQ && !idle) {
    d["now"] = cur.item.file;
    d["frame"] = player.currentFrame();
    d["frames"] = player.info().frames;
  } else d["now"] = cur.lines[0];
  d["error"] = lastError;
}

}  // namespace Runner
