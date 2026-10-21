# Neon Orbit

### Arpeggiator + FM Synth for Fingersonic Keys

![Neon Orbit cover](cover.png)

Hold a few notes. Let them orbit. Neon Orbit turns Fingersonic Keys into a self-contained electronic instrument: ten rhythmic patterns, ten FM voices, a wide octave range and light trails that follow each generated note. Turn the knobs while you play to move from glassy plucks to metallic bells, rubbery bass and floating synth textures.

**Version 0.1.0 has been tested on the instrument by the maintainer, confirmed on October 21, 2026. Compilation and all 28 host tests also pass.** Read the recovery section before installing, especially on devices produced before January 1, 2027.

[Download App0 firmware](https://github.com/lastness09/fingersonic-keys-neon-orbit/releases/tag/v0.1.0) · [Keys community firmware](https://fingersonic.com/firmware/3/) · [Developer guide](https://github.com/lastness09/fingersonic-keys-devkit) · [Validation](VALIDATION.md) · [MIT license](LICENSE)

## Make a small keyboard feel expansive

- **Ten rhythm patterns:** straight, syncopated, Euclidean, three-note bursts and randomized motion.
- **Ten FM sounds:** plucks, kalimba, bells, bass, koto, marimba and synth textures, generated directly on the device.
- **30-300 BPM:** press and turn the left encoder for tempo changes in 2 BPM increments; default 120 BPM.
- **1-8 octave-range settings:** begin with the physical keys, then expand the candidate pitches across A0-C8. The default two-range setting makes a single C3 alternate between C3 and C4.
- **Velocity-sensitive playing:** dual-contact key scanning drives synthesis and rhythmic accents.
- **Note-reactive lighting:** a bright sounding note, a colored same-name octave partner, traveling trails and short light ripples. Display intensity responds to the generated audio amplitude.
- **Eight synthesis slots:** decaying tails overlap for a fuller sound, with smoothed volume, stereo cross-echo and soft limiting.
- **No SF2 or PSRAM requirement:** this is an algorithmic synthesizer. It does not mount LittleFS or replace the existing sound bank.

The arpeggiator produces one gated note at a time; the eight synthesis slots allow release tails to overlap. The 88-note range describes available candidate pitches, not 88 simultaneous voices or 88 physical keys.

## Quick performance guide

1. Start with a low listening level. Hold one or more keys to start the arpeggiator.
2. Turn the left knob to choose a rhythm. Hold it while turning to change tempo.
3. Turn the right knob to choose a sound. Click it to switch to volume mode, then turn to adjust the level; click again to return to sound selection.
4. Hold the right knob while turning to adjust the octave range. Release all keys to stop new notes; short release tails and echo may remain.

There is no latch: the arpeggiator follows the notes you actually hold. Parameters reset at power-on; this release does not save settings.

## Complete controls

| Gesture | Action |
| --- | --- |
| Hold one or more keyboard keys | Run the selected pattern using those notes and the selected octave range |
| Release all keyboard keys | Stop the arpeggiator; allow existing synthesis/echo tails to decay |
| Turn left knob | Cycle through the ten patterns |
| Hold left knob and turn | Change tempo by 2 BPM per detent, from 30 to 300 BPM |
| Short-click right knob | Toggle sound-selection and volume modes; starts in sound mode |
| Turn right knob in sound mode | Cycle through the ten FM presets |
| Turn right knob in volume mode | Change volume by 2, on a 0-127 scale |
| Hold right knob and turn | Change octave-range setting from 1 to 8; retains the previous sound/volume mode after release |
| Keep the leftmost key, Key0, held and short-click the left knob five times within one second | Show amber lights, then select and restart into the existing App1 uploader |

Holding/turning the left knob to change tempo is not counted as a recovery click. Releasing Key0, turning the left knob or making a long press cancels the recovery-click sequence. Holding/turning the right knob changes range without toggling its sound/volume mode on release. A standalone left-knob click has no additional action.

Changing pattern restarts its 16-step cycle. Tempo and octave-range changes preserve beat phase. The range expansion retains the original notes, shifts the octave window downward near C8, merges duplicate pitches and uses the greatest contributing key velocity. With all 12 pitch classes held, range 8 can span all 88 candidate notes from A0 to C8.

## Ten patterns

Each row is a 4/4 bar of sixteen sixteenth-note positions, grouped by beat. `x` is a trigger and `.` is a rest. Each pattern has its own gate length; beat accents modify the held-note velocity.

| # | Pattern | Rhythm | Note order |
| --- | --- | --- | --- |
| 1 | Neon Up | `x.x. x.x. x.x. x.x.` | Ascending eighth notes |
| 2 | Down Drive | `xxxx xxxx xxxx xxxx` | Descending sixteenth notes |
| 3 | Mirror Sync | `x..x ..x. x..x ..x.` | Up/down bounce with syncopation |
| 4 | Wide Steps | `x.x. ..x. x.x. ..x.` | Low, high, next-low, next-high |
| 5 | Tresillo | `x... ..x. .... x...` | Ascending 6+6+4 spacing |
| 6 | Offbeat | `..x. ..x. ..x. ..x.` | Descending offbeats; the first step is a rest |
| 7 | Euclid 5 | `x..x ..x. .x.. x...` | Five triggers per bar with bouncing pitch order |
| 8 | Euclid 7 | `x.x. x.x. x..x .x..` | Seven triggers per bar, alternating outside pitches |
| 9 | Triple Burst | `xxx. xxx. xxx. xxx.` | Three repeated sixteenth notes, then a rest and next pitch |
| 10 | Random Orbit | `xx.. x.xx x..x x.x.` | Random selection; avoids adjacent repeats with multiple candidates |

## Ten FM presets

| # | Preset | Character |
| --- | --- | --- |
| 1 | Glass Pluck | Clear, bright glass-like pluck |
| 2 | Neon Kalimba | Electronic thumb-piano sparkle |
| 3 | Copper Bell | Metallic bell with non-integer modulation ratios |
| 4 | Rubber Bass | Elastic low end with a sub-octave component |
| 5 | Acid Spark | Short, feedback-colored synth bite |
| 6 | Digital Koto | Fast-decaying electronic string pluck |
| 7 | Crystal Marimba | Crisp, percussive mallet tone |
| 8 | Velvet Synth | Softer synthesis with a longer decay |
| 9 | Laser Droplet | Bright, liquid electronic accent |
| 10 | Dream Harp | Gentle harp-like sparkle and a longer tail |

The engine combines two modulation signals, a carrier harmonic and optional sub-octave energy. Preset changes apply to new notes; active voices retain their previous envelope. Audio is 44.1 kHz, signed 16-bit stereo over the documented Keys I2S pins.

## Lights that follow the sound

Held keys have a dim base glow. Each synthesized onset creates a bright primary note and a more saturated same-name note in the other physical octave. Expanded notes outside the physical 24-key range fold back to appropriate pitch-class positions. A moving trail links note positions and a short ripple spreads to neighboring LEDs.

Parameter feedback uses cyan for pattern selection, purple for sound selection, amber for tempo, green for volume and blue for octave range. Pattern and preset numbers use two LEDs each among the first 20; range uses three LEDs per setting. Feedback normally clears after about 0.9 seconds. Bright playing cues take priority. LEDs update at 50 fps outside the audio task.

## Compatibility and boundaries

This firmware is for **Fingersonic Keys**, ESP32-S3, 16 MB flash, existing App0/App1 partition layout. It uses internal RAM with PSRAM disabled. It is not a Guitar or Beats firmware.

This release intentionally focuses on arpeggiation and direct synthesis. It does not provide USB MIDI, BLE MIDI, loops, the official chord mode, transposition, pitch bend or official app control. There is no sample playback and no SF2 installation step. If you extend it into a sampler, keep banks in **SF2**: the installed App1 uploader supports only executable BIN firmware and SF2 banks.

Power, charging and battery-level management remain handled by the independent hardware chip. The application does not control charging or read battery state. It does not mount or format LittleFS, change the sound bank or write official instrument settings. The only intentional persistent write is OTA boot selection when you explicitly invoke App1 recovery.

## Installation and return to the official instrument

1. Obtain `neon-orbit-v0.1.0.bin` from the release. Check its SHA-256 against the release manifest. Upload the **application BIN only**, not a ZIP, ELF, bootloader, partition table or merged system image.
2. Enter the existing App1 uploader. With the official Keys firmware running, hold Key0 and quickly press the left knob five times. Devices produced on or after January 1, 2027 with the recovery bootloader also support holding the right knob while powering on for at least 0.5 seconds.
3. Follow the official updater's hotspot connection instructions. With the host connected to the device, send the BIN to App1's existing upload interface. For example, on Windows:

```powershell
curl.exe --fail --show-error -F "file=@C:\path\to\neon-orbit-v0.1.0.bin" http://192.168.4.1/upload
```

4. Keep power stable, let the upload finish and allow the instrument to restart. Do not run the Arduino IDE's normal full-device upload or change eFuses.
5. To leave Neon Orbit, keep Key0 held and short-click the left knob five times within one second. Amber indicates pending App1 entry. If App1 is missing, its layout is wrong or selection fails, red feedback appears and the application remains running.
6. Use the [official Keys updater](https://update.fingersonic.cn/FingersonicUploader/) from App1 to restore the official App0. The original sound bank is preserved by Neon Orbit, so a separate sound-bank restoration is normally unnecessary unless you changed it elsewhere.

**Recovery limitation:** on devices produced before January 1, 2027, App1 entry depends on a working App0. If experimental firmware cannot boot, hangs or loses its recovery gesture, normal user recovery may be unavailable and the device may be bricked. Install at your own risk. The newer right-knob gesture requires the recovery bootloader and valid App1; it is not ROM download mode and cannot repair damaged system partitions.

## Build the open-source project

Clone into a folder named `NeonOrbit`, matching `NeonOrbit.ino`:

```text
git clone https://github.com/lastness09/fingersonic-keys-neon-orbit.git NeonOrbit
cd NeonOrbit
```

For GitHub's ZIP download, extract the project and rename the sketch folder to `NeonOrbit` before opening it in Arduino IDE. Install these pinned dependencies using Arduino CLI or the IDE's board/library manager:

```text
arduino-cli config add board_manager.additional_urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli core update-index
arduino-cli core install esp32:esp32@3.3.2
arduino-cli lib install "Adafruit NeoPixel@1.15.1"
```

Build in PowerShell:

```powershell
.\build.ps1 -ArduinoCli 'C:\path\to\arduino-cli.exe'
```

The script compiles only, checks ESP32-S3 chip ID and the 2 MiB App0 limit, and writes `build/neon-orbit-v0.1.0.bin` plus `build/manifest.json`. Board settings: ESP32S3 Dev Module, 240 MHz, 16 MB flash, DIO 80 MHz, custom partition table, PSRAM disabled, Hardware CDC and JTAG, USB CDC On Boot disabled. `partitions.csv` is a local build reference; never upload its generated system image through the App0 workflow.

## Tests and architecture

`ArpEngine` schedules note/gate events in the audio sample clock. `FmSynth` renders voices and echo. `Controls` handles encoders, buttons, contacts and the recovery gesture. `PerformanceLeds` renders queued note cues outside the audio task. `ArpFirmware` connects the matrix and controls to the audio task using fixed queues. `App1Recovery` checks the dedicated uploader partition and the OTA result before restarting.

The host tests exercise the real portable engine, synth, controls, recovery selector and LED renderer with mocked device OTA calls. On Windows with the documented LLVM tools:

```text
python tests/test_core.py --llvm-bin C:/path/to/llvm/bin --preview build/neon-orbit-preview.wav -v
```

The runner currently targets Windows and needs `clang++.exe`, `llvm-dlltool.exe`, and LLVM's linker. It is not a cross-platform test runner. See [VALIDATION.md](VALIDATION.md) for checks performed and required hardware acceptance tests. The 25-second software preview demonstrates ten sound/pattern combinations and is not a recording of the actual speaker.

## License and scope

The source in this repository is the independent Neon Orbit community project, originally developed as `main2_ARP`, released under the [MIT license](LICENSE). Third-party Arduino, ESP-IDF and NeoPixel dependencies retain their respective licenses and are installed separately. This repository does not publish the official Fingersonic Keys application, the App1 uploader or factory bootloader source.
