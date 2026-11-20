#pragma once
#include "display.h"

// Real-time pixel input from Falcon Player / xLights:
//   DDP   UDP 4048 (offset 0 = first channel of the sign)
//   E1.31 UDP 5568 unicast (start universe / universe size from settings)
namespace Live {
void begin();
void restart();          // re-read enable flags after a settings change
void loop();             // receives packets, shows frames while active
bool active();
uint32_t packets();
String source();         // "DDP", "E1.31" or ""
SignCanvas& canvas();
}  // namespace Live
