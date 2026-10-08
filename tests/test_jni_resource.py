import importlib.util
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from zipfile import ZipFile

ROOT = Path(__file__).resolve().parents[1]


class JniApkResource(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.directory = tempfile.TemporaryDirectory(prefix='starfront-jni-')
        cls.temp = Path(cls.directory.name)
        cls.asset = cls.temp / 'assets/igli.bin'
        cls.asset.parent.mkdir()
        cls.asset.write_bytes(bytes(range(256)) * 1024)
        cls.original = cls.temp / 'original.bin'
        cls.original.write_bytes(cls.asset.read_bytes())
        cls.binary = cls.temp / 'jni-resource-test'
        subprocess.run([
            'cc', '-std=c11', '-D_DARWIN_C_SOURCE', '-D_POSIX_C_SOURCE=200809L',
            '-Wall', '-Wextra', '-Werror', '-Wno-unused-parameter', '-pthread',
            f'-DSF_APK_CONFIG_PATH="{cls.asset}"',
            '-I' + str(ROOT / 'tests/jni-stubs'), '-I' + str(ROOT / 'src'),
            str(ROOT / 'tests/test_jni_resource.c'), str(ROOT / 'tests/jni_audio_mock.c'),
            str(ROOT / 'tests/jni_drm_mock.c'),
            '-o', str(cls.binary),
        ], check=True, capture_output=True, text=True)

    @classmethod
    def tearDownClass(cls):
        cls.directory.cleanup()

    def setUp(self):
        shutil.copyfile(self.original, self.asset)

    def run_dispatch(self, scenario='valid'):
        return subprocess.run([str(self.binary), str(self.original), scenario],
                              capture_output=True, text=True)

    def test_actual_jni_byte_array_contract(self):
        result = self.run_dispatch()
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_missing_or_wrong_length_is_explicit_error(self):
        for case in ['missing', 'short', 'long']:
            with self.subTest(case=case):
                if case == 'missing':
                    self.asset.unlink()
                else:
                    data = self.original.read_bytes()
                    self.asset.write_bytes(data[:-1] if case == 'short' else data + b'\0')
                result = self.run_dispatch()
                self.assertEqual(result.returncode, 42)
                self.assertIn('APK resource' if case == 'missing' else 'Invalid igli.bin size', result.stderr)
                shutil.copyfile(self.original, self.asset)

    def test_other_classes_and_signatures_remain_unknown(self):
        for case in ['wrong-owner', 'wrong-signature']:
            with self.subTest(case=case):
                result = self.run_dispatch(case)
                self.assertEqual(result.returncode, 42)
                self.assertIn('Unimplemented Java callback:', result.stderr)



if __name__ == '__main__':
    unittest.main()
