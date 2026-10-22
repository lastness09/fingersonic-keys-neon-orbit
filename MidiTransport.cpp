// SPDX-License-Identifier: MIT
#include <Arduino.h>
#include <USB.h>
#include <USBMIDI.h>
#include <NimBLEDevice.h>
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "MidiTransport.h"
#include "MidiOutput.h"

#if ARDUINO_USB_MODE != 0
#error "Neon Orbit MIDI requires USB-OTG (TinyUSB), not Hardware CDC/JTAG. Use build.ps1."
#endif

namespace arp {
namespace {
constexpr char DeviceName[] = "Neon Orbit";
constexpr char ServiceUuid[] = "03b80e5a-ede8-4b33-a751-6ce34ec4c700";
constexpr char CharacteristicUuid[] = "7772e5db-3868-4112-a1a9-f2669d106bf3";
constexpr uint8_t RxCapacity = 64;
USBMIDI usbMidi("Neon Orbit MIDI");
NimBLECharacteristic* bleMidi = nullptr;
uint16_t blePeer = BLE_HS_CONN_HANDLE_NONE;
BleMidiDecoder bleDecoder; // Used only by the NimBLE host's serialized callbacks.
MidiOutput outputs[2];
portMUX_TYPE midiLock = portMUX_INITIALIZER_UNLOCKED;
StaticQueue_t rxStorage;
uint8_t rxBytes[RxCapacity * sizeof(MidiInputEvent)];
QueueHandle_t incoming = nullptr;
uint32_t inputEpoch[2] = {};
uint8_t resetInputs = 0;
bool usbStarted = false, usbMounted = false;

uint8_t indexFor(MidiSource source) { return source == MidiSource::Ble ? 1 : 0; }
MidiSource sourceFor(uint8_t index) { return index ? MidiSource::Ble : MidiSource::Usb; }

void resetInput(MidiSource source) {
  const uint8_t index = indexFor(source);
  portENTER_CRITICAL(&midiLock);
  ++inputEpoch[index];
  resetInputs |= 1 << index;
  portEXIT_CRITICAL(&midiLock);
}

void receive(MidiSource source, const MidiMessage& message) {
  if (!incoming) return;
  MidiInputEvent event;
  event.source = source;
  event.message = message;
  portENTER_CRITICAL(&midiLock);
  event.epoch = inputEpoch[indexFor(source)];
  portEXIT_CRITICAL(&midiLock);
  if (xQueueSend(incoming, &event, 0) != pdTRUE)
    resetInput(source); // Invalidate queued notes instead of losing an off forever.
}

void receiveBle(const MidiMessage& message, void*) { receive(MidiSource::Ble, message); }

class ServerCallbacks final : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* server, NimBLEConnInfo& info) override {
    bool reject;
    portENTER_CRITICAL(&midiLock);
    reject = blePeer != BLE_HS_CONN_HANDLE_NONE;
    if (!reject) {
      blePeer = info.getConnHandle();
      outputs[1].setReady(false);
    }
    portEXIT_CRITICAL(&midiLock);
    if (reject) { server->disconnect(info.getConnHandle()); return; }
    bleDecoder.reset();
    resetInput(MidiSource::Ble);
    // 7.5-15 ms requested; the central selects the actual interval.
    server->updateConnParams(info.getConnHandle(), 6, 12, 0, 400);
  }
  void onDisconnect(NimBLEServer*, NimBLEConnInfo& info, int) override {
    bool wasPeer;
    portENTER_CRITICAL(&midiLock);
    wasPeer = blePeer == info.getConnHandle();
    if (wasPeer) {
      blePeer = BLE_HS_CONN_HANDLE_NONE;
      outputs[1].setReady(false);
    }
    portEXIT_CRITICAL(&midiLock);
    if (wasPeer) {
      bleDecoder.reset();
      resetInput(MidiSource::Ble);
      NimBLEDevice::startAdvertising();
    }
  }
} serverCallbacks;

class CharacteristicCallbacks final : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* characteristic, NimBLEConnInfo& info) override {
    portENTER_CRITICAL(&midiLock);
    const bool accepted = blePeer == info.getConnHandle();
    portEXIT_CRITICAL(&midiLock);
    if (!accepted) return;
    const auto value = characteristic->getValue();
    if (!bleDecoder.decode(value.data(), value.size(), receiveBle, nullptr))
      resetInput(MidiSource::Ble);
  }
  void onSubscribe(NimBLECharacteristic*, NimBLEConnInfo& info, uint16_t value) override {
    portENTER_CRITICAL(&midiLock);
    if (blePeer == info.getConnHandle()) outputs[1].setReady(value & 1);
    portEXIT_CRITICAL(&midiLock);
  }
} characteristicCallbacks;

