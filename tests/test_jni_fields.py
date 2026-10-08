from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class JniObservedStaticField(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.directory = tempfile.TemporaryDirectory(prefix='starfront-jni-fields-')
        cls.binary = Path(cls.directory.name) / 'jni-fields-test'
        subprocess.run([
            'cc', '-std=c11', '-D_DARWIN_C_SOURCE', '-D_POSIX_C_SOURCE=200809L',
            '-Wall', '-Wextra', '-Werror', '-Wno-unused-parameter', '-pthread',
            '-I' + str(ROOT / 'tests/jni-stubs'), '-I' + str(ROOT / 'src'),
            str(ROOT / 'tests/test_jni_fields.c'), str(ROOT / 'tests/jni_audio_mock.c'),
            str(ROOT / 'tests/jni_drm_mock.c'),
            '-o', str(cls.binary),
        ], check=True, capture_output=True, text=True)

    @classmethod
    def tearDownClass(cls):
        cls.directory.cleanup()

    def test_actual_slots_context_phone_string_and_reference_lifetime(self):
        result = subprocess.run([str(self.binary), 'valid'], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_other_fields_types_receivers_and_services_remain_unknown(self):
        cases = [('unknown-owner', 'Unimplemented JNI static field'),
                 ('unknown-name', 'Unimplemented JNI static field'),
                 ('unknown-signature', 'Unimplemented JNI static field'),
                 ('wrong-receiver', 'Unsupported JNI static object field'),
                 ('invalid-field-id', 'Unsupported JNI static object field'),
                 ('wrong-read-slot', 'Unimplemented JNI slot 150')]
        for case, message in cases:
            with self.subTest(case=case):
                result = subprocess.run([str(self.binary), case], capture_output=True, text=True)
                self.assertEqual(result.returncode, 42, result.stderr)
                self.assertIn(message, result.stderr)


if __name__ == '__main__':
    unittest.main()
