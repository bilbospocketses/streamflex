# The pages of every section (3b): every page opened and every row stepped, every feature switched
# off and on (the leak pass proves each stop frees what its start made), the clock stopped in the
# middle of a render, VSync and the FPS limit live, the default menu, the dim preview, and values in
# new forms saved untouched

ARROW=$(printf ' \xE2\x80\xBA ')

# A function to make the keys that walk one page: each row stepped right, left, left and right (an
# on/off that is on ends on; one that is off ends on too, which opens the rows it greys), then down;
# 14 times, more rows than any page has
walk() { local i; for i in $(seq 14); do printf 'Right Left Left Right Down '; done; }
tour() {
    local keys="Return $(walk) BackSpace Down"          # General
    keys="$keys Return $(walk) BackSpace Down"           # Background
    keys="$keys Return Return $(walk) BackSpace BackSpace Down"   # Menus, All menus
    local page
    for page in Titles Highlight Scroll Clock Screensaver; do
        keys="$keys Return $(walk) BackSpace Down"
    done
    keys="$keys Return Return $(walk) BackSpace Down Return $(walk) BackSpace BackSpace Down"   # Controls, Keyboard, Gamepad
    keys="$keys Return BackSpace"                        # Discard, then close with nothing to save
    # shellcheck disable=SC2086
    xdotool key --delay 100 $keys
    sleep 3
}
cfg=$(writable_config f55-tour)
CFG=$cfg UNTIL='Settings closed' run_keys f55-tour Menu +tour
log=$out/f55-tour.log
ok=1
for page in General Background Menus "Menus${ARROW}All menus" Titles Highlight "Scroll indicators" Clock \
            Screensaver Controls "Controls${ARROW}Keyboard" "Controls${ARROW}Gamepad"; do
    grep -qF "Settings: page Settings${ARROW}${page}" "$log" || { echo "      page never opened: $page"; ok=2; }
done
[ "$ok" = 1 ] && [ "$(grep -c 'Settings: \[' "$log")" -ge 40 ] && grep -q 'Settings: discarded the changes' "$log" \
    && grep -q 'Settings: nothing changed' "$log" && cmp -s "$FX/f55-tour.ini" "$cfg" && ran_clean f55-tour && ok=0
result "settings: every page opens, 40+ rows change, Discard restores, the file is untouched (exit $(cat "$out/f55-tour.code"))" $ok
echo "      $(grep -c 'Settings: \[' "$log") changes"

# The same run switched every feature off and on: each stopped and started again at least once
ok=0
for feature in Overlay Highlight 'Scroll indicators' Clock Screensaver Gamepad; do
    stops=$(sed -n '/Settings opened/,/Settings closed/p' "$log" | grep -c "^$feature stopped")
    starts=$(sed -n '/Settings opened/,/Settings closed/p' "$log" | grep -c "^$feature started")
    [ "$stops" -ge 1 ] && [ "$starts" -ge 1 ] || { echo "      $feature: $stops stops, $starts starts"; ok=1; }
done
result "settings: every feature stops and starts again live" $ok

# The same run stepped Show titles (the title font's group) off and on: the title font opened again
# each time, live
opened=$(sed -n '/Settings opened/,/Settings closed/p' "$log" | grep -c '^Titles: opened')
ok=1
[ "$opened" -ge 2 ] && ok=0
result "settings: Show titles off and on opens the title font again, live" $ok
echo "      the title font opened $opened times"

# A greyed row's reason shows under the preview: the FPS limit while VSync is on. A run of its own,
# whose cursor rests on the row: the tour's keys come 100 ms apart, and a slow host draws no frame
# while the cursor is there, so the note is never logged. VSync is on in the file, so no notice is
# up to take the note's place.
CFG=$FX/f55-why.ini UNTIL='Settings: the note under the preview says Used only while VSync is off' \
    run_keys f55-why Menu Return Down Down Down Down Down Down
