# Neon Orbit validation

Current preview: **0.2.0-beta.1**, October 22, 2026.

| Version | Validation status |
| --- | --- |
| 0.1.0 | The maintainer confirmed testing on an instrument on October 21, 2026; its publication build and 28 host tests passed. This version did not implement USB/BLE MIDI. |
| 0.2.0-beta.1 | The MIDI-enabled ESP32-S3 build and all **48 host tests passed on October 22, 2026**. **On-device USB enumeration, BLE-MIDI interoperability and concurrent audio performance remain to be verified.** The earlier hardware confirmation does not validate these new transports. |

## Checks completed for the MIDI preview

- Build: Arduino ESP32 **3.3.2**, Adafruit NeoPixel **1.15.1**, NimBLE-Arduino **2.3.1**; ESP32-S3, 240 MHz, 16 MB flash, DIO 80 MHz, custom partitions, internal RAM and **USB-OTG (TinyUSB)** with USB CDC disabled.
- Application BIN: **676,880 bytes**, below the **2,097,152-byte** App0 limit. The build script checks image magic and ESP32-S3 chip ID; the release process also validates the ESP image checksum and appended hash using esptool.
- SHA-256: `95e324e334b1fdac0c745dd88ae8efa918b88743dfa3f8bf21ce2aa12f2449db`.
- Static data uses **127,380 bytes** according to the linker, leaving **200,300 bytes** of the linker's dynamic-memory budget. These are build statistics, not measurements of free heap after Bluetooth startup.
- The native suite compiles the actual portable C++ engine, synthesis, controls, recovery, LEDs, MIDI framing, input ownership and output-buffer modules with warnings as errors. Device OTA functions are mocked; USB/TinyUSB, NimBLE, radio, DAC and FreeRTOS scheduling are not simulated by this suite.
- The isolated package contains this MIT-licensed community application and public build documentation. The official product application, App1 uploader and factory bootloader remain outside this repository.

## Host test coverage

The original musical and recovery coverage remains: all ten pattern masks and paired gates; pitch orders, range limits and deduplication; dynamic input changes and tempo/range phase; release/panic; FM presets, finite bounded audio and smoothing; encoder/button/contact behavior; App1 selection failures; and LED timing, trails, amplitude response and 88-pitch mapping.

The added MIDI coverage exercises physical/USB/BLE note ownership; all 16 channels per transport; velocity-zero Note Off; per-channel sustain; CC120/121/123 semantics; source disconnect cleanup; A0-C8 limits; driving all ten patterns entirely from MIDI; USB cable/CIN/data validation; BLE packet-local running status, timestamps, system messages and fragmented ignored SysEx; truncated/malformed input plus 2,000 deterministic random packets; timestamp wrap; ordered output retries, explicit Note Off/panic, queue overflow, stale backlogs, reconnect generations and unsigned clock wrap.

Run the complete suite on Windows with LLVM's `clang++.exe`, `llvm-dlltool.exe` and linker:

```text
python tests/test_core.py --llvm-bin C:/path/to/llvm/bin --preview build/neon-orbit-midi-preview.wav -v
```

All 48 tests include the software audio preview. The preview is synthesized on the computer; it is not an instrument recording or a USB/BLE latency test.

## Required on-device MIDI acceptance

1. Boot the preview with the existing partition layout and verify all 24 keys, velocity, both encoders, every pattern/preset, LEDs and local audio.
2. Connect a USB data cable and confirm a USB MIDI device/interface named Neon Orbit / Neon Orbit MIDI appears in the intended host. Record generated channel-1 arpeggios and send channel-1 and channel-16 notes back as input in separate routing tests. Avoid a feedback route.
3. Use the intended BLE-MIDI application to discover Neon Orbit, connect and subscribe to notifications. Verify Note On/Off in both directions, then test USB and BLE together. Only one BLE central is supported.
4. Hold the same pitch through physical keys, two USB channels and BLE; release/disconnect each source in turn. Remaining owners must continue. Exercise sustain, velocity-zero Note On, CC7, programs 0-9 and CC120/121/123.
5. Test USB unplug/reconfiguration, BLE disconnect/reconnect and notification subscription changes while playing. Check that stale queued notes do not replay and no external note remains stuck. Test host congestion and the output panic path.
6. Record timing and audio stability at 30, 120 and 300 BPM with simultaneous MIDI traffic and LEDs. Measure runtime heap, Bluetooth startup, sustained CPU headroom and speaker/headphone output; no such measurements are claimed here.
7. With notes active, hold physical Key0 and short-click the left encoder five times within one second. Confirm App1 entry and restoration of a known-good App0. MIDI cannot invoke this physical gesture.
8. Confirm recovery behavior against the device's actual production date and installed bootloader. Pre-January 1, 2027 units still require a working App0 to invoke App1; an unbootable custom application can remove normal user recovery.

Until these checks are confirmed on an instrument, this MIDI build remains a prerelease. Host tests and a successful link establish software checks, not host/phone compatibility or hardware performance.
