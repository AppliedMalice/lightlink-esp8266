// Low-level panel driver for the Inova 630019 panel boards.
//
// Per band (8 rows) the ribbon carries: SRCK, RED data, GREEN data, RCK and a
// 3-bit row select (A=MSB, C=LSB). Red and green are two separate TPIC6B595
// chains clocked by the same SRCK; each chain is 200 bits long per band.
// Both bands share SRCK/RCK/row select, so we shift 4 data bits per clock.
//
// Refresh runs from timer1 in IRAM. Each row interrupt:
//   1. shifts the next row's data in (outputs keep showing the current row)
//   2. switches the row select and pulses RCK back to back
//   3. optionally schedules an "off" interrupt that latches zeros (brightness)

#include "display.h"

// ---------------- canvas ----------------
uint8_t SignCanvas::colorBits(uint16_t c) {
  uint8_t r = c >> 11, g = (c >> 5) & 0x3F;
  return (r >= 8 ? PX_RED : 0) | (g >= 16 ? PX_GREEN : 0);
}

void SignCanvas::drawPixel(int16_t x, int16_t y, uint16_t color) {
  if ((unsigned)x >= SIGN_W || (unsigned)y >= SIGN_H) return;
  pix[y][x] = colorBits(color);
}

void SignCanvas::fillScreen(uint16_t color) { memset(pix, colorBits(color), sizeof(pix)); }

// ---------------- refresh engine ----------------
namespace {
// plane[row][k]: bit0 R top, bit1 G top, bit2 R bottom, bit3 G bottom,
// k = shift order (k=0 is shifted first)
uint8_t planeA[BAND_H][SIGN_W];
uint8_t planeB[BAND_H][SIGN_W];
uint8_t (*volatile front)[SIGN_W] = planeA;

uint32_t setMask[16];
uint32_t clrMask[16];
constexpr uint32_t SRCK_M = 1u << PIN_SRCK;
constexpr uint32_t RCK_M = 1u << PIN_RCK;

volatile uint8_t curRow = 0;
volatile bool offPending = false;
volatile uint32_t periodTicks = 4166;  // timer1 @ 5 MHz (DIV16)
volatile uint32_t onTicks = 4166;
volatile uint32_t refreshes = 0;

Display::Mapping mapping;
const SignCanvas* last = nullptr;
uint8_t brightnessPct = 60;
uint16_t refreshHz = 150;

inline void IRAM_ATTR pinFast(uint8_t pin, bool v) {
  if (pin == 16) {
    if (v) GP16O |= 1; else GP16O &= ~1;
  } else {
    if (v) GPOS = (1u << pin); else GPOC = (1u << pin);
  }
}

inline void IRAM_ATTR shiftDelay() {
  for (int i = 0; i < SHIFT_NOPS; i++) __asm__ __volatile__("nop");
}

void IRAM_ATTR shiftRow(const uint8_t* d) {
  for (int k = 0; k < SIGN_W; k++) {
    uint8_t v = d[k] & 0x0F;
    GPOC = clrMask[v] | SRCK_M;  // clock low, clear data bits that are 0
    GPOS = setMask[v];           // set data bits that are 1
    shiftDelay();
    GPOS = SRCK_M;               // rising edge shifts the bit in
    shiftDelay();
  }
  GPOC = SRCK_M;
}

void IRAM_ATTR shiftZeros() {
  GPOC = clrMask[0];
  for (int k = 0; k < SIGN_W; k++) {
    GPOC = SRCK_M;
    shiftDelay();
    GPOS = SRCK_M;
    shiftDelay();
  }
  GPOC = SRCK_M;
}

inline void IRAM_ATTR latch() {
  GPOS = RCK_M;
  shiftDelay();
  GPOC = RCK_M;
}

inline void IRAM_ATTR setRow(uint8_t r) {
  pinFast(PIN_ROW_C, r & 1);
  pinFast(PIN_ROW_B, r & 2);
  pinFast(PIN_ROW_A, r & 4);
}

void IRAM_ATTR onTimer() {
  if (offPending) {
    offPending = false;
    shiftZeros();
    latch();
    timer1_write(periodTicks - onTicks);
    return;
  }
  uint8_t r = (curRow + 1) & 7;
  curRow = r;
  shiftRow(front[r]);
  setRow(r);
  latch();
  if (r == 0) refreshes++;
  if (onTicks < periodTicks) {
    offPending = true;
    timer1_write(onTicks);
  } else {
    timer1_write(periodTicks);
  }
}

void recalcTiming() {
  uint32_t p = 5000000UL / ((uint32_t)refreshHz * BAND_H);
  uint32_t on = p * brightnessPct / 100;
  const uint32_t minOn = 450;  // ~90us, must exceed the time one shift takes
  if (on < minOn) on = minOn;
  if (brightnessPct >= 100 || on + minOn >= p) on = p;  // no off phase
  noInterrupts();
  periodTicks = p;
  onTicks = on;
  interrupts();
}
}  // namespace

