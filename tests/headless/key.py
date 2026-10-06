#!/usr/bin/env python3
"""Press a key, let it go, or both, by its keycode through XTEST, with no modifier held for it.

usage: key.py key|keydown|keyup KEYSYM     (KEYSYM as xdotool names it: F5)

xdotool reaches a key's first level with the modifiers of the first entry its key type maps to that
level. Fedora's keymap maps Alt and Control to the first level of CTRL+ALT, the function keys' type,
so xdotool holds Alt down for every F key there, and a key capture catches the Alt. run.sh sends the
function keys through this instead; the key and its keycode are the same.
"""
import ctypes
import sys
import time

if len(sys.argv) != 3 or sys.argv[1] not in ("key", "keydown", "keyup"):
    sys.exit(__doc__)
x = ctypes.CDLL("libX11.so.6")
xtst = ctypes.CDLL("libXtst.so.6")
x.XOpenDisplay.restype = ctypes.c_void_p
x.XOpenDisplay.argtypes = [ctypes.c_char_p]
x.XStringToKeysym.restype = ctypes.c_ulong
x.XStringToKeysym.argtypes = [ctypes.c_char_p]
x.XKeysymToKeycode.restype = ctypes.c_ubyte
x.XKeysymToKeycode.argtypes = [ctypes.c_void_p, ctypes.c_ulong]
x.XFlush.argtypes = [ctypes.c_void_p]
x.XCloseDisplay.argtypes = [ctypes.c_void_p]
xtst.XTestFakeKeyEvent.argtypes = [ctypes.c_void_p, ctypes.c_uint, ctypes.c_int, ctypes.c_ulong]

display = x.XOpenDisplay(None)
if not display:
    sys.exit("key.py: cannot open the display")
keycode = x.XKeysymToKeycode(display, x.XStringToKeysym(sys.argv[2].encode()))
if not keycode:
    sys.exit("key.py: no keycode for " + sys.argv[2])
if sys.argv[1] in ("key", "keydown"):
    xtst.XTestFakeKeyEvent(display, keycode, 1, 0)
    x.XFlush(display)
if sys.argv[1] == "key":
    time.sleep(0.02)
if sys.argv[1] in ("key", "keyup"):
    xtst.XTestFakeKeyEvent(display, keycode, 0, 0)
    x.XFlush(display)
x.XCloseDisplay(display)
