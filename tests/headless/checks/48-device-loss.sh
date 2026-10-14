# Device loss (spec section 3): SDL's three render events, posted by a test hook
# (STREAMFLEX_TEST_RENDER_EVENT), since no software renderer sends them. A target or device reset runs
# the full reload a config change runs, once per batch of events, after they are drained: every
# texture made again, the menu and the screen kept, and the screen redrawn to the pixel. With
# settings open, their textures are made again and the page stays, an open folder browser with it,
# its image decoded again. While an app holds a size change back, a reset rebuilds the textures at
# the geometry in force; the size waits for the app. A lost device drops every texture and then the
# renderer, and a new renderer is made from the window's plan, at once or once a launched app is
# done, then the one reload; a failed attempt is tried again 2 s later, and the fifth failure ends
# StreamFlex as a failed start does. The cases that race the attempts' 2 s are in
# 48-device-loss-timing.sh.
# Not timing-sensitive: every wait ends on a log line or a bounded screen poll, but for the 1 s sleeps, which are floors after a log line (before each screenshot, so a frame drawn after the reset is on screen; before f48-sizereset ends, so a second reload would be logged), and a late shot cannot fail a correct run, whose frame is the same before and after the reset; the size and the reset that must share a batch are written while the launcher is stopped; f48-applost's 3 s sleep is a floor too (a correct run makes no attempt while the app runs, however long), the five attempts f48-giveup waits for are paced 2 s apart in wall time and end inside the 20 s line wait, and no case races a deadline
D48_EVENT=/tmp/d48-event D48_SIZE=/tmp/d48-size D48_APP=/tmp/d48-app
# A function to put the render events WORDS in the hook's file in one step (a rename), so the hook
# never reads half of them
d48_send() { printf '%s\n' "$1" > "$D48_EVENT.new" && mv -f "$D48_EVENT.new" "$D48_EVENT"; }
# The background's corner and the box the highlight's #FF00FF spans, once the frame is on screen,
# kept under TAG; d48_same says both match before and after, and that the shot before found the box
# (pixels.py's line for a screen with no #FF00FF, the same in both shots, would compare equal)
d48_box() {
    local name=$1 tag=$2 shot=/tmp/d48.xwd
    sleep 1
    screen_shows "$name" "$tag" 10,10=30,45,60 && echo "$tag corner yes" >> "$out/$name.seen"
    xwd -root -silent -out "$shot" && python3 "$HERE/pixels.py" "$shot" count 255,0,255 inside 255,0,255 \
        | sed "s/^/$tag /" >> "$out/$name.boxes"
}
d48_boxline() { sed -n "s/^$2 //p" "$out/$1.boxes" 2> /dev/null | head -1; }
d48_same() {
    case $(d48_boxline "$1" before) in
        'box '*) ;;
        *) return 1 ;;
    esac
    [ "$(d48_boxline "$1" before)" = "$(d48_boxline "$1" after)" ] \
        && grep -qx 'before corner yes' "$out/$1.seen" && grep -qx 'after corner yes' "$out/$1.seen"
}
d48_before() { wait_line 'Begin program loop' "$2" && d48_box "$1" before; }
d48_after() { wait_line 'Reload: done at 1920x1080 px' "$2" && d48_box "$1" after; }
d48_reset() { d48_send reset; }
d48_batch() { d48_send 'targets reset'; }
d48_reset_seen() { d48_send reset; wait_line 'Reload: done at ' "$2"; }
d48_app_in() { : > "$D48_APP"; wait_line 'Application detected' "$2"; }
d48_app_out() { rm -f "$D48_APP"; wait_line 'Application finished' "$2"; }
d48_at() { grep -nxF -- "$2" "$out/$1.log" | head -1 | cut -d: -f1; }
# A function to count the log's lines that are exactly LINE
d48_count() {
    local n
    n=$(grep -cxF -- "$1" "$LOG" 2> /dev/null)
    echo "${n:-0}"
}
# A function to wait up to 20 s, as wait_line does, for the exact LINE to be in the log more than
# BEFORE times; it fails when it never is, or when the launcher (PID) exits first
d48_more() {
    local line=$1 pid=$2 before=$3 i
    for i in $(seq 100); do
        [ "$(d48_count "$line")" -gt "$before" ] && return 0
        running "$pid" || break
        sleep 0.2
    done
    [ "$(d48_count "$line")" -gt "$before" ]
}
# A resize as 47-screen.sh's s47_resize makes one: SDL's own event first (which delivers it on Xvfb),
# the size hook when that line never comes; NAME.path says which
d48_resize() {
    local name=$1 pid=$2 size=$3 line=$4 before
    before=$(d48_count "$line")
    xdotool search --name '^StreamFlex$' windowsize %@ "${size%x*}" "${size#*x}" > /dev/null 2>&1
    if d48_more "$line" "$pid" "$before"; then
        echo event > "$out/$name.path"
        return 0
    fi
    xdotool search --name '^StreamFlex$' windowsize %@ 1920 1080 > /dev/null 2>&1
    echo "$size" > "$D48_SIZE"
    echo hook > "$out/$name.path"
    d48_more "$line" "$pid" "$before"
}
d48_to_720_later() { d48_resize "$1" "$2" 1280x720 'Screen: 1280x720 px, was 1920x1080; reloading when the app is done'; }
d48_reset_runs() { rm -f "$D48_EVENT" "$D48_EVENT.new" "$D48_SIZE" "$D48_APP" "$out/$1.boxes" "$out/$1.seen" "$out/$1.path"; }

