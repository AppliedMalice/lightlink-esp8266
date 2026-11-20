#pragma once
#include "display.h"
#include "settings.h"

// Maps RGB channel data (xLights / FPP "200 x 16 horizontal matrix") onto the
// bicolor canvas. Red channel -> red LEDs, green channel -> green LEDs,
// blue is ignored, so white/yellow both show as orange.
static constexpr uint32_t PIXEL_CHANNELS = (uint32_t)SIGN_W * SIGN_H * 3;  // 9600

inline void applyChannel(SignCanvas& c, uint32_t ch, uint8_t val) {
  if (ch >= PIXEL_CHANNELS) return;
  uint32_t p = ch / 3;
  uint8_t comp = ch % 3;
  if (comp == 2) return;
  int row = p / SIGN_W;
  int col = p % SIGN_W;
  if (settings.serpentine && (row & 1)) col = SIGN_W - 1 - col;
  if (settings.bottomOrigin) row = SIGN_H - 1 - row;
  uint8_t bit = comp == 0 ? PX_RED : PX_GREEN;
  uint8_t& px = c.pix[row][col];
  if (val >= settings.threshold) px |= bit; else px &= ~bit;
}
