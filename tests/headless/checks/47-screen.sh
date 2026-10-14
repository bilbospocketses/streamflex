# The screen's size and the display's scale (spec section 6). The OS's scale is logged, never applied:
# under SDL_VIDEO_X11_SCALING_FACTOR=2 the log says so, and the home grid's pixels are those of a run
# without it.
s47_shot() { wait_line 'Begin program loop' "$2" && sleep 1 && xwd -root -silent -out "$out/$1.xwd"; }
CFG=$FX/f47-grid.ini run_keys f47-scale1 +s47_shot
CFG=$FX/f47-grid.ini SDL_VIDEO_X11_SCALING_FACTOR=2 run_keys f47-scale2 +s47_shot
# The two screenshots' pixels are compared, never the files: xwd may leave the pad byte of each
# colormap entry unset (Fedora's does), so two shots of the same screen can differ there
python3 "$HERE/pixels.py" "$out/f47-scale1.xwd" same "$out/f47-scale2.xwd" > "$out/f47-same.txt" 2>&1
s47_same=$?
ok=1
grep -qx 'Screen: 1920x1080 px, pixel density 1.00, display scale 1.00' "$out/f47-scale1.log" \
    && grep -qx 'Screen: 1920x1080 px, pixel density 1.00, display scale 2.00' "$out/f47-scale2.log" \
    && [ "$s47_same" = 0 ] && ran_clean f47-scale1 && ran_clean f47-scale2 && ok=0
result "screen: a display scale of 2 is logged and not applied, the grid drawn as at 1 (exit $(cat "$out/f47-scale1.code"), $(cat "$out/f47-scale2.code"))" $ok
grep -h '^Screen: ' "$out/f47-scale1.log" "$out/f47-scale2.log" | sed 's/^/      /'
sed 's/^/      /' "$out/f47-same.txt"

