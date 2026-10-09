"""Vendor the Material Symbols glyphs the generic icons use, and their license, into glyphs/.

  python branding/library/fetch-glyphs.py

Downloads rounded/<glyph>-fill.svg for every glyph in build-library.py's GENERIC table from the pinned npm
package @material-symbols/svg-500 on jsDelivr, plus the package's LICENSE (Apache-2.0). Run it when the
table gains a glyph; the repository keeps the vendored copies so building needs no network.
Standard library only.
"""
import importlib.util
import pathlib
import urllib.request

HERE = pathlib.Path(__file__).resolve().parent
PACKAGE = "https://cdn.jsdelivr.net/npm/@material-symbols/svg-500@0.47.5/"

_spec = importlib.util.spec_from_file_location("build_library", HERE / "build-library.py")
bl = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(bl)


def fetch(url):
    with urllib.request.urlopen(url, timeout=30) as response:
        return response.read()


def main():
    out = HERE / "glyphs"
    out.mkdir(exist_ok=True)
    (out / "LICENSE").write_bytes(fetch(PACKAGE + "LICENSE"))
    glyphs = sorted({row[3] for row in bl.GENERIC})
    for glyph in glyphs:
        data = fetch(PACKAGE + f"rounded/{glyph}-fill.svg")
        if b'viewBox="0 -960 960 960"' not in data:
            raise SystemExit(f"{glyph}: unexpected view box")
        (out / f"{glyph}-fill.svg").write_bytes(data)
    print(f"vendored {len(glyphs)} glyphs and LICENSE into {out}")


if __name__ == "__main__":
    main()
