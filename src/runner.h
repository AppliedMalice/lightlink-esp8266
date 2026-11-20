#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

enum ItemType : uint8_t { IT_TEXT = 0, IT_FSEQ = 1 };
enum Color : uint8_t { COL_RED = 0, COL_GREEN = 1, COL_ORANGE = 2, COL_CYCLE = 3 };
enum Effect : uint8_t { FX_STATIC = 0, FX_SCROLL = 1, FX_BLINK = 2 };
enum Align : uint8_t { AL_LEFT = 0, AL_CENTER = 1, AL_RIGHT = 2 };

struct Item {
  ItemType type = IT_TEXT;
  String text;            // supports {time} {time12} {date} {ip} {host}; '|' or newline = 2nd line (small font)
  uint8_t font = 2;
  uint8_t color = COL_GREEN;
  uint8_t effect = FX_SCROLL;
  uint8_t align = AL_CENTER;
  uint16_t speed = 40;    // scroll speed, pixels per second
  uint16_t duration = 1;  // static/blink: seconds; scroll: passes; fseq: loops; 0 = forever
  String file;            // fseq file name (without /seq/)
};

namespace Runner {
void begin();
void loop();
bool loadPlaylist();
bool savePlaylist(JsonVariantConst items, String& err);
void playlistToJson(JsonDocument& d);
void showMessage(const Item& it);  // temporary override until it completes
void clearMessage();
void setTest(const String& pattern);  // "" to stop
void skip();
void status(JsonDocument& d);
bool itemFromJson(JsonVariantConst v, Item& it, String& err);
const char* fontName(uint8_t i);
uint8_t fontCount();
}  // namespace Runner
