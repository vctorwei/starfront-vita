from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class JniMissingTelephony(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.directory = tempfile.TemporaryDirectory(prefix='starfront-jni-platform-')
        cls.binary = Path(cls.directory.name) / 'jni-platform-test'
        subprocess.run([
            'cc', '-std=c11', '-D_DARWIN_C_SOURCE', '-D_POSIX_C_SOURCE=200809L',
            '-Wall', '-Wextra', '-Werror', '-Wno-unused-parameter', '-pthread',
            '-I' + str(ROOT / 'tests/jni-stubs'), '-I' + str(ROOT / 'src'),
            str(ROOT / 'tests/test_jni_platform.c'), str(ROOT / 'tests/jni_audio_mock.c'),
            str(ROOT / 'tests/jni_drm_mock.c'),
            '-o', str(cls.binary),
        ], check=True, capture_output=True, text=True)

    @classmethod
    def tearDownClass(cls):
        cls.directory.cleanup()

    def test_native_query_slots_null_id_and_reference_lifetimes(self):
        result = subprocess.run([str(self.binary), 'valid'], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_exact_owners_signatures_receivers_and_unknown_service_fail(self):
        cases = [
            ('wrong-da-owner', 'Unimplemented Java callback'),
            ('wrong-da-signature', 'Unimplemented Java callback'),
            ('wrong-static-receiver', 'Invalid static Game.da receiver'),
            ('wrong-service-signature', 'Unimplemented Java callback'),
            ('unknown-service', 'Unimplemented platform service: wifi'),
            ('wrong-activity-receiver', 'Invalid Activity query receiver'),
            ('wrong-id-owner', 'Unimplemented Java callback'),
            ('wrong-id-signature', 'Unimplemented Java callback'),
            ('wrong-id-receiver', 'Invalid TelephonyManager query receiver'),
            ('invalid-utf-object', 'Invalid JNI string'),
            ('wrong-d-signature', 'Unimplemented Java callback'),
            ('wrong-d-receiver', 'Invalid static Game.d receiver'),
            ('wrong-dc-signature', 'Game.dc()I'),
        ]
        for case, message in cases:
            with self.subTest(case=case):
                result = subprocess.run([str(self.binary), case], capture_output=True, text=True)
                self.assertEqual(result.returncode, 42, result.stderr)
                self.assertIn(message, result.stderr)


if __name__ == '__main__':
    unittest.main()
