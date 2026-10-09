"""Research: pull the banner's working palette (ribbon, arrow, ring, window glass, backdrop) by k-means.

Reads the committed 1600x873 banner (docs/assets/branding/streamflex-banner.jpg, a downscale of the
branding/logo/streamflex-logo.jpg original), which is where the color constants at the top of
build-svg.py came from. The regions below are pixel boxes in
that image. Needs Python with numpy + opencv-python. Prints hex colors; changes nothing.
"""
import pathlib

import cv2
import numpy as np

BANNER = pathlib.Path(__file__).resolve().parents[2] / "docs" / "assets" / "branding" / "streamflex-banner.jpg"
img = cv2.imread(str(BANNER), cv2.IMREAD_COLOR)
if img is None:
    raise SystemExit(f"cannot read {BANNER}")


def palette(label, x0, y0, x1, y1, k, smin=110, vmin=110):
    reg = img[y0:y1, x0:x1]
    hsv = cv2.cvtColor(reg, cv2.COLOR_BGR2HSV).reshape(-1, 3)
    px = reg.reshape(-1, 3)[(hsv[:, 1] >= smin) & (hsv[:, 2] >= vmin)].astype(np.float32)
    crit = (cv2.TERM_CRITERIA_EPS + cv2.TERM_CRITERIA_MAX_ITER, 50, 0.5)
    _, lab, cen = cv2.kmeans(px, k, None, crit, 5, cv2.KMEANS_PP_CENTERS)
    counts = np.bincount(lab.ravel(), minlength=k)
    print(label, f"({len(px)} px)")
    for i in np.argsort(-counts):
        b, g, r = cen[i]
        print(f"  #{int(r):02X}{int(g):02X}{int(b):02X}  share={counts[i] / len(px):5.1%}")


palette("ribbon + arrow", 490, 140, 870, 480, 8)
palette("ring", 840, 140, 1150, 470, 7)
palette("window glass (low sat allowed)", 420, 175, 690, 425, 6, smin=0, vmin=90)

for name, (x, y) in {"arrow head tip": (852, 160), "arrow orange": (800, 250), "shaft blue": (790, 360),
                     "ring purple low-left": (905, 445), "ring inner blue": (1000, 185)}.items():
    b, g, r = (int(v) for v in img[y - 2:y + 3, x - 2:x + 3].reshape(-1, 3).mean(axis=0))
    print(f"point {name:22s} #{r:02X}{g:02X}{b:02X}")
