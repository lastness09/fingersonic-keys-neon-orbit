// SPDX-License-Identifier: MIT
#pragma once
#include "MidiProtocol.h"

namespace arp {
void midiBegin();
void midiPoll();
bool midiRead(MidiInputEvent& event);
// Safe from the audio task: enqueue only, with no USB or Bluetooth stack calls.
void midiSendGenerated(uint8_t status, uint8_t note, uint8_t velocity);
void midiPanicOutputs();
}