# A change of the window's pixel size runs the one reload, geometry first (spec section 6). Whether
# SDL3's X11 driver reports a resize of a fullscreen window with no window manager is unverified, so
# s47_resize tries SDL's own event first; when its line does not come within wait_line's bound it puts
# the window back and has the test hook (STREAMFLEX_TEST_PIXEL_SIZE's file, S47_SIZE) force the size,
# which proves the reload but not SDL's event. The first run decides, and s47.path keeps which path
# every later run takes; each run's own NAME.path says which it took. The case of the 3 s launch
# timeout, which a loaded box could change, is in 47-screen-timing.sh. Every wait here ends on a log
# line or on the screen showing what it waits for, within a bound.
S47_SIZE=/tmp/s47-size S47_APP=/tmp/s47-app
S47_ARROW=$(printf ' \xE2\x80\xBA ')
rm -f "$out/s47.path"
# A function to count the log's lines that are exactly LINE
s47_count() {
    local n
    n=$(grep -cxF -- "$1" "$LOG" 2> /dev/null)
    echo "${n:-0}"
}
# A function to wait up to 20 s, as wait_line does, for the exact LINE to be in the log more than
# BEFORE times, so a line logged before the change cannot end the wait; it fails when it never is, or
# when the launcher (PID) exits first
s47_more() {
    local line=$1 pid=$2 before=$3 i
    for i in $(seq 100); do
        [ "$(s47_count "$line")" -gt "$before" ] && return 0
        running "$pid" || break
        sleep 0.2
    done
    [ "$(s47_count "$line")" -gt "$before" ]
}
# A function for a +key: make the window SIZE (WxH), then wait for the exact log LINE to come once more
s47_resize() {
    local name=$1 pid=$2 size=$3 line=$4 before
    before=$(s47_count "$line")
    if [ "$(cat "$out/s47.path" 2> /dev/null)" != hook ]; then
        xdotool search --name '^StreamFlex$' windowsize %@ "${size%x*}" "${size#*x}" > /dev/null 2>&1
        if s47_more "$line" "$pid" "$before"; then
            echo event > "$out/$name.path"
            [ -f "$out/s47.path" ] || echo event > "$out/s47.path"
            return 0
        fi
        if [ "$(cat "$out/s47.path" 2> /dev/null)" = event ]; then   # SDL's event worked before: no fallback now
            echo event > "$out/$name.path"
            return 1
        fi
        xdotool search --name '^StreamFlex$' windowsize %@ 1920 1080 > /dev/null 2>&1
        echo hook > "$out/s47.path"
    fi
    echo "$size" > "$S47_SIZE"
    echo hook > "$out/$name.path"
    s47_more "$line" "$pid" "$before"
}
s47_to_720() { s47_resize "$1" "$2" 1280x720 'Screen: 1280x720 px, was 1920x1080; reloading'; }
s47_to_720_later() { s47_resize "$1" "$2" 1280x720 'Screen: 1280x720 px, was 1920x1080; reloading when the app is done'; }
# Back to the size laid out, which logs nothing but the hook build's line for each size event
s47_back_later() { s47_resize "$1" "$2" 1920x1080 'Test hook: a size event found the window 1920x1080 px'; }
s47_app_in() { : > "$S47_APP"; wait_line 'Application detected' "$2"; }
s47_app_out() { rm -f "$S47_APP"; wait_line 'Application finished' "$2"; }
# A function to give the width of the first box in a reading of pixels.py's count ("box x0,y0 to x1,y1: ...",
# after a tag or not); nothing when there is none
s47_span() { sed -n 's/^\(.* \)\{0,1\}box \([0-9]*\),[0-9]* to \([0-9]*\),.*/\2 \3/p' | head -1 | awk '{ print $2 - $1 }'; }
s47_x1y1() { sed -n "s/^$2 box [0-9]*,[0-9]* to \([0-9]*\),\([0-9]*\):.*/\1 \2/p" "$out/$1.boxes" 2> /dev/null | head -1; }
s47_width() { grep "^$2 box " "$out/$1.boxes" 2> /dev/null | s47_span; }
# A function to keep under TAG the box the highlight's #FF00FF spans, once the screen shows it, a
# screenshot every 0.2 s for 10 s at most: for before, any box; for after, one narrower than before's,
# which only a frame drawn at the new size has. The last reading is kept either way.
s47_box() {
    local name=$1 tag=$2 shot=/tmp/s47.xwd before reading="" width i
    before=$(s47_width "$name" before)
    for i in $(seq 50); do
        reading=$(xwd -root -silent -out "$shot" 2>&1 && python3 "$HERE/pixels.py" "$shot" count 255,0,255 inside 255,0,255)
        width=$(s47_span <<< "$reading")
        [ -n "$width" ] && { [ "$tag" = before ] || [ "$width" -lt "${before:-0}" ]; } && break
        sleep 0.2
    done
    echo "$tag $reading" >> "$out/$name.boxes"
}
s47_before() { wait_line 'Begin program loop' "$2" && s47_box "$1" before; }
s47_after() { wait_line 'Reload: done at 1280x720 px' "$2" && s47_box "$1" after; }
s47_at() { grep -nxF -- "$2" "$out/$1.log" | head -1 | cut -d: -f1; }   # The line number of an exact line; empty when absent

# Main is drawn, the window becomes 1280x720, and Main is drawn again whole inside it, smaller
rm -f "$S47_SIZE" "$out/f47-resize.boxes"
CFG=$FX/f47-grid.ini STREAMFLEX_TEST_PIXEL_SIZE=$S47_SIZE run_keys f47-resize +s47_before +s47_to_720 +s47_after
log=$out/f47-resize.log
read -r s47_ax1 s47_ay1 <<< "$(s47_x1y1 f47-resize after)"
s47_wbefore=$(s47_width f47-resize before)
s47_wafter=$(s47_width f47-resize after)
# The grid (columns x rows) is the same at both sizes; only its buttons are smaller
s47_grid_before=$(grep -m1 "^Menu 'Main': [0-9]* x [0-9]* grid" "$log" | sed 's/ grid,.*//')
s47_grid_after=$(sed -n '/^Screen: 1280x720 px, was 1920x1080; reloading$/,$p' "$log" | grep -m1 "^Menu 'Main': [0-9]* x [0-9]* grid" | sed 's/ grid,.*//')
ok=1
grep -qx 'Screen: 1280x720 px, was 1920x1080; reloading' "$log" \
    && grep -qx "Reload: done at 1280x720 px, for the window's size" "$log" \
    && [ "$(grep -c '^Reload: done at ' "$log")" = 1 ] \
    && [ -n "$s47_grid_before" ] && [ "$s47_grid_before" = "$s47_grid_after" ] \
    && [ -n "${s47_ay1:-}" ] && [ "$s47_ax1" -lt 1280 ] && [ "$s47_ay1" -lt 720 ] \
    && [ -n "$s47_wbefore" ] && [ -n "$s47_wafter" ] && [ "$s47_wafter" -lt "$s47_wbefore" ] \
    && ran_clean f47-resize && ok=0
