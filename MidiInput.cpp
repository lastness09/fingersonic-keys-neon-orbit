// SPDX-License-Identifier: MIT
#include "MidiInput.h"

namespace arp {
uint8_t MidiInput::owner(MidiSource source, uint8_t channel) {
  if (source == MidiSource::Keys) return 0;
  if (channel > 15) return Owners;
  if (source == MidiSource::Usb) return 1 + channel;
  if (source == MidiSource::Ble) return 17 + channel;
  return Owners;
}

void MidiInput::reset() {
  for (uint8_t slot = 0; slot < Owners; ++slot) {
    sustain_[slot] = false;
    for (uint8_t note = 0; note < NoteCount; ++note) held_[slot][note] = latched_[slot][note] = 0;
  }
  for (auto& value : combined_) value = 0;
}

void MidiInput::update(uint8_t index, ArpEngine& engine) {
  uint8_t value = 0;
  for (uint8_t slot = 0; slot < Owners; ++slot) {
    if (held_[slot][index] > value) value = held_[slot][index];
    if (latched_[slot][index] > value) value = latched_[slot][index];
  }
  if (value == combined_[index]) return;
  combined_[index] = value;
  if (value) engine.noteOn(index + PianoLowestNote, value);
  else engine.noteOff(index + PianoLowestNote);
}

void MidiInput::noteOn(MidiSource source, uint8_t channel, uint8_t note, uint8_t velocity, ArpEngine& engine) {
  if (!velocity) { noteOff(source, channel, note, engine); return; }
  const uint8_t slot = owner(source, channel);
  if (slot >= Owners || note < PianoLowestNote || note > PianoHighestNote) return;
  const uint8_t index = note - PianoLowestNote;
  held_[slot][index] = velocity > 127 ? 127 : velocity;
  latched_[slot][index] = 0;
  update(index, engine);
}

void MidiInput::noteOff(MidiSource source, uint8_t channel, uint8_t note, ArpEngine& engine) {
  const uint8_t slot = owner(source, channel);
  if (slot >= Owners || note < PianoLowestNote || note > PianoHighestNote) return;
  const uint8_t index = note - PianoLowestNote;
  if (held_[slot][index]) {
    latched_[slot][index] = sustain_[slot] ? held_[slot][index] : 0;
    held_[slot][index] = 0;
  }
  update(index, engine);
}

void MidiInput::clearOwner(uint8_t slot, bool keepHeld, ArpEngine& engine) {
  sustain_[slot] = false;
  for (uint8_t note = 0; note < NoteCount; ++note) {
    latched_[slot][note] = 0;
    if (!keepHeld) held_[slot][note] = 0;
    update(note, engine);
  }
}

void MidiInput::control(MidiSource source, uint8_t channel, uint8_t controller, uint8_t value, ArpEngine& engine) {
  const uint8_t slot = owner(source, channel);
  if (slot == 0 || slot >= Owners) return;
  if (controller == 64) {
    sustain_[slot] = value >= 64;
    if (!sustain_[slot]) clearOwner(slot, true, engine);
  } else if (controller == 120) {
    const bool pedal = sustain_[slot];
    clearOwner(slot, false, engine);
    sustain_[slot] = pedal; // All Sound Off does not reset the controller state.
  } else if (controller == 123) {
    // All Notes Off behaves like releasing keys; an active sustain pedal holds.
    for (uint8_t note = 0; note < NoteCount; ++note)
      noteOff(source, channel, note + PianoLowestNote, engine);
  } else if (controller == 121) {
    clearOwner(slot, true, engine);
  }
}

void MidiInput::releaseSource(MidiSource source, ArpEngine& engine) {
  const uint8_t first = owner(source, 0);
  if (first >= Owners) return;
  const uint8_t end = source == MidiSource::Keys ? 1 : first + 16;
  for (uint8_t slot = first; slot < end; ++slot) {
    sustain_[slot] = false;
    for (uint8_t note = 0; note < NoteCount; ++note) held_[slot][note] = latched_[slot][note] = 0;
  }
  for (uint8_t note = 0; note < NoteCount; ++note) update(note, engine);
}
}
