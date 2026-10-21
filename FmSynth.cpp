// SPDX-License-Identifier: MIT
#include "FmSynth.h"
#include "SineTable.h"
#include <math.h>

namespace arp {
const FmPreset Presets[PresetCount] = {
  // name              ratio ratio2 index modMs decay release harmonic feedback sub gain pan
  {"Glass Pluck",       2.0f,  3.0f, 3.4f,  90,  620, 115, 0.08f, 0.0f, 0.00f, 0.67f, 0.12f},
  {"Neon Kalimba",      3.0f,  7.0f, 2.6f,  45,  380,  85, 0.16f, 0.1f, 0.00f, 0.64f, 0.35f},
  {"Copper Bell",       1.414f,2.72f, 4.8f, 230, 1350, 230, 0.12f, 0.0f, 0.00f, 0.55f, 0.65f},
  {"Rubber Bass",       1.0f,  2.0f, 3.0f,  65,  300,  65, 0.05f, 0.2f, 0.36f, 0.70f, 0.18f},
  {"Acid Spark",        1.0f,  4.0f, 6.4f,  85,  260,  65, 0.22f, 0.8f, 0.10f, 0.54f, 0.40f},
  {"Digital Koto",      2.0f,  5.0f, 5.0f,  32,  480, 110, 0.19f, 0.2f, 0.00f, 0.62f, 0.55f},
  {"Crystal Marimba",   4.0f,  9.0f, 1.8f,  35,  320,  80, 0.06f, 0.0f, 0.08f, 0.72f, 0.78f},
  {"Velvet Synth",      1.0f,  2.0f, 1.6f, 300,  850, 180, 0.28f, 0.3f, 0.18f, 0.57f, 0.30f},
  {"Laser Droplet",     3.5f,  6.0f, 7.0f,  55,  440, 100, 0.12f, 0.4f, 0.03f, 0.58f, 0.72f},
  {"Dream Harp",        2.0f,  3.0f, 1.3f, 170, 1100, 240, 0.24f, 0.0f, 0.06f, 0.65f, 0.50f},
};

float FmSynth::sine(float phase) {
  // Cycles, rather than radians. FM indices are bounded below Nyquist at note-on.
  phase -= floorf(phase);
  const float position = phase * 1024.0f;
  const uint16_t i = uint16_t(position);
  const float fraction = position - i;
  return SineTable[i] + fraction * (SineTable[i + 1] - SineTable[i]);
}

void FmSynth::begin() {
  preset_ = 0;
  volume_ = targetVolume_ = 0;
  delayPos_ = 0;
  for (auto& v : voices_) v = Voice{};
  for (int i = 0; i < 8192; ++i) delayL_[i] = delayR_[i] = 0;
  for (int n = 0; n < 128; ++n) frequencies_[n] = 440.0f * powf(2.0f, float(n - 69) / 12.0f);
  setVolume(DefaultVolume);
}

void FmSynth::setVolume(uint8_t volume) {
  if (volume > 127) volume = 127;
  const float level = float(volume) / 127.0f;
  targetVolume_ = level * level * 0.70f;
}

void FmSynth::noteOn(uint8_t note, uint8_t velocity) {
  if (note > 127 || !velocity) { noteOff(note); return; }
  Voice* selected = &voices_[0];
  for (auto& v : voices_) {
    if (!v.active) { selected = &v; break; }
    if (v.envelope < selected->envelope) selected = &v;
  }
  Voice& v = *selected;
  v = Voice{};
  v.active = true;
  v.note = note;
  v.patch = Presets[preset_];
  v.increment = frequencies_[note] / SampleRate;
  // Roll down modulation for high external MIDI notes to reduce aliasing.
  const float safeIndex = 0.42f / (v.increment * (v.patch.ratio + v.patch.ratio2)) - 1.0f;
  if (v.patch.index > safeIndex) v.patch.index = safeIndex > 0 ? safeIndex : 0;
  v.velocity = float(velocity > 127 ? 127 : velocity) / 127.0f;
  v.velocity *= v.velocity;
  v.modEnvelope = v.patch.index * (0.45f + 0.55f * v.velocity);
  v.decay = expf(-1.0f / (v.patch.decayMs * 0.001f * SampleRate));
  v.modDecay = expf(-1.0f / (v.patch.modMs * 0.001f * SampleRate));
  v.release = expf(-6.9f / (v.patch.releaseMs * 0.001f * SampleRate));
}

void FmSynth::noteOff(uint8_t note) {
  for (auto& v : voices_) if (v.active && v.note == note) v.released = true;
}

void FmSynth::panic() {
  for (auto& v : voices_) v.active = false;
  for (int i = 0; i < 8192; ++i) delayL_[i] = delayR_[i] = 0;
}

void FmSynth::render(float& left, float& right) {
  float dryL = 0, dryR = 0;
  for (auto& v : voices_) {
    if (!v.active) continue;
    v.attack += 1.0f / (0.0025f * SampleRate); // 2.5 ms click-free onset.
    if (v.attack > 1.0f) v.attack = 1.0f;
    if (v.released) v.envelope *= v.release;
    else if (v.attack < 1.0f) v.envelope = v.attack;
    else v.envelope *= v.decay;
    if (v.envelope < 0.00002f && v.attack >= 1.0f) { v.active = false; continue; }
    v.modEnvelope *= v.modDecay;
    const float mod = sine(v.mod + v.previous * v.patch.feedback * 0.08f);
    const float mod2 = sine(v.mod2) * 0.27f;
    const float carrier = sine(v.phase + (mod + mod2) * v.modEnvelope * 0.15915494f);
    const float harmonic = sine(v.phase * 2.0f + mod * 0.08f);
    const float sub = sine(v.subPhase);
    v.previous = mod;
    const float sample = (carrier + harmonic * v.patch.harmonic + sub * v.patch.sub)
                         * v.envelope * v.velocity * v.patch.gain * 0.34f;
    // Alternate slight stereo placement by pitch while keeping both channels audible.
    const float pan = (v.note & 1) ? v.patch.pan : 1.0f - v.patch.pan;
    dryL += sample * (1.0f - 0.42f * pan);
    dryR += sample * (0.58f + 0.42f * pan);
    v.phase += v.increment; if (v.phase >= 1.0f) v.phase -= 1.0f;
    v.mod += v.increment * v.patch.ratio; v.mod -= floorf(v.mod);
    v.mod2 += v.increment * v.patch.ratio2; v.mod2 -= floorf(v.mod2);
    v.subPhase += v.increment * 0.5f; if (v.subPhase >= 1.0f) v.subPhase -= 1.0f;
  }
  const float echoL = delayL_[delayPos_], echoR = delayR_[delayPos_];
  delayL_[delayPos_] = dryL + echoR * 0.28f;
  delayR_[delayPos_] = dryR + echoL * 0.28f;
  delayPos_ = (delayPos_ + 1) & 8191;
  volume_ += (targetVolume_ - volume_) * 0.0015f;
  left = (dryL + echoL * 0.22f) * volume_;
  right = (dryR + echoR * 0.22f) * volume_;
  // A bounded soft limiter also covers dense overlapping release tails.
  left /= 1.0f + fabsf(left);
  right /= 1.0f + fabsf(right);
}
}