ok=1
grep -q 'Settings: the note under the preview says Used only while VSync is off' "$out/f55-why.log" \
    && ran_clean f55-why && ok=0
result "settings: a greyed row says why under the preview (exit $(cat "$out/f55-why.code"))" $ok

# The clock switched off while its render thread is at work (the harness build's
# STREAMFLEX_TEST_CLOCK_DELAY_MS makes each render take 3 s, and one every second): stopping waits
# for the thread and frees what it made, and Enabled=false is saved. The wait is looked for while
# settings are open: quitting with the clock still running logs the same line.
render_then_off() {
    local before i
    before=$(grep -c 'Clock: rendering on its thread' "$LOG")
    for i in $(seq 50); do
        [ "$(grep -c 'Clock: rendering on its thread' "$LOG")" -gt "$before" ] && break
        sleep 0.2
    done
    xdotool key Left
    sleep 4
}
cfg=$(writable_config f55-clock)
STREAMFLEX_TEST_CLOCK_DELAY_MS=3000 CFG=$cfg UNTIL='Settings saved' \
    run_keys f55-clock Menu Down Down Down Down Down Down Return +render_then_off BackSpace BackSpace
ok=1
in_range "$out/f55-clock.log" 'Settings opened' 'Settings closed' 'Clock stopped (it waited for a render in progress)' \
    && sed -n '/^\[Clock\]/,/^\[/p' "$cfg" | grep -qx 'Enabled=false' && ran_clean f55-clock && ok=0
result "settings: the clock switched off mid-render waits for it and frees it (exit $(cat "$out/f55-clock.code"))" $ok

# VSync off, then an FPS limit of 30: the frame timing follows live, and both are saved
cfg=$(writable_config f55-frame)
CFG=$cfg run_keys f55-frame Menu Return Down Down Down Down Down Left Down Right BackSpace BackSpace
ok=1
grep -q 'Frame timing: FPS limit 30, 33 ms a frame' "$out/f55-frame.log" \
    && grep -qx 'VSync=false' "$cfg" && grep -qx 'FPSLimit=30' "$cfg" && ran_clean f55-frame && ok=0
result "settings: VSync and the FPS limit apply live and save (exit $(cat "$out/f55-frame.code"))" $ok
# Between the two, VSync is off with no FPS limit yet: frames keep the display's rate (Xvfb's 0 Hz
# reads as 60), and the log says so rather than calling it VSync
ok=1
in_range "$out/f55-frame.log" 'Settings: [General] VSync true -> false' 'Frame timing: FPS limit 30' \
        "Frame timing: VSync is off, but no FPS limit is set, so frames keep the display's 60 Hz, 16 ms a frame" \
    && ! in_range "$out/f55-frame.log" 'Settings: [General] VSync true -> false' 'Frame timing: FPS limit 30' 'Frame timing: VSync at' \
    && ran_clean f55-frame && ok=0
result "settings: VSync off with no FPS limit logs that frames keep the display's rate (exit $(cat "$out/f55-frame.code"))" $ok
grep -E 'Frame timing|VSync' "$out/f55-frame.log" | sed 's/^/      /'

# The pad's D-pad Left held on the FPS limit from 75: its first press steps to 60, and its first
# repeat (31 frames of 16 ms) to 30, whose 33 ms frames make the repeat's delay 15 frames, which the
# held count has passed already; the repeats go on all the same, on to Off. Left is held for 40
# frames however slowly they run (STREAMFLEX_TEST_PAD_FRAMES), and the step to Off is waited for.
p55_hold_left() {
    : > /tmp/pad-left; wait_line 'Settings: [General] FPSLimit 30 -> ' "$2"
    rm -f /tmp/pad-left; sleep 0.5
}
rm -f /tmp/pad-left
STREAMFLEX_TEST_PAD=/tmp/pad-left STREAMFLEX_TEST_PAD_BUTTON=dpleft STREAMFLEX_TEST_PAD_FRAMES=40 WAIT_FOR='Gamepad connected' \
    CFG=$FX/f55-padfps.ini run_keys f55-padfps Menu Return Down Down Down Down Down Down +p55_hold_left Menu
