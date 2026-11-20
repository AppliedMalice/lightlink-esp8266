#include "fseq.h"
#include <LittleFS.h>
#include "pixelmap.h"

static uint32_t rd16(const uint8_t* p) { return p[0] | (p[1] << 8); }
static uint32_t rd24(const uint8_t* p) { return p[0] | (p[1] << 8) | ((uint32_t)p[2] << 16); }
static uint32_t rd32(const uint8_t* p) {
  return p[0] | (p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

bool FseqPlayer::parse(File& f, FseqInfo& inf, std::vector<Range>* ranges) {
  uint8_t h[32];
  f.seek(0);
  if (f.read(h, sizeof(h)) < 28) { inf.error = "file too short"; return false; }
  if (!((h[0] == 'P' || h[0] == 'F') && h[1] == 'S' && h[2] == 'E' && h[3] == 'Q')) {
    inf.error = "not an fseq file";
    return false;
  }
  inf.dataOffset = rd16(h + 4);
  inf.minor = h[6];
  inf.major = h[7];
  inf.channels = rd32(h + 10);
  inf.frames = rd32(h + 14);
  inf.stepMs = h[18] ? h[18] : 50;
  inf.compression = 0;
  uint32_t frameSize = inf.channels;
  if (ranges) ranges->clear();

  if (inf.major >= 2) {
    inf.compression = h[20] & 0x0F;
    uint32_t blocks = h[21] | ((h[20] & 0xF0) << 4);
    uint8_t sparse = h[22];
    if (inf.compression != 0) {
      inf.error = "compressed fseq: export from xLights as 'V2 Uncompressed'";
      return false;
    }
    if (sparse && ranges) {
      f.seek(32 + blocks * 8);
      uint32_t off = 0;
      for (int i = 0; i < sparse; i++) {
        uint8_t r[6];
        if (f.read(r, 6) != 6) { inf.error = "bad sparse table"; return false; }
        Range rg{rd24(r), rd24(r + 3), off};
        off += rg.count;
        ranges->push_back(rg);
      }
      frameSize = off;
      inf.channels = off;
    }
  }
  if (ranges && ranges->empty()) ranges->push_back(Range{0, frameSize, 0});
  uint64_t need = (uint64_t)inf.dataOffset + (uint64_t)frameSize * inf.frames;
  if (f.size() < need) {
    // tolerate a truncated last frame, but recompute the playable count
    if (frameSize) inf.frames = (f.size() - inf.dataOffset) / frameSize;
  }
  if (inf.frames == 0) { inf.error = "no frames"; return false; }
  return true;
}

bool FseqPlayer::probe(const String& path, FseqInfo& info) {
  File f = LittleFS.open(path, "r");
  if (!f) { info.error = "cannot open"; return false; }
  std::vector<Range> r;
  bool ok = parse(f, info, &r);
  f.close();
  return ok;
}

bool FseqPlayer::open(const String& path) {
  close();
  file = LittleFS.open(path, "r");
  if (!file) { inf.error = "cannot open"; return false; }
  if (!parse(file, inf, &ranges)) { file.close(); return false; }
  frameSize = 0;
  for (auto& r : ranges) frameSize += r.count;
  rewind();
  return true;
}

void FseqPlayer::close() {
  if (file) file.close();
  ranges.clear();
  cur = -1;
}

void FseqPlayer::rewind() {
  startMs = millis();
  cur = -1;
}

bool FseqPlayer::renderFrame(SignCanvas& c, uint32_t frame) {
  static uint8_t buf[600];
  c.clear();
  const uint32_t want0 = settings.startChannel - 1;  // absolute 0-based channel
  const uint32_t want1 = want0 + PIXEL_CHANNELS;
  const uint32_t base = inf.dataOffset + frame * frameSize;
  for (auto& r : ranges) {
    uint32_t a = max(want0, r.start);
    uint32_t b = min(want1, r.start + r.count);
    if (a >= b) continue;
    if (!file.seek(base + r.frameOff + (a - r.start))) return false;
    uint32_t ch = a - want0;
    uint32_t left = b - a;
    while (left) {
      size_t n = file.read(buf, min<uint32_t>(left, sizeof(buf)));
      if (n == 0) return false;
      for (size_t i = 0; i < n; i++) applyChannel(c, ch + i, buf[i]);
      ch += n;
      left -= n;
    }
  }
  return true;
}

bool FseqPlayer::update(SignCanvas& c, bool& ended) {
  ended = false;
  if (!file) { ended = true; return false; }
  int32_t due = (millis() - startMs) / inf.stepMs;
  if (due >= (int32_t)inf.frames) { ended = true; return false; }
  if (due == cur) return false;
  cur = due;  // frames are skipped if flash reads fall behind, timing stays correct
  if (!renderFrame(c, cur)) { ended = true; return false; }
  return true;
}
