# CDMW headless: browse + extract Crimson Desert game files (no GUI)

Use this from any Linux LLM session. CDMW-Full's archive engine is pure
Python (`struct` + `lz4`) — the Qt/Rust/D3D12 GUI is never needed.

## Paths

- CDMW source (sibling checkout): `/home/subho/workspace/opencode/CrimsonDesert_mods/CDMW-Full`
- Game files: `/mnt/media/game_drive/Games/Crimson_Desert_Enhanced_Edition/drive_c/Crimson_Desert/`
- Archives live in numbered dirs (`0009/`, `0012/`, …) plus `dmmsa/`; each has
  `0.pamt` (the game's own index of that group) and `N.paz` (payload).

## One-time setup (per session)

```bash
python3 -m venv /tmp/opencode/cdmwenv
/tmp/opencode/cdmwenv/bin/pip -q install lz4 pillow
```

`lz4` is mandatory (payload decompression). `pillow` only for viewing/converting
extracted DDS.

## Browse: list entries of one archive group

```python
import sys
sys.path.insert(0, '/home/subho/workspace/opencode/CrimsonDesert_mods/CDMW-Full')
from pathlib import Path
from cdmw.core.archive_format import parse_archive_pamt

GAME = Path('/mnt/media/game_drive/Games/Crimson_Desert_Enhanced_Edition/drive_c/Crimson_Desert')
entries = parse_archive_pamt(GAME / '0012' / '0.pamt')   # 0012 = UI group
print(len(entries))
print(entries[0])   # ArchiveEntry(path=..., paz_file=..., offset=..., comp_size=..., orig_size=..., ...)
hits = [e for e in entries if 'customizeimage' in e.path.lower()]
```

`ArchiveEntry` fields that matter: `path` (archive-internal, `/`-separated),
`paz_file`, `offset`, `comp_size`, `orig_size`.

## Browse: find across ALL groups

```python
groups = sorted(GAME.glob('*/0.pamt'))   # ~40 groups; full pass takes minutes
for pamt in groups:
    for e in parse_archive_pamt(pamt):
        if 'PATTERN' in e.path.lower():
            print(pamt.parent.name, e.path, e.orig_size)
```

Prefer single-group scans while iterating; go full-game for the final coverage
pass. Wrap long runs in `timeout`.

## Extract: one entry to disk

```python
from cdmw.core.archive_extraction import extract_archive_entry
hit = [e for e in entries if e.path == 'ui/texture/image/customizeimage/foo.dds'][0]
out, decompressed, note = extract_archive_entry(hit, Path('/tmp/opencode/out/foo.dds'))
```

Bytes on disk are the plain file (decrypted + decompressed). UI art extracts
as standard DDS (seen: DXT5, 256x256) — Pillow opens DXT1/3/5 directly.

## Project-specific discovery (Character Creator icons)

Missing menu icons map to barber UI art by rule — strip `_player`, prefix the
folder: `cd_phm_00_hair_00_0022_player` ->
`ui/texture/image/customizeimage/cd_itemicon_barbershop_cd_phm_00_hair_00_0022.dds`.
Prefer the base framing over `..._oongka_...` variants when both exist.

## Gotchas

- Import `cdmw.*` with `sys.path` pointed at the CDMW checkout; do NOT install
  or import anything under `cdmw/ui` (Qt). `cdmw.core.archive_format` and
  `cdmw.core.archive_extraction` import clean headless.
- No `lz4` on system python (PEP 668) — always use the venv above.
- Full-game scans are slow; cache results (pickle the entry list) between runs.
- License: CDMW is MIT. Reuse its modules in place; if vendoring code, keep
  attribution (see CDMW-Full/LICENSE).
