# Every run the checks above made, in every file: no title was drawn wider than its button. The
# debug log names each one that is, in Truncate and Shrink modes (OversizeMode=None lets a title
# run over on purpose, and is not logged).
# Every shard runs this file (run.sh <label> [leaks] K/N): it reads the logs in its own output
# folder, so each shard checks the runs it made.
over=$(grep -l 'px wide, over its' "$out"/*.log 2> /dev/null)
ok=1
[ -z "$over" ] && ok=0
result "no run drew a title wider than its button" $ok
grep -H 'px wide, over its' "$out"/*.log 2> /dev/null | sed "s|^$out/||; s/^/      /"

# Fallback fonts (spec section 4): no run but the fallback checks' own (f72-*) attached a fallback or
# met a character no font has, so every other check's exact pixels are drawn as before. Two runs are
# exceptions, by design. f59-symbols chooses Dingy, a fixture font with glyphs for "DingyAa0" alone, as
# the title font, so its titles' other letters now come from fallbacks. f48-fallback (48-device-loss.sh)
# draws a 中 title through run.sh's box font on purpose, to see it lit again after a lost device.
fell=$(grep -lE '^Fonts: (the [0-9]+ pt .* fall back to |no installed font draws )' "$out"/*.log 2> /dev/null \
    | grep -vE "/(f72-[^/]*|f48-fallback|f59-symbols)\.log$")
ok=1
[ -z "$fell" ] && ok=0
result "no run outside the fallback checks logged a fallback font" $ok
for f in $fell; do grep -HE '^Fonts: (the [0-9]+ pt .* fall back to |no installed font draws )' "$f" | sed "s|^$out/||; s/^/      /"; done
