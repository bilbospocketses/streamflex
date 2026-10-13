# The pickers' timing-sensitive case, from 58-settings-pickers.sh: a slideshow's measures followed
# through a fixed rest. ~/duo, p58_measured and p58_values are made here as there.
# Timing-sensitive: counts the images a fixed 11 s rest sees, on 5 s image changes

# A slideshow measures each image on its loader thread, and the main thread takes the measure with
# the image, whether it cuts to it or fades it in. ~/duo holds blue.png and green.png, which take
# turns; with black titles, only blue's 2.5:1 warns, and the warning follows the image live.
rm -rf "$TESTER_HOME/duo"
mkdir -p "$TESTER_HOME/duo"
cp "$TESTER_HOME/Pictures/blue.png" "$TESTER_HOME/Pictures/green.png" "$TESTER_HOME/duo/"
chown -R tester:tester "$TESTER_HOME/duo"
p58_measured() {
    local i
    for i in $(seq 100); do
        [ "$(grep -c 'Background: the image on show has a mean luminance of' "$LOG" 2> /dev/null)" -ge 2 ] && return
        sleep 0.2
    done
}
p58_rest() { sleep 11; }
p58_values() { grep -o 'Background: the image on show has a mean luminance of [0-9.]*' "$1" | sed 's/.* //'; }
WAIT_FOR='Background set up: Slideshow' CFG=$FX/f58-slide.ini \
    run_keys f58-slide +p58_measured Menu Down Down Down Return Down Down Down +p58_rest Menu
log=$out/f58-slide.log
values=$(p58_values "$log")
lowest=$(sort -n <<< "$values" | head -1)
ok=1
[ "$(grep -c . <<< "$values")" -ge 3 ] && [ "$(sort -u <<< "$values" | grep -c .)" = 2 ] \
    && ! grep -q 'could not be measured' "$log" \
    && awk -v low="$lowest" '
        /Background: the image on show has a mean luminance of/ { last = $NF }
        /the note under the preview says Low contrast/ {
            if (index($0, "Low contrast: 2.5:1") && last == low) warned++; else wrong++
        }
        END { exit !(warned >= 1 && wrong == 0) }' "$log" \
    && grep -q 'Settings: nothing changed' "$log" && ran_clean f58-slide && ok=0
result "pickers: a slideshow's images are measured as they change, and the warning follows them (exit $(cat "$out/f58-slide.code"))" $ok
echo "      measured: $(tr '\n' ' ' <<< "$values")"