void startBle(void*) {
  // Optional radio initialization runs after the control loop is available;
  // physical App1 entry never waits for Bluetooth discovery or a connection.
  // NimBLE's own NVS-error recovery can erase the entire NVS partition.
  // Preflight without erasing: on a storage error, leave BLE disabled instead.
  if (nvs_flash_init() == ESP_OK && NimBLEDevice::init(DeviceName)) {
    auto* server = NimBLEDevice::createServer();
    if (server) {
      server->setCallbacks(&serverCallbacks, false);
      server->advertiseOnDisconnect(false);
      auto* service = server->createService(ServiceUuid);
      if (service) {
        auto* characteristic = service->createCharacteristic(CharacteristicUuid,
          NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR |
          NIMBLE_PROPERTY::NOTIFY, 512);
        if (characteristic) {
          characteristic->setCallbacks(&characteristicCallbacks);
          if (service->start()) {
            portENTER_CRITICAL(&midiLock);
            bleMidi = characteristic;
            portEXIT_CRITICAL(&midiLock);
            auto* advertising = NimBLEDevice::getAdvertising();
            advertising->enableScanResponse(true);
            advertising->addServiceUUID(ServiceUuid);
            advertising->setName(DeviceName);
            advertising->start();
          }
        }
      }
    }
  }
  vTaskDelete(nullptr);
}

bool transmit(uint8_t index, const MidiMessage& message) {
  if (!index) {
    midiEventPacket_t packet{uint8_t(message.status >> 4), message.status, message.data1, message.data2};
    return usbMidi.writePacket(&packet);
  }
  NimBLECharacteristic* characteristic;
  uint16_t peer;
  portENTER_CRITICAL(&midiLock);
  characteristic = bleMidi;
  peer = blePeer;
  portEXIT_CRITICAL(&midiLock);
  if (!characteristic || peer == BLE_HS_CONN_HANDLE_NONE) return false;
  uint8_t packet[5];
  const uint8_t size = encodeBleMidi(message, millis(), packet);
  return size && characteristic->notify(packet, size, peer);
}
}

void midiBegin() {
  incoming = xQueueCreateStatic(RxCapacity, sizeof(MidiInputEvent), rxBytes, &rxStorage);
  USB.productName(DeviceName);
  usbMidi.begin();
  usbStarted = USB.begin();
  // A failed optional task creation leaves USB, the instrument and recovery live.
  xTaskCreatePinnedToCore(startBle, "MIDI_BLE_INIT", 6144, nullptr, 1, nullptr, ARDUINO_RUNNING_CORE);
}

void midiPoll() {
  const bool mounted = usbStarted && tud_midi_mounted();
  if (mounted != usbMounted) {
    usbMounted = mounted;
    resetInput(MidiSource::Usb);
    portENTER_CRITICAL(&midiLock);
    outputs[0].setReady(mounted);
    portEXIT_CRITICAL(&midiLock);
  }
  if (mounted) {
    midiEventPacket_t packet;
    for (uint8_t n = 0; n < 16 && usbMidi.readPacket(&packet); ++n) {
      const uint8_t bytes[4] = {packet.header, packet.byte1, packet.byte2, packet.byte3};
      MidiMessage message;
      if (decodeUsbMidi(bytes, message)) receive(MidiSource::Usb, message);
    }
  }
  for (uint8_t index = 0; index < 2; ++index) {
    for (uint8_t n = 0; n < 8; ++n) {
      MidiDelivery delivery;
      portENTER_CRITICAL(&midiLock);
      const bool available = outputs[index].next(millis(), delivery);
      portEXIT_CRITICAL(&midiLock);
      if (!available || !transmit(index, delivery.message)) break;
      portENTER_CRITICAL(&midiLock);
      outputs[index].acknowledge(delivery);
      portEXIT_CRITICAL(&midiLock);
    }
  }
}

bool midiRead(MidiInputEvent& event) {
  if (!incoming) return false;
  portENTER_CRITICAL(&midiLock);
  const uint8_t resets = resetInputs;
  if (resets) {
    const uint8_t index = resets & 1 ? 0 : 1;
    resetInputs &= ~(1 << index);
    event = {};
    event.source = sourceFor(index);
    event.reset = true;
    portEXIT_CRITICAL(&midiLock);
    return true;
  }
  for (uint8_t n = 0; n < RxCapacity && xQueueReceive(incoming, &event, 0) == pdTRUE; ++n) {
    const bool current = event.epoch == inputEpoch[indexFor(event.source)];
    if (current) {
      portEXIT_CRITICAL(&midiLock);
      return true;
    }
  }
  portEXIT_CRITICAL(&midiLock);
  return false;
}

void midiSendGenerated(uint8_t status, uint8_t note, uint8_t velocity) {
  const MidiMessage message{uint8_t(status & 0xF0), note, velocity}; // Output channel 1.
  const uint32_t now = millis();
  portENTER_CRITICAL(&midiLock);
  outputs[0].push(message, now);
  outputs[1].push(message, now);
  portEXIT_CRITICAL(&midiLock);
}

void midiPanicOutputs() {
  portENTER_CRITICAL(&midiLock);
  for (auto& output : outputs) output.panic();
  portEXIT_CRITICAL(&midiLock);
}
}
