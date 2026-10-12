# The renderer (spec section 3, Task 1.5b): SDL takes the first renderer of the platform's list that
# works, and the log says which; SDL_RENDER_DRIVER beats the list; a renderer that cannot be made
# takes the startup error path; and with the Vulkan driver hidden (VK_DRIVER_FILES at nothing) SDL
# falls back to OpenGL by itself, which then draws the background, the titles, the settings screen
# and a picker as Vulkan does. The fallback runs are named gl-*, which 95-every-run.sh knows.
# VSync is asked for on each renderer: the rule re-based on SDL_GetRenderVSync.

R44_NO_VULKAN=/nonexistent/vulkan-driver.json

# Vulkan, as every run starts: the Video: and Renderer: lines, the video dump's renderer, and VSync given
CFG=$FX/f44-quit.ini run_quick f44-vulkan
log=$out/f44-vulkan.log
ok=1
grep -qx "Video: SDL's x11 driver, the vulkan renderer" "$log" && grep -qx 'Renderer: vulkan' "$log" \
    && grep -qx 'Render driver: vulkan' "$log" && grep -qE '^Max texture:   [0-9]+ px$' "$log" \
    && grep -qx 'Frame timing: VSync wanted on, the renderer gives on' "$log" \
    && ! grep -q 'Could not turn VSync' "$log" && ran_clean f44-vulkan && ok=0
result "renderer: Vulkan is chosen and named, and gives the VSync asked for (exit $(cat "$out/f44-vulkan.code"))" $ok
grep -E '^(Video|Renderer|Render driver|Max texture|Render VSync|Frame timing):' "$log" | sed 's/^/      /'

# SDL_RENDER_DRIVER beats the list: StreamFlex passes none, and the log says the variable chose
SDL_RENDER_DRIVER=opengl CFG=$FX/f44-quit.ini run_quick f44-override
log=$out/f44-override.log
ok=1
grep -qx "Video: SDL's x11 driver, the opengl renderer" "$log" && grep -qx 'Renderer: opengl (from SDL_RENDER_DRIVER)' "$log" \
    && ! grep -qx 'Renderer: vulkan' "$log" && ran_clean f44-override && ok=0
result "renderer: SDL_RENDER_DRIVER=opengl beats the list (exit $(cat "$out/f44-override.code"))" $ok
grep -E '^(Video|Renderer):' "$log" | sed 's/^/      /'

# No renderer at all (a name SDL does not know) takes the startup error path: SDL's reason, then exit 1
SDL_RENDER_DRIVER=nosuchdriver STREAMFLEX_TEST_NO_MESSAGE_BOX=1 CFG=$FX/f44-quit.ini run_quick f44-nodriver
log=$out/f44-nodriver.log
ok=1
grep -qx 'Could not initialize renderer' "$log" && ! grep -q "^Video: SDL's " "$log" && ran_clean f44-nodriver 1 && ok=0
result "renderer: none to be had reports SDL's reason and exits 1 (exit $(cat "$out/f44-nodriver.code"))" $ok
grep -A1 'Could not initialize renderer' "$log" | sed 's/^/      /'

# The automatic fallback: Vulkan hidden, SDL moves down the list by itself, and the log says so.
# VSync is asked for on OpenGL too, on and then off.
VK_DRIVER_FILES=$R44_NO_VULKAN CFG=$FX/f44-quit.ini run_quick gl-quit
VK_DRIVER_FILES=$R44_NO_VULKAN CFG=$FX/f42-fps10.ini run_quick gl-fps10
ok=1
grep -qx "Video: SDL's x11 driver, the opengl renderer" "$out/gl-quit.log" \
    && grep -qx 'Renderer: opengl (vulkan was unavailable)' "$out/gl-quit.log" \
    && ! grep -qx 'Renderer: vulkan' "$out/gl-quit.log" \
    && grep -qx 'Frame timing: VSync wanted on, the renderer gives on' "$out/gl-quit.log" \
    && grep -qx 'Frame timing: VSync wanted off, the renderer gives off' "$out/gl-fps10.log" \
    && ran_clean gl-quit && ran_clean gl-fps10 && ok=0