# A device reset: the reload runs once, every group refreshed, and the screen redraws to the pixel
d48_reset_runs f48-reset
CFG=$FX/f48-loss.ini STREAMFLEX_TEST_RENDER_EVENT=$D48_EVENT run_keys f48-reset +d48_before +d48_reset +d48_after
log=$out/f48-reset.log
ok=1
[ "$(grep -cx 'Renderer: device lost; reloading' "$log")" = 1 ] && [ "$(grep -c '^Reload: done at ' "$log")" = 1 ] \
    && grep -qx 'Reload: done at 1920x1080 px, for a device reset or loss' "$log" \
    && in_range "$log" 'Renderer: device lost; reloading' 'Reload: done at ' 'Settings: refreshed the title font' \
    && in_range "$log" 'Renderer: device lost; reloading' 'Reload: done at ' 'Settings: refreshed the layout' \
    && ! in_range "$log" 'Renderer: device lost; reloading' 'Reload: done at ' 'Gamepad stopped' \
    && [ "$(grep -cx 'Renderer: vulkan' "$log")" = 1 ] && d48_same f48-reset && ran_clean f48-reset && ok=0
result "device: a reset reloads once, leaves the gamepad running, and redraws to the pixel (exit $(cat "$out/f48-reset.code"))" $ok
sed 's/^/      /' "$out/f48-reset.boxes" 2> /dev/null

# A reset while the screensaver dims: the reload makes its layer again and keeps it dimming (spec
# section 3: the screen's state is kept), so the frame stays dimmed, and the screensaver neither stops
# nor comes on again. The background #1E2D3C under the default 70 % dim reads about 9,14,18. The
# first reading waits for the hook build's line that the dim has ended.
d48_dimmed() {
    wait_line "Test hook: the screensaver's dim ended at tick " "$2" \
        && screen_shows "$1" before 10,10=9,14,18 && echo 'before dim yes' >> "$out/$1.seen"
}
d48_dim_after() {
    wait_line 'Reload: done at ' "$2" && screen_shows "$1" after 10,10=9,14,18 && echo 'after dim yes' >> "$out/$1.seen"
}
d48_reset_runs f48-dim
CFG=$FX/f48-dim.ini STREAMFLEX_TEST_RENDER_EVENT=$D48_EVENT run_keys f48-dim +d48_dimmed +d48_reset +d48_dim_after
log=$out/f48-dim.log
ok=1
grep -qx 'before dim yes' "$out/f48-dim.seen" && grep -qx 'after dim yes' "$out/f48-dim.seen" \
    && grep -qx 'Screensaver: its layer was made again at 1920x1080 px, still dimming' "$log" \
    && [ "$(grep -cx 'Screensaver on' "$log")" = 1 ] && ! grep -qx 'Screensaver off' "$log" \
    && ! in_range "$log" 'Renderer: device lost; reloading' 'Reload: done at ' 'Screensaver stopped' \
    && ran_clean f48-dim && ok=0
