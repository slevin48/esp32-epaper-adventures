#pragma once
#include <Arduino.h>

// One wrapped line of text, ready to draw.
struct Line {
  const uint8_t *font;
  String text;
  int gapBefore;  // extra pixels above this line
};
