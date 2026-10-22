// SPDX-License-Identifier: MIT
// Neon Orbit community firmware based on the documented Fingersonic Keys hardware interface.
// Left: turn = pattern, hold + turn = BPM.
// Right: click = tone/volume, turn = selected value, hold + turn = octave range.
// USB/BLE MIDI input drives the arp; generated notes are sent on MIDI channel 1.
// Recovery: hold Key0 and tap the left encoder five times within one second.
#include "ArpFirmware.h"

void setup() { arpSetup(); }
void loop() { arpLoop(); }
