# The low-battery glyph (spec section 7). The harness build's STREAMFLEX_TEST_PAD_POWER file holds
# one line, "<state> <percent>", with an optional pad letter in front, and each new line is pushed
# as the battery event a real pad sends, for the first pad StreamFlex holds (A) or the second (B).
# The rules:
# - the glyph shows only for a pad on its battery at or below 40 % (amber), red at or below 15 %;
# - once shown, it hides again only above 45 %;
# - it takes the top corner opposite the clock's alignment, at the clock's margin, on or off;
# - it is never drawn under the settings screen.
# This proves StreamFlex's handling and the glyph's levels, colors and corner, not that SDL's drivers
# read a real pad: only hands-on hardware can.
# Not timing-sensitive: each step waits for its own log line (20 s), each screen read retries for 10 s, and what must not happen after a report is read from the log's order (the launcher handles a pushed report and draws its frame in one pass of its loop), never from a sleep
F66_AMBER=255,176,0
F66_RED=232,32,32
F66_BLACK=0,0,0
f66_at=1830

# A function to write the power file's line whole: the launcher reads the file once a frame
f66_write() { printf '%s\n' "$1" > /tmp/f66-power.tmp && mv -f /tmp/f66-power.tmp /tmp/f66-power; }

# A function to count the "Gamepad power changed" lines in the launcher's log so far. A step counts
# from the log, not from a counter of its own: the leak pass may run a run again from its start
# (with_tracer_retry), with a new log and the same +keys.
f66_changes() {
    local n
    n=$(grep -c '^Gamepad power changed: ' "$LOG" 2> /dev/null)
    echo "${n:-0}"
}

# A function to wait up to 20 s for the log to hold N lines holding LINE (PID: the launcher)
f66_wait_count() {
    local line=$1 n=$2 pid=$3 i c
    for i in $(seq 100); do
        c=$(grep -cF -- "$line" "$LOG" 2> /dev/null)
        [ "${c:-0}" -ge "$n" ] && return 0
        running "$pid" || break
        sleep 0.2
    done
    c=$(grep -cF -- "$line" "$LOG" 2> /dev/null)
    [ "${c:-0}" -ge "$n" ]
}

# A function to read the screen at the glyph's left edge and inside its charge bar, for the glyph at
# X,54 18 px tall, and write "TAG yes" to NAME.seen when both show COLOR, else "TAG no"
f66_look() {
    local name=$1 tag=$2 x=$3 color=$4 t=2 mid=63
    if screen_shows "$name" "$tag" "$x,$mid=$color" "$((x + 2 * t + 1)),$mid=$color"; then
        echo "$tag yes" >> "$out/$name.seen"
    else
        echo "$tag no" >> "$out/$name.seen"
    fi
}

# A function for one step of a run (NAME, PID, TAG, the file's LINE, the COLOR then expected at the
# glyph's place): write the line, wait for the launcher's next "Gamepad power changed" line and a
# second more for the frame that draws it, then look
f66_step() {
    local name=$1 pid=$2 tag=$3 line=$4 color=$5 before
    before=$(f66_changes)
    f66_write "$line"
    if ! f66_wait_count 'Gamepad power changed: ' "$((before + 1))" "$pid"; then
        echo "$tag: the launcher never took '$line'" >> "$out/$name.pixels"
        echo "$tag no" >> "$out/$name.seen"
        return
    fi
    sleep 1
    f66_look "$name" "$tag" "$f66_at" "$color"
}

# A function to tell whether the log FILE has a line holding FROM, a line holding TO after it, and
# no line starting with BAD between the two (fixed strings, passed to awk as in_range passes them).
# The launcher logs a report's change and then draws the frame that shows it, in one pass of its
# loop, before it reads the power file again, so a glyph a report shows or hides is logged before
# the next report's line.
f66_none_between() {
    from=$2 to=$3 bad=$4 awk '
        !open && index($0, ENVIRON["from"]) { open = 1; next }
        open && index($0, ENVIRON["bad"]) == 1 { found = 1 }
        open && index($0, ENVIRON["to"]) { closed = 1; exit }
        END { exit !(closed && !found) }
    ' "$1"
}

