#!/usr/bin/env python3
"""Prepare a user's own Android 1.0.0 files locally. No downloads or activation bypass.
MIT, Copyright (c) 2026 vctorwei. Game files remain their owners' property.
"""
import argparse
import hashlib
import json
import re
import shutil
from pathlib import Path
from zipfile import BadZipFile, ZipFile

HERE = Path(__file__).resolve().parent
SO_SHA = 'a362b3b46cacad41d3d6c2961b5e4c1dce0027dd4a3c027471652665c275a618'
EFFECTS_SHA = '58d3ef9914542db76c556801e1d23ca90db347374d98c865bbd23e3a096720f9'
APK_FILES = {
    'lib/armeabi-v7a/libstarfront.so': ('libstarfront.so', SO_SHA),
    'res/raw/igli.bin': ('apk/igli.bin', '0f883fe7c6436642e5f12881cab5e4ef93ea9959428c9932115dac109750a162'),
    'res/raw/serialkey.txt': ('apk/serialkey.txt', 'cd3898ada1fac95de686dc43e4112729cfc9d1abec0c81fb229be86e10bb1a63'),
}
SHADERS = (
    ('UnlitMultiTextureNoVertexColorVS', 184397, 709,
     '81ee383863c28fce8fd8c39d8c516ab1554c525346133794100c121dd2957421',
     '78457e2e5546e612869967924541a307b584b2059c2fb7b1ec3e5e92fe4a70aa'),
    ('UnlitMultiTextureBlendNoVertexColorFS', 183327, 1070,
     '0d3c1f939670b795024248c98029d3bd90643c6409b74e18235483823f80559c',
     'e69d6c442f64cde450f2231f2746dce75337648c57e941c82edc72685964e428'),
)


def digest(data):
    return hashlib.sha256(data).hexdigest()


def terrain_shaders(effects):
    if digest(effects) != EFFECTS_SHA:
        raise ValueError('Unsupported effects.gla: use matching Android 1.0.0 data')
    notices = json.loads((HERE / 'shader-notices.json').read_text())
    result = {}
    for name, offset, size, original_sha, expected_sha in SHADERS:
        original = effects[offset:offset + size]
        if digest(original) != original_sha:
            raise ValueError('Shader source checksum mismatch')
        lines = []
        for line in original.decode('ascii').splitlines():
            if re.search(r'^(?:attribute|varying).*\b(?:Color0|vColor0)\b', line):
                continue
            if 'vColor0 = Color0' in line or line.lstrip().startswith('//gl_FragColor'):
                continue
            line = line.replace(' * vColor0', '').replace('varying\t  ', 'varying   ').replace('mediump\tvec4', 'mediump vec4')
            if re.match(r'^\s*layer\s*=', line):
                line = '\tlayer =' + line.split('=', 1)[1]
            if not line.strip():
                if lines and lines[-1] != '':
                    lines.append('')
            else:
                if line == '}' and lines and lines[-1] == '':
                    lines.pop()
                lines.append(line)
        data = (notices[name] + '\n'.join(lines).strip() + '\n').encode('ascii')
        if digest(data) != expected_sha:
            raise ValueError('Unexpected shader transformation output')
        result['GloftSFHP/TerrainShaders/' + name + '.glsl'] = data
    return result


def target(root, relative):
    p = root / relative
    # Never follow a user-data symlink into another directory.
    for candidate in (p, *p.parents):
        if candidate.is_symlink():
            raise ValueError('Symlink output is not supported: ' + str(candidate))
        if candidate == root:
            break
    return p


def write_immutable(root, relative, data):
    p = target(root, relative)
    if p.exists():
        if not p.is_file() or p.read_bytes() != data:
            raise ValueError('Existing file differs; keep it and choose a new output folder: ' + str(p))
        return
    p.parent.mkdir(parents=True, exist_ok=True)
    with p.open('xb') as out:
        out.write(data)


def prepare(apk, data_source, output):
    output = Path(output).absolute()
    expected = json.loads((HERE / 'game-files.json').read_text())
    with ZipFile(apk) as z:
        if len(z.namelist()) != len(set(z.namelist())):
            raise ValueError('Duplicate APK member')
        small = {}
        for member, (dest, sha) in APK_FILES.items():
            data = z.read(member)
            if digest(data) != sha:
                raise ValueError('Unsupported APK resource: ' + member)
            small[dest] = data
        initial_save = z.read('assets/data.save')
    archive = None
    try:
        source = Path(data_source)
        if source.is_dir():
            base = source / 'GloftSFHP' if (source / 'GloftSFHP').is_dir() else source
            sources = {name: base / name.removeprefix('GloftSFHP/') for name in expected}
            for name, p in sources.items():
                if p.is_symlink() or not p.is_file() or p.stat().st_size != expected[name]:
                    raise ValueError('Missing or wrong-sized data file: ' + str(p))
            effects = sources['GloftSFHP/effects.gla'].read_bytes()
        else:
            archive = ZipFile(source)
            if len(archive.namelist()) != len(set(archive.namelist())):
                raise ValueError('Duplicate archive member')
            sources = {}
            for name, size in expected.items():
                candidates = [n for n in (name, name.removeprefix('GloftSFHP/')) if n in archive.namelist()]
                if len(candidates) != 1 or archive.getinfo(candidates[0]).file_size != size:
                    raise ValueError('Missing or wrong-sized archive entry: ' + name)
                sources[name] = candidates[0]
            effects = archive.read(sources['GloftSFHP/effects.gla'])
        small.update(terrain_shaders(effects))
        # Validate all small immutable outputs before copying the large data set.
        for name, data in small.items():
            p = target(output, name)
            if p.exists() and (not p.is_file() or p.read_bytes() != data):
                raise ValueError('Existing file differs; use a new output folder: ' + str(p))
        for name, src in sources.items():
            dest = target(output, name)
            if dest.exists():
                if not dest.is_file() or dest.stat().st_size != expected[name]:
                    raise ValueError('Existing data differs: ' + str(dest))
                # Original data is never replaced, and saves are not in this list.
                continue
            dest.parent.mkdir(parents=True, exist_ok=True)
            with (archive.open(src) if archive else src.open('rb')) as inp, dest.open('xb') as out:
                shutil.copyfileobj(inp, out, 1024 * 1024)
        for name, data in small.items():
            write_immutable(output, name, data)
        for name in ('GloftSFHP/data.save', 'save/files/data.save'):
            p = target(output, name)
            if not p.exists():
                p.parent.mkdir(parents=True, exist_ok=True)
                with p.open('xb') as out:
                    out.write(initial_save)
        return {'original_data_files': len(expected), 'local_generated_shaders': 2,
                'existing_saves_preserved': True}
    finally:
        if archive:
            archive.close()


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--apk', type=Path, required=True, help='Your legally obtained Android 1.0.0 APK')
    parser.add_argument('--data', type=Path, required=True, help='Matching unpacked GloftSFHP folder or ZIP-compatible OBB/data archive')
    parser.add_argument('--output', type=Path, required=True, help='Local starfront folder to copy to ux0:data/starfront')
    args = parser.parse_args()
    try:
        result = prepare(args.apk, args.data, args.output)
    except (OSError, ValueError, KeyError, BadZipFile) as error:
        parser.exit(1, 'Preparation failed: ' + str(error) + '\n')
    print(json.dumps(result, indent=2))
    print('Copy this folder to ux0:data/starfront: ' + str(args.output))
