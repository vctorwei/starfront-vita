#!/usr/bin/env python3
"""Size the supplied game's existing artwork for Vita shell packaging."""
import argparse
import hashlib
import io
import json
from pathlib import Path
from zipfile import ZipFile

from PIL import Image, ImageOps

ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / 'assets/sce_sys'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--apk', type=Path, required=True, help='Your own Android 1.0.0 APK')
    args = parser.parse_args()
    records = []
    with ZipFile(args.apk) as archive:
        for member, target, size in [
            ('res/drawable/icon.png', 'icon0.png', (128, 128)),
            ('res/drawable/gi_background.png', 'pic0.png', (960, 544)),
            ('res/drawable/gi_background.png', 'livearea/contents/bg-r2.png', (840, 500)),
            ('res/drawable/gi_background.png', 'livearea/contents/startup-r2.png', (280, 158)),
        ]:
            source = archive.read(member)
            image = Image.open(io.BytesIO(source))
            source_size = image.size
            if target == 'icon0.png':
                image = image.convert('RGBA').resize(size, Image.Resampling.LANCZOS)
                alpha = image.getchannel('A')
                image = image.convert('RGB').quantize(colors=127, method=Image.Quantize.MEDIANCUT,
                                                     dither=Image.Dither.FLOYDSTEINBERG)
                image.paste(127, mask=alpha.point(lambda value: 255 if value < 128 else 0))
                image.info['transparency'] = 127
            else:
                # Preserve the full artwork and title, with narrow black padding.
                image = ImageOps.pad(image.convert('RGB'), size,
                                     method=Image.Resampling.LANCZOS, color='black')
                image = image.quantize(colors=128, method=Image.Quantize.MEDIANCUT,
                                       dither=Image.Dither.FLOYDSTEINBERG)
            path = OUTPUT / target
            path.parent.mkdir(parents=True, exist_ok=True)
            image.save(path, format='PNG', bits=8, optimize=True)
            records.append({
                'source_member': member, 'source_size': source_size,
                'source_sha256': hashlib.sha256(source).hexdigest(),
                'path': str(path.relative_to(ROOT)), 'size': size,
                'sha256': hashlib.sha256(path.read_bytes()).hexdigest(),
                'format': 'PNG-8 indexed palette, non-interlaced',
            })
    report = {'source_apk_sha256': hashlib.sha256(args.apk.read_bytes()).hexdigest(),
              'policy': 'Existing game artwork only; resized and palette-converted for Vita shell.',
              'images': records}
    (ROOT / 'reports').mkdir(exist_ok=True)
    (ROOT / 'reports/livearea-assets.json').write_text(json.dumps(report, indent=2) + '\n')
    print('Prepared 4 Vita shell images from supplied game artwork.')


if __name__ == '__main__':
    main()
