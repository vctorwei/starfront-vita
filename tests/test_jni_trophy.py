from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class JniTrophy(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.build = tempfile.TemporaryDirectory(prefix='starfront-trophy-build-')
        cls.binary = Path(cls.build.name) / 'trophy'
        subprocess.run([
            'cc', '-std=c11', '-D_DARWIN_C_SOURCE', '-D_POSIX_C_SOURCE=200809L',
            '-Wall', '-Wextra', '-Werror', '-Wno-unused-parameter', '-pthread',
            '-I' + str(ROOT / 'tests/jni-stubs'), '-I' + str(ROOT / 'src'),
            str(ROOT / 'tests/test_jni_trophy.c'), str(ROOT / 'tests/jni_audio_mock.c'),
            str(ROOT / 'tests/jni_drm_mock.c'), '-o', str(cls.binary),
        ], check=True, capture_output=True, text=True)

    @classmethod
    def tearDownClass(cls):
        cls.build.cleanup()

    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix='starfront-trophy-data-')
        self.addCleanup(self.directory.cleanup)
        self.path = Path(self.directory.name) / 'androidTrophy.dat'

    def invoke(self, index, slot=141, kind='valid', path=None):
        return subprocess.run([str(self.binary), str(path or self.path), str(index),
                               str(slot), kind], capture_output=True, text=True)

    def test_all_void_abis_preserve_earlier_achievements_across_restarts(self):
        for index, slot in [(0, 141), (12, 142), (99, 143)]:
            result = self.invoke(index, slot)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn('local achievement recorded', result.stdout)
        values = list(map(int, self.path.read_text().splitlines()))
        self.assertEqual(values, [1 if i in (0, 12, 99) else 127 for i in range(100)])
        before = self.path.stat().st_mtime_ns
        self.assertEqual(self.invoke(12).returncode, 0)
        self.assertEqual(before, self.path.stat().st_mtime_ns)
        self.assertFalse(self.path.with_suffix('.dat.bak').exists())

    def test_java_decimal_values_line_endings_and_partial_records(self):
        self.path.write_bytes(b'+0001\r\n-2147483648\r2147483647\n127')
        self.assertEqual(self.invoke(99).returncode, 0)
        values = list(map(int, self.path.read_text().splitlines()))
        self.assertEqual(values[:4], [1, -2147483648, 2147483647, 127])
        self.assertEqual(values[4:99], [127] * 95)
        self.assertEqual(values[99], 1)

    def test_invalid_ids_never_create_or_modify_files(self):
        for index in [-1, 100, 101, 2147483647]:
            self.assertEqual(self.invoke(index).returncode, 0)
            self.assertFalse(self.path.exists())
        self.path.write_text('127\n')
        self.assertEqual(self.invoke(100).returncode, 0)
        self.assertEqual(self.path.read_text(), '127\n')

    def test_corrupt_records_are_preserved_and_error_is_nonfatal(self):
        for data in [b'not-an-int\n', b' 1\n', b'1 \n', b'\n', b'+\n',
                     b'2147483648\n', b'-2147483649\n', b'1\x00\n', b'127\n' * 101]:
            with self.subTest(data=data[:20]):
                self.path.write_bytes(data)
                result = self.invoke(12)
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertIn('record unavailable', result.stdout)
                self.assertEqual(self.path.read_bytes(), data)

    def test_write_failures_preserve_previous_record_and_recovery_backup(self):
        self.path.write_text('127\n')
        temporary = self.path.with_suffix('.dat.tmp')
        temporary.mkdir()
        self.assertIn('record unavailable', self.invoke(12).stdout)
        self.assertEqual(self.path.read_text(), '127\n')
        temporary.rmdir()
        backup = self.path.with_suffix('.dat.bak')
        backup.write_text('recovery copy')
        self.assertIn('record unavailable', self.invoke(12).stdout)
        self.assertEqual(backup.read_text(), 'recovery copy')
        self.assertEqual(self.path.read_text(), '127\n')
        self.assertIn('record unavailable', self.invoke(12, path=self.path / 'absent').stdout)

    def test_only_exact_owner_signature_and_static_receiver_are_accepted(self):
        for kind in ['owner', 'signature', 'receiver']:
            with self.subTest(kind=kind):
                result = self.invoke(12, kind=kind)
                self.assertEqual(result.returncode, 42, result.stderr)
                self.assertIn('notifyTrophy', result.stderr)
                self.assertFalse(self.path.exists())


if __name__ == '__main__':
    unittest.main()
