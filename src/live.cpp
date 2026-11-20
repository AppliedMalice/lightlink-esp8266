#include "live.h"
#include <WiFiUdp.h>
#include "pixelmap.h"
#include "settings.h"

namespace {
WiFiUDP ddpUdp;
WiFiUDP e131Udp;
bool ddpOn = false, e131On = false;
SignCanvas liveCanvas;
uint8_t pkt[1500];
uint32_t lastPacketMs = 0;
uint32_t lastDataMs = 0;
uint32_t pktCount = 0;
bool dirty = false;
const char* src = "";

void commit() {
  Display::show(liveCanvas);
  dirty = false;
}

void handleDdp(int len) {
  if (len < 10) return;
  uint8_t flags = pkt[0];
  if ((flags & 0xC0) != 0x40) return;   // version 1 only
  if (pkt[3] == 246 || pkt[3] == 250 || pkt[3] == 251) return;  // status/config/control queries
  int hdr = (flags & 0x10) ? 14 : 10;   // timecode present
  uint32_t off = ((uint32_t)pkt[4] << 24) | ((uint32_t)pkt[5] << 16) | (pkt[6] << 8) | pkt[7];
  uint16_t dlen = (pkt[8] << 8) | pkt[9];
  if (hdr + dlen > len) dlen = len - hdr;
  for (uint16_t i = 0; i < dlen; i++) applyChannel(liveCanvas, off + i, pkt[hdr + i]);
  dirty = true;
  src = "DDP";
  if (flags & 0x01) commit();  // push flag = end of frame
}

void handleE131(int len) {
  if (len < 126) return;
  if (memcmp(pkt + 4, "ASC-E1.17", 9) != 0) return;
  uint32_t vector = ((uint32_t)pkt[18] << 24) | ((uint32_t)pkt[19] << 16) | (pkt[20] << 8) | pkt[21];
  if (vector != 0x00000004) return;     // data packets only (ignore sync)
  if (pkt[125] != 0) return;            // DMX start code
  uint16_t univ = (pkt[113] << 8) | pkt[114];
  uint16_t count = ((pkt[123] << 8) | pkt[124]);
  if (count < 1) return;
  count -= 1;
  if (126 + count > len) count = len - 126;
  if (univ < settings.e131Universe) return;
  uint32_t usize = settings.e131UniverseSize;
  uint32_t base = (uint32_t)(univ - settings.e131Universe) * usize;
  if (base >= PIXEL_CHANNELS) return;
  if (count > usize) count = usize;
  for (uint16_t i = 0; i < count; i++) applyChannel(liveCanvas, base + i, pkt[126 + i]);
  dirty = true;
  src = "E1.31";
  uint16_t lastUniv = settings.e131Universe + (PIXEL_CHANNELS - 1) / usize;
  if (univ == lastUniv) commit();
}
}  // namespace

namespace Live {

void restart() {
  if (ddpOn) ddpUdp.stop();
  if (e131On) e131Udp.stop();
  ddpOn = settings.ddp && ddpUdp.begin(4048);
  e131On = settings.e131 && e131Udp.begin(5568);
}

void begin() { restart(); }

void loop() {
  for (int budget = 0; budget < 24; budget++) {
    bool got = false;
    if (ddpOn) {
      int n = ddpUdp.parsePacket();
      if (n > 0) {
        int len = ddpUdp.read(pkt, sizeof(pkt));
        got = true;
        lastPacketMs = millis();
        pktCount++;
        handleDdp(len);
      }
    }
    if (e131On) {
      int n = e131Udp.parsePacket();
      if (n > 0) {
        int len = e131Udp.read(pkt, sizeof(pkt));
        got = true;
        lastPacketMs = millis();
        pktCount++;
        handleE131(len);
      }
    }
    if (!got) break;
    lastDataMs = millis();
  }
  // Senders without push flags / partial universes: commit after a short gap
  if (dirty && millis() - lastDataMs > 30) commit();
}

bool active() { return pktCount && (millis() - lastPacketMs) < settings.liveTimeoutMs; }
uint32_t packets() { return pktCount; }
String source() { return active() ? String(src) : String(""); }
SignCanvas& canvas() { return liveCanvas; }

}  // namespace Live
