// SPDX-License-Identifier: MIT
#pragma once
#include "ArpEngine.h"
#include "MidiProtocol.h"

namespace arp {
// Audio-task-owned note ownership: physical keys and all 16 channels on each
// transport remain independent, including sustain and disconnect cleanup.
class MidiInput {
public:
  void reset();
  void noteOn(MidiSource source, uint8_t channel, uint8_t note, uint8_t velocity, ArpEngine& engine);
  void noteOff(MidiSource source, uint8_t channel, uint8_t note, ArpEngine& engine);
  void control(MidiSource source, uint8_t channel, uint8_t controller, uint8_t value, ArpEngine& engine);
  void releaseSource(MidiSource source, ArpEngine& engine);
private:
  static constexpr uint8_t NoteCount = PianoHighestNote - PianoLowestNote + 1;
  static constexpr uint8_t Owners = 33;
  uint8_t held_[Owners][NoteCount] = {};
  uint8_t latched_[Owners][NoteCount] = {};
  uint8_t combined_[NoteCount] = {};
  bool sustain_[Owners] = {};
  static uint8_t owner(MidiSource source, uint8_t channel);
  void update(uint8_t index, ArpEngine& engine);
  void clearOwner(uint8_t slot, bool keepHeld, ArpEngine& engine);
};
}
