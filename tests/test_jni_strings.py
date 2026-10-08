from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class JniUtfStringLifetime(unittest.TestCase):
    def test_actual_jni_slots_own_independent_utf_buffers(self):
        with tempfile.TemporaryDirectory(prefix='starfront-jni-strings-') as directory:
            binary = Path(directory) / 'jni-strings-test'
            subprocess.run([
                'cc', '-std=c11', '-D_DARWIN_C_SOURCE', '-D_POSIX_C_SOURCE=200809L',
                '-Wall', '-Wextra', '-Werror', '-Wno-unused-parameter', '-pthread',
                '-I' + str(ROOT / 'tests/jni-stubs'), '-I' + str(ROOT / 'src'),
                str(ROOT / 'tests/test_jni_strings.c'), str(ROOT / 'tests/jni_audio_mock.c'),
                str(ROOT / 'tests/jni_drm_mock.c'),
                '-o', str(binary),
            ], check=True, capture_output=True, text=True)
            result = subprocess.run([str(binary)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)


if __name__ == '__main__':
    unittest.main()