result "device: a reset while the screensaver dims keeps it dimming (exit $(cat "$out/f48-dim.code"))" $ok
sed 's/^/      /' "$out/f48-dim.seen" 2> /dev/null
grep -E '^Screensaver' "$log" | sed 's/^/      /'

# A target reset and a device reset in one batch of events: one reload, for both
d48_reset_runs f48-batch
CFG=$FX/f48-loss.ini STREAMFLEX_TEST_RENDER_EVENT=$D48_EVENT run_keys f48-batch +d48_before +d48_batch +d48_after
log=$out/f48-batch.log
ok=1
[ "$(grep -cx 'Renderer: device lost; reloading' "$log")" = 1 ] && [ "$(grep -c '^Reload: done at ' "$log")" = 1 ] \
    && grep -qx 'Reload: done at 1920x1080 px, for a render target reset and a device reset or loss' "$log" \
    && d48_same f48-batch && ran_clean f48-batch && ok=0
result "device: a target reset and a device reset in one batch reload once (exit $(cat "$out/f48-batch.code"))" $ok

# A new size and a device reset in one batch: one reload, geometry first, for both. The size goes
# through the size hook's file, as only the hook can be timed this way. Both files are written while
# the launcher is stopped, so its next pass reads both; and the render hook pushes a size its file
# took since the size hook's read in that pass first, so a stop that lands between the two hooks
# still brings both events in one batch, the size first. The files sit at the top of /tmp, which
# the leak pass's retry puts back as they were before the run. The reset's own line is logged once,
# after its push and before the reload, though a size was marked first in the batch.
d48_both() {
    wait_line 'Begin program loop' "$2" || return 1
    kill -STOP "$2"
    echo 1280x720 > "$D48_SIZE"
    d48_send reset
    kill -CONT "$2"
    wait_line 'Reload: done at ' "$2" && sleep 1
}
d48_reset_runs f48-sizereset
CFG=$FX/f48-loss.ini STREAMFLEX_TEST_PIXEL_SIZE=$D48_SIZE STREAMFLEX_TEST_RENDER_EVENT=$D48_EVENT \
    run_keys f48-sizereset +d48_both
log=$out/f48-sizereset.log
ok=1
[ "$(grep -c '^Reload: done at ' "$log")" = 1 ] \
    && grep -qx "Reload: done at 1280x720 px, for the window's size and a device reset or loss" "$log" \
    && [ "$(grep -cx 'Renderer: device lost; reloading' "$log")" = 1 ] \
    && in_range "$log" "Test hook: pushed the render event 'reset'" 'Reload: done at ' 'Renderer: device lost; reloading' \
    && ran_clean f48-sizereset && ok=0
result "device: a new size and a device reset in one batch reload once, geometry first (exit $(cat "$out/f48-sizereset.code"))" $ok
grep -E "^(Screen|Reload|Renderer):|^Test hook: (pushed the render event|the window's size reads|a size event found)" "$log" | sed 's/^/      /'

