# The screen's size and the display's scale (spec section 6). The OS's scale is logged, never applied:
# under SDL_VIDEO_X11_SCALING_FACTOR=2 the log says so, and the home grid's pixels are those of a run
# without it.
s47_shot() { wait_line 'Begin program loop' "$2" && sleep 1 && xwd -root -silent -out "$out/$1.xwd"; }
CFG=$FX/f47-grid.ini run_keys f47-scale1 +s47_shot
CFG=$FX/f47-grid.ini SDL_VIDEO_X11_SCALING_FACTOR=2 run_keys f47-scale2 +s47_shot
ok=1
grep -qx 'Screen: 1920x1080 px, pixel density 1.00, display scale 1.00' "$out/f47-scale1.log" \
    && grep -qx 'Screen: 1920x1080 px, pixel density 1.00, display scale 2.00' "$out/f47-scale2.log" \
    && [ -s "$out/f47-scale1.xwd" ] && cmp -s "$out/f47-scale1.xwd" "$out/f47-scale2.xwd" \
    && ran_clean f47-scale1 && ran_clean f47-scale2 && ok=0
result "screen: a display scale of 2 is logged and not applied, the grid drawn as at 1 (exit $(cat "$out/f47-scale1.code"), $(cat "$out/f47-scale2.code"))" $ok
grep -h '^Screen: ' "$out/f47-scale1.log" "$out/f47-scale2.log" | sed 's/^/      /'
