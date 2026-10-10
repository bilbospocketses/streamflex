"""Build the icon library's generated files from their sources.

Sources: the GENERIC table below; glyphs/<glyph>-fill.svg (Material Symbols Rounded, filled, vendored by
fetch-glyphs.py); brands.ini (kept by import-brand.py and by hand).
Outputs, committed because they ship: assets/icons/library/generic/<name>.svg,
generic/LICENSE-material-symbols.txt, icons.ini and brands/NOTICE.md.

  python branding/library/build-library.py            write the outputs
  python branding/library/build-library.py --check    write nothing; exit 1 if any output is stale

Standard library only.
"""
import argparse
import math
import pathlib
import re
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import libtools  # noqa: E402

CANVAS = 512
RADIUS = round(CANVAS * libtools.OUTLINE_RADIUS, 2)       # 112.64
GLYPH_BOX = 0.60        # the glyph's 960-unit em box, as a share of the plate side
LIGHTNESS = 52.0        # CIE L* of every media plate
VIVID = 0.85            # media chroma: this share of the hue's sRGB maximum at LIGHTNESS
MIN_CONTRAST = 3.0      # white glyph against its plate
FIXED_LCH = {"system": (42.0, 6.0, 260.0), "devices": (46.0, 18.0, 250.0)}
GENERAL_HEX = "#07606C"  # StreamFlex teal: TEAL_BG in branding/icon/build-svg.py

GENERIC = [
    # name, title, group, Material Symbols glyph, media hue slot (media only), optical trim
    ("power", "Power", "system", "power_settings_new", None, 1.0),
    ("restart", "Restart", "system", "restart_alt", None, 1.0),
    ("sleep", "Sleep", "system", "bedtime", None, 1.0),
    ("quit", "Exit", "system", "logout", None, 1.0),
    ("settings", "Settings", "system", "settings", None, 1.0),
    ("back", "Back", "system", "arrow_back", None, 1.0),
    ("lock", "Lock", "system", "lock", None, 1.0),
    ("user", "Switch user", "system", "switch_account", None, 1.0),
    ("movies", "Movies", "media", "movie", 0, 1.0),
    ("news", "News", "media", "newspaper", 1, 1.0),
    ("kids", "Kids", "media", "toys", 2, 1.0),
    ("sports", "Sports", "media", "sports_soccer", 3, 1.0),
    ("music", "Music", "media", "music_note", 4, 1.0),
    ("podcasts", "Podcasts", "media", "podcasts", 5, 1.0),
    ("audiobooks", "Audiobooks", "media", "headphones", 6, 1.0),
    ("photos", "Photos", "media", "photo", 7, 1.0),
    ("tv-shows", "TV shows", "media", "tv", 8, 1.0),
    ("live-tv", "Live TV", "media", "live_tv", 9, 1.0),
    ("games", "Games", "media", "sports_esports", 10, 1.0),
    ("emulators", "Retro games", "media", "joystick", 11, 1.0),
    ("radio", "Radio", "media", "radio", 12, 1.0),
    ("apps", "Apps", "general", "apps", None, 1.0),
    ("web", "Web", "general", "language", None, 1.0),
    ("folder", "Folder", "general", "folder", None, 1.0),
    ("favorites", "Favorites", "general", "favorite", None, 1.0),
    ("home", "Home", "general", "home", None, 1.0),
    ("search", "Search", "general", "search", None, 1.0),
    ("info", "Info", "general", "info", None, 1.0),
    ("download", "Downloads", "general", "download", None, 1.0),
    ("terminal", "Terminal", "general", "terminal", None, 1.0),
    ("desktop", "Desktop", "general", "desktop_windows", None, 1.0),
    ("display", "Display", "devices", "display_settings", None, 1.0),
    ("audio", "Sound", "devices", "volume_up", None, 1.0),
    ("bluetooth", "Bluetooth", "devices", "bluetooth", None, 1.0),
    ("network", "Network", "devices", "wifi", None, 1.0),
    ("gamepad", "Controllers", "devices", "gamepad", None, 1.0),
]

XN, YN, ZN = 0.95047, 1.0, 1.08883   # D65 white


