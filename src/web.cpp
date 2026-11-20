#include "web.h"
#include <ESP8266HTTPUpdateServer.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <LittleFS.h>
#include "display.h"
#include "fseq.h"
#include "live.h"
#include "runner.h"
#include "settings.h"
#include "webui.h"

namespace {
ESP8266WebServer server(80);
ESP8266HTTPUpdateServer updater;
uint32_t rebootAt = 0;

File upFile;
String upName, upErr;

bool authed() {
  if (!settings.webPass.length()) return true;
  if (server.authenticate(settings.webUser.c_str(), settings.webPass.c_str())) return true;
  server.requestAuthentication(BASIC_AUTH, "LightLink");
  return false;
}

void sendJson(JsonDocument& d, int code = 200) {
  String out;
  serializeJson(d, out);
  server.send(code, "application/json", out);
}

void sendErr(const String& e, int code = 400) {
  JsonDocument d;
  d["ok"] = false;
  d["error"] = e;
  sendJson(d, code);
}

void sendOk() { server.send(200, "application/json", "{\"ok\":true}"); }

bool bodyJson(JsonDocument& d) {
  if (!server.hasArg("plain")) return false;
  return !deserializeJson(d, server.arg("plain"));
}

// accept "red"/"green"/"orange"/"cycle", "static"/"scroll"/"blink", ... or numbers
uint8_t enumArg(const String& v, const char* const* names, uint8_t n, uint8_t def) {
  if (!v.length()) return def;
  for (uint8_t i = 0; i < n; i++)
    if (v.equalsIgnoreCase(names[i])) return i;
  if (isDigit(v[0])) return (uint8_t)v.toInt();
  return def;
}
const char* const COLORS[] = {"red", "green", "orange", "cycle"};
const char* const EFFECTS[] = {"static", "scroll", "blink"};
const char* const ALIGNS[] = {"left", "center", "right"};

String sanitizeName(String n) {
  int s = max(n.lastIndexOf('/'), n.lastIndexOf('\\'));
  if (s >= 0) n = n.substring(s + 1);
  String out;
  for (size_t i = 0; i < n.length(); i++) {
    char c = n[i];
    out += (isAlphaNumeric(c) || c == '.' || c == '-' || c == '_') ? c : '_';
  }
  String low = out;
  low.toLowerCase();
  if (!low.endsWith(".fseq")) return "";
  String stem = out.substring(0, out.length() - 5);
  if (stem.length() > 21) stem = stem.substring(0, 21);  // LittleFS 31 char names incl ".fseq.part"
  if (!stem.length()) return "";
  return stem + ".fseq";
}

// ---------------- handlers ----------------
void hRoot() {
  if (!authed()) return;
  server.send_P(200, "text/html", WEB_UI);
}

void hStatus() {
  if (!authed()) return;
  static uint32_t lastCount = 0, lastMs = 0;
  static float hz = 0;
  uint32_t now = millis(), cnt = Display::refreshCount();
  if (now - lastMs >= 1000) {
    if (lastMs) hz = (cnt - lastCount) * 1000.0f / (now - lastMs);
    lastCount = cnt;
    lastMs = now;
  }
  JsonDocument d;
  Runner::status(d);
  d["version"] = FW_VERSION;
  d["uptime"] = now / 1000;
  d["heap"] = ESP.getFreeHeap();
  d["refreshHz"] = (int)hz;
  bool sta = WiFi.status() == WL_CONNECTED;
  d["ip"] = sta ? WiFi.localIP().toString() : WiFi.softAPIP().toString();
  d["ssid"] = sta ? WiFi.SSID() : String("");
  d["rssi"] = sta ? WiFi.RSSI() : 0;
  d["ap"] = (WiFi.getMode() & WIFI_AP) != 0;
  d["hostname"] = settings.hostname;
  d["livePackets"] = Live::packets();
  d["liveSource"] = Live::source();
  d["brightness"] = settings.brightness;
  FSInfo fi;
  LittleFS.info(fi);
  d["fsTotal"] = fi.totalBytes;
  d["fsUsed"] = fi.usedBytes;
  time_t t = time(nullptr);
  d["timeValid"] = t > 1600000000;
  sendJson(d);
}

void hGetConfig() {
  if (!authed()) return;
  JsonDocument d;
  settingsToJson(d, false);
  sendJson(d);
}

void hSetConfig() {
  if (!authed()) return;
  JsonDocument d;
  if (!bodyJson(d)) return sendErr("bad json");
  bool net = settingsFromJson(d.as<JsonVariantConst>());
  if (!saveSettings()) return sendErr("could not save", 500);
  applyDisplaySettings();
  Live::restart();
  configTime(settings.tz.c_str(), settings.ntp.c_str());
  JsonDocument r;
  r["ok"] = true;
  r["reboot"] = net;
  if (net) rebootAt = millis() + 1500;
  sendJson(r);
}

void hBrightness() {
  if (!authed()) return;
  if (!server.hasArg("value")) return sendErr("value required");
  settings.brightness = constrain(server.arg("value").toInt(), 5, 100);
  Display::setBrightness(settings.brightness);
  if (server.arg("save") != "0") saveSettings();
  sendOk();
}

void hGetPlaylist() {
  if (!authed()) return;
  JsonDocument d;
  Runner::playlistToJson(d);
  sendJson(d);
}

void hSetPlaylist() {
  if (!authed()) return;
  JsonDocument d;
  if (!bodyJson(d)) return sendErr("bad json");
  String err;
  if (!Runner::savePlaylist(d["items"], err)) return sendErr(err);
  sendOk();
}

void hMessage() {
  if (!authed()) return;
  JsonDocument d;
  Item it;
  String err;
  if (server.hasArg("plain") && bodyJson(d)) {
    if (!Runner::itemFromJson(d.as<JsonVariantConst>(), it, err)) return sendErr(err);
  } else {
    it.type = IT_TEXT;
    it.text = server.arg("text");
    if (!it.text.length()) return sendErr("text required");
    it.color = enumArg(server.arg("color"), COLORS, 4, COL_ORANGE);
    it.effect = enumArg(server.arg("effect"), EFFECTS, 3, FX_SCROLL);
    it.align = enumArg(server.arg("align"), ALIGNS, 3, AL_CENTER);
    if (server.hasArg("font")) it.font = server.arg("font").toInt();
    if (server.hasArg("speed")) it.speed = server.arg("speed").toInt();
    it.duration = server.hasArg("duration") ? server.arg("duration").toInt() : (it.effect == FX_SCROLL ? 2 : 10);
    if (it.font >= Runner::fontCount()) it.font = 2;
  }
  if (it.type != IT_TEXT) return sendErr("messages must be text");
  Runner::showMessage(it);
  sendOk();
}

void hClear() {
  if (!authed()) return;
  Runner::clearMessage();
  sendOk();
}

void hSkip() {
  if (!authed()) return;
  Runner::skip();
  sendOk();
}

void hTest() {
  if (!authed()) return;
  Runner::setTest(server.arg("pattern"));
  sendOk();
}

void hFiles() {
  if (!authed()) return;
  JsonDocument d;
  JsonArray a = d["files"].to<JsonArray>();
  Dir dir = LittleFS.openDir("/seq");
  while (dir.next()) {
    if (dir.isDirectory() || dir.fileName().endsWith(".part")) continue;
    JsonObject o = a.add<JsonObject>();
    o["name"] = dir.fileName();
    o["size"] = dir.fileSize();
    FseqInfo fi;
    if (FseqPlayer::probe("/seq/" + dir.fileName(), fi)) {
      o["frames"] = fi.frames;
      o["stepMs"] = fi.stepMs;
      o["channels"] = fi.channels;
      o["version"] = String(fi.major) + "." + String(fi.minor);
    } else {
      o["error"] = fi.error;
    }
  }
  FSInfo fi;
  LittleFS.info(fi);
  d["fsTotal"] = fi.totalBytes;
  d["fsUsed"] = fi.usedBytes;
  sendJson(d);
}

void hDelete() {
  if (!authed()) return;
  String n = sanitizeName(server.arg("name"));
  if (!n.length() || !LittleFS.remove("/seq/" + n)) return sendErr("not found", 404);
  sendOk();
}

void hUploadData() {
  HTTPUpload& u = server.upload();
  if (u.status == UPLOAD_FILE_START) {
    upErr = "";
    upName = sanitizeName(u.filename);
    if (settings.webPass.length() &&
        !server.authenticate(settings.webUser.c_str(), settings.webPass.c_str())) {
      upErr = "unauthorized";
      return;
    }
    if (!upName.length()) { upErr = "file must be a .fseq"; return; }
    LittleFS.mkdir("/seq");
    upFile = LittleFS.open("/seq/" + upName + ".part", "w");
    if (!upFile) upErr = "cannot create file";
  } else if (u.status == UPLOAD_FILE_WRITE) {
    if (upFile && !upErr.length() && upFile.write(u.buf, u.currentSize) != u.currentSize) {
      upErr = "write failed (filesystem full?)";
    }
  } else if (u.status == UPLOAD_FILE_END || u.status == UPLOAD_FILE_ABORTED) {
    if (upFile) upFile.close();
    String part = "/seq/" + upName + ".part";
    if (u.status == UPLOAD_FILE_ABORTED && !upErr.length()) upErr = "upload aborted";
    if (upErr.length()) {
      LittleFS.remove(part);
    } else {
      LittleFS.remove("/seq/" + upName);
      LittleFS.rename(part, "/seq/" + upName);
    }
  }
}

void hUploadDone() {
  if (upErr.length()) return sendErr(upErr);
  FseqInfo fi;
  JsonDocument d;
  d["ok"] = true;
  d["name"] = upName;
  if (FseqPlayer::probe("/seq/" + upName, fi)) {
    d["frames"] = fi.frames;
    d["stepMs"] = fi.stepMs;
    d["channels"] = fi.channels;
  } else {
    d["warning"] = fi.error;
  }
  sendJson(d);
}

void hFrame() {
  if (!authed()) return;
  const SignCanvas* c = Display::lastShown();
  String out;
  out.reserve((SIGN_W + 1) * SIGN_H);
  for (int y = 0; y < SIGN_H; y++) {
    for (int x = 0; x < SIGN_W; x++) out += (char)('0' + (c ? (c->pix[y][x] & 3) : 0));
    out += '\n';
  }
  server.send(200, "text/plain", out);
}

void hReboot() {
  if (!authed()) return;
  rebootAt = millis() + 500;
  sendOk();
}

void hNotFound() {
  // captive portal: send everything unknown to the UI while in AP mode
  if ((WiFi.getMode() & WIFI_AP) && !server.uri().startsWith("/api")) {
    server.sendHeader("Location", String("http://") + WiFi.softAPIP().toString() + "/", true);
    server.send(302, "text/plain", "");
    return;
  }
  server.send(404, "text/plain", "not found");
}
}  // namespace

