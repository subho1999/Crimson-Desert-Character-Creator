"""Verifies the assembled release zip against the expected package layout.

Usage: python tools/check_release.py dist/<zip>
Exits nonzero if any required entry is missing; otherwise prints the
entry count and uncompressed size for comparison with the reference
(Nexus) package.
"""
import os
import sys
import zipfile

REQUIRED_TOP = {
    'Character Creator/CharacterCreator.asi',
    'Character Creator/mod.json',
    'Character Creator/README.txt',
    'Character Creator/THIRD_PARTY.txt',
    'Character Creator/Equip All Armor.json',
}
REQUIRED_PREFIXES = (
    'Character Creator/Character Creator Enhanced/0009/',
    'Character Creator/Character Creator Enhanced/0012/',
)


def main():
    if len(sys.argv) != 2:
        raise SystemExit('usage: python tools/check_release.py dist/<zip>')
    path = sys.argv[1]
    with zipfile.ZipFile(path) as z:
        names = z.namelist()
        if len(names) != len(set(names)):
            raise SystemExit('duplicate entries in the zip')
        missing = sorted(REQUIRED_TOP - set(names))
        if missing:
            raise SystemExit(f'missing entries: {missing}')
        for prefix in REQUIRED_PREFIXES:
            if not any(n.startswith(prefix) for n in names):
                raise SystemExit(f'nothing under {prefix}')
        size = sum(i.file_size for i in z.infolist())
    print(f'{os.path.basename(path)}: {len(names)} files, {size} bytes uncompressed')
    for n in sorted(names):
        if not (n.startswith(REQUIRED_PREFIXES[0]) or n.startswith(REQUIRED_PREFIXES[1])):
            print(f'  {n}')


if __name__ == '__main__':
    main()
