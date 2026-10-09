"""Read the color at chosen points of a screenshot taken with `xwd -root`, and compare it; or count
the pixels of one color inside the box that another color spans.

Usage: python3 pixels.py <file.xwd> x,y=r,g,b [x,y=r,g,b ...]
       python3 pixels.py <file.xwd> count r,g,b inside r,g,b

The first form prints one line per point, "x,y r,g,b (want r,g,b)", and exits 0 only when every
point is within 4 of the color it wants in each channel, 1 when any is not. The second finds the
box spanned by the pixels of exactly the second color (an icon's frame, say), and prints
"box x0,y0 to x1,y1: N pixels of r,g,b" for the pixels of exactly the first color inside it; it
exits 0, or 1 when no pixel has the second color. Both exit 2 when the file is not a screenshot
they can read.
"""
import struct
import sys

TOLERANCE = 4


def channel(value, mask):
    """Take one color channel out of a pixel value, scaled to 0-255."""
    if mask == 0:
        return 0
    shift = (mask & -mask).bit_length() - 1
    bits = bin(mask).count("1")
    return ((value & mask) >> shift) * 255 // ((1 << bits) - 1)


def exact(rgb, masks):
    """The pixel value, under the color masks, of a color given as 0-255 channels."""
    value = 0
    for level, mask in zip(rgb, masks):
        shift = (mask & -mask).bit_length() - 1
        bits = bin(mask).count("1")
        value |= (level * ((1 << bits) - 1) // 255) << shift
    return value


def color(text):
    return tuple(int(n) for n in text.split(","))


def count_inside(data, start, width, height, bytes_per_line, size, byte_order, masks, want, frame):
    """Print how many pixels of exactly `want` lie inside the box the pixels of exactly `frame` span."""
    mask = masks[0] | masks[1] | masks[2]
    want_value, frame_value = exact(want, masks), exact(frame, masks)
    endian = "<" if byte_order == 0 else ">"
    rows = []
    for y in range(height):
        row = data[start + y * bytes_per_line:start + y * bytes_per_line + width * size]
        if size == 4:
            values = [v & mask for (v,) in struct.iter_unpack(endian + "I", row)]
        else:
            order = "little" if byte_order == 0 else "big"
            values = [int.from_bytes(row[x:x + size], order) & mask for x in range(0, len(row), size)]
        rows.append(values)
    xs, ys = [], []
    for y, values in enumerate(rows):
        found = [x for x, v in enumerate(values) if v == frame_value]
        if found:
            xs += (found[0], found[-1])
            ys.append(y)
    if not ys:
        print(f"no pixel of {','.join(map(str, frame))} on screen")
        return 1
    x0, x1, y0, y1 = min(xs), max(xs), ys[0], ys[-1]
    count = sum(rows[y][x0:x1 + 1].count(want_value) for y in range(y0, y1 + 1))
    print(f"box {x0},{y0} to {x1},{y1}: {count} pixels of {','.join(map(str, want))}")
    return 0


def main():
    with open(sys.argv[1], "rb") as f:
        data = f.read()
    if len(data) < 100:
        print("not an xwd file: too short")
        return 2
    # The header is 25 big-endian 32-bit fields, whatever the machine; the window name follows it
    (header_size, version, pixmap_format, _depth, width, height, _xoffset, byte_order, _unit,
     _bit_order, _pad, bits_per_pixel, bytes_per_line, _visual_class, red_mask, green_mask,
     blue_mask, _bits_per_rgb, _colormap_entries, ncolors) = struct.unpack(">25I", data[:100])[:20]
    if version != 7 or pixmap_format != 2 or bits_per_pixel not in (24, 32):
        print(f"cannot read this xwd file: version {version}, format {pixmap_format}, {bits_per_pixel} bpp")
        return 2
    start = header_size + ncolors * 12
    size = bits_per_pixel // 8
    if sys.argv[2:3] == ["count"]:
        if len(sys.argv) != 6 or sys.argv[4] != "inside":
            print("usage: pixels.py <file.xwd> count r,g,b inside r,g,b")
            return 2
        return count_inside(data, start, width, height, bytes_per_line, size, byte_order,
                            (red_mask, green_mask, blue_mask), color(sys.argv[3]), color(sys.argv[5]))
    ok = True
    for arg in sys.argv[2:]:
        point, want = arg.split("=")
        x, y = (int(n) for n in point.split(","))
        want = color(want)
        if not (0 <= x < width and 0 <= y < height):
            print(f"{point} is outside the {width} x {height} screen")
            ok = False
            continue
        offset = start + y * bytes_per_line + x * size
        value = int.from_bytes(data[offset:offset + size], "little" if byte_order == 0 else "big")
        got = tuple(channel(value, mask) for mask in (red_mask, green_mask, blue_mask))
        print(f"{point} {','.join(map(str, got))} (want {','.join(map(str, want))})")
        ok = ok and all(abs(g - w) <= TOLERANCE for g, w in zip(got, want))
    return 0 if ok else 1


sys.exit(main())
