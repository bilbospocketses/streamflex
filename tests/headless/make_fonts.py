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

A rename replaces OLD in both spellings a name table uses (one byte per letter, and UTF-16 big
endian), in place, so no offset moves; FreeType does not check table checksums. A collection is
the TTC header (version 1.0) and each font's own tables, with each table's offset moved to where
the font now starts. A blank font holds only the tables FreeType needs, its one glyph empty."""
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


def blank(dst, family, chars):
    glyphs = 2
    head = struct.pack(">IIIIHHqqhhhhHHhhh", 0x00010000, 0x00010000, 0, 0x5F0F3CF5, 0x000B, 1000, 0, 0,
                       0, 0, 500, 700, 0, 8, 2, 0, 0)
    hhea = struct.pack(">IhhhHhhhhhhhhhhhH", 0x00010000, 800, -200, 0, 500, 0, 0, 500, 1, 0, 0, 0, 0, 0, 0, 0, glyphs)
    maxp = struct.pack(">IH13H", 0x00010000, glyphs, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0)
    hmtx = struct.pack(">HhHh", 500, 0, 500, 0)
    loca = struct.pack(">3H", 0, 0, 0)
    glyf = b"\0" * 4
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
    font = sfnt({"head": head, "hhea": hhea, "maxp": maxp, "hmtx": hmtx, "loca": loca, "glyf": glyf,
                 "cmap": cmap, "name": name})
    open(dst, "wb").write(font)


if __name__ == "__main__":
    if len(sys.argv) == 6 and sys.argv[1] == "rename":
        rename(*sys.argv[2:6])
    elif len(sys.argv) >= 4 and sys.argv[1] == "ttc":
        ttc(sys.argv[2], sys.argv[3:])
    elif len(sys.argv) == 5 and sys.argv[1] == "blank":
        blank(sys.argv[2], sys.argv[3], sys.argv[4])
    else:
        sys.exit(__doc__)
