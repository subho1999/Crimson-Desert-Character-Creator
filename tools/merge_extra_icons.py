"""Merges extra option icons into build/CharacterCreator.data.

The data blob (embedded into the ASI, unpacked at first start) is generated
from the author's private sources and cannot be rebuilt here. This tool
patches it deterministically instead: for every JPEG in extra_icons/, it
finds menu.txt mesh lines with no icon ('-') whose mesh matches the art
(base name, with or without the '_player' suffix), sets the icon field, and
appends the files. Never overwrites an existing icon. Run in CI before the
MSVC build so releases carry the icons (fresh data.version stamp unpacks
them); or rerun locally any time.

Usage: python tools/merge_extra_icons.py
"""
import os
import struct
import sys

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
DATA = os.path.join(ROOT, 'build', 'CharacterCreator.data')
EXTRA = os.path.join(ROOT, 'extra_icons')
MAGIC = b'CCDATA1\x00'
PREFIX = 'cd_itemicon_barbershop_'


def read_blob(path):
    with open(path, 'rb') as f:
        d = f.read()
    if not d.startswith(MAGIC):
        raise SystemExit(f'{path}: no CCDATA1 magic')
    p = len(MAGIC)
    e = d.index(b'\x00', p)
    version = d[p:e].decode('utf-8')
    p = e + 1
    count = struct.unpack_from('<I', d, p)[0]
    p += 4
    files = []
    for _ in range(count):
        ln = struct.unpack_from('<H', d, p)[0]
        p += 2
        name = d[p:p + ln].decode('utf-8')
        p += ln
        sz = struct.unpack_from('<I', d, p)[0]
        p += 4
        files.append([name, d[p:p + sz]])
        p += sz
    if p != len(d):
        raise SystemExit(f'{path}: trailing bytes after {count} files')
    return version, files


def write_blob(path, version, files):
    out = bytearray(MAGIC)
    out += version.encode('utf-8') + b'\x00'
    out += struct.pack('<I', len(files))
    for name, blob in files:
        raw = name.encode('utf-8')
        out += struct.pack('<H', len(raw)) + raw
        out += struct.pack('<I', len(blob)) + blob
    with open(path, 'wb') as f:
        f.write(out)


def main():
    only_check = '--check' in sys.argv
    version, files = read_blob(DATA)
    names = [n for n, _ in files]
    menu_idx = names.index('menu.txt')
    menu = files[menu_idx][1].decode('utf-8')
    lines = menu.split('\n')
    by_mesh = {}
    for idx, line in enumerate(lines):
        if line.startswith('mesh '):
            q = line.split()
            by_mesh.setdefault(q[6], []).append(idx)
    patched, added = 0, []
    for art in sorted(os.listdir(EXTRA)):
        if not art.lower().endswith('.jpg'):
            continue
        base = os.path.splitext(art)[0].lower()
        if not base.startswith(PREFIX):
            print(f'  skip {art}: not barbershop art')
            continue
        stem = base[len(PREFIX):]
        stems = [stem, stem + '_player']
        if stem.startswith('oongka_'):
            # Alternate barber framing of the same mesh (e.g. oongka_ + base).
            stems += [stem[len('oongka_'):], stem[len('oongka_'):] + '_player']
        # Only lines still without an icon; never overwrite existing art.
        fresh = [(i, m) for i, m in [(idx, mesh) for mesh, idxs in
                 ((mm, by_mesh.get(mm, [])) for mm in stems) for idx in idxs]
                 if lines[i].split()[5] == '-']
        for idx in sorted({i for i, _ in fresh}):
            q = lines[idx].split()
            assert q[5] == '-', lines[idx]
            q[5] = art
            lines[idx] = ' '.join(q)
            patched += 1
        with open(os.path.join(EXTRA, art), 'rb') as f:
            payload = f.read()
        dest = f'icons/{art}'
        if dest not in names:
            files.append([dest, payload])
            names.append(dest)
            added.append(art)
        elif files[names.index(dest)][1] != payload:
            raise SystemExit(f'{dest}: different bytes already packed')
    if only_check:
        print(f'{EXTRA}: {patched} menu lines would patch, {len(added)} icons would append')
        return
    files[menu_idx][1] = '\n'.join(lines).encode('utf-8')
    write_blob(DATA, version, files)
    print(f'merged: {patched} menu lines, {len(added)} icons into {DATA}')


if __name__ == '__main__':
    main()
