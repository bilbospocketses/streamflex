# Startup timing (spec section 8). With -d:
# - each menu render logs one Timing: line beside its "rendered its buttons" line, which stays as it
#   was;
# - a background image, or the slideshow's first image, logs one;
# - the program loop's start logs how long after the window opened the first menu was shown, a span
#   holding the background's load and the first menu's render, so it is no less than their two totals
#   less 2 ms of rounding.
# Every figure is a whole number of milliseconds. A line's parts add up to no more than its total
# plus 2 ms, the rounding of up to four parts and the total. A menu's icons decode on 1 to 4 threads
# (Task 1.13), logged once: its texture is still a share of its total, but its read and decode are
# the work done on every thread, so together they are held to the total times the threads. No
# absolute time is asserted: ASan and lavapipe distort it. The icons are one 512 px PNG drawn 48
# times in each of two menus, so decoding takes long enough for a part counted twice to show in a sum.
# The highlighted entry draws its _selected icon, and the entry beside it its own.
# The image formats are primed on the main thread before anything decodes (SDL_image sets each
# format up on its first load, unsynchronized), and a priming that fails says so.
# Not timing-sensitive: no result reads how long anything took, only that a line's parts, each timed apart inside its total (or, for the decode threads, inside the total on each thread), add up to no more than it plus rounding, which holds however loaded the box is; the waits are for a log line, and for the icons on screen, bounded at 60 s as a key's frame is
rm -rf "$TESTER_HOME/t60"
mkdir -p "$TESTER_HOME/t60"
python3 "$HERE/make_images.py" --keyed "$TESTER_HOME/t60/icon.png" 0,200,200 rgba
printf 'not a picture\n' > "$TESTER_HOME/t60/broken.png"
python3 "$HERE/make_images.py" --keyed "$TESTER_HOME/t60/sel.png" 200,40,40 rgb
python3 "$HERE/make_images.py" --keyed "$TESTER_HOME/t60/sel_selected.png" 40,200,40 rgb
chown -R tester:tester "$TESTER_HOME/t60"

T60_PARTS='\(read [0-9]+, decode [0-9]+, texture [0-9]+\)'
T60_BACKGROUND_PARTS='\(read [0-9]+, decode [0-9]+, luminance [0-9]+, texture [0-9]+\)'

# A function to tell whether a Timing: line's parts (inside its brackets) add up to no more than the
# total before them plus 2 ms
t60_adds_up() {
    local line=$1 total sum=0 n
    total=$(sed -nE 's/.* in ([0-9]+) ms \(.*/\1/p' <<< "$line")
    for n in $(sed -nE 's/.*\(([^)]*)\).*/\1/p' <<< "$line" | grep -oE '[0-9]+'); do sum=$((sum + n)); done
    [ -n "$total" ] && [ "$sum" -le $((total + 2)) ]
}

# A function to tell whether a menu's Timing: line adds up under parallel decode. Its texture is made
# on the main thread, a share of its total: at most the total plus 2 ms. Its read and decode are the
# work done, summed over the decode threads the log LOG names, each working inside the total: at most
# the total times that many threads, plus their rounding, half a millisecond for each of the two
# parts and for each thread's share of the rounded total (threads / 2 + 1 in whole ms)
t60_menu_adds_up() {
    local line=$1 log=$2 total read decode texture threads
    total=$(sed -nE 's/.* in ([0-9]+) ms \(.*/\1/p' <<< "$line")
    read=$(sed -nE 's/.*\(read ([0-9]+),.*/\1/p' <<< "$line")
    decode=$(sed -nE 's/.*, decode ([0-9]+),.*/\1/p' <<< "$line")
    texture=$(sed -nE 's/.*, texture ([0-9]+)\).*/\1/p' <<< "$line")
    threads=$(sed -nE 's/^Icons: decoding on ([0-9]+) threads$/\1/p' "$log" | head -1)
    [ -n "$total" ] && [ -n "$threads" ] && [ "$texture" -le $((total + 2)) ] \
        && [ $((read + decode)) -le $((total * threads + threads / 2 + 1)) ]
}

# A function to check a menu's render in the log LOG: one "rendered its buttons" line, as it always
# read, one Timing: line of the shipped shape with 48 icons and 48 titles, and its parts adding up
t60_menu() {
    local log=$1 menu=$2 lines
    lines=$(grep -E "^Timing: menu '$menu': " "$log")
    [ "$(grep -cE "^Menu '$menu': rendered its buttons at [0-9]+ px\$" "$log")" = 1 ] \
        && [ "$(grep -c . <<< "$lines")" = 1 ] \
        && grep -qE "^Timing: menu '$menu': 48 icons at [0-9]+ px in [0-9]+ ms $T60_PARTS, 48 titles in [0-9]+ ms\$" <<< "$lines" \
        && t60_menu_adds_up "$lines" "$log"
}