def lch_to_linear(L, C, h):
    """CIE LCh(ab), D65, to linear sRGB (may fall outside 0..1)."""
    a, b = C * math.cos(math.radians(h)), C * math.sin(math.radians(h))
    fy = (L + 16) / 116
    fx, fz = fy + a / 500, fy - b / 200

    def finv(t):
        return t ** 3 if t ** 3 > 216 / 24389 else (116 * t - 16) / (24389 / 27)

    X, Y, Z = XN * finv(fx), YN * finv(fy), ZN * finv(fz)
    return (3.2404542 * X - 1.5371385 * Y - 0.4985314 * Z,
            -0.9692660 * X + 1.8760108 * Y + 0.0415560 * Z,
            0.0556434 * X - 0.2040259 * Y + 1.0572252 * Z)


def in_gamut(rgb):
    return all(-1e-9 <= c <= 1 + 1e-9 for c in rgb)


def max_chroma(L, h):
    c = 0.0
    while in_gamut(lch_to_linear(L, c + 0.5, h)):
        c += 0.5
    return c


def encode(c):
    c = min(max(c, 0.0), 1.0)
    return 12.92 * c if c <= 0.0031308 else 1.055 * c ** (1 / 2.4) - 0.055


def linear_to_hex(rgb):
    return "#" + "".join(f"{round(encode(x) * 255):02X}" for x in rgb)


def hex_to_linear(hex_color):
    values = [int(hex_color[i:i + 2], 16) / 255 for i in (1, 3, 5)]
    return [v / 12.92 if v <= 0.04045 else ((v + 0.055) / 1.055) ** 2.4 for v in values]


def contrast_with_white(hex_color):
    luminance = sum(w * c for w, c in zip((0.2126, 0.7152, 0.0722), hex_to_linear(hex_color)))
    return 1.05 / (luminance + 0.05)


def plate_color(group, slot):
    if group == "media":
        h = 25 + slot * 360 / 13
        return linear_to_hex(lch_to_linear(LIGHTNESS, math.floor(max_chroma(LIGHTNESS, h) * VIVID), h))
    if group == "general":
        return GENERAL_HEX
    return linear_to_hex(lch_to_linear(*FIXED_LCH[group]))


def glyph_paths(glyph):
    text = (libtools.GLYPHS / f"{glyph}-fill.svg").read_text(encoding="utf-8")
    if 'viewBox="0 -960 960 960"' not in text:
        raise ValueError(f"{glyph}: expected the Material Symbols view box 0 -960 960 960")
    paths = re.findall(r'<path d="([^"]+)"', text)
    if not paths:
        raise ValueError(f"{glyph}: no <path d=...> found")
    return paths


def generic_svg(color, paths, trim):
    scale = CANVAS * GLYPH_BOX / 960 * trim
    x = (CANVAS - 960 * scale) / 2
    y = x + 960 * scale                  # the glyph's y runs from -960 to 0
    body = "".join(f'<path d="{d}" fill="#FFFFFF"/>' for d in paths)
    return (f'<svg xmlns="http://www.w3.org/2000/svg" width="{CANVAS}" height="{CANVAS}" '
            f'viewBox="0 0 {CANVAS} {CANVAS}">\n'
            f'<rect width="{CANVAS}" height="{CANVAS}" rx="{RADIUS:g}" ry="{RADIUS:g}" fill="{color}"/>\n'
            f'<g transform="translate({x:.3f} {y:.3f}) scale({scale:.6f})">{body}</g>\n'
            f'</svg>\n')


NOTICE_HEAD = """# Brand icons

Every icon in this folder is the trademark and artwork of the company named beside it. StreamFlex includes
them only to identify each service on its launcher button. They are not covered by StreamFlex's GPL-3.0
license.

Near-black pixels in them (#000000 to #020202) are lifted to #030303, so no opaque pixel is within one step
of the color key of Windows' Transparent mode (#010101), and scaling these icons (checked at 128, 256 and
512 px) blends no opaque pixels onto it.

If you own one of these marks and want it removed, open an issue at
https://github.com/bilbospocketses/streamflex/issues and it will be taken out.

| Icon | Service | Owner | Source |
|---|---|---|---|
"""

MANIFEST_HEAD = [
    "; The StreamFlex icon library manifest. GENERATED by branding/library/build-library.py:",
    "; edit its GENERIC table or branding/library/brands.ini, then re-run it. Do not edit this file.",
    "; One section per icon; the section name is the icon's name.",
]


MAX_MANIFEST_LINE = 190   # inih reads each line into a 200-byte buffer (INI_MAX_LINE); keep a margin


