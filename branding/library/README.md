# Icon library tools

The library itself is `assets/icons/library/`: `icons.ini` (the manifest), `generic/` (our SVGs) and `brands/`
(owners' app icons). Everything there except the brand PNGs is generated; never edit it by hand.

| Tool | Does |
|---|---|
| `build-library.py` | Writes the generic SVGs, `icons.ini` and `brands/NOTICE.md` from the `GENERIC` table and `brands.ini`. `--check` fails if anything is stale. |
| `fetch-glyphs.py` | Vendors the Material Symbols glyphs (pinned 0.47.5) into `glyphs/`. Run it when `GENERIC` gains a glyph. |
| `import-brand.py` | Cuts one owner's app icon to the shared outline, lifts its near-black pixels (#000000 to #020202) to #030303, so no opaque pixel is within one step of Windows' Transparent color key (#010101), and records its provenance in `brands.ini`. |
| `check-library.py` | The CI check: current outputs, every file listed, brand art sized and shaped correctly, and no opaque pixel within one step of #010101. |
| `build-gallery.py` | Writes the docs site's Icon Library page; `pages.yml` runs it. |
| `review-sheet.py` | Writes `build/review.html`: every icon at five sizes on four backgrounds, for review by eye. |

**Add a generic icon:** add a row to `GENERIC` in `build-library.py`, run `fetch-glyphs.py` and then `build-library.py`.

**Match an app to an icon by another name:** add the names to `MATCH` in `build-library.py`, in lower case (an old name, or the one an app installs under; a vendor's name in front needs none), then run `build-library.py`. It writes them into `icons.ini` as `match = ...`, and the launcher's icon matching (`src/iconmatch.c`) compares an app's name with each icon's name, title and these. An alias that is another icon's name, title or alias is refused.

**Add a brand icon:**
1. Download its art: the Play listing icon at `=s512`, or the owner's larger press-kit art.
2. Run `import-brand.py`.
3. Fill in `title`, `group`, `owner` and `android` in `brands.ini`.
4. Run `build-library.py`.

**Remove a brand icon on request:** delete its section from `brands.ini` and its PNG, then run `build-library.py`.

The tests: `python -m unittest discover -s branding/library -p "test_*.py" -v` (needs Pillow).
