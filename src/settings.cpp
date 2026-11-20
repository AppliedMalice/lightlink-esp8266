#include "settings.h"
#include <LittleFS.h>
#include "display.h"

Settings settings;
static const char* CFG = "/config.json";

void settingsToJson(JsonDocument& d, bool secrets) {
  d["ssid"] = settings.ssid;
  d["pass"] = secrets ? settings.pass : (settings.pass.length() ? "********" : "");
  d["hostname"] = settings.hostname;
  d["apPass"] = secrets ? settings.apPass : "********";
  d["webUser"] = settings.webUser;
  d["webPass"] = secrets ? settings.webPass : (settings.webPass.length() ? "********" : "");
  d["tz"] = settings.tz;
  d["ntp"] = settings.ntp;
  d["brightness"] = settings.brightness;
  d["refreshHz"] = settings.refreshHz;
  d["reverseX"] = settings.reverseX;
  d["flipY"] = settings.flipY;
  d["swapBands"] = settings.swapBands;
  d["startChannel"] = settings.startChannel;
  d["serpentine"] = settings.serpentine;
  d["bottomOrigin"] = settings.bottomOrigin;
  d["threshold"] = settings.threshold;
  d["ddp"] = settings.ddp;
  d["e131"] = settings.e131;
  d["e131Universe"] = settings.e131Universe;
  d["e131UniverseSize"] = settings.e131UniverseSize;
  d["liveTimeoutMs"] = settings.liveTimeoutMs;
}

template <typename T>
static bool upd(JsonVariantConst v, const char* k, T& field) {
  if (v[k].isNull()) return false;
  T n = v[k].as<T>();
  bool changed = !(n == field);
  field = n;
  return changed;
}

static bool updSecret(JsonVariantConst v, const char* k, String& field) {
  if (v[k].isNull()) return false;
  String n = v[k].as<String>();
  if (n == "********") return false;  // masked value echoed back: keep
  bool changed = n != field;
  field = n;
  return changed;
}

bool settingsFromJson(JsonVariantConst v) {
  bool net = false;
  net |= upd(v, "ssid", settings.ssid);
  net |= updSecret(v, "pass", settings.pass);
  net |= upd(v, "hostname", settings.hostname);
  net |= updSecret(v, "apPass", settings.apPass);
  upd(v, "webUser", settings.webUser);
  updSecret(v, "webPass", settings.webPass);
  net |= upd(v, "tz", settings.tz);
  net |= upd(v, "ntp", settings.ntp);
  upd(v, "brightness", settings.brightness);
  upd(v, "refreshHz", settings.refreshHz);
  upd(v, "reverseX", settings.reverseX);
  upd(v, "flipY", settings.flipY);
  upd(v, "swapBands", settings.swapBands);
  upd(v, "startChannel", settings.startChannel);
  upd(v, "serpentine", settings.serpentine);
  upd(v, "bottomOrigin", settings.bottomOrigin);
  upd(v, "threshold", settings.threshold);
  upd(v, "ddp", settings.ddp);
  upd(v, "e131", settings.e131);
  upd(v, "e131Universe", settings.e131Universe);
  upd(v, "e131UniverseSize", settings.e131UniverseSize);
  upd(v, "liveTimeoutMs", settings.liveTimeoutMs);
  if (settings.startChannel < 1) settings.startChannel = 1;
  if (settings.e131UniverseSize < 3 || settings.e131UniverseSize > 512) settings.e131UniverseSize = 510;
  if (settings.e131Universe < 1) settings.e131Universe = 1;
  if (settings.apPass.length() < 8) settings.apPass = "lightlink";
  if (settings.hostname.length() == 0) settings.hostname = "lightlink";
  return net;
}

bool loadSettings() {
  File f = LittleFS.open(CFG, "r");
  if (!f) return false;
  JsonDocument d;
  DeserializationError e = deserializeJson(d, f);
  f.close();
  if (e) return false;
  settingsFromJson(d.as<JsonVariantConst>());
  return true;
}

bool saveSettings() {
  JsonDocument d;
  settingsToJson(d, true);
  File f = LittleFS.open(CFG, "w");
  if (!f) return false;
  serializeJson(d, f);
  f.close();
  return true;
}

void applyDisplaySettings() {
  Display::Mapping m;
  m.reverseX = settings.reverseX;
  m.flipY = settings.flipY;
  m.swapBands = settings.swapBands;
  Display::setMapping(m);
  Display::setBrightness(settings.brightness);
  Display::setRefreshHz(settings.refreshHz);
}