# With settings open on the Background page: the reset makes their textures again (the preview anew,
# placed where it was), the preview shows the background, and the page and its cursor stay
d48_settings_after() {
    wait_line 'Reload: done at 1920x1080 px' "$2" && look "$1" "$2" after 'Reload: done at 1920x1080 px' 30,30=30,45,60 @5,1074=11,22,32
}
d48_reset_runs f48-settings
CFG=$FX/f48-loss.ini STREAMFLEX_TEST_RENDER_EVENT=$D48_EVENT run_keys f48-settings Menu Down Return +d48_reset +d48_settings_after Down Menu
log=$out/f48-settings.log
ok=1
[ "$(grep -c '^Settings: the preview is at ' "$log")" = 2 ] \
    && [ "$(grep '^Settings: the preview is at ' "$log" | sort -u | wc -l)" = 1 ] \
    && grep -qx 'after yes' "$out/f48-settings.seen" \
    && sed -n '/^Reload: done at /,$p' "$log" | grep -q "^Settings: the cursor's row reads Color: " \
    && grep -q 'Settings: nothing changed' "$log" && ran_clean f48-settings && ok=0
result "device: a reset with settings open makes their textures again and keeps the page (exit $(cat "$out/f48-settings.code"))" $ok
sed 's/^/      /' "$out/f48-settings.seen" 2> /dev/null

# With the Background image's folder browser open on blue.png (60-settings-background.sh's f60-preview
# keys and config: Mode to Image, then the Image row's browser), the reset keeps the browser open and
# decodes the image again, as only the textures are made again: the image's line is logged once more
# after the reload, the preview shows it, and settings are not re-measured (a reflow would re-measure
# them and close the browser). The probe is check 60's shows_blue, written here as check 60 may run
# in the other shard. The count before the reset is taken once the image's first line is in.
D48_BLUE='Settings: the preview shows /home/tester/Pictures/blue.png'
d48_browsing() { d48_blue=; wait_line "$D48_BLUE" "$2" && d48_blue=$(d48_count "$D48_BLUE"); }
d48_browse_after() {
    [ -n "$d48_blue" ] && wait_line 'Reload: done at 1920x1080 px' "$2" && d48_more "$D48_BLUE" "$2" "$d48_blue" \
        && look "$1" "$2" after "$D48_BLUE" 30,30=40,70,160
}
d48_reset_runs f48-browse
CFG=$FX/f60-color.ini STREAMFLEX_TEST_RENDER_EVENT=$D48_EVENT \
    run_keys f48-browse Menu Down Return Right Down Return +d48_browsing +d48_reset +d48_browse_after Menu
log=$out/f48-browse.log
ok=1
[ "$(grep -cxF "$D48_BLUE" "$log")" = 2 ] && sed -n '/^Reload: done at /,$p' "$log" | grep -qxF "$D48_BLUE" \
    && grep -qx 'after yes' "$out/f48-browse.seen" && ! grep -q '^Settings: re-measured for ' "$log" \
    && ran_clean f48-browse && ok=0
result "device: a reset with the folder browser open keeps it open and shows its image again (exit $(cat "$out/f48-browse.code"))" $ok
sed 's/^/      /' "$out/f48-browse.seen" 2> /dev/null
grep -E '^(Reload: done|Settings: (the preview shows|re-measured))' "$log" | sed 's/^/      /'

# While an app holds a size change back, a reset rebuilds the textures at the size laid out; the new
# size is laid out once the app is done
d48_reset_runs f48-appreset
CFG=$FX/f48-loss.ini STREAMFLEX_TEST_APP=$D48_APP STREAMFLEX_TEST_PIXEL_SIZE=$D48_SIZE STREAMFLEX_TEST_RENDER_EVENT=$D48_EVENT \
    UNTIL='Reload: done at 1280x720 px' run_keys f48-appreset +d48_app_in +d48_to_720_later +d48_reset_seen +d48_app_out
