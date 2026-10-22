// SPDX-License-Identifier: MIT
#pragma once
#include "MidiProtocol.h"

namespace arp {
struct MidiDelivery {
  MidiMessage message;
  uint32_t epoch = 0, serial = 0;
};

// A bounded, nonblocking output queue. Transport code locks around these short
// methods, then performs USB/BLE calls outside the lock and outside audioTask.
class MidiOutput {
public:
  void setReady(bool ready);
  void panic();
  bool push(MidiMessage message, uint32_t now);
  bool next(uint32_t now, MidiDelivery& delivery);
  void acknowledge(const MidiDelivery& delivery);
  bool ready() const { return ready_; }
private:
  static constexpr uint8_t Capacity = 64;
  static constexpr uint32_t MaxAgeMs = 100;
  struct Entry { MidiMessage message; uint32_t at; } queue_[Capacity];
  uint8_t head_ = 0, count_ = 0, panicStep_ = 0;
  uint32_t epoch_ = 0, serial_ = 0;
  int16_t active_ = -1;
  bool ready_ = false;
};
}