result "renderer: with Vulkan hidden SDL falls back to OpenGL, which gives VSync on and off (exit $(cat "$out/gl-quit.code"), $(cat "$out/gl-fps10.code"))" $ok
grep -hE '^(Video|Renderer|Frame timing):' "$out/gl-quit.log" "$out/gl-fps10.log" | sed 's/^/      /'

# The home screen on each renderer: the background color in the screen's corner, and the one title,
# in a magenta nothing else has, spanning the same box with as many exactly magenta pixels. A title
# is a texture copied 1:1, so the two renderers must agree to the pixel.
r44_home() {
    local name=$1 pid=$2 shot=/tmp/r44.xwd
    wait_line 'Begin program loop' "$pid" || return
    screen_shows "$name" corner 10,10=30,45,60 && echo 'corner yes' >> "$out/$name.seen"
    xwd -root -silent -out "$shot" && python3 "$HERE/pixels.py" "$shot" count 255,0,255 inside 255,0,255 > "$out/$name.box" 2>&1
}
CFG=$FX/f44-draw.ini run_keys f44-home +r44_home
VK_DRIVER_FILES=$R44_NO_VULKAN CFG=$FX/f44-draw.ini run_keys gl-home +r44_home
ok=1
grep -qx 'corner yes' "$out/f44-home.seen" && grep -qx 'corner yes' "$out/gl-home.seen" \
    && grep -qE '^box [0-9]+,[0-9]+ to [0-9]+,[0-9]+: [1-9][0-9]* pixels of 255,0,255$' "$out/f44-home.box" \
    && [ "$(cat "$out/f44-home.box")" = "$(cat "$out/gl-home.box")" ] \
    && ran_clean f44-home && ran_clean gl-home && ok=0
result "renderer: OpenGL draws the background and a title as Vulkan does (exit $(cat "$out/f44-home.code"), $(cat "$out/gl-home.code"))" $ok
echo "      vulkan: $(cat "$out/f44-home.box" 2> /dev/null)"
echo "      opengl: $(cat "$out/gl-home.box" 2> /dev/null)"

# The settings screen on OpenGL: its backdrop, the preview's color, then an image the browser
# highlights (the keys of 60-settings-background.sh's f60-preview)
r44_black() { look "$1" "$2" black 'Settings opened' 30,30=0,0,0 @5,1074=11,22,32; }
r44_blue() { look "$1" "$2" blue 'Settings: the preview shows /home/tester/Pictures/blue.png' 30,30=40,70,160; }
VK_DRIVER_FILES=$R44_NO_VULKAN CFG=$FX/f60-color.ini run_keys gl-settings Menu +r44_black Down Return Right Down Return +r44_blue Menu
ok=1
grep -qx 'black yes' "$out/gl-settings.seen" && grep -qx 'blue yes' "$out/gl-settings.seen" \
    && grep -q 'Settings: nothing changed' "$out/gl-settings.log" && ran_clean gl-settings && ok=0
result "renderer: OpenGL draws the settings screen and the preview's image (exit $(cat "$out/gl-settings.code"))" $ok
sed 's/^/      /' "$out/gl-settings.seen" 2> /dev/null

# A picker on OpenGL: a color typed in the color picker's hex editor shows in the preview (the keys of
# 58-settings-pickers.sh's f58-hex); the Menu key puts it back and closes, so nothing is saved
r44_hex() { look "$1" "$2" hex 'Settings: previewing #102030' 30,30=16,32,48; }
r44_hex_keys="Down Down Down Down Return Up Right Right Up Up Right Right Up Up Up"
# shellcheck disable=SC2086
VK_DRIVER_FILES=$R44_NO_VULKAN CFG=$FX/f60-color.ini run_keys gl-picker Menu Down Return Down Return $r44_hex_keys +r44_hex Menu
ok=1
grep -qx 'hex yes' "$out/gl-picker.seen" && grep -q 'Settings: nothing changed' "$out/gl-picker.log" \
    && ran_clean gl-picker && ok=0
result "renderer: OpenGL draws the color picker's preview (exit $(cat "$out/gl-picker.code"))" $ok
