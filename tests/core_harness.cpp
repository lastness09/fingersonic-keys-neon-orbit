#include "ArpEngine.h"
#include "FmSynth.h"
#include "Controls.h"
#include "App1Recovery.h"
#include "PerformanceLeds.h"
#include "esp_partition.h"
#include <stddef.h>

extern "C" {
int _fltused = 0;
void* memset(void* target, int value, size_t size) {
  auto* p = static_cast<unsigned char*>(target);
  while (size--) *p++ = static_cast<unsigned char>(value);
  return target;
}
}

static arp::ArpEngine engine;
static arp::FmSynth synth;
static arp::Encoder encoder;
static arp::Button button;
static arp::RecoveryGesture gesture;
static arp::DualContactKey contact;
static arp::PerformanceLeds lights;
static esp_partition_t partition;
static int present, setResult, setCalls, restarts, findErrors;

const esp_partition_t* esp_partition_find_first(int type, int subtype, const char* label) {
  if (type != ESP_PARTITION_TYPE_APP || subtype != ESP_PARTITION_SUBTYPE_APP_OTA_1 || label) ++findErrors;
  return present ? &partition : nullptr;
}
int esp_ota_set_boot_partition(const esp_partition_t* selected) {
  ++setCalls;
  if (selected != &partition) ++findErrors;
  return setResult;
}
void esp_restart() { ++restarts; }

#define API extern "C" __declspec(dllexport)
API void reset() { engine.reset(); synth.begin(); lights.reset(); }
API void note_on(int note, int vel) { engine.noteOn(note, vel); }
API void note_off(int note) { engine.noteOff(note); }
API void panic() { engine.panic(); synth.panic(); }
API void set_pattern(int value) { engine.setPattern(value); }
API void set_bpm(int value) { engine.setBpm(value); }
API int get_bpm() { return engine.bpm(); }
API void set_octaves(int value) { engine.setOctaves(value); }
API int get_octaves() { return engine.octaves(); }
API unsigned tick() {
  const auto e = engine.tick();
  return unsigned(e.on + 1) | (unsigned(e.off + 1) << 8) | (unsigned(e.velocity) << 16);
}
API unsigned held() { return engine.heldKeyMask(); }
API int active() { return engine.activeNote(); }
API unsigned serial() { return engine.triggerSerial(); }
API unsigned pattern_mask(int index) { return arp::Patterns[index].hits; }
API int led_for_note(int note) { return arp::ArpEngine::ledForNote(note); }
API void light_trigger(int note, int velocity, int step, int bpm, unsigned at) {
  arp::LightCue cue;
  cue.note = note;
  cue.velocity = velocity;
  cue.step = step;
  cue.bpm = bpm;
  cue.at = at;
  lights.trigger(cue);
}
API void light_render(unsigned now, unsigned heldMask, int peak, int preset, unsigned char* rgb) {
  arp::LedPixel pixels[arp::KeyCount];
  lights.render(now, heldMask, peak, preset, pixels);
  for (int i = 0; i < arp::KeyCount; ++i) {
    rgb[i * 3] = pixels[i].red;
    rgb[i * 3 + 1] = pixels[i].green;
    rgb[i * 3 + 2] = pixels[i].blue;
  }
}
API void set_preset(int value) { synth.setPreset(value); }
API void set_volume(int value) { synth.setVolume(value); }
API void synth_on(int note, int velocity) { synth.noteOn(note, velocity); }
API void synth_off(int note) { synth.noteOff(note); }
API void render(float* out, int frames, int withArp) {
  for (int i = 0; i < frames; ++i) {
    if (withArp) {
      const auto e = engine.tick();
      if (e.off >= 0) synth.noteOff(e.off);
      if (e.on >= 0) synth.noteOn(e.on, e.velocity);
    }
    synth.render(out[i * 2], out[i * 2 + 1]);
  }
}
API void controls_reset() {
  encoder = arp::Encoder{};
  button = arp::Button{};
  gesture = arp::RecoveryGesture{};
  contact = arp::DualContactKey{};
}
API int encoder_update(int ab) { return encoder.update(ab); }
API int button_update(int down, unsigned now) { return button.update(down, now); }
API int button_tap(unsigned now) { return button.tap(now); }
API void button_rotate() { button.rotated(); }
API int recovery_update(int key0, int tap, int cancel, unsigned now) {
  return gesture.update(key0, tap, cancel, now);
}
API int contact_update(int a, int b, unsigned now) { return contact.update(a, b, now); }
API int app1_case(int exists, unsigned address, unsigned size, int result) {
  present = exists;
  partition = {address, size};
  setResult = result;
  setCalls = restarts = findErrors = 0;
  return enterApp1();
}
API int app1_set_calls() { return setCalls; }
API int app1_restarts() { return restarts; }
API int app1_find_errors() { return findErrors; }
