"""Review sheet: every frame composited at native size on white, Explorer #f3f3f3 and a dark taskbar tone,
plus a 4-6x nearest-neighbor zoom of the small frames so the pixel grid can be judged. This is how the
design was reviewed. Run after build-icon.ps1, which writes the frames to build/frames/. Writes
build/review_native.png and build/review_zoom.png. Needs Python with numpy + opencv-python.
"""
import pathlib

import cv2
import numpy as np

HERE = pathlib.Path(__file__).resolve().parent
D = HERE / "build"
F = D / "frames"
SIZES = [256, 128, 96, 64, 48, 40, 32, 24, 20, 16]
BGS = {"white": (255, 255, 255), "explorer": (243, 243, 243), "taskbar": (32, 32, 32)}


def over(rgba, bg):
    a = rgba[:, :, 3:4].astype(np.float32) / 255.0
    return (rgba[:, :, :3].astype(np.float32) * a + np.array(bg[::-1], np.float32) * (1 - a)).astype(np.uint8)


frames = {s: cv2.imread(str(F / f"ico_{s:03d}.png"), cv2.IMREAD_UNCHANGED) for s in SIZES}
missing = [s for s, im in frames.items() if im is None]
if missing:
    raise SystemExit(f"no frames for {missing} in {F}: run build-icon.ps1 first")

rows = []
for name, bg in BGS.items():
    row = np.full((270, 20 + sum(s + 14 for s in SIZES), 3), bg[::-1], np.uint8)
    x = 10
    for s in SIZES:
        row[262 - s:262, x:x + s] = over(frames[s], bg)
        x += s + 14
    rows.append(row)
sheet = np.vstack(rows)
cv2.imwrite(str(D / "review_native.png"), sheet)

# zoomed small frames, taskbar + explorer backgrounds
zrows = []
for name in ("explorer", "taskbar"):
    tiles = []
    for s in (48, 40, 32, 24, 20, 16):
        z = 6 if s <= 24 else 4
        big = cv2.resize(over(frames[s], BGS[name]), (s * z, s * z), interpolation=cv2.INTER_NEAREST)
        t = np.full((200, big.shape[1] + 12, 3), BGS[name][::-1], np.uint8)
        t[4:4 + big.shape[0], 6:6 + big.shape[1]] = big
        tiles.append(t)
    zrows.append(np.hstack(tiles))
w = max(r.shape[1] for r in zrows)
zrows = [np.pad(r, ((0, 0), (0, w - r.shape[1]), (0, 0)), constant_values=128) for r in zrows]
cv2.imwrite(str(D / "review_zoom.png"), np.vstack(zrows))
print(f"sheets written: {D / 'review_native.png'}, {D / 'review_zoom.png'}")
