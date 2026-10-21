// SPDX-License-Identifier: MIT
#pragma once
#include "config.h"

namespace arp {
enum class Order : uint8_t { Up, Down, Bounce, OutsideIn, Random, RepeatThree };
struct Pattern {
  const char* name;
  uint16_t hits;       // Bit 0 is the first sixteenth in a 4/4 bar.
  uint8_t gate;        // Percent of a sixteenth, always below 100.
  Order order;
};
extern const Pattern Patterns[PatternCount];

struct ArpEvent {
  int16_t off = -1;
  int16_t on = -1;
  uint8_t velocity = 0;
  uint8_t step = 0;
};

// Owned exclusively by the audio task. The clock advances once per PCM frame;
// filesystem, MIDI transport and LED timing cannot move its step boundaries.
class ArpEngine {
public:
  void reset();
  void noteOn(uint8_t note, uint8_t velocity);
  void noteOff(uint8_t note);
  void panic();
  void setPattern(uint8_t pattern);
  void setBpm(uint16_t bpm);
  void setOctaves(uint8_t octaves);
  ArpEvent tick();
  uint32_t heldKeyMask() const;
  int16_t activeNote() const { return active_; }
  uint8_t step() const { return step_; }
  uint32_t triggerSerial() const { return triggerSerial_; }
  uint16_t bpm() const { return bpm_; }
  uint8_t pattern() const { return pattern_; }
  uint8_t octaves() const { return octaves_; }
  static uint8_t ledForNote(uint8_t note);

private:
  uint8_t notes_[128] = {};
  uint8_t pool_[128] = {};
  uint8_t poolVelocity_[128] = {};
  uint8_t count_ = 0;
  uint8_t pattern_ = 0, step_ = 0;
  uint8_t octaves_ = DefaultArpOctaves;
  uint16_t bpm_ = DefaultBpm;
  uint32_t ordinal_ = 0, triggerSerial_ = 0, rng_ = 0x6D2B79F5;
  uint32_t gateFrames_ = 0;
  float remaining_ = 0, stepFrames_ = 0;
  int16_t active_ = -1, pendingOff_ = -1, previous_ = -1;
  bool running_ = false;
  void rebuildPool();
  uint8_t selectNote();
};
}
