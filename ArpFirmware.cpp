// SPDX-License-Identifier: MIT
#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "ArpFirmware.h"
#include "ArpEngine.h"
#include "FmSynth.h"
#include "PerformanceLeds.h"
#include "Controls.h"
#include "AudioOutput.h"
#include "App1Recovery.h"

namespace {
using namespace arp;
Adafruit_NeoPixel leds(KeyCount, LedPin, NEO_GRB + NEO_KHZ800);
ArpEngine engine;
FmSynth synth;
AudioOutput audio;
DualContactKey keys[KeyCount];
Encoder leftEncoder, rightEncoder;
Button leftButton, rightButton;
RecoveryGesture recovery;
PerformanceLeds performanceLeds;

enum class CommandType : uint8_t { On, Off, Pattern, Tempo, Preset, Volume, Octaves, Panic };
struct Command { CommandType type; uint8_t note; uint16_t value; };
constexpr uint8_t QueueLength = 64;
StaticQueue_t queueStorage;
uint8_t queueBytes[QueueLength * sizeof(Command)];
QueueHandle_t commands = nullptr;
constexpr uint8_t CueQueueLength = 16;
StaticQueue_t cueQueueStorage;
uint8_t cueQueueBytes[CueQueueLength * sizeof(LightCue)];
QueueHandle_t lightCues = nullptr;
portMUX_TYPE sharedLock = portMUX_INITIALIZER_UNLOCKED;
bool emergencyPanic = false, audioFailed = false;
struct VisualState {
  uint32_t held = 0, generation = 0;
  uint8_t peak = 0;
} visual;

uint16_t bpm = DefaultBpm;
uint8_t pattern = 0, preset = 0, volume = DefaultVolume;
uint8_t octaves = DefaultArpOctaves;
bool toneMode = true, recoveryPending = false;
uint32_t recoveryAt = 0, errorAt = 0, bootAt = 0;
bool recoveryError = false;
enum class Overlay : uint8_t { None, Pattern, Tempo, Preset, Volume, Octaves };
Overlay overlay = Overlay::None;
uint32_t overlayAt = 0;

void send(CommandType type, uint16_t value = 0, uint8_t note = 0) {
  Command command{type, note, value};
  if (commands && xQueueSend(commands, &command, 0) == pdTRUE) return;
  // Queue saturation must stop sound rather than losing a release indefinitely.
  portENTER_CRITICAL(&sharedLock);
  emergencyPanic = true;
  portEXIT_CRITICAL(&sharedLock);
}

void showOverlay(Overlay kind) { overlay = kind; overlayAt = millis(); }

void clearPerformance() {
  engine.panic();
  synth.panic();
  xQueueReset(lightCues);
  portENTER_CRITICAL(&sharedLock);
  visual.held = 0;
  visual.peak = 0;
  ++visual.generation;
  portEXIT_CRITICAL(&sharedLock);
}

void audioTask(void*) {
  int16_t pcm[BlockFrames * 2];
  synth.begin();
  engine.reset();
  for (;;) {
    bool panic;
    portENTER_CRITICAL(&sharedLock);
    panic = emergencyPanic;
    emergencyPanic = false;
    portEXIT_CRITICAL(&sharedLock);
    if (panic) { xQueueReset(commands); clearPerformance(); }
    Command command;
    // A bounded drain protects the audio deadline even if controls are noisy.
    for (uint8_t i = 0; i < QueueLength && xQueueReceive(commands, &command, 0) == pdTRUE; ++i) {
      switch (command.type) {
        case CommandType::On: engine.noteOn(command.note, command.value); break;
        case CommandType::Off: engine.noteOff(command.note); break;
        case CommandType::Pattern: engine.setPattern(command.value); break;
        case CommandType::Tempo: engine.setBpm(command.value); break;
        case CommandType::Preset: synth.setPreset(command.value); break;
        case CommandType::Volume: synth.setVolume(command.value); break;
        case CommandType::Octaves: engine.setOctaves(command.value); break;
        case CommandType::Panic: clearPerformance(); break;
      }
    }
    LightCue cue;
    bool hasCue = false;
    float blockPeak = 0;
    for (uint16_t i = 0; i < BlockFrames; ++i) {
      const ArpEvent event = engine.tick();
      if (event.off >= 0) synth.noteOff(event.off);
      if (event.on >= 0) {
        synth.noteOn(event.on, event.velocity);
        cue.note = event.on;
        cue.velocity = event.velocity;
        cue.step = event.step;
        cue.bpm = engine.bpm();
        hasCue = true;
      }
      float left, right;
      synth.render(left, right);
      const float absL = left < 0 ? -left : left, absR = right < 0 ? -right : right;
      if (absL > blockPeak) blockPeak = absL;
      if (absR > blockPeak) blockPeak = absR;
      pcm[i * 2] = int16_t(left * 32767.0f);
      pcm[i * 2 + 1] = int16_t(right * 32767.0f);
    }
    if (!audio.write(pcm)) {
      clearPerformance();
      portENTER_CRITICAL(&sharedLock);
      audioFailed = true;
      portEXIT_CRITICAL(&sharedLock);
      vTaskDelete(nullptr); // Controls and App1 recovery remain alive.
    }
    // Max tempo is one onset per 50 ms; a 128-frame block is only 2.9 ms.
    // Preserve each onset through a separate nonblocking queue, including short
    // gates which may start and finish between two LED frames.
    if (hasCue) {
      cue.at = millis();
      if (xQueueSend(lightCues, &cue, 0) != pdTRUE) {
        LightCue oldest;
        xQueueReceive(lightCues, &oldest, 0);
        xQueueSend(lightCues, &cue, 0);
      }
    }
    const uint8_t peak = blockPeak >= 0.102f ? 255 : uint8_t(blockPeak * 2500.0f);
    const uint32_t held = engine.heldKeyMask();
    portENTER_CRITICAL(&sharedLock);
    visual.held = held;
    if (peak > visual.peak) visual.peak = peak;
    portEXIT_CRITICAL(&sharedLock);
  }
}

uint8_t keyIndex(uint8_t pair, uint8_t column) {
  uint8_t index = pair * 6 + column;
  // Documented Keys wiring swaps; LED indices are already logical order.
  if (index == 5) return 6;
  if (index == 6) return 5;
  if (index == 17) return 18;
  if (index == 18) return 17;
  return index;
}

void pollEncoders();

void scanKeys() {
  // Documented matrix scan: drive HIGH, then verify ownership after
  // releasing the column. All sense pins remain inputs throughout the scan.
  for (uint8_t column = 0; column < 6; ++column) {
    bool high[8], low[8];
    digitalWrite(Columns[column], HIGH);
    pinMode(Columns[column], OUTPUT);
    delayMicroseconds(ColumnSettleUs);
    for (uint8_t i = 0; i < 8; ++i) high[i] = digitalRead(Contacts[i]);
    delayMicroseconds(3);
    for (uint8_t i = 0; i < 8; ++i) high[i] = high[i] && digitalRead(Contacts[i]);
    pinMode(Columns[column], INPUT_PULLDOWN);
    delayMicroseconds(ColumnSettleUs);
    for (uint8_t i = 0; i < 8; ++i) low[i] = digitalRead(Contacts[i]);
    delayMicroseconds(3);
    for (uint8_t i = 0; i < 8; ++i) low[i] = low[i] || digitalRead(Contacts[i]);
    const uint32_t now = micros();
    for (uint8_t pair = 0; pair < 4; ++pair) {
      const uint8_t index = keyIndex(pair, column);
      const int16_t event = keys[index].update(high[pair * 2] && !low[pair * 2],
                                              high[pair * 2 + 1] && !low[pair * 2 + 1], now);
      if (!recoveryPending) {
        if (event > 0) send(CommandType::On, event, BaseNote + index);
        else if (event < 0) send(CommandType::Off, 0, BaseNote + index);
      }
    }
    // Read rotation between columns as well as between complete scans.
    pollEncoders();
  }
}

void pollEncoders() {
  const uint32_t now = millis();
  const bool leftDown = digitalRead(LeftSwitch) == LOW;
  const bool rightDown = digitalRead(RightSwitch) == LOW;
  const int8_t leftEdge = leftButton.update(leftDown, now);
  const int8_t rightEdge = rightButton.update(rightDown, now);
  const int8_t left = leftEncoder.update((digitalRead(LeftA) << 1) | digitalRead(LeftB));
  const int8_t right = rightEncoder.update((digitalRead(RightA) << 1) | digitalRead(RightB));
  if (recoveryPending) return;
  const bool leftTap = leftEdge == -1 && leftButton.tap(now);
  const bool cancel = left != 0 || (leftEdge == -1 && !leftButton.tap(now));
  if (recovery.update(keys[0].active(), leftTap, cancel, now)) {
    recoveryPending = true;
    recoveryAt = now;
    send(CommandType::Panic);
    return;
  }
  if (left) {
    leftButton.rotated();
    // Raw state makes an immediate press-and-turn adjust BPM even during debounce.
    if (leftDown || leftButton.held()) {
      bpm = constrain(int(bpm) + left * 2, int(MinBpm), int(MaxBpm));
      send(CommandType::Tempo, bpm);
      showOverlay(Overlay::Tempo);
    } else {
      pattern = (int(pattern) + left + PatternCount) % PatternCount;
      send(CommandType::Pattern, pattern);
      showOverlay(Overlay::Pattern);
    }
  }
  // The original right-knob workflow: short click toggles tone/volume, then turn.
  if (rightEdge == -1 && rightButton.tap(now)) {
    toneMode = !toneMode;
    showOverlay(toneMode ? Overlay::Preset : Overlay::Volume);
  }
  if (right) {
    rightButton.rotated();
    if (rightDown || rightButton.held()) {
      octaves = constrain(int(octaves) + right, int(MinArpOctaves), int(MaxArpOctaves));
      send(CommandType::Octaves, octaves);
      showOverlay(Overlay::Octaves);
    } else if (toneMode) {
      preset = (int(preset) + right + PresetCount) % PresetCount;
      send(CommandType::Preset, preset);
      showOverlay(Overlay::Preset);
    } else {
      volume = constrain(int(volume) + right * 2, 0, 127);
      send(CommandType::Volume, volume);
      showOverlay(Overlay::Volume);
    }
  }
}

void updateLeds() {
  static uint32_t frameAt = 0, seenGeneration = 0;
  uint32_t now = millis();
  if (uint32_t(now - frameAt) < 20) return; // 50 fps, outside the audio task.
  frameAt = now;
  VisualState state;
  bool failed;
  portENTER_CRITICAL(&sharedLock);
  state = visual;
  visual.peak = 0; // Consume the maximum amplitude across all intervening blocks.
  failed = audioFailed;
  portEXIT_CRITICAL(&sharedLock);
  if (state.generation != seenGeneration) {
    performanceLeds.reset();
    seenGeneration = state.generation;
  }
  LightCue cue;
  for (uint8_t i = 0; i < CueQueueLength && xQueueReceive(lightCues, &cue, 0) == pdTRUE; ++i)
    performanceLeds.trigger(cue);
  now = millis();
  LedPixel pixels[KeyCount];
  performanceLeds.render(now, state.held, state.peak, preset, pixels);
  if (overlay != Overlay::None && uint32_t(now - overlayAt) > 900) overlay = Overlay::None;
  for (uint8_t i = 0; i < KeyCount; ++i) {
    const LedPixel& pixel = pixels[i];
    uint32_t color = leds.Color(pixel.red, pixel.green, pixel.blue);
    const uint8_t brightness = max(pixel.red, max(pixel.green, pixel.blue));
    // Low-level overlays leave the bright sounding note and its trail visible.
    if (brightness < 60) {
      if (overlay == Overlay::Pattern && i / 2 == pattern && i < 20)
        color = leds.Color(0, 75, 110);
      if (overlay == Overlay::Preset && i / 2 == preset && i < 20)
        color = leds.Color(95, 20, 90);
      if (overlay == Overlay::Volume && i < (volume * 24 + 126) / 127)
        color = leds.Color(0, 65, 22);
      if (overlay == Overlay::Tempo && i < 1 + (bpm - MinBpm) * 23 / (MaxBpm - MinBpm))
        color = leds.Color(65, 24, 0);
      if (overlay == Overlay::Octaves && i < octaves * 3)
        color = leds.Color(0, 45, 95);
    }
    if (uint32_t(now - bootAt) < 650 && !state.held)
      color = leds.ColorHSV(uint16_t(i * 2730 + now * 40), 220, i <= uint32_t(now - bootAt) * 24 / 650 ? 110 : 0);
    if (failed) color = leds.Color(80, 0, 0);
    if (recoveryError && uint32_t(now - errorAt) < 1200) color = leds.Color(110, 0, 0);
    if (recoveryPending) color = leds.Color(80, 30, 0);
    leds.setPixelColor(i, color);
  }
  leds.show();
}
}