namespace Display {

void begin() {
  const uint8_t pins[] = {PIN_SRCK, PIN_R1, PIN_G1, PIN_R2, PIN_G2,
                          PIN_RCK, PIN_ROW_A, PIN_ROW_B, PIN_ROW_C};
  for (uint8_t p : pins) {
    pinMode(p, OUTPUT);
    digitalWrite(p, LOW);
  }
  const uint8_t dataPins[4] = {PIN_R1, PIN_G1, PIN_R2, PIN_G2};
  for (int v = 0; v < 16; v++) {
    uint32_t s = 0, c = 0;
    for (int b = 0; b < 4; b++) {
      if (v & (1 << b)) s |= 1u << dataPins[b];
      else c |= 1u << dataPins[b];
    }
    setMask[v] = s;
    clrMask[v] = c;
  }
  memset(planeA, 0, sizeof(planeA));
  memset(planeB, 0, sizeof(planeB));
  // Clear whatever the latches powered up with before starting the scan
  shiftZeros();
  latch();
  recalcTiming();
  timer1_attachInterrupt(onTimer);
  timer1_enable(TIM_DIV16, TIM_EDGE, TIM_SINGLE);
  timer1_write(periodTicks);
}

void setMapping(const Mapping& m) { mapping = m; if (last) show(*last); }

void setBrightness(uint8_t pct) {
  brightnessPct = constrain(pct, 5, 100);
  recalcTiming();
}

void setRefreshHz(uint16_t hz) {
  refreshHz = constrain(hz, 60, 400);
  recalcTiming();
}

void show(const SignCanvas& c) {
  uint8_t(*back)[SIGN_W] = (front == planeA) ? planeB : planeA;
  for (int pr = 0; pr < BAND_H; pr++) {
    int ly = mapping.flipY ? (BAND_H - 1 - pr) : pr;
    const uint8_t* top = c.pix[(mapping.swapBands ? BAND_H : 0) + ly];
    const uint8_t* bot = c.pix[(mapping.swapBands ? 0 : BAND_H) + ly];
    uint8_t* out = back[pr];
    if (mapping.reverseX) {
      for (int k = 0; k < SIGN_W; k++) {
        int x = SIGN_W - 1 - k;
        out[k] = (top[x] & 3) | ((bot[x] & 3) << 2);
      }
    } else {
      for (int k = 0; k < SIGN_W; k++) out[k] = (top[k] & 3) | ((bot[k] & 3) << 2);
    }
  }
  front = back;  // ISR picks it up on the next row (ISR cannot be preempted by loop)
  last = &c;
}

void blank() {
  uint8_t(*back)[SIGN_W] = (front == planeA) ? planeB : planeA;
  memset(back, 0, sizeof(planeA));
  front = back;
}

const SignCanvas* lastShown() { return last; }
uint32_t refreshCount() { return refreshes; }

}  // namespace Display
