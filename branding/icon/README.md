# StreamFlex app icon — source

Everything needed to regenerate the app icon. The icon ships as these files, and this folder produces all of them:

| Output | Used as |
|---|---|
| `docs/streamflex.svg` | Linux scalable icon (`hicolor/scalable/apps`), installed by `CMakeLists.txt` |
| `docs/streamflex.png` | Linux 48×48 icon (`hicolor/48x48/apps`) |
| `docs/streamflex-32.png`, `-24.png`, `-16.png` | Linux 32, 24 and 16 px icons (`hicolor/<size>x<size>/apps`, installed as `streamflex.png`), drawn from the small master so menus and panels don't scale the detailed artwork down |
| `docs/assets/icons/favicon.png` | Documentation site favicon (32×32) |
| `config/streamflex.ico` | Windows executable icon, compiled in through `config/streamflex.rc` |

## Files

| File | What it is |
|---|---|
| `build-svg.py` | **The source.** One shared geometry and palette, written out as two vector masters. Edit this, not the SVGs |
| `streamflex-full.svg` | Master for 256–48 px: glow, highlight, window detail. Identical to `docs/streamflex.svg` |
| `streamflex-small.svg` | Master for 40–16 px and the favicon: bolder strokes, no glow, no fine detail, so it stays legible when tiny |
| `build-icon.ps1` | Runs the whole pipeline, installs the outputs above, and verifies the `.ico` |
| `pack-ico.py` | Packs the ten frames into the `.ico`: the 256 entry as PNG, every other entry as 32-bit BMP with an AND mask |
| `parse-ico.py` | Checks the `.ico` structure: entry count, sizes, bit depth, offsets in bounds |
| `review-sheet.py` | Review sheet for judging a design by eye: every frame at real size on white, Explorer gray and a dark taskbar, plus 4–6× pixel zooms of the small frames |
| `render-check.py` | Portability check: renders both masters at 512 px in Chromium (Skia) and in Inkscape (cairo) and prints the RMSE between them |
| `palette.py` | Research tool that re-derives the palette from the banner by k-means. It prints colors and changes nothing |

## Regenerating

```powershell
pwsh branding/icon/build-icon.ps1
```

Requirements: PowerShell 7, Python 3 with `numpy` and `opencv-python`, Inkscape 1.x, and ImageMagick 7, all on `PATH`.

The script renders every frame straight from its vector master (never by resizing another frame). It forces 8-bit RGBA and strips PNG date chunks, so an unchanged source rebuilds **byte for byte**. It ends by printing `git status` for the icon files, and an empty result means the rebuild reproduced the committed icon exactly. Intermediate frames go to `build/`, which is not committed.

On Windows the script also reads the new `.ico` the way Windows does. WIC, Explorer's decoder, must return all ten frames as 32-bit BGRA. GDI+ must return every size exactly, except 256, which it can only read as the 128 frame. And `rc.exe` must compile `config/streamflex.rc` against it. The `rc.exe` check needs the Windows SDK and says so when it skips.

After changing the design, run `python branding/icon/review-sheet.py` and look at `build/review_native.png` and `build/review_zoom.png`: the small sizes are where a design breaks. Also run `python branding/icon/render-check.py`. It needs the Python `playwright` package with its Chromium installed. Inkscape and librsvg both rasterize through cairo, so the two always agree with each other, and that agreement proves nothing about how other renderers will draw the icon. Chromium is an independent engine. The approved design measures a normalized RMSE of about 0.0058 against Inkscape, which is edge antialiasing only. A much larger number means an SVG feature one engine draws differently.

## Design notes

- The mark is the logo's teal ring looping out from behind a rising ribbon arrow (blue → magenta → orange), with an app-window glyph in the ring, on a dark-teal rounded plate. Unlike the logo banner it has **no penguin**, so the icon stays platform-neutral.
- The colors are the logo's own. They were sampled by k-means from the 1600×873 banner, `docs/assets/branding/streamflex-banner.jpg`, a downscale of the 2816×1536 original, `branding/logo/streamflex-logo.jpg`. They're recorded as constants at the top of `build-svg.py`. `palette.py` re-derives them from the banner: it reproduces 12 of the 15 exactly, all four ring colors, all four ribbon colors, the cyan glow and three of the four glass tones. The two backdrop colors (`NAVY`, `TEAL_BG`) and the lightest glass tone (`GLASS_HI`) were chosen by hand.
- At 32 px and below, the straight shaft beside the ring can read as a lowercase "p". This was seen and accepted when the design was approved, since it mirrors the logo's arrow-beside-ring layout. If it ever needs changing, the first things to try in `build-svg.py` are a stronger S-curve on the small variant's shaft, or a gap between the shaft and the ring.
- The SVGs use **no filters**. The glow is a radial-gradient ring and the shadows are stacked translucent shapes, so Qt's SVG renderer before Qt 6.7, librsvg, browsers and Inkscape all draw it the same way.
- The `.ico` keeps the 256 frame PNG-compressed, the Windows Vista-and-later convention. Explorer, WIC and `rc.exe` read it; only legacy GDI+ (`System.Drawing.Icon`) falls back to the 128 frame, and StreamFlex does not use it.
