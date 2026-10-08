"""Run the audio backend against a device stub that retains queued buffers."""
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class AudioTrackBackend(unittest.TestCase):
    def test_continuous_pcm_lifetime_backpressure_and_controls(self):
        cc = shutil.which('cc')
        if not cc:
            self.skipTest('host C compiler unavailable')
        with tempfile.TemporaryDirectory(prefix='starfront-audio-') as folder:
            binary = Path(folder) / 'audio-test'
            subprocess.run([
                cc, '-std=c11', '-Wall', '-Wextra', '-Werror', '-pthread',
                '-I' + str(ROOT / 'tests/stubs'), '-I' + str(ROOT / 'src'),
                str(ROOT / 'src/audio_track.c'), str(ROOT / 'tests/test_audio_track.c'),
                '-o', str(binary),
            ], check=True, timeout=30)
            subprocess.run([str(binary)], check=True, timeout=10)


if __name__ == '__main__':
    unittest.main()