UNTIL="Timing: menu 'More': " run_keys f60-icons Return
log=$out/f60-icons.log
ok=1
t60_menu "$log" Main && t60_menu "$log" More && ran_clean f60-icons && ok=0
result "timing: each menu render logs one Timing: line beside its rendered line, its figures adding up (exit $(cat "$out/f60-icons.code"))" $ok
grep -E "^Timing: |^Menu '(Main|More)': rendered" "$log" | sed 's/^/      /'
ok=1
[ "$(grep -cE '^Icons: decoding on [1-4] threads$' "$log")" = 1 ] && ok=0
result "timing: the icons decode on 1 to 4 threads, logged once" $ok
grep -E '^Icons: ' "$log" | sed 's/^/      /'
ok=1
background=$(grep -E '^Timing: background: ' "$log")
[ "$(grep -c . <<< "$background")" = 1 ] \
    && grep -qE "^Timing: background: image in [0-9]+ ms $T60_BACKGROUND_PARTS\$" <<< "$background" \
    && t60_adds_up "$background" && ok=0
result "timing: a background image logs its read, decode, luminance and texture" $ok
ok=1
startup=$(sed -nE 's/^Timing: startup: first menu shown ([0-9]+) ms after the window opened$/\1/p' "$log")
main_total=$(sed -nE "s/^Timing: menu 'Main': [0-9]+ icons at [0-9]+ px in ([0-9]+) ms .*/\\1/p" "$log")
background_total=$(sed -nE 's/^Timing: background: image in ([0-9]+) ms .*/\1/p' "$log")
[ "$(grep -cE '^Timing: startup: first menu shown [0-9]+ ms after the window opened$' "$log")" = 1 ] \
    && [[ $main_total =~ ^[0-9]+$ ]] && [[ $background_total =~ ^[0-9]+$ ]] \
    && [ $((startup + 2)) -ge $((main_total + background_total)) ] && ok=0
result "timing: the program loop's start logs when the first menu was shown, counting the background and the first menu" $ok
ok=1
[ "$(grep -cx 'Images: primed the PNG, JPEG and WebP loaders' "$log")" = 1 ] \
    && precedes "$log" 'Images: primed the PNG, JPEG and WebP loaders' 'Timing: background: ' \
    && ! grep -q 'Could not prime' "$log" && ok=0
result "timing: the PNG, JPEG and WebP loaders are primed before anything decodes" $ok
ok=1
reason=$(grep -A1 -xF 'Could not load image /home/tester/t60/broken.png' "$log" | sed -n 2p)
[ -n "$reason" ] && [ "$reason" != 'not a regular file' ] \
    && grep -qE "^Menu 'More': rendered its buttons at [0-9]+ px\$" "$log" && ok=0
result "timing: an icon that is not an image logs SDL_image's reason, and the menu still renders" $ok
echo "      ${reason:-no reason logged}"

# The slideshow's first image, loaded on the main thread
CFG=$FX/f30-slideshow.ini run_quick f60-slides
ok=1
slide=$(grep -E '^Timing: background: ' "$out/f60-slides.log")
[ "$(grep -c . <<< "$slide")" = 1 ] \
    && grep -qE "^Timing: background: Slideshow image in [0-9]+ ms $T60_BACKGROUND_PARTS\$" <<< "$slide" \
    && t60_adds_up "$slide" && ran_clean f60-slides && ok=0
result "timing: the slideshow's first image logs its read, decode, luminance and texture (exit $(cat "$out/f60-slides.code"))" $ok
echo "      ${slide:-no Timing: background: line}"

# The selected icon: the highlighted entry draws its _selected copy (green frame) and the other entry
# its own icon (red frame). A function for a +key (run_keys), once the program loop has begun: once both
# frames are on screen, write the box each color spans to NAME.seen, trying for up to 60 s, as long as
# run_keys waits for a key's frame (a loaded runner has drawn nothing for 13 s)
t60_selected() {
    local name=$1 shot=/tmp/t60-selected.xwd i
    for i in $(seq 300); do
        : > "$out/$name.seen"
        xwd -root -silent -out "$shot" \
            && echo "selected $(python3 "$HERE/pixels.py" "$shot" count 40,200,40 inside 40,200,40)" >> "$out/$name.seen" \
            && echo "plain $(python3 "$HERE/pixels.py" "$shot" count 200,40,40 inside 200,40,40)" >> "$out/$name.seen" \
            && grep -q '^selected box ' "$out/$name.seen" && grep -q '^plain box ' "$out/$name.seen" && return
        sleep 0.2
    done
    cp "$shot" "$out/$name.xwd" 2> /dev/null
}
CFG=$FX/f60-selected.ini WAIT_FOR='Begin program loop' run_keys f60-selected +t60_selected
t60_selected_right=$(sed -n 's/^selected box [0-9]*,[0-9]* to \([0-9]*\),.*/\1/p' "$out/f60-selected.seen")
t60_plain_left=$(sed -n 's/^plain box \([0-9]*\),.*/\1/p' "$out/f60-selected.seen")
ok=1
[ -n "$t60_selected_right" ] && [ -n "$t60_plain_left" ] && [ "$t60_selected_right" -lt "$t60_plain_left" ] \
    && ran_clean f60-selected && ok=0
result "icons: the highlighted entry draws its _selected icon, and the entry beside it its own (exit $(cat "$out/f60-selected.code"))" $ok
sed 's/^/      /' "$out/f60-selected.seen" 2> /dev/null
