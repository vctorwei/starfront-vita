from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class JniAudioTrackAbi(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.directory = tempfile.TemporaryDirectory(prefix='starfront-jni-audio-')
        cls.binary = Path(cls.directory.name) / 'jni-audio-test'
        subprocess.run([
            'cc', '-std=c11', '-D_DARWIN_C_SOURCE', '-D_POSIX_C_SOURCE=200809L',
            '-Wall', '-Wextra', '-Werror', '-Wno-unused-parameter', '-pthread',
            '-I' + str(ROOT / 'tests/jni-stubs'), '-I' + str(ROOT / 'src'),
            str(ROOT / 'tests/test_jni_audio.c'), str(ROOT / 'tests/jni_audio_mock.c'),
            str(ROOT / 'tests/jni_drm_mock.c'),
            '-o', str(cls.binary),
        ], check=True, capture_output=True, text=True)

    @classmethod
    def tearDownClass(cls):
        cls.directory.cleanup()

    def run_case(self, case):
        return subprocess.run([str(self.binary), case], capture_output=True, text=True)

    def test_actual_slots_pcm_bounds_and_reference_lifetimes(self):
        result = self.run_case('valid')
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_actual_vm_getenv_compatible_versions_and_audio_query(self):
        result = self.run_case('vm-versions')
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_platform_failures_and_short_writes_are_explicit(self):
        for case, operation in [('create-failure', 'constructor'), ('write-failure', 'write'),
                                ('release-failure', 'release'), ('short-write', 'write')]:
            with self.subTest(case=case):
                result = self.run_case(case)
                self.assertEqual(result.returncode, 42, result.stderr)
                self.assertIn(f'AudioTrack {operation} failed', result.stderr)
                if case != 'short-write':
                    self.assertIn('80260007', result.stderr)
                self.assertIn('destroyed=0', result.stderr)

    def test_unknown_signatures_classes_and_invalid_critical_release_fail(self):
        for case, message in [('unknown-constructor', 'Unimplemented JNI constructor'),
                              ('unknown-method', 'Unimplemented Java callback'),
                              ('wrong-class', 'nonvirtual receiver/class'),
                              ('critical-pointer', 'Invalid JNI critical array release'),
                              ('critical-mode', 'Invalid JNI critical array release'),
                              ('critical-unacquired', 'JNI critical array was not acquired')]:
            with self.subTest(case=case):
                result = self.run_case(case)
                self.assertEqual(result.returncode, 42, result.stderr)
                self.assertIn(message, result.stderr)


if __name__ == '__main__':
    unittest.main()
