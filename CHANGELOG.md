# Changelog

## 0.2.0-beta.1 - October 22, 2026

- Add USB MIDI 1.0 device input/output using the Arduino core's TinyUSB interface; switch the build to USB-OTG with CDC disabled.
- Add BLE MIDI input/output with the standard service/characteristic and Neon Orbit device name using NimBLE-Arduino 2.3.1.
- Send generated arpeggiator notes on channel 1 to both connected transports; accept note input on channels 1-16, sustain, internal volume and preset selection.
- Separate physical-key, per-channel USB and BLE note ownership. Add disconnect, pedal, queue-pressure, stale-output, retry and recovery cleanup.
- Keep transport stack calls outside the audio task and perform optional BLE initialization in a background task.
- Expand native validation from 28 to 48 tests and document pending device interoperability checks. This preview does not inherit the 0.1.0 hardware validation status.

## 0.1.0 - October 21, 2026

Initial standalone arpeggiator/FM instrument with ten patterns, ten presets, 1-8 range settings, velocity response, reactive lighting, stereo echo and App1 recovery. Maintainer-confirmed device testing. No USB/BLE MIDI in this version.
