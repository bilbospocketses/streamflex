"""Write build/review.html: every library icon at 32, 48, 64, 128 and 256 px on light, gray, dark and photo
backgrounds, brand icons first, with 512-only brand art marked. Open it in a browser to judge the set by eye.
Standard library only.
"""
import html
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import libtools  # noqa: E402

SIZES = (32, 48, 64, 128, 256)
BACKGROUNDS = {
    "light": "#FFFFFF", "gray": "#808080", "dark": "#101418",
    "photo": "linear-gradient(135deg,#c9d8e6 0%,#e9d9b8 45%,#7a9a6b 100%)",
}


def main():
    sections = libtools.read_sections(libtools.LIBRARY / "icons.ini")
    brands = {name: keys for name, keys in libtools.read_sections(libtools.BRANDS_INI)}
    ordered = [s for s in sections if s[1]["file"].startswith("brands/")] + \
              [s for s in sections if s[1]["file"].startswith("generic/")]
    out = libtools.HERE / "build"
    out.mkdir(exist_ok=True)
    rows = []
    for background, css in BACKGROUNDS.items():
        rows.append(f'<h2>{background}</h2><div class="bg" style="background:{css}">')
        for name, keys in ordered:
            src = (libtools.LIBRARY / keys["file"]).relative_to(libtools.ROOT).as_posix()
            flag = " (512 only)" if brands.get(name, {}).get("size") == "512" else ""
            imgs = "".join(f'<img src="../../../{src}" width="{s}" height="{s}">' for s in SIZES)
            rows.append(f'<div class="icon">{imgs}<span>{html.escape(name)}{flag}</span></div>')
        rows.append("</div>")
    page = ("<!doctype html><meta charset=utf-8><title>Icon library review</title><style>"
            "body{font:13px system-ui,sans-serif;margin:16px}.bg{display:flex;flex-direction:column;gap:6px;"
            "padding:12px;border-radius:8px}.icon{display:flex;align-items:flex-end;gap:10px}"
            ".icon span{color:#fff;text-shadow:0 1px 2px #000;margin-left:8px}</style>" + "".join(rows))
    (out / "review.html").write_text(page, encoding="utf-8")
    print(f"wrote {out / 'review.html'} with {len(ordered)} icons")


if __name__ == "__main__":
    main()
