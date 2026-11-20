#pragma once
#include <Arduino.h>
#include <FS.h>
#include <vector>
#include "display.h"

struct FseqInfo {
  uint8_t major = 0, minor = 0;
  uint32_t channels = 0;
  uint32_t frames = 0;
  uint8_t stepMs = 50;
  uint8_t compression = 0;
  uint16_t dataOffset = 0;
  String error;
};

// Plays FPP/xLights .fseq files (V1, or V2 uncompressed incl. sparse ranges)
class FseqPlayer {
 public:
  static bool probe(const String& path, FseqInfo& info);
  bool open(const String& path);
  void close();
  bool isOpen() const { return (bool)file; }
  const FseqInfo& info() const { return inf; }
  // Restart timing at frame 0
  void rewind();
  // If a new frame is due, renders it into the canvas and returns true.
  // Sets 'ended' when the last frame has been passed.
  bool update(SignCanvas& c, bool& ended);
  uint32_t currentFrame() const { return cur; }

 private:
  struct Range { uint32_t start, count, frameOff; };
  static bool parse(File& f, FseqInfo& info, std::vector<Range>* ranges);
  bool renderFrame(SignCanvas& c, uint32_t frame);
  File file;
  FseqInfo inf;
  std::vector<Range> ranges;
  uint32_t frameSize = 0;
  uint32_t startMs = 0;
  int32_t cur = -1;
};
