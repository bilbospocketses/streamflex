"""Read the color at chosen points of a screenshot taken with `xwd -root`, and compare it; count
the pixels of one color inside the box that another color spans; or compare two screenshots.

Usage: python3 pixels.py <file.xwd> x,y=r,g,b [x,y=r,g,b ...]
       python3 pixels.py <file.xwd> count r,g,b inside r,g,b
       python3 pixels.py <file.xwd> same <other.xwd>

The first form prints one line per point, "x,y r,g,b (want r,g,b)", and exits 0 only when every
point is within 4 of the color it wants in each channel, 1 when any is not. The second finds the
box spanned by the pixels of exactly the second color (an icon's frame, say), and prints
"box x0,y0 to x1,y1: N pixels of r,g,b" for the pixels of exactly the first color inside it; it
exits 0, or 1 when no pixel has the second color. The third compares the two screenshots' pixels
alone, never their headers or colormaps (each XWDColor entry carries a pad byte that xwd may leave
unset: Fedora's does), and prints "same pixels: W x H", exiting 0, or "N pixels differ, in the box
x0,y0 to x1,y1", or that the sizes differ, exiting 1. All exit 2 when a file is not a screenshot
they can read.
"""
import collections
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


Screenshot = collections.namedtuple(
    "Screenshot", "data start width height bytes_per_line size byte_order masks")


def load(path):
    """Read a screenshot and the header fields the modes use: a Screenshot, or the reason it cannot be read."""
    try:
        with open(path, "rb") as f:
            data = f.read()
    except OSError as error:
        return f"cannot read {path}: {error.strerror}"
    if len(data) < 100:
        return f"not an xwd file: too short ({path})"
    # The header is 25 big-endian 32-bit fields, whatever the machine; the window name follows it,
    # then ncolors 12-byte XWDColor entries, then the pixels
    (header_size, version, pixmap_format, _depth, width, height, _xoffset, byte_order, _unit,
     _bit_order, _pad, bits_per_pixel, bytes_per_line, _visual_class, red_mask, green_mask,
     blue_mask, _bits_per_rgb, _colormap_entries, ncolors) = struct.unpack(">25I", data[:100])[:20]
    if version != 7 or pixmap_format != 2 or bits_per_pixel not in (24, 32):
        return f"cannot read this xwd file: version {version}, format {pixmap_format}, {bits_per_pixel} bpp ({path})"
    start = header_size + ncolors * 12
    if len(data) < start + height * bytes_per_line:
        return f"not an xwd file: its pixels are cut short ({path})"
    return Screenshot(data, start, width, height, bytes_per_line, bits_per_pixel // 8, byte_order,
                      (red_mask, green_mask, blue_mask))


def rows(shot):
    """The screenshot's pixel values, row by row, with only the color masks' bits kept."""
    mask = shot.masks[0] | shot.masks[1] | shot.masks[2]
    endian = "<" if shot.byte_order == 0 else ">"
    order = "little" if shot.byte_order == 0 else "big"
    out = []
    for y in range(shot.height):
        offset = shot.start + y * shot.bytes_per_line
        row = shot.data[offset:offset + shot.width * shot.size]
        if shot.size == 4:
            out.append([v & mask for (v,) in struct.iter_unpack(endian + "I", row)])
        else:
            out.append([int.from_bytes(row[x:x + shot.size], order) & mask for x in range(0, len(row), shot.size)])
    return out


def same_pixels(a, b):
    """Print whether two screenshots show the same pixels, comparing the pixel data alone."""
    if (a.width, a.height) != (b.width, b.height):
        print(f"the sizes differ: {a.width} x {a.height} and {b.width} x {b.height}")
        return 1
    if (a.size, a.masks) != (b.size, b.masks):
        print("cannot compare: the two screenshots store their pixels differently")
        return 2
    xs, ys, count = [], [], 0
    for y, (row_a, row_b) in enumerate(zip(rows(a), rows(b))):
        if row_a == row_b:
            continue
        found = [x for x, (p, q) in enumerate(zip(row_a, row_b)) if p != q]
        count += len(found)
        xs += (found[0], found[-1])
        ys.append(y)
    if not count:
        print(f"same pixels: {a.width} x {a.height}")
        return 0
    print(f"{count} pixels differ, in the box {min(xs)},{ys[0]} to {max(xs)},{ys[-1]}")
    return 1


def count_inside(shot, want, frame):
    """Print how many pixels of exactly `want` lie inside the box the pixels of exactly `frame` span."""
    want_value, frame_value = exact(want, shot.masks), exact(frame, shot.masks)
    values_by_row = rows(shot)
    xs, ys = [], []
    for y, values in enumerate(values_by_row):
        found = [x for x, v in enumerate(values) if v == frame_value]
        if found:
            xs += (found[0], found[-1])
            ys.append(y)
    if not ys:
        print(f"no pixel of {','.join(map(str, frame))} on screen")
        return 1
    x0, x1, y0, y1 = min(xs), max(xs), ys[0], ys[-1]
    count = sum(values_by_row[y][x0:x1 + 1].count(want_value) for y in range(y0, y1 + 1))
    print(f"box {x0},{y0} to {x1},{y1}: {count} pixels of {','.join(map(str, want))}")
    return 0


def main():
    shot = load(sys.argv[1])
    if isinstance(shot, str):
        print(shot)
        return 2
    if sys.argv[2:3] == ["same"]:
        if len(sys.argv) != 4:
            print("usage: pixels.py <file.xwd> same <other.xwd>")
            return 2
        other = load(sys.argv[3])
        if isinstance(other, str):
            print(other)
            return 2
        return same_pixels(shot, other)
    if sys.argv[2:3] == ["count"]:
        if len(sys.argv) != 6 or sys.argv[4] != "inside":
            print("usage: pixels.py <file.xwd> count r,g,b inside r,g,b")
            return 2
        return count_inside(shot, color(sys.argv[3]), color(sys.argv[5]))
    ok = True
    for arg in sys.argv[2:]:
        point, want = arg.split("=")
        x, y = (int(n) for n in point.split(","))
        want = color(want)
        if not (0 <= x < shot.width and 0 <= y < shot.height):
            print(f"{point} is outside the {shot.width} x {shot.height} screen")
            ok = False
            continue
        offset = shot.start + y * shot.bytes_per_line + x * shot.size
        value = int.from_bytes(shot.data[offset:offset + shot.size], "little" if shot.byte_order == 0 else "big")
        got = tuple(channel(value, mask) for mask in shot.masks)
        print(f"{point} {','.join(map(str, got))} (want {','.join(map(str, want))})")
        ok = ok and all(abs(g - w) <= TOLERANCE for g, w in zip(got, want))
    return 0 if ok else 1


sys.exit(main())