d48_kept=$(d48_at f48-appreset 'Reload: done at 1920x1080 px, for a device reset or loss')
d48_finished=$(d48_at f48-appreset 'Application finished')
d48_resized=$(d48_at f48-appreset "Reload: done at 1280x720 px, for the window's size")
ok=1
[ -n "$d48_kept" ] && [ -n "$d48_finished" ] && [ -n "$d48_resized" ] \
    && [ "$d48_kept" -lt "$d48_finished" ] && [ "$d48_finished" -lt "$d48_resized" ] \
    && [ "$(grep -c '^Reload: done at ' "$out/f48-appreset.log")" = 2 ] && ran_clean f48-appreset && ok=0
result "device: a reset while an app holds a size back keeps the size laid out ($(cat "$out/f48-appreset.path" 2> /dev/null); exit $(cat "$out/f48-appreset.code"))" $ok
grep -E '^(Screen|Reload|Renderer|Application)' "$out/f48-appreset.log" | sed 's/^/      /'

# A lost device (SDL_EVENT_RENDER_DEVICE_LOST): every texture dropped (the fixture holds 7: three icons,
# three titles, the highlight), the renderer destroyed, a new one made from the platform's list, then
# the one reload, and the screen redrawn to the pixel. A second loss in the same batch does nothing.
d48_lost() { d48_send 'lost lost'; }
d48_reset_runs f48-lost
CFG=$FX/f48-loss.ini STREAMFLEX_TEST_RENDER_EVENT=$D48_EVENT run_keys f48-lost +d48_before +d48_lost +d48_after
log=$out/f48-lost.log
ok=1
[ "$(grep -cx 'Renderer: device lost and not recovered; creating a new renderer' "$log")" = 1 ] \
    && grep -qx 'Renderer: dropped 7 textures before the renderer went' "$log" \
    && grep -qx 'Renderer: creating a new renderer, attempt 1 of 5' "$log" \
    && ! grep -q 'attempt 2 of 5' "$log" \
    && [ "$(grep -cx 'Renderer: vulkan' "$log")" = 2 ] \
    && [ "$(grep -cx "Video: SDL's x11 driver, the vulkan renderer" "$log")" = 2 ] \
    && grep -qx 'Reload: done at 1920x1080 px, for a device reset or loss' "$log" \
    && d48_same f48-lost && ran_clean f48-lost && ok=0
result "device: a lost device gets a new Vulkan renderer and redraws to the pixel (exit $(cat "$out/f48-lost.code"))" $ok
grep -E '^(Renderer|Reload|Video):' "$log" | sed 's/^/      /'

# With every texture holder on (f48-lostall.ini), the lost device drops each one's textures: the
# three buttons' icons and titles and the highlight (7), the image background (1), its overlay (1),
# the scroll indicators (1), the clock's time and date (2) and the screensaver's layer (1): 13. Each
# exists from before the loop (the clock renders both as it starts), so the count is exact. A holder
# left out would keep a pointer its renderer freed, which only a release build trips on.
d48_lostall() { wait_line 'Begin program loop' "$2" && d48_send lost && wait_line 'Reload: done at 1920x1080 px' "$2"; }
d48_reset_runs f48-lostall
CFG=$FX/f48-lostall.ini STREAMFLEX_TEST_RENDER_EVENT=$D48_EVENT run_keys f48-lostall +d48_lostall
log=$out/f48-lostall.log
ok=1
grep -qx 'Renderer: dropped 13 textures before the renderer went' "$log" \
    && grep -qx 'Reload: done at 1920x1080 px, for a device reset or loss' "$log" \
    && [ "$(grep -cx 'Renderer: vulkan' "$log")" = 2 ] && ran_clean f48-lostall && ok=0
result "device: a lost device drops the textures of every feature that holds one (exit $(cat "$out/f48-lostall.code"))" $ok
grep -E '^(Renderer|Reload):' "$log" | sed 's/^/      /'

# With settings open, a lost device's new renderer makes their textures again, and the page stays.
# Settings' own textures (their text cache and preview) are dropped too, so the count is over the
# launcher's 7; how far over depends on the rows drawn.
d48_lost_settings_after() {
    wait_line 'Reload: done at 1920x1080 px' "$2" && look "$1" "$2" after 'Reload: done at 1920x1080 px' 30,30=30,45,60 @5,1074=11,22,32
}
d48_reset_runs f48-settings-lost
CFG=$FX/f48-loss.ini STREAMFLEX_TEST_RENDER_EVENT=$D48_EVENT run_keys f48-settings-lost Menu Down Return +d48_lost +d48_lost_settings_after Down Menu
log=$out/f48-settings-lost.log
d48_dropped=$(sed -n 's/^Renderer: dropped \([0-9]*\) textures before the renderer went$/\1/p' "$log" | head -1)
ok=1
[ "${d48_dropped:-0}" -gt 7 ] \
    && [ "$(grep -c '^Settings: the preview is at ' "$log")" = 2 ] && grep -qx 'after yes' "$out/f48-settings-lost.seen" \
    && sed -n '/^Reload: done at /,$p' "$log" | grep -q "^Settings: the cursor's row reads Color: " \
    && grep -q 'Settings: nothing changed' "$log" && ran_clean f48-settings-lost && ok=0
result "device: a lost device with settings open makes their textures again and keeps the page (exit $(cat "$out/f48-settings-lost.code"))" $ok
echo "      dropped: ${d48_dropped:-none}"

# An edit made in settings before the device is lost survives the new renderer and its reload, and is
# saved (the keys of 47-screen.sh's f47-settings: Color steps from #000000 to #1E1E1E)
d48_lost_reloaded() { d48_send lost; wait_line 'Reload: done at 1920x1080 px' "$2"; }
d48_reset_runs f48-editlost
cfg=$(writable_config f60-color)
CFG=$cfg STREAMFLEX_TEST_RENDER_EVENT=$D48_EVENT run_keys f48-editlost Menu Down Return Down Right +d48_lost_reloaded BackSpace BackSpace
log=$out/f48-editlost.log
ok=1
precedes "$log" 'Settings: [Background] Color #000000 -> #1E1E1E' 'Renderer: device lost and not recovered' \
    && grep -qx 'Reload: done at 1920x1080 px, for a device reset or loss' "$log" \
    && grep -qx 'Color=#1E1E1E' "$cfg" && ran_clean f48-editlost && ok=0
result "device: an edit made before a lost device is kept through the new renderer and saved (exit $(cat "$out/f48-editlost.code"))" $ok
grep -E '^(Settings: \[Background\]|Renderer: device|Reload:)' "$log" | sed 's/^/      /'

# A GPU that no longer offers Vulkan (the test hook leaves it out of the new renderer's list): the new
# renderer falls down the list to OpenGL, the screen keeps its size (OpenGL replaces the native window:
# no size change, no second Screen: line), and the redraw is pixel-correct
d48_reset_runs f48-skip
CFG=$FX/f48-loss.ini STREAMFLEX_TEST_RENDER_EVENT=$D48_EVENT STREAMFLEX_TEST_RENDERER_SKIP=vulkan \
    run_keys f48-skip +d48_before +d48_lost +d48_after
log=$out/f48-skip.log
ok=1
[ "$(grep -E '^Renderer: [a-z0-9]+( \([^)]*\))?$' "$log" | tr '\n' '|')" = 'Renderer: vulkan|Renderer: opengl (vulkan was unavailable)|' ] \
    && [ "$(grep -c '^Screen: ' "$log")" = 1 ] && d48_same f48-skip && ran_clean f48-skip && ok=0
result "device: a new renderer falls back to OpenGL when Vulkan is gone, at the same size (exit $(cat "$out/f48-skip.code"))" $ok
grep -E '^(Renderer|Screen):' "$log" | sed 's/^/      /'

# A device lost while an app runs: nothing more until the app is done, then the new renderer
d48_lost_seen() { d48_send lost; wait_line 'Renderer: device lost and not recovered' "$2"; sleep 3; }
d48_reset_runs f48-applost
CFG=$FX/f48-loss.ini STREAMFLEX_TEST_APP=$D48_APP STREAMFLEX_TEST_RENDER_EVENT=$D48_EVENT \
    UNTIL='Reload: done at 1920x1080 px' run_keys f48-applost +d48_app_in +d48_lost_seen +d48_app_out
log=$out/f48-applost.log
d48_finished=$(d48_at f48-applost 'Application finished')
d48_attempt=$(d48_at f48-applost 'Renderer: creating a new renderer, attempt 1 of 5')
ok=1
[ -n "$d48_finished" ] && [ -n "$d48_attempt" ] && [ "$d48_finished" -lt "$d48_attempt" ] \
    && ! in_range "$log" 'Renderer: device lost and not recovered' 'Application finished' 'creating a new renderer, attempt' \
    && [ "$(grep -cx 'Renderer: vulkan' "$log")" = 2 ] && ran_clean f48-applost && ok=0
result "device: a device lost while an app runs gets its new renderer once the app is done (exit $(cat "$out/f48-applost.code"))" $ok
grep -E '^(Renderer|Application|Reload):' "$log" | sed 's/^/      /'

# Every attempt fails: after the fifth, the startup error path: SDL's reason, then exit 1
d48_reset_runs f48-giveup
CFG=$FX/f48-loss.ini STREAMFLEX_TEST_RENDER_EVENT=$D48_EVENT STREAMFLEX_TEST_RENDERER_FAIL=5 STREAMFLEX_TEST_NO_MESSAGE_BOX=1 \
    UNTIL='Quitting program' run_keys f48-giveup +d48_lost
log=$out/f48-giveup.log
ok=1
grep -qx 'Renderer: creating a new renderer, attempt 5 of 5' "$log" && ! grep -q 'attempt 6 of 5' "$log" \
    && [ "$(grep -cx 'Could not initialize renderer' "$log")" = 5 ] && ran_clean f48-giveup 1 && ok=0
result "device: five failed attempts take the startup error path, exit 1 (exit $(cat "$out/f48-giveup.code"))" $ok

# A fallback title is lit again after a lost device's reload (spec section 9). It needs Task 1.16's
# fallback-font fixture, the fontconfig file run.sh makes whose only font is sf-box.ttf (filled
# squares for U+4E2D and others); until that task lands this check does not run, and says so on the
# console with no result line. Task 1.16 deletes the guard. The title is U+4E2D, which only the box
# font has; its glyph is a filled box in the title's magenta, so the box it spans is nearly all
# magenta, before the loss and after.
D48_FONTS=$TESTER_HOME/fcbox/fonts.conf
if [ -f "$D48_FONTS" ]; then
    d48_reset_runs f48-fallback
    FONTCONFIG_FILE=$D48_FONTS CFG=$FX/f48-fallback.ini STREAMFLEX_TEST_RENDER_EVENT=$D48_EVENT \
        run_keys f48-fallback +d48_before +d48_lost +d48_after
    read -r d48_x0 d48_y0 d48_x1 d48_y1 d48_n <<< "$(d48_boxline f48-fallback after | sed 's/box \([0-9]*\),\([0-9]*\) to \([0-9]*\),\([0-9]*\): \([0-9]*\) .*/\1 \2 \3 \4 \5/')"
    ok=1
    d48_same f48-fallback && [ -n "${d48_n:-}" ] \
        && [ "$((2 * d48_n))" -ge "$(((d48_x1 - d48_x0 + 1) * (d48_y1 - d48_y0 + 1)))" ] \
        && ran_clean f48-fallback && ok=0
    result "device: a fallback title is lit again after a lost device's reload (exit $(cat "$out/f48-fallback.code"))" $ok
    sed 's/^/      /' "$out/f48-fallback.boxes" 2> /dev/null
else
    echo "      device: the fallback title's redraw waits for Task 1.16's $D48_FONTS"
fi
