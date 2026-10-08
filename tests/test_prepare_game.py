"""Small synthetic archives exercise copying and save preservation without game data."""
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
from zipfile import ZipFile

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('prepare_game', ROOT / 'scripts/prepare_game.py')
p = importlib.util.module_from_spec(spec)
spec.loader.exec_module(p)


class Preparation(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.base = Path(self.tmp.name)
        self.source = self.base / 'data'
        self.source.mkdir()
        self.output = self.base / 'out'
        self.apk = self.base / 'game.apk'
        self.config = self.base / 'config'
        self.config.mkdir()
        self.files = {'GloftSFHP/effects.gla': 5, 'GloftSFHP/menu.gla': 4}
        (self.config / 'game-files.json').write_text(json.dumps(self.files))
        for name, size in self.files.items():
            (self.source / Path(name).name).write_bytes(b'x' * size)
        with ZipFile(self.apk, 'w') as z:
            z.writestr('lib/test.so', b'library')
            z.writestr('res/raw/config', b'config')
            z.writestr('assets/data.save', b'initial save')
        overrides = {'lib/test.so': ('libstarfront.so', p.digest(b'library')),
                     'res/raw/config': ('apk/igli.bin', p.digest(b'config'))}
        for mock in (patch.object(p, 'HERE', self.config),
                     patch.object(p, 'APK_FILES', overrides),
                     patch.object(p, 'terrain_shaders', return_value={
                         'GloftSFHP/TerrainShaders/test.glsl': b'generated locally'})):
            mock.start()
            self.addCleanup(mock.stop)

    def test_directory_and_archive_match_and_preserve_saves(self):
        p.prepare(self.apk, self.source, self.output)
        for rel in ('GloftSFHP/data.save', 'save/files/data.save'):
            (self.output / rel).write_bytes(b'player progress')
        archive = self.base / 'data.obb'
        with ZipFile(archive, 'w') as z:
            for name in self.files:
                z.writestr(name, (self.source / Path(name).name).read_bytes())
            z.writestr('../../outside', b'must not extract')
        p.prepare(self.apk, archive, self.output)
        self.assertEqual((self.output / 'apk/igli.bin').read_bytes(), b'config')
        for rel in ('GloftSFHP/data.save', 'save/files/data.save'):
            self.assertEqual((self.output / rel).read_bytes(), b'player progress')
        self.assertFalse((self.base / 'outside').exists())
        self.assertEqual((self.output / 'GloftSFHP/TerrainShaders/test.glsl').read_bytes(), b'generated locally')

    def test_wrong_apk_rejected_before_output(self):
        with ZipFile(self.apk, 'w') as z:
            z.writestr('lib/test.so', b'wrong library')
        with self.assertRaisesRegex(ValueError, 'Unsupported APK'):
            p.prepare(self.apk, self.source, self.output)
        self.assertFalse(self.output.exists())

    def test_missing_data_rejected_before_output(self):
        (self.source / 'menu.gla').unlink()
        with self.assertRaisesRegex(ValueError, 'data file'):
            p.prepare(self.apk, self.source, self.output)
        self.assertFalse(self.output.exists())

    def test_conflicting_resource_is_not_overwritten(self):
        (self.output / 'apk').mkdir(parents=True)
        existing = self.output / 'apk/igli.bin'
        existing.write_bytes(b'keep this')
        with self.assertRaisesRegex(ValueError, 'Existing file differs'):
            p.prepare(self.apk, self.source, self.output)
        self.assertEqual(existing.read_bytes(), b'keep this')
        self.assertFalse((self.output / 'GloftSFHP').exists())

    def test_output_symlink_rejected(self):
        elsewhere = self.base / 'elsewhere'
        elsewhere.mkdir()
        self.output.mkdir()
        (self.output / 'apk').symlink_to(elsewhere, target_is_directory=True)
        with self.assertRaisesRegex(ValueError, 'Symlink'):
            p.prepare(self.apk, self.source, self.output)
        self.assertEqual(list(elsewhere.iterdir()), [])

    def test_ambiguous_data_archive_rejected(self):
        archive = self.base / 'data.obb'
        with ZipFile(archive, 'w') as z:
            for name, size in self.files.items():
                z.writestr(name, b'x' * size)
                z.writestr(Path(name).name, b'x' * size)
        with self.assertRaisesRegex(ValueError, 'archive entry'):
            p.prepare(self.apk, archive, self.output)
        self.assertFalse(self.output.exists())


if __name__ == '__main__':
    unittest.main()