log=$out/f55-padfps.log
ok=1
precedes "$log" 'Settings: [General] FPSLimit 75 -> 60' 'Settings: [General] FPSLimit 60 -> 30' \
    && precedes "$log" 'Settings: [General] FPSLimit 60 -> 30' 'Settings: [General] FPSLimit 30 -> ' \
    && [ "$(grep -c 'Gamepad ButtonDPadLeft detected' "$log")" = 1 ] && ran_clean f55-padfps && ok=0
result "settings: a held pad's repeats go on past a step that shortens their delay (exit $(cat "$out/f55-padfps.code"))" $ok
grep -E 'FPSLimit|Frame timing' "$log" | sed 's/^/      /'
rm -f /tmp/pad-left

# Only VSync off with no FPS limit logs frames keeping the display's rate: VSync on (f55-frame as it
# starts) and VSync off with a limit above the display's rate (f55-padfps's 75, as it starts) are
# still VSync at that rate
ok=1
[ "$(grep -m1 '^Frame timing: ' "$out/f55-frame.log")" = 'Frame timing: VSync at 60 Hz, 16 ms a frame' ] \
    && [ "$(grep -m1 '^Frame timing: ' "$out/f55-padfps.log")" = 'Frame timing: VSync at 60 Hz, 16 ms a frame' ] && ok=0
result "settings: VSync on, or a limit above the display's rate, still logs VSync at the display's rate" $ok

# The default menu changed to Games: :home (a Home hotkey) closes settings to Games, and it saves
cfg=$(writable_config f55-default)
CFG=$cfg run_keys f55-default Menu Return Right Home
ok=1
sed -n '/Key Home (#4000004A) detected/,$p' "$out/f55-default.log" | grep -q "Loading menu 'Games'" \
    && grep -qx 'DefaultMenu=Games' "$cfg" && ran_clean f55-default && ok=0
result "settings: a new default menu is where :home goes, at once (exit $(cat "$out/f55-default.code"))" $ok

# While the Screensaver page is open, the preview shows the dim level: white under a 50% black
shows_dim() { look "$1" "$2" dim "Settings: page Settings${ARROW}Screensaver" 30,30=128,128,128; }
CFG=$FX/f55-dim.ini run_keys f55-dim Menu Down Down Down Down Down Down Down Return +shows_dim Menu
ok=1
grep -qx 'dim yes' "$out/f55-dim.seen" && ran_clean f55-dim && ok=0
result "settings: the Screensaver page's preview shows the dim level (exit $(cat "$out/f55-dim.code"))" $ok

# The same without render targets, where the scene fills the screen behind the column: a point of the
# screen right of the column, above the menu, shows the dim too
shows_dim_behind() { look "$1" "$2" dim "Settings: page Settings${ARROW}Screensaver" @1500,100=128,128,128; }
STREAMFLEX_TEST_NO_RENDER_TARGETS=1 CFG=$FX/f55-dim.ini \
    run_keys f55-dimflat Menu Down Down Down Down Down Down Down Return +shows_dim_behind Menu
ok=1
grep -q 'Settings: the renderer has no render targets' "$out/f55-dimflat.log" \
    && grep -qx 'dim yes' "$out/f55-dimflat.seen" && ran_clean f55-dimflat && ok=0
result "settings: without render targets the Screensaver page still shows the dim level (exit $(cat "$out/f55-dimflat.code"))" $ok

