// LightLink ESP8266 controller
// Replacement brain for an Inova LightLink CFM16.200 (200x16 red/green LED sign)
#include <Arduino.h>
#include <ArduinoOTA.h>
#include <DNSServer.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <LittleFS.h>
#include <time.h>
#include "display.h"
#include "live.h"
#include "runner.h"
#include "settings.h"
#include "web.h"

static DNSServer dns;
static bool apMode = false;

static void splash(const char* line1, const char* line2) {
  SignCanvas& bootCanvas = Live::canvas();  // reuse: idle until packets arrive
  bootCanvas.clear();
  bootCanvas.setFont(nullptr);
  bootCanvas.setTextSize(1);
  bootCanvas.setTextWrap(false);
  bootCanvas.setTextColor(C_GREEN);
  bootCanvas.setCursor(1, 0);
  bootCanvas.print(line1);
  bootCanvas.setTextColor(C_ORANGE);
  bootCanvas.setCursor(1, 8);
  bootCanvas.print(line2);
  Display::show(bootCanvas);
}

static void startAP() {
  char name[24];
  snprintf(name, sizeof name, "LightLink-%04X", (unsigned)(ESP.getChipId() & 0xFFFF));
  WiFi.mode(WIFI_AP_STA);  // keep trying the configured network in the background
  WiFi.softAP(name, settings.apPass.c_str());
  dns.start(53, "*", WiFi.softAPIP());
  apMode = true;
  Serial.printf("AP mode: %s / %s  http://%s/\n", name, settings.apPass.c_str(),
                WiFi.softAPIP().toString().c_str());
  char l2[40];
  snprintf(l2, sizeof l2, "%s 192.168.4.1", name);
  splash("WiFi setup: join", l2);
}

void setup() {
  Serial.begin(115200);
  Serial.println("\nLightLink ESP8266 " FW_VERSION);
  Display::begin();  // first: stop the panels showing power-up garbage

  if (!LittleFS.begin()) {
    Serial.println("Formatting LittleFS...");
    LittleFS.format();
    LittleFS.begin();
  }
  LittleFS.mkdir("/seq");
  loadSettings();
  applyDisplaySettings();
  splash("LightLink ESP8266", "v" FW_VERSION " starting...");

  WiFi.persistent(false);
  WiFi.hostname(settings.hostname);
  WiFi.setAutoReconnect(true);
  if (settings.ssid.length()) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(settings.ssid, settings.pass);
    splash("Connecting to", settings.ssid.c_str());
    uint32_t t0 = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < 20000) delay(100);
  }
  if (WiFi.status() != WL_CONNECTED) startAP();
  else Serial.printf("Connected, IP %s\n", WiFi.localIP().toString().c_str());

  configTime(settings.tz.c_str(), settings.ntp.c_str());
  MDNS.begin(settings.hostname.c_str());
  MDNS.addService("http", "tcp", 80);

  ArduinoOTA.setHostname(settings.hostname.c_str());
  if (settings.webPass.length()) ArduinoOTA.setPassword(settings.webPass.c_str());
  ArduinoOTA.onStart([]() { Display::blank(); });
  ArduinoOTA.begin();

  Web::begin();
  Live::begin();
  Runner::begin();
}

void loop() {
  if (apMode) dns.processNextRequest();
  Web::loop();
  ArduinoOTA.handle();
  MDNS.update();
  Live::loop();
  Runner::loop();
}
