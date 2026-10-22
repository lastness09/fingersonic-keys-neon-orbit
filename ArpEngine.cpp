// SPDX-License-Identifier: MIT
#include "ArpEngine.h"

namespace arp {
const Pattern Patterns[PatternCount] = {
  {"Neon Up",       0x5555, 76, Order::Up},
  {"Down Drive",    0xFFFF, 50, Order::Down},
  {"Mirror Sync",   0x4949, 68, Order::Bounce},
  {"Wide Steps",    0x4545, 85, Order::OutsideIn},
  {"Tresillo",      0x1041, 92, Order::Up},
  {"Offbeat",       0x4444, 64, Order::Down},
  {"Euclid 5",      0x1249, 72, Order::Bounce},
  {"Euclid 7",      0x2955, 58, Order::OutsideIn},
  {"Triple Burst",  0x7777, 32, Order::RepeatThree},
  {"Random Orbit",  0x59D3, 60, Order::Random},
};

void ArpEngine::reset() {
  for (auto& v : notes_) v = 0;
  for (auto& v : poolVelocity_) v = 0;
  count_ = 0;
  pattern_ = step_ = 0;
  octaves_ = DefaultArpOctaves;
  ordinal_ = triggerSerial_ = gateFrames_ = 0;
  rng_ = 0x6D2B79F5;
  active_ = pendingOff_ = previous_ = -1;
  remaining_ = 0;
  running_ = false;
  bpm_ = DefaultBpm;
  setBpm(bpm_);
}

void ArpEngine::noteOn(uint8_t note, uint8_t velocity) {
  if (note < PianoLowestNote || note > PianoHighestNote) return;
  if (!velocity) { noteOff(note); return; }
  notes_[note] = velocity > 127 ? 127 : velocity;
  rebuildPool();
}

void ArpEngine::noteOff(uint8_t note) {
  if (note > 127) return;
  notes_[note] = 0;
  rebuildPool();
}

void ArpEngine::panic() {
  for (auto& v : notes_) v = 0;
  rebuildPool();
}

void ArpEngine::setOctaves(uint8_t octaves) {
  if (octaves < MinArpOctaves) octaves = MinArpOctaves;
  if (octaves > MaxArpOctaves) octaves = MaxArpOctaves;
  octaves_ = octaves;
  rebuildPool(); // Keep the clock running when the range is changed live.
}

void ArpEngine::rebuildPool() {
  const bool wasEmpty = count_ == 0;
  count_ = 0;
  for (auto& velocity : poolVelocity_) velocity = 0;
  for (int root = PianoLowestNote; root <= PianoHighestNote; ++root) {
    if (!notes_[root]) continue;
    // Expand upward first. If the selected span reaches C8, shift the span
    // downward by whole octaves. At 8 octaves every playable occurrence of the
    // held pitch class in A0--C8 is included; never clamp to a wrong pitch class.
    const int roomAbove = (PianoHighestNote - root) / 12;
    const int first = octaves_ <= roomAbove + 1 ? 0 : roomAbove + 1 - octaves_;
    for (int octave = first; octave < first + octaves_; ++octave) {
      const int note = root + octave * 12;
      if (note < PianoLowestNote || note > PianoHighestNote) continue;
      if (notes_[root] > poolVelocity_[note]) poolVelocity_[note] = notes_[root];
    }
  }
  for (int n = 0; n < 128; ++n) {
    // A sorted union deduplicates octave overlaps when both C3 and C4 are held.
    if (poolVelocity_[n]) pool_[count_++] = n;
  }
  if (active_ >= 0 && !poolVelocity_[active_]) {
    pendingOff_ = active_;
    active_ = -1;
    gateFrames_ = 0;
  }
  if (!count_) {
    running_ = false;
    remaining_ = 0;
    step_ = 0;
    ordinal_ = 0;
    previous_ = -1;
  } else if (wasEmpty) {
    running_ = true;
    remaining_ = 0; // First step is aligned with the first key press.
    step_ = 0;
    ordinal_ = 0;
  }
}

void ArpEngine::setBpm(uint16_t bpm) {
  if (bpm < MinBpm) bpm = MinBpm;
  if (bpm > MaxBpm) bpm = MaxBpm;
  const float newFrames = float(SampleRate) * 60.0f / (float(bpm) * 4.0f);
  if (stepFrames_ > 0) remaining_ *= newFrames / stepFrames_;
  stepFrames_ = newFrames;
  bpm_ = bpm;
}

void ArpEngine::setPattern(uint8_t pattern) {
  pattern_ = pattern % PatternCount;
  // Restart the bar, with an explicit off before any retrigger.
  if (active_ >= 0) pendingOff_ = active_;
  active_ = -1;
  gateFrames_ = 0;
  step_ = 0;
  ordinal_ = 0;
  remaining_ = 0;
}

uint8_t ArpEngine::selectNote() {
  uint32_t index = 0;
  switch (Patterns[pattern_].order) {
    case Order::Up: index = ordinal_ % count_; break;
    case Order::Down: index = count_ - 1 - ordinal_ % count_; break;
    case Order::Bounce: {
      const uint32_t cycle = count_ > 1 ? count_ * 2 - 2 : 1;
      index = ordinal_ % cycle;
      if (index >= count_) index = cycle - index;
      break;
    }
    case Order::OutsideIn: {
      const uint32_t position = ordinal_ % count_;
      index = (position & 1) ? count_ - 1 - position / 2 : position / 2;
      break;
    }
    case Order::RepeatThree: index = (ordinal_ / 3) % count_; break;
    case Order::Random:
      rng_ ^= rng_ << 13; rng_ ^= rng_ >> 17; rng_ ^= rng_ << 5;
      index = rng_ % count_;
      if (count_ > 1 && pool_[index] == previous_) index = (index + 1) % count_;
      break;
  }
  ++ordinal_;
  previous_ = pool_[index];
  return pool_[index];
}

ArpEvent ArpEngine::tick() {
  ArpEvent event;
  if (pendingOff_ >= 0) { event.off = pendingOff_; pendingOff_ = -1; }
  if (gateFrames_ && --gateFrames_ == 0 && active_ >= 0) {
    event.off = active_;
    active_ = -1;
  }
  if (!running_ || !count_) return event;
  if (remaining_ <= 0) {
    if (active_ >= 0) { event.off = active_; active_ = -1; }
    const Pattern& p = Patterns[pattern_];
    if (p.hits & (uint16_t(1) << step_)) {
      const uint8_t note = selectNote();
      const uint8_t velocity = poolVelocity_[note];
      // Quarter-note accents and softer offbeats preserve playing dynamics.
      const uint16_t strength = step_ % 4 == 0 ? 127 : (step_ % 2 == 0 ? 111 : 96);
      event.on = active_ = note;
      event.step = step_;
      event.velocity = uint8_t((velocity * strength + 63) / 127);
      if (!event.velocity) event.velocity = 1;
      gateFrames_ = uint32_t(stepFrames_ * p.gate * 0.01f);
      ++triggerSerial_;
    }
    step_ = (step_ + 1) % 16;
    remaining_ += stepFrames_;
  }
  remaining_ -= 1.0f;
  return event;
}

uint8_t ArpEngine::ledForNote(uint8_t note) {
  if (note >= BaseNote && note < BaseNote + KeyCount) return note - BaseNote;
  return uint8_t((int(note) - BaseNote + 120) % 12) + (note >= BaseNote + 12 ? 12 : 0);
}

uint32_t ArpEngine::heldKeyMask() const {
  uint32_t mask = 0;
  // Input roots (physical or MIDI) are held; generated notes light on attack.
  for (uint8_t note = 0; note < 128; ++note)
    if (notes_[note]) mask |= uint32_t(1) << ledForNote(note);
  return mask;
}
}
