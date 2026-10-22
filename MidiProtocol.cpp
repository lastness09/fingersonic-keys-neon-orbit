// SPDX-License-Identifier: MIT
#include "MidiProtocol.h"

namespace arp {
uint8_t midiDataLength(uint8_t status) {
  if (status < 0x80 || status >= 0xF0) return 0;
  return (status & 0xE0) == 0xC0 ? 1 : 2;
}

bool decodeUsbMidi(const uint8_t packet[4], MidiMessage& message) {
  const uint8_t size = midiDataLength(packet[1]);
  if ((packet[0] >> 4) != 0 || !size || (packet[0] & 15) != (packet[1] >> 4) ||
      packet[2] > 127 || (size == 2 && packet[3] > 127)) return false;
  message = {packet[1], packet[2], size == 2 ? packet[3] : uint8_t(0)};
  return true;
}

uint8_t encodeBleMidi(const MidiMessage& message, uint32_t milliseconds, uint8_t out[5]) {
  const uint8_t length = midiDataLength(message.status);
  if (!length || message.data1 > 127 || (length == 2 && message.data2 > 127)) return 0;
  out[0] = 0x80 | ((milliseconds >> 7) & 0x3F);
  out[1] = 0x80 | (milliseconds & 0x7F);
  out[2] = message.status;
  out[3] = message.data1;
  out[4] = message.data2;
  return length + 3;
}

bool BleMidiDecoder::decode(const uint8_t* bytes, size_t length, Emit emit, void* context) {
  if (!bytes || length < 2 || (bytes[0] & 0xC0) != 0x80 || !emit) {
    reset();
    return false;
  }
  uint8_t running = 0;
  size_t i = 1;
  while (i < length) {
    const bool timestamp = bytes[i] & 0x80;
    if (timestamp && ++i >= length) { reset(); return false; }
    uint8_t status = bytes[i];
    if (sysex_) {
      // SysEx is ignored, including continuation packets without a timestamp.
      // The end marker (and any interspersed real-time byte) has a timestamp.
      if (timestamp && status == 0xF7) { sysex_ = false; ++i; continue; }
      if (timestamp && status >= 0xF8) { ++i; continue; }
      if (timestamp && status == 0xF0) { ++i; continue; }
      if (status & 0x80) { reset(); return false; }
      while (i < length && !(bytes[i] & 0x80)) ++i;
      continue;
    }
    if (status & 0x80) {
      if (!timestamp) { reset(); return false; }
      ++i;
      if (status == 0xF0) { sysex_ = true; running = 0; continue; }
      if (status >= 0xF8 || status == 0xF6 || status == 0xF7) continue;
      if (status >= 0xF0) {
        const uint8_t commonLength = status == 0xF2 ? 2 :
                                     (status == 0xF1 || status == 0xF3 ? 1 : 0);
        if (!commonLength || length - i < commonLength) { reset(); return false; }
        for (uint8_t n = 0; n < commonLength; ++n)
          if (bytes[i++] & 0x80) { reset(); return false; }
        continue; // BLE system messages do not cancel packet-local running status.
      }
      running = status;
    } else {
      if (!running) { reset(); return false; }
      status = running;
    }
    const uint8_t count = midiDataLength(status);
    if (!count || length - i < count) { reset(); return false; }
    const uint8_t first = bytes[i++];
    const uint8_t second = count == 2 ? bytes[i++] : 0;
    if (first > 127 || second > 127) { reset(); return false; }
    emit({status, first, second}, context);
  }
  return true;
}
}
