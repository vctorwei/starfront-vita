import hashlib
from pathlib import Path
import subprocess
import tempfile
import unittest
from zipfile import ZipFile

ROOT = Path(__file__).resolve().parents[1]


class JniOriginalSerialResource(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.directory = tempfile.TemporaryDirectory(prefix='starfront-jni-drm-')
        cls.temp = Path(cls.directory.name)
        cls.asset = cls.temp / 'serialkey.txt'
        cls.original = b'test-serial'  # Synthetic fixture, not a game resource.
        cls.binary = cls.temp / 'jni-drm-test'
        subprocess.run([
            'cc', '-std=c11', '-D_DARWIN_C_SOURCE', '-D_POSIX_C_SOURCE=200809L',
            '-Wall', '-Wextra', '-Werror', '-Wno-unused-parameter', '-pthread',
            f'-DSF_APK_SERIAL_PATH="{cls.asset}"',
            '-I' + str(ROOT / 'tests/jni-stubs'), '-I' + str(ROOT / 'src'),
            str(ROOT / 'tests/test_jni_drm.c'), str(ROOT / 'tests/jni_audio_mock.c'),
            str(ROOT / 'tests/jni_drm_mock.c'), '-o', str(cls.binary),
        ], check=True, capture_output=True, text=True)

    @classmethod
    def tearDownClass(cls):
        cls.directory.cleanup()

    def setUp(self):
        self.asset.write_bytes(self.original)

    def run_dispatch(self, scenario='valid'):
        return subprocess.run([str(self.binary), scenario], capture_output=True, text=True)

    def test_original_bytes_actual_slots_constructor_once_and_array_lifetimes(self):
        for scenario in ['valid', 'loaded']:
            with self.subTest(scenario=scenario):
                result = self.run_dispatch(scenario)
                self.assertEqual(result.returncode, 0, result.stderr)

    def test_constructor_precedes_resource_and_backend_errors_are_preserved(self):
        self.asset.unlink()
        result = self.run_dispatch('fail-prepare')
        self.assertEqual(result.returncode, 42, result.stderr)
        self.assertIn('DRM constructor failed: result -5 code -5', result.stderr)
        self.assertIn('System 13 Crypto 0xffffedcc', result.stderr)
        self.assertIn('DRM_MOCK prepared=1 state=0 assigned=0', result.stderr)
        self.assertNotIn('Cannot open APK serial resource', result.stderr)
        self.asset.write_bytes(self.original)
        for scenario, stage, counters in [
            ('fail-state', 'constructor state failed: result -4 code -4', 'state=1 assigned=0'),
            ('fail-serial', 'resource serial assignment failed: result -5 code -5', 'state=1 assigned=1'),
        ]:
            with self.subTest(scenario=scenario):
                result = self.run_dispatch(scenario)
                self.assertEqual(result.returncode, 42, result.stderr)
                self.assertIn(stage, result.stderr)
                self.assertIn(counters, result.stderr)
                if scenario == 'fail-serial':
                    self.assertIn('System 13 Crypto 0xffffedcc', result.stderr)

    def test_missing_or_wrong_size_stops_after_constructor_without_assigning(self):
        for scenario in ['missing', 'short', 'long']:
            with self.subTest(scenario=scenario):
                if scenario == 'missing':
                    self.asset.unlink()
                else:
                    self.asset.write_bytes(self.original[:-1] if scenario == 'short' else self.original + b'\0')
                result = self.run_dispatch()
                self.assertEqual(result.returncode, 42, result.stderr)
                self.assertIn('Cannot open APK serial resource' if scenario == 'missing' else 'Invalid serialkey.txt size', result.stderr)
                self.assertIn('DRM_MOCK prepared=1 state=1 assigned=0', result.stderr)
                self.asset.write_bytes(self.original)

    def test_unknown_owner_signature_and_receiver_do_not_construct(self):
        for scenario in ['wrong-owner', 'wrong-signature', 'wrong-static-receiver']:
            with self.subTest(scenario=scenario):
                result = self.run_dispatch(scenario)
                self.assertEqual(result.returncode, 42, result.stderr)
                self.assertIn('DRM_MOCK prepared=0 state=0 assigned=0', result.stderr)
                self.assertIn('Invalid static Game.dc receiver' if scenario == 'wrong-static-receiver' else 'Unimplemented Java callback', result.stderr)


if __name__ == '__main__':
    unittest.main()
