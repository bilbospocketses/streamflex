"""Shared paths and file formats for the icon library tools. Standard library only
(outline_mask, added with the brand tools, and the chroma key helpers import Pillow when called)."""
import hashlib
import pathlib
import re

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parents[1]
LIBRARY = ROOT / "assets" / "icons" / "library"
GLYPHS = HERE / "glyphs"
BRANDS_INI = HERE / "brands.ini"

NAME_RE = re.compile(r"^[a-z0-9-]{1,32}$")
GENERIC_GROUPS = ("system", "media", "general", "devices")
BRAND_GROUPS = ("video", "free-tv", "servers", "music", "games", "web")
OUTLINE_RADIUS = 0.22           # corner radius as a share of the side: the app icon's plate, 54 of 244
BRAND_MIN, BRAND_MAX = 512, 1024
BRAND_KEYS = ("title", "group", "owner", "android", "source", "art", "fill", "size", "sha256")
# StreamFlex's default chroma key, #010101. Transparent mode on Windows shows through every pixel of
# the window that is exactly this color, including pixels the renderer's scaling averages onto it
# from dark neighbors. So brand art keeps its opaque pixels two steps off it, as the launcher does
# when it loads an icon (src/chroma.c): one within a step of it in every channel becomes OFF_KEY.
CHROMA_KEY = (1, 1, 1)
OFF_KEY = (3, 3, 3)


def read_sections(path):
    """Read an INI file into [(section, {key: value})] in file order. A repeated section or key is an error."""
    sections, current, seen = [], None, set()
    for number, raw in enumerate(pathlib.Path(path).read_text(encoding="utf-8").splitlines(), 1):
        line = raw.strip()
        if not line or line.startswith(";") or line.startswith("#"):
            continue
        if line.startswith("[") and line.endswith("]"):
            name = line[1:-1].strip()
            if name in seen:
                raise ValueError(f"{path}:{number}: section [{name}] appears twice")
            seen.add(name)
            current = (name, {})
            sections.append(current)
            continue
        if "=" not in line or current is None:
            raise ValueError(f"{path}:{number}: expected 'key = value' inside a section: {raw!r}")
        key, value = (part.strip() for part in line.split("=", 1))
        if key in current[1]:
            raise ValueError(f"{path}:{number}: key '{key}' appears twice in [{current[0]}]")
        current[1][key] = value
    return sections


def write_brands(sections, path=BRANDS_INI):
    """Write brands.ini with a fixed key order, so hand edits and tool edits never fight over layout."""
    lines = [
        "; Brand icons, one section per icon, in the order the manifest lists them.",
        "; import-brand.py writes source, art, fill, size and sha256; title, group, owner and android are edited by hand.",
        "; After any change, run build-library.py to regenerate assets/icons/library/icons.ini and brands/NOTICE.md.",
    ]
    for name, keys in sections:
        lines += ["", f"[{name}]"]
        lines += [f"{key} = {keys[key]}" for key in BRAND_KEYS if keys.get(key)]
    pathlib.Path(path).write_text("\n".join(lines) + "\n", encoding="utf-8", newline="\n")


def sha256_file(path):
    return hashlib.sha256(pathlib.Path(path).read_bytes()).hexdigest()


def key_mask(image):
    """Mode L, 255 where an RGBA image's pixel is fully opaque and within one step of CHROMA_KEY in every
    channel, else 0. Needs Pillow."""
    from PIL import ImageChops
    mask = image.getchannel("A").point(lambda v: 255 if v == 255 else 0)
    for band, level in zip(image.split(), CHROMA_KEY):
        near = band.point(lambda v, level=level: 255 if abs(v - level) <= 1 else 0)
        mask = ImageChops.multiply(mask, near)
    return mask


def key_pixels(image):
    """How many of an RGBA image's pixels are fully opaque and within one step of CHROMA_KEY. Needs Pillow."""
    return key_mask(image).histogram()[255]


def keep_off_key(image):
    """Make each fully opaque pixel of an RGBA image within one step of CHROMA_KEY into OFF_KEY, in place;
    returns how many changed. Needs Pillow."""
    mask = key_mask(image)
    count = mask.histogram()[255]
    if count:
        image.paste(OFF_KEY + (255,), (0, 0) + image.size, mask)
    return count


def outline_mask(size, supersample=4):
    """The shared outline at size x size: mode L, 255 inside, 0 outside, a box-filtered edge between.
    Box filtering keeps the interior exactly 255 and the exterior exactly 0. Needs Pillow."""
    from PIL import Image, ImageDraw
    big = size * supersample
    mask = Image.new("L", (big, big), 0)
    ImageDraw.Draw(mask).rounded_rectangle((0, 0, big - 1, big - 1), radius=OUTLINE_RADIUS * big, fill=255)
    return mask.resize((size, size), Image.Resampling.BOX)
