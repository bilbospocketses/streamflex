#!/usr/bin/env python3
"""Fixture fonts for the font picker's checks (59-settings-fonts.sh), made from the bundled fonts with
no font tools:

  make_fonts.py rename SRC DST OLD NEW   copy SRC to DST with its family name OLD replaced by NEW
                                         (the same length), wherever the name table spells it
  make_fonts.py ttc DST SRC...           pack each SRC (a single-face font) into one collection, face
                                         0 first; a SRC of - is a face that does not open (zeros)
  make_fonts.py blank DST FAMILY CHARS   write a TrueType font with a glyph for each character of
                                         CHARS and no other, as a symbol font has none of "Aa0";
                                         FAMILY - names no family
  make_fonts.py box DST FAMILY CHARS     the same, with each glyph a filled square instead of empty:
                                         the fallback-font checks' font of boxes

A rename replaces OLD in both spellings a name table uses (one byte per letter, and UTF-16 big
endian), in place, so no offset moves; FreeType does not check table checksums. A collection is
the TTC header (version 1.0) and each font's own tables, with each table's offset moved to where
the font now starts. A blank font holds only the tables FreeType needs, its one glyph empty;
a box font's glyph is one square contour."""
import struct
import sys


def rename(src, dst, old, new):
    if len(old) != len(new):
        sys.exit(f"rename: '{old}' and '{new}' differ in length")
    data = open(src, "rb").read()
    count = 0
    for encode in (lambda text: text.encode("latin-1"), lambda text: text.encode("utf-16-be")):
        count += data.count(encode(old))
        data = data.replace(encode(old), encode(new))
    if count == 0:
        sys.exit(f"rename: '{old}' is not in {src}")
    open(dst, "wb").write(data)


def ttc(dst, sources):
    fonts = [b"\0" * 12 if src == "-" else open(src, "rb").read() for src in sources]
    header = 12 + 4 * len(fonts)
    offsets = []
    at = (header + 3) & ~3
    for font in fonts:
        offsets.append(at)
        at = (at + len(font) + 3) & ~3
    out = bytearray(at)
    out[0:header] = b"ttcf" + struct.pack(">II", 0x00010000, len(fonts)) + struct.pack(f">{len(fonts)}I", *offsets)
    for font, base in zip(fonts, offsets):
        font = bytearray(font)
        tables = struct.unpack(">H", font[4:6])[0]
        for i in range(tables):
            record = 12 + 16 * i
            offset = struct.unpack(">I", font[record + 8:record + 12])[0]
            font[record + 8:record + 12] = struct.pack(">I", offset + base)
        out[base:base + len(font)] = font
    open(dst, "wb").write(out)


def sfnt(tables):
    """The bytes of a TrueType font holding TABLES (tag -> bytes), in tag order, each 4-aligned"""
    tags = sorted(tables)
    power = 1
    while power * 2 <= len(tags):
        power *= 2
    selector = power.bit_length() - 1
    header = struct.pack(">IHHHH", 0x00010000, len(tags), power * 16, selector, len(tags) * 16 - power * 16)
    at = len(header) + 16 * len(tags)
    records = b""
    body = b""
    for tag in tags:
        data = tables[tag]
        padded = data + b"\0" * (-len(data) % 4)
        checksum = sum(struct.unpack(f">{len(padded) // 4}I", padded)) & 0xFFFFFFFF
        records += tag.encode("ascii") + struct.pack(">III", checksum, at + len(body), len(data))
        body += padded
    return header + records + body


# A box font's glyph: one closed square contour, its points clockwise (TrueType fills a clockwise
# contour), every one on the curve
BOX_POINTS = [(50, 0), (50, 700), (750, 700), (750, 0)]


def box_glyph():
    """The glyf entry of BOX_POINTS: one contour, each point's x and y a 16-bit delta from the last"""
    xs = [x for x, _ in BOX_POINTS]
    ys = [y for _, y in BOX_POINTS]
    dx = [x - last for x, last in zip(xs, [0] + xs[:-1])]
    dy = [y - last for y, last in zip(ys, [0] + ys[:-1])]
    return struct.pack(">hhhhh", 1, min(xs), min(ys), max(xs), max(ys)) \
        + struct.pack(">HH", len(BOX_POINTS) - 1, 0) + bytes([0x01] * len(BOX_POINTS)) \
        + struct.pack(f">{len(dx)}h", *dx) + struct.pack(f">{len(dy)}h", *dy)


def font(dst, family, chars, filled):
    """A TrueType font with a glyph for each character of CHARS and no other: empty (blank), or the
    filled square of BOX_POINTS (box)"""
    glyphs = 2
    glyph = box_glyph() if filled else b""
    advance = 800 if filled else 500
    x_max = 750 if filled else 500
    head = struct.pack(">IIIIHHqqhhhhHHhhh", 0x00010000, 0x00010000, 0, 0x5F0F3CF5, 0x000B, 1000, 0, 0,
                       0, 0, x_max, 700, 0, 8, 2, 0, 0)
    hhea = struct.pack(">IhhhHhhhhhhhhhhhH", 0x00010000, 800, -200, 0, advance, 0, 0, x_max, 1, 0, 0, 0, 0, 0, 0, 0,
                       glyphs)
    points, contours = (len(BOX_POINTS), 1) if filled else (0, 0)
    maxp = struct.pack(">IH13H", 0x00010000, glyphs, points, contours, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0)
    hmtx = struct.pack(">HhHh", 500, 0, advance, BOX_POINTS[0][0] if filled else 0)
    loca = struct.pack(">3H", 0, 0, len(glyph) // 2)
    glyf = glyph if filled else b"\0" * 4
    # cmap format 4: a segment for each character, to glyph 1, then the closing 0xFFFF segment
    codes = sorted({ord(c) for c in chars}) + [0xFFFF]
    count = len(codes)
    deltas = [(1 - code) & 0xFFFF for code in codes[:-1]] + [1]
    power = 1
    while power * 2 <= count:
        power *= 2
    sub = struct.pack(">HHHHHHH", 4, 16 + 8 * count, 0, 2 * count, 2 * power, power.bit_length() - 1,
                      2 * count - 2 * power) \
        + struct.pack(f">{count}H", *codes) + b"\0\0" + struct.pack(f">{count}H", *codes) \
        + struct.pack(f">{count}H", *deltas) + struct.pack(f">{count}H", *([0] * count))
    cmap = struct.pack(">HHHHI", 0, 1, 3, 1, 12) + sub
    names = [(2, "Regular")] if family == "-" else [(1, family), (2, "Regular")]
    strings = b""
    records = b""
    for name_id, text in names:
        data = text.encode("utf-16-be")
        records += struct.pack(">6H", 3, 1, 0x409, name_id, len(data), len(strings))
        strings += data
    name = struct.pack(">3H", 0, len(names), 6 + 12 * len(names)) + records + strings
    data = sfnt({"head": head, "hhea": hhea, "maxp": maxp, "hmtx": hmtx, "loca": loca, "glyf": glyf,
                 "cmap": cmap, "name": name})
    open(dst, "wb").write(data)


if __name__ == "__main__":
    if len(sys.argv) == 6 and sys.argv[1] == "rename":
        rename(*sys.argv[2:6])
    elif len(sys.argv) >= 4 and sys.argv[1] == "ttc":
        ttc(sys.argv[2], sys.argv[3:])
    elif len(sys.argv) == 5 and sys.argv[1] in ("blank", "box"):
        font(sys.argv[2], sys.argv[3], sys.argv[4], sys.argv[1] == "box")
    else:
        sys.exit(__doc__)