result "screen: a new size reloads once, and Main is drawn whole at 1280x720 ($(cat "$out/f47-resize.path" 2> /dev/null); exit $(cat "$out/f47-resize.code"))" $ok
sed 's/^/      /' "$out/f47-resize.boxes" 2> /dev/null
grep -E '^(Screen|Reload|Layout area):' "$log" | sed 's/^/      /'

# With settings open on the Background page, an edit made (Color steps to #1E1E1E): the screen is
# measured again, the page and the row under the cursor survive, and the edit is saved
cfg=$(writable_config f60-color)
rm -f "$S47_SIZE"
CFG=$cfg STREAMFLEX_TEST_PIXEL_SIZE=$S47_SIZE run_keys f47-settings Menu Down Return Down Right +s47_to_720 BackSpace BackSpace
log=$out/f47-settings.log
s47_row_before=$(sed -n '/Settings: \[Background\] Color #000000 -> #1E1E1E/,/^Settings: re-measured for /p' "$log" | grep "the cursor's row reads" | tail -1)
s47_row_after=$(sed -n '/^Settings: re-measured for /,$p' "$log" | grep -m1 "the cursor's row reads")
ok=1
grep -qx 'Settings: re-measured for 1280x720 px' "$log" && [ -n "$s47_row_before" ] && [ "$s47_row_before" = "$s47_row_after" ] \
    && sed -n '/^Settings: re-measured for /,$p' "$log" | grep -qxF "Settings: page Settings${S47_ARROW}Background" \
    && grep -qx 'Color=#1E1E1E' "$cfg" && ran_clean f47-settings && ok=0
result "screen: a new size keeps the settings page, its cursor's row and its edit (exit $(cat "$out/f47-settings.code"))" $ok
echo "      before: ${s47_row_before:-none}"
echo "      after:  ${s47_row_after:-none}"

# With the color picker open, a new size closes it as Back would, putting its color back, and the page
# under it stays; nothing is saved
rm -f "$S47_SIZE"
CFG=$FX/f60-color.ini STREAMFLEX_TEST_PIXEL_SIZE=$S47_SIZE run_keys f47-picker Menu Down Return Down Return +s47_to_720 BackSpace BackSpace
log=$out/f47-picker.log
ok=1
in_range "$log" 'Screen: 1280x720 px, was 1920x1080; reloading' 'Settings: re-measured for 1280x720 px' \
        'Settings: closed the picker for [Background] Color' \
    && grep -q 'Settings: the color picker put \[Background\] Color back' "$log" \
    && sed -n '/^Settings: re-measured for /,$p' "$log" | grep -qxF "Settings: page Settings${S47_ARROW}Background" \
    && grep -q 'Settings: nothing changed' "$log" && ran_clean f47-picker && ok=0
result "screen: a new size closes an open picker as Back would, and keeps the page under it (exit $(cat "$out/f47-picker.code"))" $ok

# With the Background image's folder browser open (60-settings-background.sh's f60-preview keys: Mode
# to Image, then the Image row's browser, on blue.png), a new size closes it as Back would and keeps
# the page. Closed, Down moves the page's cursor; still open, it would move the browser's to the next
# image, green.png, and preview it. Menu then leaves settings, putting the Mode back: nothing is saved.
# s47_settle's 2 s only give a browser left open time to decode green.png; the result on code that
# closes it does not depend on them.
s47_browsing() { wait_line 'Settings: the preview shows /home/tester/Pictures/blue.png' "$2"; }
s47_settle() { sleep 2; }
rm -f "$S47_SIZE"
CFG=$FX/f60-color.ini STREAMFLEX_TEST_PIXEL_SIZE=$S47_SIZE run_keys f47-browser Menu Down Return Right Down Return +s47_browsing \
    +s47_to_720 Down +s47_settle Menu