# The steps, as +keys (see run_keys). The first reads the screen a second after the loop begins, so
# a window not yet drawn, black too, cannot pass for one drawn with no glyph.
f66_none() {
    wait_line 'Begin program loop' "$2" || { echo "none no" >> "$out/$1.seen"; return; }
    sleep 1
    f66_look "$1" none "$f66_at" "$F66_BLACK"
}
f66_low()      { f66_step "$1" "$2" low 'battery 40' "$F66_AMBER"; }
f66_critical() { f66_step "$1" "$2" critical 'battery 10' "$F66_RED"; }
f66_full()     { f66_step "$1" "$2" full 'battery 100' "$F66_BLACK"; }
f66_charging() { f66_step "$1" "$2" charging 'charging 20' "$F66_BLACK"; }
f66_charged()  { f66_step "$1" "$2" charged 'charged 100' "$F66_BLACK"; }
f66_unknown()  { f66_step "$1" "$2" unknown 'unknown -1' "$F66_BLACK"; }
f66_again()    { f66_step "$1" "$2" again 'battery 40' "$F66_AMBER"; }
f66_hold()     { f66_step "$1" "$2" hold 'battery 44' "$F66_AMBER"; }
f66_hide()     { f66_step "$1" "$2" hide 'battery 46' "$F66_BLACK"; }
# The same report again, for pad A by its letter: the hook pushes it, and nothing changes or is
# logged. A report of 47 follows it, and once that is logged the 46 before it was handled too.
f66_same() {
    local before
    before=$(f66_changes)
    f66_write 'A battery 46'
    if f66_wait_count "Test hook: pad A's power is battery 46" 2 "$2" && f66_write 'battery 47' \
        && wait_line ', on battery, 47%' "$2" && [ "$(f66_changes)" = "$((before + 1))" ]; then
        echo "same yes" >> "$out/$1.seen"
    else
        echo "same no" >> "$out/$1.seen"
    fi
}

# One pad, the clock off and aligned left: the glyph's levels, colors and edges, top right
rm -f /tmp/f66-power /tmp/f66-power.tmp /tmp/pad-none
f66_at=1830
STREAMFLEX_TEST_PAD=/tmp/pad-none STREAMFLEX_TEST_PAD_POWER=/tmp/f66-power WAIT_FOR='Gamepad power: ' \
    run_keys f66-power +f66_none +f66_low +f66_critical +f66_full +f66_charging +f66_charged +f66_unknown \
    +f66_again +f66_hold +f66_hide +f66_same
log=$out/f66-power.log
seen=$out/f66-power.seen
indicator=$(grep '^Battery indicator: ' "$log" | tr '\n' '|')
ok=1
grep -qxE 'Gamepad power: instance id [0-9]+, unknown, unknown, no percent' "$log" \
    && grep -qx 'none yes' "$seen" && ran_clean f66-power && ok=0
result "battery: a pad that reports nothing logs unknown power and draws no glyph (exit $(cat "$out/f66-power.code"))" $ok
ok=1
case $indicator in
    'Battery indicator: shown, low, 40% at 1830,54 36x18|Battery indicator: shown, critical, 10% at 1830,54 36x18|Battery indicator: hidden|'*) ok=0 ;;
esac
[ $ok = 0 ] && grep -qx 'low yes' "$seen" && grep -qx 'critical yes' "$seen" && grep -qx 'full yes' "$seen" || ok=1
result "battery: on battery at 40% the glyph is amber, at 10% red, at 100% gone, top right at the clock's margin" $ok
echo "      ${indicator:-no indicator lines}"
ok=1
grep -qx 'charging yes' "$seen" && grep -qx 'charged yes' "$seen" && grep -qx 'unknown yes' "$seen" \
    && f66_none_between "$log" ', charging, 20%' ', on battery, 40%' 'Battery indicator: shown' && ok=0
result "battery: a charging, charged or unknown pad draws nothing" $ok
ok=1
[ "$indicator" = 'Battery indicator: shown, low, 40% at 1830,54 36x18|Battery indicator: shown, critical, 10% at 1830,54 36x18|Battery indicator: hidden|Battery indicator: shown, low, 40% at 1830,54 36x18|Battery indicator: hidden|' ] \
    && grep -qx 'again yes' "$seen" && grep -qx 'hold yes' "$seen" && grep -qx 'hide yes' "$seen" \
    && f66_none_between "$log" ', on battery, 44%' ', on battery, 46%' 'Battery indicator: hidden' && ok=0
result "battery: 44% after 40% keeps the glyph, and 46% hides it" $ok
ok=0
for want in 'on battery, 40%' 'on battery, 10%' 'on battery, 100%' 'charging, 20%' 'charged, 100%' \
            'unknown, no percent' 'on battery, 44%' 'on battery, 46%'; do
    grep -qE "^Gamepad power changed: instance id [0-9]+, unknown, $want\$" "$log" || { ok=1; echo "      no change to '$want' logged"; }