namespace Web {
void begin() {
  server.on("/", HTTP_GET, hRoot);
  server.on("/api/status", HTTP_GET, hStatus);
  server.on("/api/config", HTTP_GET, hGetConfig);
  server.on("/api/config", HTTP_POST, hSetConfig);
  server.on("/api/brightness", HTTP_ANY, hBrightness);
  server.on("/api/playlist", HTTP_GET, hGetPlaylist);
  server.on("/api/playlist", HTTP_POST, hSetPlaylist);
  server.on("/api/message", HTTP_ANY, hMessage);
  server.on("/api/clear", HTTP_ANY, hClear);
  server.on("/api/skip", HTTP_ANY, hSkip);
  server.on("/api/test", HTTP_ANY, hTest);
  server.on("/api/files", HTTP_GET, hFiles);
  server.on("/api/files", HTTP_DELETE, hDelete);
  server.on("/api/delete", HTTP_POST, hDelete);
  server.on("/api/upload", HTTP_POST, hUploadDone, hUploadData);
  server.on("/api/frame", HTTP_GET, hFrame);
  server.on("/api/reboot", HTTP_POST, hReboot);
  server.onNotFound(hNotFound);
  if (settings.webPass.length())
    updater.setup(&server, "/update", settings.webUser.c_str(), settings.webPass.c_str());
  else
    updater.setup(&server, "/update");
  server.begin();
}

void loop() {
  server.handleClient();
  if (rebootAt && (int32_t)(millis() - rebootAt) >= 0) {
    Display::blank();
    delay(50);
    ESP.restart();
  }
}
}  // namespace Web