# A regression pin for Tasks 1-3's saving (it passes without Task 6's screen code).
# Values in forms the table never read before, or past its steps, survive a save of something else
# byte for byte: Wrap around is changed, and that is the only line that changes. Icon spacing is
# stepped away from its 40 px and back, which changes nothing.
cfg=$(writable_config f55-odd)
CFG=$cfg run_keys f55-odd Menu Return Down Right BackSpace Down Down Return Return Down Down Down Right Left BackSpace BackSpace BackSpace
ok=1
[ "$(changed_lines "$FX/f55-odd.ini" "$cfg")" = 2 ] && grep -qx 'WrapEntries=true' "$cfg" \
    && grep -q 'Settings saved 1 change(s)' "$out/f55-odd.log" && ran_clean f55-odd && ok=0
result "settings: values in new forms are saved untouched when something else changes (exit $(cat "$out/f55-odd.code"))" $ok
diff "$FX/f55-odd.ini" "$cfg" | sed 's/^/      /'

# A regression pin for the layout group (it passes without Task 6's screen code); its teeth come from
# the mutant that drops refresh_layout() from run_refresh(), which leaves the box where it was.
# A new vertical centre moves the grid on screen, not only in the log: the box the highlight's
# colour (#FF00FF, which nothing else on screen has) spans in the preview is lower once Vertical
# centre steps from 50% to 55%, and 55% is saved
highlight_box() {
    local name=$1 pid=$2 tag=$3 line=$4 shot=/tmp/box.xwd
    xdotool search --name '^StreamFlex$' windowmove %@ 0 0 windowsize %@ 1920 1080 > /dev/null 2>&1
    wait_line "$line" "$pid" && sleep 1 && xwd -root -silent -out "$shot" \
        && python3 "$HERE/pixels.py" "$shot" count 255,0,255 inside 255,0,255 | sed "s/^/$tag /" >> "$out/$name.boxes"
}
box_before() { highlight_box "$1" "$2" before 'Settings opened'; }
box_after() { highlight_box "$1" "$2" after 'Settings: [Layout] VCenter'; }
box_top() { sed -n "s/^$1 box [0-9]*,\([0-9]*\) to .*/\1/p" "$out/f55-vcenter.boxes" | head -1; }
cfg=$(writable_config f55-vcenter)
rm -f "$out/f55-vcenter.boxes"
CFG=$cfg run_keys f55-vcenter Menu Down Down Return Return Down Down Down Down +box_before Right +box_after \
    BackSpace BackSpace BackSpace
top_before=$(box_top before)
top_after=$(box_top after)
ok=1
[ -n "$top_before" ] && [ -n "$top_after" ] && [ "$top_after" -gt "$top_before" ] \
    && grep -q 'Settings: \[Layout\] VCenter 50% -> 55%' "$out/f55-vcenter.log" \
    && sed -n '/^\[Layout\]/,/^\[/p' "$cfg" | grep -qx 'VCenter=55%' && ran_clean f55-vcenter && ok=0
result "settings: a new vertical centre moves the grid on screen, and saves (exit $(cat "$out/f55-vcenter.code"))" $ok
sed 's/^/      /' "$out/f55-vcenter.boxes" 2> /dev/null

# The rows drawn follow the model's kinds: under the cursor, a picker row that steps (the default
# menu) shows Left and Right arrows, and one that does not (the startup command) the › marker. On
# the Clock page, off, every row under On is greyed with a reason, so the cursor may rest on each:
# with it on the second row the page has not scrolled to its end, and the caption says why the row
# is greyed.
LEFT_MARK=$(printf '\xE2\x80\xB9')
RIGHT_MARK=$(printf '\xE2\x80\xBA')
CFG=$FX/f55-frame.ini run_keys f55-rows Menu Return Down Down Down Down Down Down Down Down Down BackSpace \
    Down Down Down Down Down Down Return Down Menu