done
grep -qx 'same yes' "$seen" || ok=1
result "battery: each report is logged with the pad's state and percent, and one that changes nothing is not" $ok
sed 's/^/      /' "$seen" 2> /dev/null

# Two pads: the lower is drawn, and pulling it out hides the glyph when the other is charging
f66_plugged()  { f66_wait_count 'Gamepad power: ' 2 "$2" || echo "plug: the second pad never opened" >> "$out/$1.pixels"; }
f66_a_low()    { f66_step "$1" "$2" a-low 'A battery 40' "$F66_AMBER"; }
f66_b_low()    { f66_step "$1" "$2" b-critical 'B battery 10' "$F66_RED"; }
f66_a_charge() { f66_step "$1" "$2" a-charging 'A charging 50' "$F66_RED"; }
f66_unplug()   { rm -f /tmp/pad-plug; wait_line 'Battery indicator: hidden' "$2"; sleep 1; f66_look "$1" unplugged "$f66_at" "$F66_BLACK"; }
rm -f /tmp/f66-power /tmp/pad-none
: > /tmp/pad-plug
chmod 666 /tmp/pad-plug
f66_at=1830
STREAMFLEX_TEST_PAD=/tmp/pad-none STREAMFLEX_TEST_PAD_PLUG=/tmp/pad-plug STREAMFLEX_TEST_PAD_POWER=/tmp/f66-power \
    CFG=$FX/f66-power.ini WAIT_FOR='Test hook: pad plugged in' \
    run_keys f66-two +f66_plugged +f66_a_low +f66_b_low +f66_a_charge +f66_unplug
seen=$out/f66-two.seen
ok=1
[ "$(grep '^Battery indicator: ' "$out/f66-two.log" | tr '\n' '|')" = 'Battery indicator: shown, low, 40% at 1830,54 36x18|Battery indicator: shown, critical, 10% at 1830,54 36x18|Battery indicator: hidden|' ] \
    && grep -qx 'a-low yes' "$seen" && grep -qx 'b-critical yes' "$seen" && grep -qx 'a-charging yes' "$seen" \
    && grep -qx 'unplugged yes' "$seen" && ran_clean f66-two && ok=0
result "battery: with two pads the lower is drawn, and pulling it out hides the glyph (exit $(cat "$out/f66-two.code"))" $ok
sed 's/^/      /' "$seen" 2> /dev/null
rm -f /tmp/pad-plug

# The clock on and aligned right: the glyph takes the top left corner
f66_right() { f66_step "$1" "$2" right-low 'battery 40' "$F66_AMBER"; }
rm -f /tmp/f66-power
f66_at=54
STREAMFLEX_TEST_PAD=/tmp/pad-none STREAMFLEX_TEST_PAD_POWER=/tmp/f66-power CFG=$FX/f66-right.ini WAIT_FOR='Gamepad power: ' \
    run_keys f66-right +f66_right
ok=1
grep -qx 'Battery indicator: shown, low, 40% at 54,54 36x18' "$out/f66-right.log" \
    && grep -qx 'right-low yes' "$out/f66-right.seen" && ran_clean f66-right && ok=0
result "battery: with the clock on the right the glyph takes the top left corner (exit $(cat "$out/f66-right.code"))" $ok

# Settings open over a shown glyph: with no render targets the scene fills the screen behind the
# settings (a test hook), drawn as the preview is, and the glyph's place there stays black. Settings
# that never opened read "no": a Menu key that quit would leave a black screen too.
f66_preview() {
    wait_line 'Settings opened over menu' "$2" || { echo "preview no" >> "$out/$1.seen"; return; }
    sleep 1
    f66_look "$1" preview "$f66_at" "$F66_BLACK"
}
rm -f /tmp/f66-power
f66_at=1830
STREAMFLEX_TEST_PAD=/tmp/pad-none STREAMFLEX_TEST_PAD_POWER=/tmp/f66-power STREAMFLEX_TEST_NO_RENDER_TARGETS=1 \
    CFG=$FX/f66-power.ini WAIT_FOR='Gamepad power: ' run_keys f66-preview +f66_critical Menu +f66_preview
ok=1
grep -qx 'critical yes' "$out/f66-preview.seen" && grep -qx 'preview yes' "$out/f66-preview.seen" \
    && ran_clean f66-preview && ok=0
result "battery: the glyph is never drawn under the settings screen (exit $(cat "$out/f66-preview.code"))" $ok
rm -f /tmp/f66-power /tmp/f66-power.tmp
