# Device loss's timing-sensitive cases, from 48-device-loss.sh: the attempts at a new renderer after a
# lost device come 2 s apart, and these cases race that pace. The lost device is posted by the render
# event hook (STREAMFLEX_TEST_RENDER_EVENT) and the attempts fail by STREAMFLEX_TEST_RENDERER_FAIL.
# Timing-sensitive: a key and a click must land inside the 4 s before the third attempt makes the renderer, and a quit must end StreamFlex within 1000 ms of its TERM
T48_EVENT=/tmp/t48-event
# A function to put the render events WORDS in the hook's file in one step (a rename), as
# 48-device-loss.sh's d48_send does
t48_send() { printf '%s\n' "$1" > "$T48_EVENT.new" && mv -f "$T48_EVENT.new" "$T48_EVENT"; }
# The background's corner and the box the highlight's #FF00FF spans, once the frame is on screen, kept
# under TAG; t48_same says both match before and after, and that the shot before found the box (as
# 48-device-loss.sh's d48_box and d48_same, which may run in the other set)
t48_box() {
    local name=$1 tag=$2 shot=/tmp/t48.xwd
    sleep 1
    screen_shows "$name" "$tag" 10,10=30,45,60 && echo "$tag corner yes" >> "$out/$name.seen"
    xwd -root -silent -out "$shot" && python3 "$HERE/pixels.py" "$shot" count 255,0,255 inside 255,0,255 \
        | sed "s/^/$tag /" >> "$out/$name.boxes"
}
t48_boxline() { sed -n "s/^$2 //p" "$out/$1.boxes" 2> /dev/null | head -1; }
t48_same() {
    case $(t48_boxline "$1" before) in
        'box '*) ;;
        *) return 1 ;;
    esac
    [ "$(t48_boxline "$1" before)" = "$(t48_boxline "$1" after)" ] \
        && grep -qx 'before corner yes' "$out/$1.seen" && grep -qx 'after corner yes' "$out/$1.seen"
}
t48_before() { wait_line 'Begin program loop' "$2" && t48_box "$1" before; }
t48_after() { wait_line 'Reload: done at 1920x1080 px' "$2" && t48_box "$1" after; }
t48_reset_runs() { rm -f "$T48_EVENT" "$T48_EVENT.new" "$out/$1.boxes" "$out/$1.seen"; }

# The first two attempts fail (the test hook): the third, 4 s later, makes the renderer. A key pressed
# while there is none is ignored, though its entry's command is :quit, and its frame is the first drawn
# once there is a renderer again: nothing is drawn before attempt 3. A click then is ignored too, though
# MouseSelect is on (f48-retry.ini) and would run the same :quit; it logs nothing, so its proof is that
# StreamFlex quits only after attempt 3. The click goes to xdotool itself: it is no key, so no frame
# is waited for.
t48_lost_waiting() { t48_send lost; wait_line 'Renderer: creating a new renderer, attempt 1 of 5' "$2"; }
t48_click() { command xdotool click 1; }
t48_reset_runs f48-retry
CFG=$FX/f48-retry.ini STREAMFLEX_TEST_RENDER_EVENT=$T48_EVENT STREAMFLEX_TEST_RENDERER_FAIL=2 \
    run_keys f48-retry +t48_before +t48_lost_waiting '!Return' +t48_click +t48_after
log=$out/f48-retry.log
ok=1
grep -qx 'Renderer: creating a new renderer, attempt 3 of 5' "$log" && ! grep -q 'attempt 4 of 5' "$log" \
    && [ "$(grep -cx 'Could not initialize renderer' "$log")" = 2 ] \
    && grep -qx 'Key ignored: there is no renderer yet' "$log" \
    && precedes "$log" 'Renderer: creating a new renderer, attempt 3 of 5' 'Quitting program' \
    && precedes "$log" 'Renderer: creating a new renderer, attempt 3 of 5' 'Test hook: a frame was drawn after a key' \
    && t48_same f48-retry && ran_clean f48-retry && ok=0
result "device: two failed attempts, then the third makes the renderer, and a key meanwhile is ignored (exit $(cat "$out/f48-retry.code"))" $ok
grep -E '^(Renderer|Key ignored|Could not initialize)' "$log" | sed 's/^/      /'

# Between the attempts the loop keeps pumping events: a quit (TERM, which SDL turns into a quit event)
# sent right after the second attempt ends StreamFlex at once, well inside the 2 s before the third is
# due. The quit is timed: a loop that blocked through the wait would also quit before attempt 3, only
# up to 2 s late. The loop sleeps 100 ms a pass while there is no renderer, so 1000 ms is ample.
t48_quit_timed() {
    local name=$1 pid=$2 i
    t48_send lost
    wait_line 'Renderer: creating a new renderer, attempt 2 of 5' "$pid" || return
    date +%s%N > "$out/$name.term"
    kill -TERM "$pid"
    for i in $(seq 200); do
        grep -q '^Quitting program' "$LOG" 2> /dev/null && break
        running "$pid" || break
        sleep 0.05
    done
    grep -q '^Quitting program' "$LOG" 2> /dev/null && date +%s%N > "$out/$name.quit"
}
t48_reset_runs f48-quitlive
rm -f "$out/f48-quitlive.term" "$out/f48-quitlive.quit"
CFG=$FX/f48-loss.ini STREAMFLEX_TEST_RENDER_EVENT=$T48_EVENT STREAMFLEX_TEST_RENDERER_FAIL=5 \
    run_keys f48-quitlive +t48_quit_timed
log=$out/f48-quitlive.log
t48_gap=$(( ($(cat "$out/f48-quitlive.quit" 2> /dev/null || echo 0) - $(cat "$out/f48-quitlive.term" 2> /dev/null || echo 0)) / 1000000 ))
ok=1
grep -qx 'Renderer: creating a new renderer, attempt 2 of 5' "$log" && ! grep -q 'attempt 3 of 5' "$log" \
    && precedes "$log" 'attempt 2 of 5' 'Quitting program' && [ -s "$out/f48-quitlive.quit" ] \
    && [ "$t48_gap" -ge 0 ] && [ "$t48_gap" -lt 1000 ] && ran_clean f48-quitlive && ok=0
result "device: the quit path stays live between the attempts, the quit within 1 s (exit $(cat "$out/f48-quitlive.code"))" $ok
echo "      TERM to 'Quitting program': $t48_gap ms"
