# Device loss (spec section 3): SDL's three render events, posted by a test hook
# (STREAMFLEX_TEST_RENDER_EVENT), since no software renderer sends them. A target or device reset runs
# the full reload a config change runs, once per batch of events, after they are drained: every
# texture made again, the menu and the screen kept, and the screen redrawn to the pixel. With
# settings open, their textures are made again and the page stays. While an app holds a size change
# back, a reset rebuilds the textures at the geometry in force; the size waits for the app.
# Not timing-sensitive: every wait ends on a log line or a bounded screen poll, the size and the reset that must share a batch are written while the launcher is stopped, and no case races a deadline
D48_EVENT=/tmp/d48-event D48_SIZE=/tmp/d48-size D48_APP=/tmp/d48-app
# A function to put the render events WORDS in the hook's file in one step (a rename), so the hook
# never reads half of them
d48_send() { printf '%s\n' "$1" > "$D48_EVENT.new" && mv -f "$D48_EVENT.new" "$D48_EVENT"; }
# The background's corner and the box the highlight's #FF00FF spans, once the frame is on screen,
# kept under TAG; d48_same says both match before and after
d48_box() {
    local name=$1 tag=$2 shot=/tmp/d48.xwd
    sleep 1
    screen_shows "$name" "$tag" 10,10=30,45,60 && echo "$tag corner yes" >> "$out/$name.seen"
    xwd -root -silent -out "$shot" && python3 "$HERE/pixels.py" "$shot" count 255,0,255 inside 255,0,255 \
        | sed "s/^/$tag /" >> "$out/$name.boxes"
}
d48_boxline() { sed -n "s/^$2 //p" "$out/$1.boxes" 2> /dev/null | head -1; }
d48_same() {
    [ -n "$(d48_boxline "$1" before)" ] && [ "$(d48_boxline "$1" before)" = "$(d48_boxline "$1" after)" ] \
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
# the leak pass's retry puts back as they were before the run.
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
