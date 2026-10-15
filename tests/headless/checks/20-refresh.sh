# Item 22: Xvfb reports a refresh rate of 0, as some VMs and remote desktops do. The launcher
# must start, use 60 Hz and say so, instead of dividing by zero. Whether the 0 reaches it depends
# on the SDL underneath, which may stand a rate in for a display that says 0: then the launcher must
# use the rate it is given, and not claim a fallback it never made. What this SDL reports is asked,
# through the library the launcher uses, rounded to whole hertz as the launcher rounds it.
# Both branches rest on the X display itself saying 0 Hz, which is asserted first: a display that
# said 60 would make the 60 branch pass with no fallback exercised anywhere.
x_hz=$(xrandr --verbose 2> "$out/f22-xrandr.err" \
    | awk '/[*]current/ { cur = 1; next } cur && $1 == "v:" { print $NF; exit }')
sdl_hz=$(python3 - 2> "$out/f22-sdl.err" << 'EOF'
import ctypes
sdl = ctypes.CDLL("libSDL3.so.0")
class DisplayMode(ctypes.Structure):
    _fields_ = [("displayID", ctypes.c_uint32), ("format", ctypes.c_uint32), ("w", ctypes.c_int),
                ("h", ctypes.c_int), ("pixel_density", ctypes.c_float), ("refresh_rate", ctypes.c_float),
                ("refresh_rate_numerator", ctypes.c_int), ("refresh_rate_denominator", ctypes.c_int),
                ("internal", ctypes.c_void_p)]
sdl.SDL_Init.restype = ctypes.c_bool
sdl.SDL_Init.argtypes = [ctypes.c_uint32]
sdl.SDL_GetPrimaryDisplay.restype = ctypes.c_uint32
sdl.SDL_GetDesktopDisplayMode.restype = ctypes.POINTER(DisplayMode)
sdl.SDL_GetDesktopDisplayMode.argtypes = [ctypes.c_uint32]
if not sdl.SDL_Init(0x20):
    raise SystemExit(1)
mode = sdl.SDL_GetDesktopDisplayMode(sdl.SDL_GetPrimaryDisplay())
if not mode:
    raise SystemExit(1)
print(int(mode.contents.refresh_rate + 0.5))
sdl.SDL_Quit()
EOF
)
run_quick f22-refresh
ok=1
if [ "$x_hz" = 0.00Hz ] && ran_clean f22-refresh && grep -qx 'Refresh rate:  60 Hz' "$out/f22-refresh.log"; then
    case $sdl_hz in
        0) grep -q 'The display reports no refresh rate, using 60 Hz' "$out/f22-refresh.log" && ok=0 ;;
        60) grep -q 'reports no refresh rate' "$out/f22-refresh.log" || ok=0 ;;
    esac
fi
result "item 22: a display that reports 0 Hz starts at 60 Hz (X says ${x_hz:-nothing}, SDL says ${sdl_hz:-nothing} Hz, exit $(cat "$out/f22-refresh.code"))" $ok
[ "$x_hz" = 0.00Hz ] || echo "      the X display does not report 0 Hz, so this check tests nothing it names: fix the premise"
grep -m2 -E 'runtime error|AddressSanitizer' "$out/f22-refresh.err" | sed 's/^/      /'

# The log names SDL's video driver and the renderer it chose as the launcher starts, the first
# thing a hands-on check reads: here the X11 driver, and whichever renderer this SDL picked
ok=1
grep -qE "^Video: SDL's x11 driver, the [a-z0-9_]+ renderer$" "$out/f22-refresh.log" && ok=0
result "the log names SDL's video driver and renderer at startup" $ok
grep '^Video:' "$out/f22-refresh.log" | sed 's/^/      /'
