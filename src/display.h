#pragma once
#include <Adafruit_GFX.h>
#include "config.h"

// RGB565 colors understood by the canvas (anything with enough red lights red,
// enough green lights green, both = orange/amber)
#define C_OFF    0x0000
#define C_RED    0xF800
#define C_GREEN  0x07E0
#define C_ORANGE 0xFFE0

// 200x16 framebuffer, 2 bits per pixel (PX_RED | PX_GREEN), drawable with
// all the Adafruit_GFX primitives and fonts.
class SignCanvas : public Adafruit_GFX {
 public:
  SignCanvas() : Adafruit_GFX(SIGN_W, SIGN_H) { clear(); }
  void drawPixel(int16_t x, int16_t y, uint16_t color) override;
  void fillScreen(uint16_t color) override;
  void clear() { memset(pix, 0, sizeof(pix)); }
  inline void setPx(int x, int y, uint8_t v) {
    if ((unsigned)x < SIGN_W && (unsigned)y < SIGN_H) pix[y][x] = v;
  }
  static uint8_t colorBits(uint16_t c565);
  uint8_t pix[SIGN_H][SIGN_W];
};

namespace Display {
struct Mapping {
  bool reverseX = false;   // first bit shifted = rightmost column instead of leftmost
  bool flipY = false;      // row select 0 = bottom row of the band
  bool swapBands = false;  // ribbon "1" drives the bottom band
};

void begin();
void setMapping(const Mapping& m);
void setBrightness(uint8_t pct);   // 5..100
void setRefreshHz(uint16_t hz);    // full-frame refresh, 60..400
void show(const SignCanvas& c);    // pack and hand to the refresh ISR
void blank();
const SignCanvas* lastShown();
uint32_t refreshCount();           // number of full refresh cycles so far
}  // namespace Display
