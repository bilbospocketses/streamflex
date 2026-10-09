"""Portability check for the SVG masters: render each in Chromium (Skia) and in Inkscape (cairo) at 512 px
and compare them with ImageMagick.

Inkscape and librsvg both rasterize through cairo, so they always agree with each other and prove nothing
about portability. Chromium is an independent engine. The approved design differs from Inkscape only by
edge antialiasing: normalized RMSE about 0.0058 for both masters. A much larger number means an SVG
feature one engine draws differently (keep the masters filter-free).

Needs the Python `playwright` package with its Chromium installed, Inkscape 1.x and ImageMagick 7.
Writes review PNGs to build/ (gitignored). Run after editing build-svg.py and rebuilding the masters.
"""
import pathlib
import re
import shutil
import subprocess

from playwright.sync_api import sync_playwright

HERE = pathlib.Path(__file__).resolve().parent
OUT = HERE / "build"
OUT.mkdir(exist_ok=True)
INKSCAPE = shutil.which("inkscape.com") or shutil.which("inkscape")
if not INKSCAPE:
    raise SystemExit("Inkscape not found on PATH")

with sync_playwright() as p:
    b = p.chromium.launch()
    pg = b.new_page(viewport={"width": 512, "height": 512})
    for v in ("full", "small"):
        svg = (HERE / f"streamflex-{v}.svg").read_text(encoding="utf-8")
        svg = svg.replace('width="256" height="256"', 'width="512" height="512"', 1)
        pg.set_content(f'<html><body style="margin:0;background:transparent">{svg}</body></html>')
        pg.screenshot(path=str(OUT / f"review_{v}_512_chromium.png"), omit_background=True,
                      clip={"x": 0, "y": 0, "width": 512, "height": 512})
    b.close()

for v in ("full", "small"):
    ink = OUT / f"review_{v}_512_inkscape.png"
    subprocess.run([INKSCAPE, str(HERE / f"streamflex-{v}.svg"), "--export-type=png", "--export-width=512",
                    "--export-height=512", f"--export-filename={ink}"], check=True, capture_output=True)
    # `magick compare` exits 1 whenever the images differ at all; the metric on stderr is the result.
    r = subprocess.run(["magick", "compare", "-metric", "RMSE", str(ink),
                        str(OUT / f"review_{v}_512_chromium.png"), "null:"],
                       capture_output=True, text=True, encoding="utf-8", errors="replace")
    m = re.search(r"\(([0-9.eE+-]+)\)", r.stderr)
    if r.returncode not in (0, 1) or not m:
        raise SystemExit(f"{v}: compare failed: {r.stderr.strip()}")
    print(f"{v:5}  Chromium vs Inkscape at 512 px: RMSE {r.stderr.strip()}  (normalized {float(m.group(1)):.4f})")
