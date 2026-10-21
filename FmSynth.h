// SPDX-License-Identifier: MIT
#pragma once
#include "config.h"

namespace arp {
struct FmPreset {
  const char* name;
  float ratio, ratio2, index, modMs, decayMs, releaseMs;
  float harmonic, feedback, sub, gain, pan;
};
extern const FmPreset Presets[PresetCount];

class FmSynth {
public:
  void begin();
  void setPreset(uint8_t preset) { preset_ = preset % PresetCount; }
  void setVolume(uint8_t volume);
  void noteOn(uint8_t note, uint8_t velocity);
  void noteOff(uint8_t note);
  void panic();
  void render(float& left, float& right);
private:
  struct Voice {
    bool active = false, released = false;
    uint8_t note = 0;
    float phase = 0, mod = 0, mod2 = 0, subPhase = 0, increment = 0;
    float envelope = 0, attack = 0, modEnvelope = 0, previous = 0;
    float decay = 0, modDecay = 0, release = 0, velocity = 0;
    FmPreset patch = {};
  } voices_[8];
  float frequencies_[128] = {};
  // Fixed internal-RAM stereo echo; no SoundFont or PSRAM allocation.
  float delayL_[8192] = {}, delayR_[8192] = {};
  uint16_t delayPos_ = 0;
  float volume_ = 0, targetVolume_ = 0;
  uint8_t preset_ = 0;
  static float sine(float phase);
};
}
