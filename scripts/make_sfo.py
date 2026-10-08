#!/usr/bin/env python3
"""Write current VitaSDK game metadata even with an older softfp SDK.

Defaults match Gun Bros v1.0 and the corrected VitaSDK mksfoex schema:
https://github.com/vitasdk/vita-toolchain/issues/282
Only the packaging tool is replaced; the compiler and game executable stay intact.
"""
import argparse
import re
import struct
from pathlib import Path


def encode_sfo(values):
    keys, entries, data = bytearray(), bytearray(), bytearray()
    for name, value in sorted(values.items()):
        key_offset = len(keys)
        keys.extend(name.encode('ascii') + b'\0')
        if isinstance(value, int):
            payload = struct.pack('<I', value)
            kind, capacity = 0x404, 4
        else:
            payload = value.encode('utf-8') + b'\0'
            kind = 0x204
            capacity = {'TITLE': 128, 'STITLE': 52}.get(name, (len(payload) + 3) & ~3)
            if len(payload) > capacity:
                raise ValueError(f'{name} exceeds {capacity - 1} UTF-8 bytes')
        entries.extend(struct.pack('<HHIII', key_offset, kind, len(payload), capacity, len(data)))
        data.extend(payload + bytes(capacity - len(payload)))
    keys.extend(bytes(-len(keys) % 4))
    key_at = 20 + len(entries)
    return struct.pack('<5I', 0x46535000, 0x101, key_at, key_at + len(keys), len(values)) + entries + keys + data


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('-d', action='append', default=[], metavar='KEY=INTEGER')
    parser.add_argument('-s', action='append', default=[], metavar='KEY=STRING')
    parser.add_argument('title')
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    values = {
        'APP_VER': '01.00', 'ATTRIBUTE': 0x8000, 'ATTRIBUTE2': 12,
        'ATTRIBUTE_MINOR': 16, 'CATEGORY': 'gd',
        'GC_RO_SIZE': 0, 'GC_RW_SIZE': 0, 'PARENTAL_LEVEL': 0,
        'PSP2_DISP_VER': '00.000', 'PSP2_SYSTEM_VER': 0,
        'REGION_DENY': 0, 'SAVEDATA_MAX_SIZE': 1048576,
        'STITLE': args.title, 'TITLE': args.title, 'VERSION': '01.00',
    }
    for option, numeric in [(args.d, True), (args.s, False)]:
        for assignment in option:
            key, separator, value = assignment.partition('=')
            if not separator or not re.fullmatch(r'[A-Z][A-Z0-9_]*', key):
                parser.error(f'invalid field: {assignment}')
            values[key] = int(value, 0) if numeric else value
    title_id = values.get('TITLE_ID', '')
    if not re.fullmatch(r'[A-Z0-9]{9}', title_id):
        parser.error('TITLE_ID must contain nine uppercase letters/digits')
    values.setdefault('CONTENT_ID', f'HB0001-{title_id}_00-0000000000000000')
    for key in ['APP_VER', 'VERSION']:
        if not re.fullmatch(r'\d{2}\.\d{2}', values[key]) or values[key] == '00.00':
            parser.error(f'{key} must be a nonzero NN.NN version')
    args.output.write_bytes(encode_sfo(values))


if __name__ == '__main__':
    main()
