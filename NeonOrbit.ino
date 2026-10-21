// SPDX-License-Identifier: MIT
// Neon Orbit community firmware based on the documented Fingersonic Keys hardware interface.
// Left: turn = pattern, hold + turn = BPM.
// Right: click = tone/volume, turn = selected value, hold + turn = octave range.
// Recovery: hold Key0 and tap the left encoder five times within one second.
#include "ArpFirmware.h"

void setup() { arpSetup(); }
void loop() { arpLoop(); }
