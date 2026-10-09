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
