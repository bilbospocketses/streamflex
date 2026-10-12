# Every run that opened a window, in every file: it started under SDL's x11 driver on the Vulkan
# renderer (spec section 9), but for the runs that ask for another renderer on purpose: the fallback
# subset (gl-*, Vulkan hidden) and the override run (f44-override). Only a run's first Video: and
# Renderer: lines are read: a device loss's later ones are its own check's.
# Every shard runs this file (run.sh <label> [leaks] K/N): it reads the logs in its own output
# folder, so each shard checks the runs it made.
e95_bad=() e95_runs=0
e95_chosen='^Renderer: [a-z0-9]+( \([^)]*\))?$'
for e95_log in "$out"/*.log; do
    [ -f "$e95_log" ] || continue
    grep -q "^Video: SDL's " "$e95_log" || continue
    e95_name=$(basename "$e95_log" .log)
    case $e95_name in
        gl-*) e95_video="Video: SDL's x11 driver, the opengl renderer" e95_renderer='Renderer: opengl (vulkan was unavailable)' ;;
        f44-override) e95_video="Video: SDL's x11 driver, the opengl renderer" e95_renderer='Renderer: opengl (from SDL_RENDER_DRIVER)' ;;
        *) e95_video="Video: SDL's x11 driver, the vulkan renderer" e95_renderer='Renderer: vulkan' ;;
    esac
    e95_runs=$((e95_runs + 1))
    e95_first_video=$(grep -m1 "^Video: SDL's " "$e95_log")
    e95_first_renderer=$(grep -m1 -E "$e95_chosen" "$e95_log")
    [ "$e95_first_video" = "$e95_video" ] && [ "$e95_first_renderer" = "$e95_renderer" ] \
        || e95_bad+=("$e95_name: '$e95_first_video', '${e95_first_renderer:-no Renderer: line}'")
done
ok=1
[ "$e95_runs" -gt 0 ] && [ "${#e95_bad[@]}" = 0 ] && ok=0
result "every run started on Vulkan under x11, but the runs that ask for OpenGL" $ok
echo "      $e95_runs runs read"
for e95_line in "${e95_bad[@]}"; do echo "      $e95_line"; done
