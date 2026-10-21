// SPDX-License-Identifier: MIT
#pragma once
#include <stdint.h>

// Hardware mapping and audio format inherited from the Fingersonic Keys hardware contract.
namespace arp {
constexpr uint32_t SampleRate = 44100;
constexpr uint16_t BlockFrames = 128;
constexpr uint8_t KeyCount = 24;
constexpr uint8_t BaseNote = 48;
constexpr uint8_t MinArpOctaves = 1, MaxArpOctaves = 8, DefaultArpOctaves = 2;
constexpr uint8_t PianoLowestNote = 21, PianoHighestNote = 108; // A0--C8, 88 keys.
constexpr uint8_t PatternCount = 10;
constexpr uint8_t PresetCount = 10;
constexpr uint16_t MinBpm = 30;
constexpr uint16_t MaxBpm = 300;
constexpr uint16_t DefaultBpm = 120;
constexpr uint8_t DefaultVolume = 100;
constexpr uint8_t LedPin = 2;
constexpr uint8_t LedBrightness = 64;
constexpr uint8_t LeftA = 5, LeftB = 16, LeftSwitch = 13;
constexpr uint8_t RightA = 1, RightB = 3, RightSwitch = 38;
constexpr uint8_t Columns[] = {12, 11, 10, 8, 7, 6};
constexpr uint8_t Contacts[] = {48, 47, 39, 21, 18, 17, 15, 14};
constexpr uint8_t BclkPin = 40, DataPin = 41, WsPin = 42;
constexpr uint32_t ContactDebounceUs = 500;
constexpr uint32_t SlowPressUs = 22000;
constexpr uint32_t ColumnSettleUs = 30;
}