log=$out/f47-browser.log
ok=1
grep -qx 'Settings: re-measured for 1280x720 px' "$log" \
    && sed -n '/^Settings: re-measured for /,$p' "$log" | grep -qxF "Settings: page Settings${S47_ARROW}Background" \
    && ! sed -n '/^Settings: re-measured for /,$p' "$log" | grep -q 'Settings: the preview shows /home/tester/Pictures/green.png' \
    && grep -q 'Settings: nothing changed' "$log" && ran_clean f47-browser && ok=0
result "screen: a new size closes the folder browser as Back would, and keeps the page under it (exit $(cat "$out/f47-browser.code"))" $ok
grep -E '^Settings: (re-measured|page|the preview shows|the cursor)' "$log" | sed 's/^/      /'

# While an app is in front (the STREAMFLEX_TEST_APP hook), a new size waits; when the app is done
# the size is read again and the reload runs then, once
rm -f "$S47_SIZE" "$S47_APP"
CFG=$FX/f47-grid.ini STREAMFLEX_TEST_APP=$S47_APP STREAMFLEX_TEST_PIXEL_SIZE=$S47_SIZE UNTIL='Reload: done at 1280x720 px' \
    run_keys f47-appwait +s47_app_in +s47_to_720_later +s47_app_out
s47_waits=$(s47_at f47-appwait 'Screen: 1280x720 px, was 1920x1080; reloading when the app is done')
s47_finished=$(s47_at f47-appwait 'Application finished')
s47_reloaded=$(s47_at f47-appwait "Reload: done at 1280x720 px, for the window's size")
ok=1
[ -n "$s47_waits" ] && [ -n "$s47_finished" ] && [ -n "$s47_reloaded" ] \
    && [ "$s47_waits" -lt "$s47_finished" ] && [ "$s47_finished" -lt "$s47_reloaded" ] \
    && [ "$(grep -c '^Reload: done at ' "$out/f47-appwait.log")" = 1 ] && ran_clean f47-appwait && ok=0
result "screen: a new size while an app is in front waits until it is done (exit $(cat "$out/f47-appwait.code"))" $ok
grep -E '^(Screen|Reload|Application|Test hook: an app|Test hook: the app)' "$out/f47-appwait.log" | sed 's/^/      /'

# A size that came back before the app is done reloads nothing
rm -f "$S47_SIZE" "$S47_APP"
CFG=$FX/f47-grid.ini STREAMFLEX_TEST_APP=$S47_APP STREAMFLEX_TEST_PIXEL_SIZE=$S47_SIZE \
    run_keys f47-appback +s47_app_in +s47_to_720_later +s47_back_later +s47_app_out
ok=1
grep -qx 'Screen: 1920x1080 px again; nothing to reload' "$out/f47-appback.log" \
    && ! grep -q '^Reload: done at ' "$out/f47-appback.log" && ran_clean f47-appback && ok=0
result "screen: a size that came back by the time the app is done reloads nothing (exit $(cat "$out/f47-appback.code"))" $ok
grep -E '^(Screen|Reload|Application|Test hook: a size event)' "$out/f47-appback.log" | sed 's/^/      /'

# A move to another display reads its refresh rate and works the frame timing out again (a test hook
# pushes SDL_EVENT_WINDOW_DISPLAY_CHANGED: Xvfb has one display). Read from the hook's own line on, in
# case SDL sends the event of its own as it makes the window.
rm -f /tmp/s47-display
s47_moved() { sed -n '/^Test hook: the window moved to a display$/,$p' "$1" 2> /dev/null; }
s47_display() {
    local i
    : > /tmp/s47-display
    for i in $(seq 100); do
        s47_moved "$LOG" | grep -qx 'Frame timing: VSync at 60 Hz, 16 ms a frame' && return 0
        running "$2" || return 1
        sleep 0.2
    done
    return 1
}
CFG=$FX/f47-grid.ini STREAMFLEX_TEST_DISPLAY_EVENT=/tmp/s47-display run_keys f47-display +s47_display
log=$out/f47-display.log
ok=1
s47_moved "$log" | grep -qE '^Screen: now on display [0-9]+, 60 Hz$' \
    && [ "$(s47_moved "$log" | grep -c '^Frame timing: VSync at 60 Hz, 16 ms a frame$')" = 1 ] \
    && ran_clean f47-display && ok=0
result "screen: a move to another display works the frame timing out again (exit $(cat "$out/f47-display.code"))" $ok
s47_moved "$log" | grep -E '^(Test hook|Screen|Frame timing):' | sed 's/^/      /'
