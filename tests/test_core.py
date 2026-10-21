"""Exercise the actual portable C++ engine, synth, controls and App1 selector.

Windows: python tests/test_core.py --llvm-bin <directory containing clang++.exe>
No board is connected, and the OTA APIs are mocked.
"""
import argparse
import array
import ctypes
import hashlib
import math
from pathlib import Path
import subprocess
import tempfile
import unittest
import wave

ROOT = Path(__file__).resolve().parents[1]
RATE = 44100


class FirmwareTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory(prefix="keys_arp_test_")
        folder = Path(cls.temp.name)
        definition = folder / "math.def"
        definition.write_text("LIBRARY ucrtbase.dll\nEXPORTS\nexpf\npowf\nfloorf\n")
        mathlib = folder / "math.lib"
        subprocess.run([str(LLVM / "llvm-dlltool.exe"), "-m", "i386:x86-64",
                        "-d", str(definition), "-l", str(mathlib)], check=True)
        dll = folder / "arp_tests.dll"
        subprocess.run([
            str(LLVM / "clang++.exe"), "--target=x86_64-pc-windows-msvc", "-fuse-ld=lld",
            "-std=c++17", "-O2", "-ffreestanding", "-fno-builtin", "-fno-exceptions",
            "-fno-rtti", "-shared", "-nostdlib", "-Wall", "-Wextra", "-Werror",
            "-Wl,/noentry", "-I", str(ROOT / "tests/stubs"), "-I", str(ROOT),
            str(ROOT / "tests/core_harness.cpp"), str(ROOT / "ArpEngine.cpp"),
            str(ROOT / "FmSynth.cpp"), str(ROOT / "PerformanceLeds.cpp"),
            str(ROOT / "App1Recovery.cpp"), str(mathlib),
            "-o", str(dll),
        ], check=True)
        cls.lib = ctypes.CDLL(str(dll))
        signatures = {
            "note_on": [ctypes.c_int] * 2, "note_off": [ctypes.c_int],
            "set_pattern": [ctypes.c_int], "set_bpm": [ctypes.c_int],
            "set_octaves": [ctypes.c_int],
            "pattern_mask": [ctypes.c_int], "led_for_note": [ctypes.c_int],
            "set_preset": [ctypes.c_int], "set_volume": [ctypes.c_int],
            "synth_on": [ctypes.c_int] * 2, "synth_off": [ctypes.c_int],
            "render": [ctypes.POINTER(ctypes.c_float), ctypes.c_int, ctypes.c_int],
            "encoder_update": [ctypes.c_int],
            "button_update": [ctypes.c_int, ctypes.c_uint],
            "button_tap": [ctypes.c_uint],
            "recovery_update": [ctypes.c_int] * 3 + [ctypes.c_uint],
            "contact_update": [ctypes.c_int, ctypes.c_int, ctypes.c_uint],
            "app1_case": [ctypes.c_int, ctypes.c_uint, ctypes.c_uint, ctypes.c_int],
            "light_trigger": [ctypes.c_int] * 4 + [ctypes.c_uint],
            "light_render": [ctypes.c_uint, ctypes.c_uint, ctypes.c_int, ctypes.c_int,
                             ctypes.POINTER(ctypes.c_ubyte)],
        }
        for name, signature in signatures.items():
            getattr(cls.lib, name).argtypes = signature
        cls.lib.tick.restype = ctypes.c_uint

    @classmethod
    def tearDownClass(cls):
        ctypes.windll.kernel32.FreeLibrary.argtypes = [ctypes.c_void_p]
        ctypes.windll.kernel32.FreeLibrary(cls.lib._handle)
        cls.temp.cleanup()

    def setUp(self):
        self.lib.reset()
        self.lib.controls_reset()

    def event(self):
        value = self.lib.tick()
        return (value & 255) - 1, ((value >> 8) & 255) - 1, value >> 16

    def collect(self, frames):
        ons, offs = [], []
        active = None
        for frame in range(frames):
            on, off, vel = self.event()
            if off >= 0:
                self.assertEqual(active, off, f"unmatched off at {frame}")
                active = None
                offs.append((frame, off))
            if on >= 0:
                self.assertIsNone(active, f"overlapping gate at {frame}")
                self.assertTrue(1 <= vel <= 127)
                active = on
                ons.append((frame, on, vel))
        return ons, offs

    def audio(self, frames, with_arp=False):
        samples = (ctypes.c_float * (frames * 2))()
        self.lib.render(samples, frames, with_arp)
        return samples

    def light_frame(self, now, held=0, peak=255, preset=0):
        pixels = (ctypes.c_ubyte * 72)()
        self.lib.light_render(now, held, peak, preset, pixels)
        return [tuple(pixels[i:i + 3]) for i in range(0, 72, 3)]

    def test_all_ten_rhythms_match_their_grids_and_balance_gates(self):
        masks = set()
        for index in range(10):
            with self.subTest(pattern=index):
                self.lib.reset()
                self.lib.set_octaves(1)
                self.lib.set_pattern(index)
                mask = self.lib.pattern_mask(index)
                masks.add(mask)
                for note in (48, 52, 55, 59):
                    self.lib.note_on(note, 100)
                ons, offs = self.collect(RATE * 2)  # Exactly one bar at 120 BPM.
                self.assertEqual(len(ons), mask.bit_count())
                self.assertEqual(len(ons), len(offs))
                expected = [math.ceil(step * RATE / 8) for step in range(16) if mask & (1 << step)]
                self.assertEqual([x[0] for x in ons], expected)
                self.assertTrue(all(x[1] in (48, 52, 55, 59) for x in ons))
        self.assertEqual(len(masks), 10)

    def test_pitch_orders_and_single_key(self):
        for pattern, expected in [(0, [48, 52, 55, 48]), (1, [55, 52, 48, 55]),
                                  (2, [48, 52, 55, 52]), (3, [48, 55, 52, 48]),
                                  (8, [48, 48, 48, 52])]:
            self.lib.reset()
            self.lib.set_octaves(1)
            self.lib.set_pattern(pattern)
            for note in (55, 48, 52):
                self.lib.note_on(note, 120)
            ons, _ = self.collect(RATE * 2)
            self.assertEqual([x[1] for x in ons[:4]], expected)
        for pattern in range(10):
            self.lib.reset()
            self.lib.set_octaves(1)
            self.lib.set_pattern(pattern)
            self.lib.note_on(60, 80)
            ons, _ = self.collect(RATE * 2)
            self.assertTrue(ons)
            self.assertTrue(all(x[1] == 60 for x in ons))

    def test_random_order_does_not_repeat_adjacent_notes(self):
        self.lib.set_pattern(9)
        for note in (48, 50, 52, 55):
            self.lib.note_on(note, 127)
        ons, _ = self.collect(RATE * 4)
        self.assertTrue(all(a[1] != b[1] for a, b in zip(ons, ons[1:])))

    def test_release_last_note_stops_and_new_chord_restarts(self):
        self.lib.note_on(48, 100)
        self.assertEqual(self.event()[0], 48)
        self.lib.note_off(48)
        self.assertEqual(self.event()[1], 48)
        self.assertEqual(self.lib.held(), 0)
        self.assertEqual(self.collect(20000), ([], []))
        self.lib.note_on(55, 127)
        self.assertEqual(self.event()[0], 55)

    def test_note_on_zero_is_release_and_leds_follow_notes(self):
        self.lib.note_on(48, 127)
        self.lib.note_on(71, 80)
        self.assertEqual(self.lib.held(), (1 << 0) | (1 << 23))
        self.event()
        self.lib.note_on(48, 0)
        self.assertEqual(self.event()[1], 48)
        self.assertEqual(self.lib.held(), 1 << 23)
        for note in range(48, 72):
            self.assertEqual(self.lib.led_for_note(note), note - 48)

    def test_pattern_change_releases_before_retrigger(self):
        for pattern in range(10):
            self.lib.reset()
            self.lib.set_octaves(1)
            self.lib.note_on(48, 127)
            self.assertEqual(self.event()[0], 48)
            self.lib.set_pattern(pattern)
            on, off, _ = self.event()
            self.assertEqual(off, 48)
            self.assertEqual(on, 48 if self.lib.pattern_mask(pattern) & 1 else -1)

    def test_one_c_expands_to_upper_c_by_default(self):
        self.assertEqual(self.lib.get_octaves(), 2)
        self.lib.note_on(48, 100)
        self.assertEqual(self.lib.held(), 1)
        ons, _ = self.collect(RATE)
        self.assertEqual([x[1] for x in ons], [48, 60, 48, 60])
        self.lib.note_off(48)
        self.event()
        self.assertEqual(self.lib.active(), -1)
        self.assertEqual(self.collect(RATE), ([], []))

    def test_generated_note_releases_with_its_source(self):
        self.lib.note_on(48, 100)
        for _ in range(math.ceil(RATE / 4) + 1):
            event = self.event()
        self.assertEqual(event[0], 60)
        self.lib.note_off(48)
        self.assertEqual(self.event()[1], 60)
        self.assertEqual(self.lib.held(), 0)

    def test_octave_overlap_is_deduplicated_and_retains_other_root(self):
        self.lib.note_on(48, 80)
        self.lib.note_on(60, 127)
        ons, _ = self.collect(RATE)
        self.assertEqual([x[1] for x in ons], [48, 60, 72, 48])
        self.lib.note_off(60)
        self.event()
        ons = [self.event()[0] for _ in range(RATE)]
        self.assertTrue(60 in ons)
        self.assertFalse(72 in ons)
        self.assertEqual(self.lib.held(), 1)

    def test_all_octave_ranges_are_clipped_to_88_keys(self):
        self.lib.set_octaves(0)
        self.assertEqual(self.lib.get_octaves(), 1)
        self.lib.set_octaves(255)
        self.assertEqual(self.lib.get_octaves(), 8)
        for octaves in range(1, 9):
            with self.subTest(octaves=octaves):
                self.lib.reset()
                self.lib.set_octaves(octaves)
                self.lib.set_pattern(1)
                self.lib.set_bpm(300)
                self.lib.note_on(48, 127)
                ons, _ = self.collect(RATE * 2)
                expected_low = min(48, 108 - (octaves - 1) * 12)
                expected = set(range(expected_low, expected_low + octaves * 12, 12))
                self.assertEqual({x[1] for x in ons}, expected)
                self.assertTrue(all(21 <= x[1] <= 108 for x in ons))
        self.lib.reset()
        self.lib.set_octaves(8)
        self.lib.set_pattern(1)
        self.lib.set_bpm(300)
        for note in range(48, 60):
            self.lib.note_on(note, 100)
        ons, _ = self.collect(RATE * 5)
        self.assertEqual({x[1] for x in ons}, set(range(21, 109)))

    def test_reduce_octave_range_releases_removed_note_without_restart(self):
        self.lib.set_octaves(8)
        self.lib.set_pattern(1)
        self.lib.note_on(48, 127)
        self.assertEqual(self.event()[0], 108)
        self.lib.set_octaves(1)
        on, off, _ = self.event()
        self.assertEqual((on, off), (-1, 108))
        ons, _ = self.collect(RATE)
        self.assertTrue(all(x[1] == 48 for x in ons))
        self.assertAlmostEqual(ons[0][0], RATE / 8 - 2, delta=1)

    def test_same_pitch_class_echoes_across_both_octaves(self):
        for note, target, twin in [(48, 0, 12), (60, 12, 0), (72, 12, 0),
                                   (21, 9, 21), (108, 12, 0)]:
            with self.subTest(note=note):
                self.lib.reset()
                self.lib.light_trigger(note, 127, 0, 120, 100)
                pixels = self.light_frame(100)
                self.assertEqual(max(pixels[target]), 255)
                self.assertGreater(max(pixels[twin]), 150)
                self.assertGreater(max(pixels[target]), max(pixels[twin]))
                lit = {i for i, rgb in enumerate(pixels) if max(rgb)}
                self.assertEqual(lit, {target, twin})

    def test_leds_have_sound_level_velocity_and_accent_dynamics(self):
        peaks = []
        for velocity, step, level in [(127, 0, 255), (63, 0, 255),
                                       (127, 1, 255), (127, 0, 20), (127, 0, 0)]:
            self.lib.reset()
            self.lib.light_trigger(48, velocity, step, 120, 100)
            pixels = self.light_frame(100, peak=level)
            peaks.append(max(pixels[0]))
        self.assertGreater(peaks[0], peaks[1])
        self.assertGreater(peaks[0], peaks[2])
        self.assertGreater(peaks[0], peaks[3])
        self.assertEqual(peaks[4], 0)

    def test_leds_decay_ripple_and_octave_comet(self):
        self.lib.light_trigger(48, 127, 0, 120, 100)
        initial = self.light_frame(100)
        wave = self.light_frame(122)
        self.assertEqual(max(initial[1]), 0)
        self.assertGreater(max(wave[1]), 0)
        self.assertGreater(max(initial[0]), max(self.light_frame(160)[0]))
        self.lib.light_trigger(60, 127, 2, 120, 350)
        mid = self.light_frame(396)
        self.assertGreater(max(mid[5]), 30)
        settled = self.light_frame(900, held=1, peak=0)
        self.assertTrue(0 < max(settled[0]) <= 13)
        self.assertEqual(sum(sum(rgb) for rgb in settled[1:]), 0)

    def test_short_gate_cues_still_flash_and_stale_cues_expire(self):
        self.lib.light_trigger(48, 127, 0, 300, 100)
        self.assertGreater(max(self.light_frame(120)[0]), 100)
        self.assertEqual(sum(sum(rgb) for rgb in self.light_frame(600)), 0)
        self.lib.light_trigger(60, 127, 0, 300, 100)
        self.assertEqual(sum(sum(rgb) for rgb in self.light_frame(1000)), 0)

    def test_led_time_wrap_and_all_88_pitches_map_to_valid_leds(self):
        for note in range(21, 109):
            led = self.lib.led_for_note(note)
            self.assertTrue(0 <= led < 24)
            self.assertEqual((48 + led) % 12, note % 12)
        self.lib.light_trigger(60, 127, 0, 120, 0xFFFFFFF0)
        self.assertGreater(max(self.light_frame(4)[12]), 100)

    def test_tempo_limits_and_long_run_drift(self):
        self.lib.set_bpm(1)
        self.assertEqual(self.lib.get_bpm(), 30)
        self.lib.set_bpm(999)
        self.assertEqual(self.lib.get_bpm(), 300)
        self.lib.set_pattern(1)
        self.lib.set_bpm(137)
        self.lib.note_on(60, 127)
        ons, _ = self.collect(RATE * 8)
        for ordinal, (frame, _, _) in enumerate(ons):
            self.assertLessEqual(abs(frame - ordinal * RATE * 60 / (137 * 4)), 1.1)

    def test_tempo_change_keeps_phase_and_panic_clears(self):
        self.lib.set_pattern(1)
        self.lib.note_on(60, 127)
        self.event()
        for _ in range(2000):
            self.event()
        self.lib.set_bpm(240)
        ons = []
        for i in range(4000):
            if self.event()[0] >= 0:
                ons.append(i)
        self.assertAlmostEqual(ons[0], (RATE / 8 - 2001) / 2, delta=2)
        self.lib.panic()
        self.event()
        self.assertEqual(self.lib.held(), 0)
        self.assertEqual(self.lib.active(), -1)

    def test_all_fm_presets_are_distinct_finite_and_decay_to_silence(self):
        hashes = set()
        for preset in range(10):
            with self.subTest(preset=preset):
                self.lib.reset()
                self.lib.set_preset(preset)
                self.lib.synth_on(60, 127)
                attack = self.audio(RATE // 4)
                peak = max(abs(x) for x in attack)
                self.assertGreater(peak, 0.02)
                self.assertLess(peak, 1)
                self.assertTrue(all(math.isfinite(x) for x in attack))
                hashes.add(hashlib.sha256(bytes(attack)).digest())
                self.lib.synth_off(60)
                tail = self.audio(RATE * 3)
                self.assertLess(max(abs(x) for x in tail[-2000:]), 0.0001)
        self.assertEqual(len(hashes), 10)

    def test_synth_handles_extreme_notes_voice_stealing_and_mute(self):
        for preset in range(10):
            self.lib.reset()
            self.lib.set_preset(preset)
            for note in (0, 12, 24, 48, 60, 72, 84, 96, 108, 120, 127):
                self.lib.synth_on(note, 127)
                audio = self.audio(256)
                self.assertTrue(all(math.isfinite(x) and abs(x) < 1 for x in audio))
        self.lib.set_volume(0)
        muted = self.audio(RATE)
        self.assertLess(max(abs(x) for x in muted[-1000:]), 1e-6)

    def test_encoder_bounce_invalid_jumps_and_both_directions(self):
        update = self.lib.encoder_update
        self.assertEqual([update(x) for x in (3, 1, 0, 2, 3)], [0, 0, 0, 0, 1])
        self.assertEqual([update(x) for x in (2, 0, 1, 3)], [0, 0, 0, -1])
        self.assertEqual(sum(update(x) for x in (1, 3, 1, 3)), 0)
        self.assertEqual(sum(update(x) for x in (0, 3, 0, 3)), 0)

    def test_button_debounce_tap_and_rotation(self):
        update = self.lib.button_update
        self.assertEqual(update(1, 10), 0)
        self.assertEqual(update(0, 11), 0)
        self.assertEqual(update(1, 12), 0)
        self.assertEqual(update(1, 24), 1)
        self.assertEqual(update(0, 80), 0)
        self.assertEqual(update(0, 92), -1)
        self.assertTrue(self.lib.button_tap(92))
        self.lib.button_rotate()
        self.assertFalse(self.lib.button_tap(92))

    def test_immediate_hold_and_turn_does_not_become_a_click(self):
        update = self.lib.button_update
        self.assertEqual(update(1, 100), 0)
        self.lib.button_rotate()  # Turn before the 12 ms switch debounce completes.
        self.assertEqual(update(0, 102), 0)
        self.assertEqual(update(1, 104), 0)
        self.assertEqual(update(1, 116), 1)
        self.assertEqual(update(0, 140), 0)
        self.assertEqual(update(0, 152), -1)
        self.assertFalse(self.lib.button_tap(152))
        self.assertEqual(update(1, 200), 0)
        self.assertEqual(update(1, 212), 1)
        self.assertEqual(update(0, 250), 0)
        self.assertEqual(update(0, 262), -1)
        self.assertTrue(self.lib.button_tap(262))

    def test_app1_gesture_requires_key0_five_taps_and_no_rotation(self):
        update = self.lib.recovery_update
        for i in range(8):
            self.assertFalse(update(0, 1, 0, 100 * i))
        for i in range(4):
            self.assertFalse(update(1, 1, 0, 100 * i))
        self.assertTrue(update(1, 1, 0, 400))
        for i in range(4):
            self.assertFalse(update(1, 1, 0, 1000 + 100 * i))
        self.assertFalse(update(1, 0, 1, 1350))
        self.assertFalse(update(1, 1, 0, 1400))
        self.assertFalse(update(0, 0, 0, 1500))
        self.assertFalse(update(1, 1, 0, 1510))

    def test_app1_window_and_millis_wrap(self):
        update = self.lib.recovery_update
        for t in (0, 300, 600, 900, 1200):
            self.assertFalse(update(1, 1, 0, t))
        self.lib.controls_reset()
        for i in range(4):
            self.assertFalse(update(1, 1, 0, (0xFFFFFF00 + i * 100) & 0xFFFFFFFF))
        self.assertTrue(update(1, 1, 0, (0xFFFFFF00 + 400) & 0xFFFFFFFF))

    def test_actual_app1_selector_only_restarts_after_success(self):
        for exists, address, size, result, success, calls in [
            (1, 0x210000, 0x100000, 0, True, 1),
            (0, 0x210000, 0x100000, 0, False, 0),
            (1, 0x10000, 0x100000, 0, False, 0),
            (1, 0x210000, 0x200000, 0, False, 0),
            (1, 0x210000, 0x100000, -1, False, 1),
        ]:
            self.assertEqual(bool(self.lib.app1_case(exists, address, size, result)), success)
            self.assertEqual(self.lib.app1_set_calls(), calls)
            self.assertEqual(self.lib.app1_restarts(), int(success))
            self.assertEqual(self.lib.app1_find_errors(), 0)

    def test_dual_contacts_both_orders_bounce_release_and_timeout(self):
        update = self.lib.contact_update
        for first in (0, 1):
            self.lib.controls_reset()
            a, b = (1, 0) if first == 0 else (0, 1)
            self.assertEqual(update(a, b, 1000), 0)
            self.assertEqual(update(a, b, 1600), 0)
            self.assertEqual(update(1, 1, 4000), 0)
            self.assertTrue(63 <= update(1, 1, 4600) <= 127)
            self.assertEqual(update(a, b, 5000), 0)
            self.assertEqual(update(a, b, 5600), 0)
            self.assertEqual(update(0, 0, 6000), 0)
            self.assertEqual(update(0, 0, 6600), -1)
            self.assertEqual(update(0, 0, 7200), 0)
        self.lib.controls_reset()
        self.assertEqual(update(1, 0, 1000), 0)
        self.assertEqual(update(0, 0, 1200), 0)
        self.assertEqual(update(0, 0, 2000), 0)
        self.assertEqual(update(1, 0, 3000), 0)
        self.assertEqual(update(1, 0, 3600), 0)
        self.assertEqual(update(1, 0, 25000), 63)
        self.assertEqual(update(1, 1, 26000), 0)
        self.assertEqual(update(1, 1, 26600), 0)

    def test_render_arp_preview(self):
        if not PREVIEW:
            self.skipTest("--preview not requested")
        pcm = array.array("h")
        for preset in range(10):
            self.lib.reset()
            self.lib.set_preset(preset)
            self.lib.set_pattern(preset)
            for note in (48, 52, 55, 59):
                self.lib.note_on(note, 115)
            samples = self.audio(RATE * 2, with_arp=True)
            pcm.extend(round(x * 32767) for x in samples)
            for note in (48, 52, 55, 59):
                self.lib.note_off(note)
            tail = self.audio(RATE // 2, with_arp=True)
            pcm.extend(round(x * 32767) for x in tail)
        PREVIEW.parent.mkdir(parents=True, exist_ok=True)
        with wave.open(str(PREVIEW), "wb") as output:
            output.setnchannels(2)
            output.setsampwidth(2)
            output.setframerate(RATE)
            output.writeframes(pcm.tobytes())


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--llvm-bin", required=True, type=Path)
    parser.add_argument("--preview", type=Path)
    args, remaining = parser.parse_known_args()
    LLVM, PREVIEW = args.llvm_bin, args.preview
    unittest.main(argv=[__file__, *remaining])
