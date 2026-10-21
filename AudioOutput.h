// SPDX-License-Identifier: MIT
#pragma once
#include <Arduino.h>
#include "driver/i2s_std.h"
#include "config.h"

class AudioOutput {
public:
  bool begin();
  bool write(const int16_t* stereo);
private:
  i2s_chan_handle_t tx_ = nullptr;
};