void arpSetup() {
  pinMode(arp::LeftA, INPUT_PULLUP);
  pinMode(arp::LeftB, INPUT_PULLUP);
  pinMode(arp::LeftSwitch, INPUT_PULLUP);
  pinMode(arp::RightA, INPUT_PULLUP);
  pinMode(arp::RightB, INPUT_PULLUP);
  pinMode(arp::RightSwitch, INPUT_PULLUP);
  for (uint8_t pin : arp::Columns) pinMode(pin, INPUT_PULLDOWN);
  for (uint8_t pin : arp::Contacts) pinMode(pin, INPUT_PULLDOWN);
  leds.begin();
  leds.setBrightness(arp::LedBrightness);
  leds.clear();
  leds.show();
  bootAt = millis();
  commands = xQueueCreateStatic(QueueLength, sizeof(Command), queueBytes, &queueStorage);
  lightCues = xQueueCreateStatic(CueQueueLength, sizeof(LightCue), cueQueueBytes, &cueQueueStorage);
  performanceLeds.reset();
  // No LittleFS mount, SF2 load, settings writes or PSRAM requirement.
  if (!commands || !lightCues || !audio.begin() ||
      xTaskCreatePinnedToCore(audioTask, "ARP_AUDIO", 6144, nullptr, 8, nullptr, 0) != pdPASS)
    audioFailed = true;
}

void arpLoop() {
  scanKeys();
  pollEncoders();
  updateLeds();
  if (recoveryPending && uint32_t(millis() - recoveryAt) >= 250) {
    // Existing App1 is selected only on the explicit Key0 + five-click gesture.
    // Selection errors leave the controls running, with a red error indication.
    if (!enterApp1()) {
      recoveryPending = false;
      recoveryError = true;
      errorAt = millis();
    }
  }
  delay(1);
}
