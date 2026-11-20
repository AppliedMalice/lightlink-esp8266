#pragma once
#include <Arduino.h>

#define FW_VERSION "1.0.0"

// ---------------------------------------------------------------------------
// Sign geometry: Inova LightLink CFM16.200
//   5 panel boards (630019) per band, 40 bicolor columns each = 200 columns
//   2 bands of 8 rows = 16 rows, 1:8 multiplexed via a 3-bit row select
// ---------------------------------------------------------------------------
#define SIGN_W 200
#define SIGN_H 16
#define BAND_H 8

// ---------------------------------------------------------------------------
// ESP8266 GPIO assignment (Wemos D1 mini / NodeMCU labels in comments)
// Every signal goes through a 74HCT245 (3.3V -> 5V) before the ribbon.
// Data and clock pins MUST be GPIO 0..15 (fast GPOS/GPOC registers).
// ---------------------------------------------------------------------------
#define PIN_SRCK   14  // D5  shift clock  -> ribbon pins 4 and 7 (both bands)
#define PIN_R1     13  // D7  red data, top band     -> top ribbon pin 2
#define PIN_G1     12  // D6  green data, top band   -> top ribbon pin 9
#define PIN_R2      5  // D1  red data, bottom band  -> bottom ribbon pin 2
#define PIN_G2      4  // D2  green data, bottom band-> bottom ribbon pin 9
#define PIN_RCK    15  // D8  latch clock    -> ribbon pin 1 (both bands)
#define PIN_ROW_A  16  // D0  row select MSB -> ribbon pin 5 (both bands)
#define PIN_ROW_B   0  // D3  row select     -> ribbon pin 6 (both bands)
#define PIN_ROW_C   2  // D4  row select LSB -> ribbon pin 3 (both bands)

static_assert(PIN_SRCK < 16 && PIN_R1 < 16 && PIN_G1 < 16 && PIN_R2 < 16 &&
              PIN_G2 < 16 && PIN_RCK < 16,
              "shift/latch/data pins must be GPIO0..15");

#ifndef SHIFT_NOPS
#define SHIFT_NOPS 6
#endif

// Pixel value bits used throughout the framebuffer
#define PX_RED   0x01
#define PX_GREEN 0x02
#define PX_ORANGE (PX_RED | PX_GREEN)