def manifest_problems(lines):
    """The launcher reads icons.ini with inih, which splits a line longer than its buffer and cuts a value at
    an inline comment (';' after whitespace; '#' is guarded too). Refuse both at build time."""
    problems, section = [], "?"
    for line in lines:
        if line.startswith("[") and line.endswith("]"):
            section = line
        if len(line) > MAX_MANIFEST_LINE:
            problems.append(f"{section}: manifest line is {len(line)} characters, longer than {MAX_MANIFEST_LINE}: {line[:60]}...")
        if "=" in line and not line.startswith(";") and re.search(r"\s[;#]", line.split("=", 1)[1]):
            problems.append(f"{section}: '{line}' would be cut at an inline comment (' ;' or ' #')")
    return problems


def build_outputs(brands_ini=libtools.BRANDS_INI):
    """Return ({library-relative path: text}, [errors])."""
    outputs, errors, names = {}, [], set()
    manifest = list(MANIFEST_HEAD)
    for name, title, group, glyph, slot, trim in GENERIC:
        if not libtools.NAME_RE.match(name) or name in names:
            errors.append(f"generic '{name}': invalid or repeated name")
        names.add(name)
        color = plate_color(group, slot)
        if contrast_with_white(color) < MIN_CONTRAST:
            errors.append(f"generic '{name}': plate {color} gives white only {contrast_with_white(color):.2f}:1")
        outputs[f"generic/{name}.svg"] = generic_svg(color, glyph_paths(glyph), trim)
        manifest += ["", f"[{name}]", f"title = {title}", f"group = {group}", f"file = generic/{name}.svg"]

    rows = []
    for name, keys in libtools.read_sections(brands_ini):
        if not libtools.NAME_RE.match(name) or name in names:
            errors.append(f"brand '{name}': invalid name, or it repeats another icon's")
        names.add(name)
        missing = [key for key in ("title", "group", "owner", "source", "size", "sha256") if not keys.get(key)]
        if missing:
            errors.append(f"brand '{name}': brands.ini is missing {', '.join(missing)}")
            continue
        if keys["group"] not in libtools.BRAND_GROUPS:
            errors.append(f"brand '{name}': group '{keys['group']}' is not one of {', '.join(libtools.BRAND_GROUPS)}")
        png = libtools.LIBRARY / "brands" / f"{name}.png"
        if not png.is_file():
            errors.append(f"brand '{name}': {png} is missing; run import-brand.py")
        elif libtools.sha256_file(png) != keys["sha256"]:
            errors.append(f"brand '{name}': {png.name} does not match the sha256 in brands.ini; re-import it")
        manifest += ["", f"[{name}]", f"title = {keys['title']}", f"group = {keys['group']}",
                     f"file = brands/{name}.png", f"owner = {keys['owner']}"]
        if keys.get("android"):
            manifest.append(f"android = {keys['android']}")
        manifest += [f"source = {keys['source']}", f"sha256 = {keys['sha256']}"]
        rows.append(f"| `{name}.png` | {keys['title']} | {keys['owner']} | {keys['source']} |")

    errors += manifest_problems(manifest)
    outputs["icons.ini"] = "\n".join(manifest) + "\n"
    outputs["brands/NOTICE.md"] = NOTICE_HEAD + ("\n".join(rows) + "\n" if rows else "| (none yet) | | | |\n")
    outputs["generic/LICENSE-material-symbols.txt"] = (libtools.GLYPHS / "LICENSE").read_text(encoding="utf-8")
    return outputs, errors


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--check", action="store_true", help="write nothing; exit 1 if any output is stale")
    args = parser.parse_args()

    outputs, errors = build_outputs()
    if errors:
        print("\n".join(f"error: {e}" for e in errors))
        return 1

    changed = []
    for rel, text in outputs.items():
        path = libtools.LIBRARY / rel
        data = text.encode("utf-8")
        if not path.is_file() or path.read_bytes() != data:
            changed.append(rel)
            if not args.check:
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(data)
    for path in sorted((libtools.LIBRARY / "generic").glob("*.svg")):
        if f"generic/{path.name}" not in outputs:
            changed.append(f"generic/{path.name} (not in the GENERIC table)")
            if not args.check:
                path.unlink()

    if args.check:
        if changed:
            print("stale library outputs (run build-library.py):\n  " + "\n  ".join(changed))
            return 1
        print(f"library outputs are current ({len(outputs)} files)")
        return 0
    print(f"updated {len(changed)} of {len(outputs)} library files")
    return 0


if __name__ == "__main__":
    sys.exit(main())
