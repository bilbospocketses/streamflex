"""Import one brand icon into the library: check the art, cut it to the shared outline, record provenance.

  python branding/library/import-brand.py <name> <image> --source <page URL> [--art <image URL>] [--fill '#RRGGBB']

- The image must be square and at least 512 px. Art above 1024 px is reduced to 1024; art is never enlarged.
- A palette, grayscale or RGB image is converted to RGBA; an embedded color profile is converted to sRGB.
- Transparent pixels inside the outline (an old round or padded logo) are refused unless --fill names a solid
  color to put behind the art. Choose the art's own background color and say so in the task report.
- Opaque pixels within one step of the default chroma key, #010101, in every channel (#000000 to #020202)
  become #030303: Transparent mode on Windows shows through the key, and scaling averages them onto it.
- Writes assets/icons/library/brands/<name>.png, and records source, art, fill, size and sha256 in
  branding/library/brands.ini. Set title, group, owner and android there by hand, then run build-library.py.
Needs Pillow.
"""
import argparse
import io
import pathlib
import re
import sys

from PIL import Image, ImageChops, ImageCms

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import libtools  # noqa: E402


NEAR_OPAQUE = 250   # alpha at or above this counts as opaque


class ImportRefused(Exception):
    pass


def load_rgba(image_path):
    with Image.open(image_path) as source:
        source.load()
        profile = source.info.get("icc_profile")
        image = source.convert("RGBA")
    if profile:
        image = ImageCms.profileToProfile(image, ImageCms.ImageCmsProfile(io.BytesIO(profile)),
                                          ImageCms.createProfile("sRGB"), outputMode="RGBA")
    return image


def import_brand(name, image_path, source, art=None, fill=None, library=libtools.LIBRARY,
                 brands_ini=libtools.BRANDS_INI):
    if not libtools.NAME_RE.match(name):
        raise ImportRefused(f"'{name}' is not a valid icon name (a-z, 0-9 and '-', up to 32 characters)")
    if fill is not None and not re.fullmatch(r"#[0-9A-Fa-f]{6}", fill):
        raise ImportRefused(f"--fill '{fill}' is not a color; write it as #RRGGBB, for example #13405F")
    image = load_rgba(image_path)
    width, height = image.size
    if width != height:
        raise ImportRefused(f"{image_path} is {width}x{height}; brand art must be square")
    if width < libtools.BRAND_MIN:
        raise ImportRefused(f"{image_path} is {width} px; brand art must be at least {libtools.BRAND_MIN} px "
                            f"(it is never enlarged)")
    size = min(width, libtools.BRAND_MAX)
    if width > size:
        image = image.resize((size, size), Image.Resampling.LANCZOS)
    # Store art often carries alpha 250-254 from its encoder. That is not transparency: make it opaque.
    image.putalpha(image.getchannel("A").point(lambda v: 255 if v >= NEAR_OPAQUE else v))

    mask = libtools.outline_mask(size)
    inside = mask.point(lambda v: 255 if v == 255 else 0)
    holes = ImageChops.multiply(ImageChops.invert(image.getchannel("A")), inside).getbbox()
    if holes is not None:
        if fill is None:
            raise ImportRefused(f"{image_path} has transparent pixels inside the outline (around {holes}). "
                                f"Look at it: if it is an old round or padded logo, re-run with --fill "
                                f"'#RRGGBB' set to its own background color")
        rgb = tuple(int(fill[i:i + 2], 16) for i in (1, 3, 5))
        image = Image.alpha_composite(Image.new("RGBA", (size, size), rgb + (255,)), image)

    image.putalpha(ImageChops.darker(image.getchannel("A"), mask))
    off_key = libtools.keep_off_key(image)
    destination = pathlib.Path(library) / "brands" / f"{name}.png"
    destination.parent.mkdir(parents=True, exist_ok=True)
    image.save(destination, format="PNG", optimize=True)
    digest = libtools.sha256_file(destination)

    brands_ini = pathlib.Path(brands_ini)
    sections = libtools.read_sections(brands_ini) if brands_ini.is_file() else []
    keys = dict(sections).get(name)
    if keys is None:
        keys = {}
        sections.append((name, keys))
    keys.update({"source": source, "size": str(size), "sha256": digest})
    for key, value in (("art", art), ("fill", fill)):
        if value:
            keys[key] = value
        else:
            keys.pop(key, None)
    libtools.write_brands(sections, brands_ini)
    return {"size": size, "sha256": digest, "reduced": width > size, "filled": fill is not None, "off_key": off_key}


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("name")
    parser.add_argument("image")
    parser.add_argument("--source", required=True, help="the listing or press-kit page the art came from")
    parser.add_argument("--art", help="the direct URL of the image file")
    parser.add_argument("--fill", help="#RRGGBB to put behind transparent art")
    args = parser.parse_args()
    try:
        info = import_brand(args.name, args.image, args.source, args.art, args.fill)
    except ImportRefused as refused:
        print(f"refused: {refused}")
        return 1
    note = " (reduced to 1024)" if info["reduced"] else ""
    note += f" (filled with {args.fill})" if info["filled"] else ""
    note += f" ({info['off_key']} pixel(s) near the chroma key #010101 made #030303)" if info["off_key"] else ""
    print(f"imported {args.name}: {info['size']} px{note}, sha256 {info['sha256'][:12]}")
    keys = dict(libtools.read_sections(libtools.BRANDS_INI))[args.name]
    todo = [key for key in ("title", "group", "owner") if not keys.get(key)]
    if todo:
        print(f"now set {', '.join(todo)} (and android, if it has one) for [{args.name}] in brands.ini")
    print("then run build-library.py")
    return 0


if __name__ == "__main__":
    sys.exit(main())
