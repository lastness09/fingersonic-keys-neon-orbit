// SPDX-License-Identifier: MIT
#include "MidiOutput.h"

namespace arp {
void MidiOutput::setReady(bool ready) {
  if (ready_ == ready) return;
  ready_ = ready;
  ++epoch_;
  active_ = -1;
  panic(); // A new session never receives queued notes from the old one.
}

void MidiOutput::panic() {
  head_ = count_ = 0;
  panicStep_ = ready_ ? (active_ >= 0 ? 3 : 2) : 0;
  ++serial_;
}

bool MidiOutput::push(MidiMessage message, uint32_t now) {
  if (!ready_ || panicStep_) return true;
  if (count_ == Capacity) { panic(); return false; }
  queue_[(head_ + count_) % Capacity] = {message, now};
  ++count_;
  return true;
}

bool MidiOutput::next(uint32_t now, MidiDelivery& delivery) {
  if (!ready_) return false;
  if (count_ && uint32_t(now - queue_[head_].at) > MaxAgeMs) panic();
  MidiMessage message;
  if (panicStep_ == 3) message = {0x80, uint8_t(active_), 0};
  else if (panicStep_) message = {0xB0, uint8_t(panicStep_ == 2 ? 120 : 123), 0};
  else if (count_) message = queue_[head_].message;
  else return false;
  delivery = {message, epoch_, serial_};
  return true;
}

void MidiOutput::acknowledge(const MidiDelivery& delivery) {
  if (!ready_ || delivery.epoch != epoch_ || delivery.serial != serial_) return;
  const auto& message = delivery.message;
  if ((message.status & 0xF0) == 0x90 && message.data2) active_ = message.data1;
  else if (((message.status & 0xF0) == 0x80 || (message.status & 0xF0) == 0x90) &&
           active_ == message.data1) active_ = -1;
  if (panicStep_) {
    if (panicStep_ == 2) active_ = -1;
    --panicStep_;
  } else if (count_) {
    head_ = (head_ + 1) % Capacity;
    --count_;
  }
  ++serial_;
}
}
