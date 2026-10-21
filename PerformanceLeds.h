// SPDX-License-Identifier: MIT
#pragma once
#include "config.h"

namespace arp {
struct LedPixel { uint8_t red = 0, green = 0, blue = 0; };

// Delivered by the audio task only after its PCM block has been accepted by I2S.
struct LightCue {
  uint32_t at = 0;
  uint16_t bpm = DefaultBpm;
  uint8_t note = BaseNote, velocity = 0, step = 0;
};

// Pure, time-based renderer shared by the firmware and host tests. It owns no
// transport or GPIO and never runs in the audio task.
class PerformanceLeds {
public:
  void reset();
  void trigger(const LightCue& cue);
  void render(uint32_t now, uint32_t held, uint8_t audioPeak,
              uint8_t preset, LedPixel* pixels);
private:
  struct Pulse {
    LightCue cue;
    int8_t previous = -1;
    bool valid = false;
  } pulses_[8];
  uint8_t next_ = 0;
  int8_t previous_ = -1;
  uint32_t frameAt_ = 0;
  uint8_t energy_ = 0;
  bool hasFrame_ = false;
  static void layer(LedPixel& pixel, uint8_t hue, uint8_t saturation, uint8_t value);
};
}
