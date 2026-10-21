# Neon Orbit validation

Release: **0.1.0**, October 21, 2026.

The maintainer confirmed actual on-device validation on October 21, 2026. Compilation, binary inspection and the 28 host tests were separately repeated for publication.

## Checks completed

- The portable C++ modules' **28 existing host tests passed again** on October 21, 2026. The suite directly compiles and exercises the arpeggiator, FM synth, controls, LED renderer and App1 selector. Device OTA functions and restart are mocked.
- The application build uses Arduino ESP32 core **3.3.2**, Adafruit NeoPixel **1.15.1**, ESP32-S3, 240 MHz, 16 MB flash, DIO 80 MHz, custom partitions and no PSRAM.
- App0 image size, ESP32-S3 chip ID, checksum and embedded validation hash are checked before publication. The exact release size and SHA-256 are supplied by the release's `manifest.json`.
- The publication package is isolated from the official `main` application. It contains this community project's sources, public hardware mapping, tests and build instructions. No official application source, App1 source, bootloader image, partition-table BIN, encryption key, credential or full-device provisioning script is included.

## Host test coverage

The suite covers all ten pattern masks and gate pairing; ascending, descending, bounce, outside-in, repeated and randomized orders; one-key behavior; 1-8 range settings; A0-C8 boundaries; pitch deduplication and note ownership; dynamic pool changes and phase-preserving tempo/range updates; release-all/panic behavior; 30-300 BPM limits and long-run scheduling; all ten FM presets, bounded/finite audio and voice reuse; volume smoothing; encoder direction, bounce and invalid transitions; button debounce and press/turn interactions; dual-contact order, noise and slow-press behavior; recovery click cancellation, timeouts and clock wrap; rejected App1 layouts and failed OTA selection; LED note/partner brightness, audio-amplitude response, trails, ripples, short gates and all 88 candidate pitch mappings.

The test runner is Windows-specific. A 25-second preview rendered by the same host synth cycles through ten sound/pattern combinations. It demonstrates software output, not hardware audio quality.

## Hardware validation and device acceptance

On-device testing is maintainer-confirmed. The publication automation itself did not flash or operate an instrument. Detailed timing, CPU-headroom measurements and a per-hardware-revision test matrix were not supplied. When installing on another unit, use this acceptance checklist:

1. All 24 physical keys, contact order, slow/fast presses, multi-key input and release behavior.
2. Encoder direction/detents, sound/volume mode, tempo adjustment and 1-8 range changes during playback.
3. Actual DAC, speaker/headphone sound, output level, clipping, noise, CPU headroom and sustained audio stability.
4. Physical LED order, note/octave partner cues, audio response, overlays and brightness.
5. Key0 plus five short left-knob clicks entering the original App1 uploader, and restoration of a known-good official App0.
6. Recovery behavior appropriate to the device's actual production date and installed bootloader.

The maintainer confirmation records actual device testing; it does not establish measured performance for every unit or hardware revision. Older units remain dependent on a working App0 to invoke App1; a failed custom application can remove the normal recovery path. The release page and README retain this limitation.
