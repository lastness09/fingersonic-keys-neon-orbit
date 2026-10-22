// SPDX-License-Identifier: MIT
#pragma once
#include <stddef.h>
#include <stdint.h>

namespace arp {
enum class MidiSource : uint8_t { Keys, Usb, Ble };
struct MidiMessage {
  uint8_t status = 0, data1 = 0, data2 = 0;
};
struct MidiInputEvent {
  MidiSource source = MidiSource::Usb;
  MidiMessage message;
  uint32_t epoch = 0;
  bool reset = false;
};
uint8_t midiDataLength(uint8_t status);
bool decodeUsbMidi(const uint8_t packet[4], MidiMessage& message);
uint8_t encodeBleMidi(const MidiMessage& message, uint32_t milliseconds, uint8_t out[5]);

// Channel running status is packet-local. Only unsupported SysEx continuation
// state survives between BLE packets; no message body is dynamically allocated.
class BleMidiDecoder {
public:
  using Emit = void (*)(const MidiMessage&, void*);
  void reset() { sysex_ = false; }
  bool decode(const uint8_t* bytes, size_t length, Emit emit, void* context);
private:
  bool sysex_ = false;
};
}