log=$out/f55-rows.log
clock_rows=$(sed -n '/Settings: page Settings.*Clock$/,/Settings closed/p' "$log" | grep -oE 'Settings: rows [0-9]+ to [0-9]+ of 13 on show')
ok=1
grep -qF "Settings: the cursor's row reads Default menu: $LEFT_MARK Main $RIGHT_MARK" "$log" \
    && grep "Settings: the cursor's row reads Startup command: " "$log" | grep -vF "$LEFT_MARK" | grep -qF " $RIGHT_MARK" \
    && [ -n "$clock_rows" ] && ! grep -qv 'rows 0 to' <<< "$clock_rows" \
    && sed -n '/Settings: page Settings.*Clock$/,/Settings closed/p' "$log" \
       | grep -q 'Settings: the note under the preview says The clock is off' \
    && grep -q 'Settings: nothing changed' "$log" && ran_clean f55-rows && ok=0
result "settings: picker rows show their arrows or marker, and greyed rows neither scroll early nor hide why (exit $(cat "$out/f55-rows.code"))" $ok
grep -E "Settings: (the cursor's row reads (Default menu|Startup command|Show date)|rows [0-9]+ to [0-9]+ of 13)" "$log" | sed 's/^/      /'

# The Device row names the pads present (the harness's virtual one): Right steps from Any to it, and
# the row reads its name, never "Pad 0 (not connected)"; Left puts Any back, so nothing is saved
rm -f /tmp/pad-none
STREAMFLEX_TEST_PAD=/tmp/pad-none CFG=$FX/f55-frame.ini WAIT_FOR='Gamepad connected' \
    run_keys f55-pads Menu Down Down Down Down Down Down Down Down Return Down Return Down Right Left Menu
log=$out/f55-pads.log
device=$(grep -F "Settings: the cursor's row reads Device: $LEFT_MARK" "$log" | grep -vF "$LEFT_MARK Any $RIGHT_MARK" | head -1)
ok=1
[ -n "$device" ] && ! grep -q 'not connected' <<< "$device" && grep -q 'Settings: \[Gamepad\] DeviceIndex -1 -> 0' "$log" \
    && grep -q 'Settings: nothing changed' "$log" && ran_clean f55-pads && ok=0
result "settings: the Device row names the pad present (exit $(cat "$out/f55-pads.code"))" $ok
echo "      ${device:-the Device row never named a pad}"

# A pad plugged in while settings are open joins the Device row's list at once, and one pulled out
# leaves it (the harness build's STREAMFLEX_TEST_PAD_PLUG attaches a virtual pad while the file it
# names exists): Right then steps to the new pad and the row names it; pulled out, the row says it is
# not connected. Left puts Any back, so nothing is saved.
plug_in() { : > /tmp/pad-plug; wait_line 'Test hook: pad plugged in' "$2"; sleep 1; }
pull_out() { rm -f /tmp/pad-plug; wait_line 'Test hook: pad unplugged' "$2"; sleep 1; }
rm -f /tmp/pad-plug
STREAMFLEX_TEST_PAD_PLUG=/tmp/pad-plug CFG=$FX/f55-frame.ini \
    run_keys f55-plug Menu Down Down Down Down Down Down Down Down Return Down Return Down +plug_in Right +pull_out Left Menu
log=$out/f55-plug.log
named=$(sed -n '/Test hook: pad plugged in/,/Test hook: pad unplugged/p' "$log" \
        | grep -F "Settings: the cursor's row reads Device: $LEFT_MARK" | grep -vF "$LEFT_MARK Any $RIGHT_MARK" \
        | grep -v 'not connected' | head -1)
ok=1
[ -n "$named" ] && sed -n '/Test hook: pad unplugged/,$p' "$log" \
        | grep -qF "Settings: the cursor's row reads Device: $LEFT_MARK Pad 0 (not connected) $RIGHT_MARK" \
    && grep -q 'Settings: nothing changed' "$log" && ran_clean f55-plug && ok=0
result "settings: a pad plugged in or pulled out while they are open joins or leaves the Device row (exit $(cat "$out/f55-plug.code"))" $ok
grep -E "Test hook: pad|Settings: the cursor's row reads Device" "$log" | sed 's/^/      /'
rm -f /tmp/pad-plug
