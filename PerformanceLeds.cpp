// SPDX-License-Identifier: MIT
#include "PerformanceLeds.h"
#include "ArpEngine.h"

namespace arp {
namespace {
uint8_t fade(uint32_t age, uint32_t hold, uint32_t end) {
  if (age >= end) return 0;
  if (age <= hold) return 255;
  const uint32_t linear = (end - age) * 255 / (end - hold);
  return uint8_t(linear * linear / 255); // Pluck-shaped release.
}
uint8_t scale(uint16_t value, uint16_t amount) { return uint8_t(value * amount / 255); }
int distance(int a, int b) { return a > b ? a - b : b - a; }
uint16_t clampTime(uint16_t value, uint16_t minimum, uint16_t maximum) {
  return value < minimum ? minimum : (value > maximum ? maximum : value);
}
}

void PerformanceLeds::reset() {
  for (auto& pulse : pulses_) pulse = Pulse{};
  next_ = energy_ = 0;
  previous_ = -1;
  frameAt_ = 0;
  hasFrame_ = false;
}

void PerformanceLeds::trigger(const LightCue& cue) {
  if (cue.note > 127 || cue.velocity == 0) return;
  Pulse& pulse = pulses_[next_];
  pulse.cue = cue;
  if (pulse.cue.bpm < MinBpm) pulse.cue.bpm = MinBpm;
  if (pulse.cue.bpm > MaxBpm) pulse.cue.bpm = MaxBpm;
  if (pulse.cue.velocity > 127) pulse.cue.velocity = 127;
  pulse.previous = previous_;
  pulse.valid = true;
  previous_ = ArpEngine::ledForNote(cue.note);
  next_ = (next_ + 1) % 8;
}

void PerformanceLeds::layer(LedPixel& pixel, uint8_t hue, uint8_t saturation, uint8_t value) {
  const uint16_t h = uint16_t(hue) * 6;
  const uint8_t region = h >> 8, fraction = h & 255;
  const uint8_t p = scale(value, 255 - saturation);
  const uint8_t q = scale(value, 255 - scale(saturation, fraction));
  const uint8_t t = scale(value, 255 - scale(saturation, 255 - fraction));
  uint8_t r, g, b;
  switch (region) {
    case 0: r = value; g = t; b = p; break;
    case 1: r = q; g = value; b = p; break;
    case 2: r = p; g = value; b = t; break;
    case 3: r = p; g = q; b = value; break;
    case 4: r = t; g = p; b = value; break;
    default: r = value; g = p; b = q; break;
  }
  // Maximum blending preserves a crisp note highlight under overlapping waves.
  if (r > pixel.red) pixel.red = r;
  if (g > pixel.green) pixel.green = g;
  if (b > pixel.blue) pixel.blue = b;
}

void PerformanceLeds::render(uint32_t now, uint32_t held, uint8_t audioPeak,
                             uint8_t preset, LedPixel* pixels) {
  const uint32_t elapsed = hasFrame_ ? uint32_t(now - frameAt_) : 0;
  frameAt_ = now;
  hasFrame_ = true;
  // Measured output amplitude gives attacks a fast rise and a short visual tail.
  const uint32_t drop = elapsed * 2;
  energy_ = energy_ > drop ? uint8_t(energy_ - drop) : 0;
  if (audioPeak > energy_) energy_ = audioPeak;
  const uint8_t gain = energy_ ? uint8_t(85 + uint16_t(energy_) * 170 / 255) : 0;
  for (uint8_t key = 0; key < KeyCount; ++key) {
    pixels[key] = LedPixel{};
    if (held & (uint32_t(1) << key))
      layer(pixels[key], uint8_t(132 + (key % 12) * 21 + preset * 3), 235, 13);
  }
  for (auto& pulse : pulses_) {
    if (!pulse.valid) continue;
    const uint32_t age = uint32_t(now - pulse.cue.at);
    // Future/very old events never become a new flash if the UI was delayed.
    if (age >= 440) { pulse.valid = false; continue; }
    const uint8_t target = ArpEngine::ledForNote(pulse.cue.note);
    const uint8_t twin = target < 12 ? target + 12 : target - 12;
    const uint8_t hue = uint8_t(132 + (pulse.cue.note % 12) * 21 + preset * 3);
    const uint16_t stepMs = 15000 / pulse.cue.bpm;
    const uint16_t releaseMs = clampTime(stepMs * 3 / 2, 75, 185);
    const uint8_t accent = pulse.cue.step % 4 == 0 ? 255 : (pulse.cue.step % 2 == 0 ? 225 : 195);
    const uint8_t peak = scale(100 + uint16_t(pulse.cue.velocity) * 155 / 127, accent);
    const uint8_t body = scale(scale(peak, gain), fade(age, 16, releaseMs));
    // Both physical occurrences of a pitch class flash on every actual note.
    // The exact/folded sounding octave is hotter and less saturated.
    layer(pixels[target], hue, 76, body);
    layer(pixels[twin], hue, 230, scale(body, 184));
    const uint8_t halo = scale(scale(peak, gain), fade(age, 0, releaseMs + 90));
    layer(pixels[target], hue, 210, scale(halo, 110));
    layer(pixels[twin], hue, 235, scale(halo, 80));

    const uint16_t spacing = clampTime(stepMs / 5, 9, 22);
    for (uint8_t key = 0; key < KeyCount; ++key) {
      const int d = distance(key, target);
      if (d < 1 || d > 4 || age < uint32_t(d * spacing)) continue;
      const uint8_t wave = scale(scale(peak, gain),
          scale(fade(age - d * spacing, 0, 55), uint8_t((5 - d) * 24)));
      layer(pixels[key], uint8_t(hue + d * 7), 235, wave);
    }

    // A comet travels from the preceding sounding key to the new one. C3/C4
    // thus make a visible twelve-key octave jump rather than a stationary blink.
    const uint16_t travelMs = clampTime(stepMs * 3 / 4, 45, 160);
    if (pulse.previous >= 0 && pulse.previous != target && age < travelMs) {
      const int span = int(target) - pulse.previous;
      const int head = pulse.previous + span * int(age) / travelMs;
      const uint8_t value = scale(scale(peak, gain), 145);
      layer(pixels[head], uint8_t(hue + 15), 175, value);
      const int tail = head - (span > 0 ? 1 : -1);
      if (tail >= 0 && tail < KeyCount) layer(pixels[tail], hue, 240, scale(value, 85));
    }
  }
}
}
