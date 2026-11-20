#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

struct Settings {
  // network
  String ssid;
  String pass;
  String hostname = "lightlink";
  String apPass = "lightlink";      // setup AP password (8+ chars)
  String webUser = "admin";
  String webPass;                   // empty = no auth on the web UI / API
  String tz = "EST5EDT,M3.2.0,M11.1.0";
  String ntp = "pool.ntp.org";
  // display
  uint8_t brightness = 60;
  uint16_t refreshHz = 150;
  bool reverseX = false;
  bool flipY = false;
  bool swapBands = false;
  // pixel data (FSEQ + live)
  uint32_t startChannel = 1;        // 1-based channel of pixel 0 in FSEQ / E1.31
  bool serpentine = false;
  bool bottomOrigin = false;
  uint8_t threshold = 64;           // channel value that counts as "on"
  // live inputs
  bool ddp = true;
  bool e131 = true;
  uint16_t e131Universe = 1;
  uint16_t e131UniverseSize = 510;
  uint16_t liveTimeoutMs = 2500;
};

extern Settings settings;

bool loadSettings();
bool saveSettings();
void settingsToJson(JsonDocument& doc, bool withSecrets);
// returns true if a network setting changed (reboot needed)
bool settingsFromJson(JsonVariantConst v);
void applyDisplaySettings();
