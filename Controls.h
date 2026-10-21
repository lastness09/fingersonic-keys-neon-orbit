// SPDX-License-Identifier: MIT
#pragma once
#include "config.h"

namespace arp {
class Encoder {
public:
  int8_t update(uint8_t ab) {
    if (previous_ == 0xFF) { previous_ = ab; return 0; }
    if (ab == previous_) return 0;
    static const int8_t table[16] = {0,-1,1,0, 1,0,0,-1, -1,0,0,1, 0,1,-1,0};
    const uint8_t old = previous_;
    previous_ = ab;
    if ((old ^ ab) == 3) { accumulator_ = 0; return 0; }
    accumulator_ += table[(old << 2) | ab];
    if (accumulator_ >= 4 || accumulator_ <= -4) {
      const int8_t direction = accumulator_ > 0 ? 1 : -1;
      accumulator_ = 0;
      return direction;
    }
    return 0;
  }
private:
  uint8_t previous_ = 0xFF;
  int8_t accumulator_ = 0;
};

class Button {
public:
  // Returns +1 on debounced press, -1 on release, zero otherwise.
  int8_t update(bool down, uint32_t now) {
    if (down != raw_) { raw_ = down; changed_ = now; }
    if (raw_ && !tracking_) { tracking_ = true; moved_ = false; }
    if (!raw_ && !stable_ && uint32_t(now - changed_) >= 12) tracking_ = false;
    if (stable_ != raw_ && uint32_t(now - changed_) >= 12) {
      stable_ = raw_;
      // Keep rotations seen during the raw press's debounce interval. Clearing
      // moved_ here would turn a quick hold-and-turn into a click on release.
      if (stable_) pressedAt_ = now;
      else tracking_ = false;
      return stable_ ? 1 : -1;
    }
    return 0;
  }
  bool held() const { return stable_; }
  bool rawHeld() const { return raw_; }
  void rotated() { moved_ = true; }
  bool tap(uint32_t now) const { return !moved_ && uint32_t(now - pressedAt_) < 350; }
private:
  bool raw_ = false, stable_ = false, moved_ = false, tracking_ = false;
  uint32_t changed_ = 0, pressedAt_ = 0;
};

// The Keys recovery gesture: Key0 held + five left-switch taps in 1000 ms.
// Key0 must stay held throughout; rotation and long presses cancel the sequence.
class RecoveryGesture {
public:
  bool update(bool key0, bool tap, bool cancel, uint32_t now) {
    if (!key0 || cancel) { count_ = 0; return false; }
    if (!tap) return false;
    if (!count_ || uint32_t(now - start_) > 1000) { start_ = now; count_ = 0; }
    if (++count_ == 5) { count_ = 0; return true; }
    return false;
  }
private:
  uint8_t count_ = 0;
  uint32_t start_ = 0;
};

// Scan-independent contact state; both contacts must release to end a note.
class DualContactKey {
public:
  // >0 = velocity / note-on, -1 = note-off, 0 = no event.
  int16_t update(bool a, bool b, uint32_t now) {
    const bool incoming[2] = {a, b};
    for (uint8_t i = 0; i < 2; ++i) {
      if (incoming[i] != raw_[i]) { raw_[i] = incoming[i]; changed_[i] = now; }
      if (raw_[i] != stable_[i] && uint32_t(now - changed_[i]) >= ContactDebounceUs)
        stable_[i] = raw_[i];
    }
    if (!stable_[0] && !stable_[1]) {
      first_ = -1;
      if (active_) { active_ = false; return -1; }
      return 0;
    }
    if (first_ < 0) {
      first_ = stable_[0] ? 0 : 1;
      // Preserve the raw edge time, rather than adding the debounce duration.
      firstAt_ = changed_[first_];
    }
    if (!active_ && ((stable_[0] && stable_[1]) || uint32_t(now - firstAt_) >= SlowPressUs)) {
      active_ = true;
      uint32_t elapsed = uint32_t(now - firstAt_);
      if (stable_[0] && stable_[1]) elapsed = uint32_t(changed_[first_ ^ 1] - firstAt_);
      if (elapsed <= 100) return 127;
      if (elapsed >= SlowPressUs) return 63;
      return 127 - ((elapsed - 100) * 64 / (SlowPressUs - 100));
    }
    return 0;
  }
  bool active() const { return active_; }
private:
  bool raw_[2] = {}, stable_[2] = {}, active_ = false;
  uint32_t changed_[2] = {}, firstAt_ = 0;
  int8_t first_ = -1;
};
}
