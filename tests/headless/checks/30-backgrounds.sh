# The background's startup paths, which now all go through reload_background(): an image, a
# missing image, a slideshow, a slideshow folder with one image, one with none, and none at all.
# The setting is kept as chosen even when the launcher falls back to the colour.

run_quick f30-image
ok=1
ran_clean f30-image && ! grep -q "Couldn't load background image" "$out/f30-image.log" \
    && grep -q 'Background set up: Image' "$out/f30-image.log" && ok=0
result "an image background loads (exit $(cat "$out/f30-image.code"))" $ok

run_quick f30-missing
ok=1
ran_clean f30-missing && grep -q "Couldn't load background image" "$out/f30-missing.log" \
    && grep -A2 'Background ===' "$out/f30-missing.log" | grep -qE 'Mode:\s+Image$' && ok=0
result "a missing image falls back to the colour, and the setting stays Image (exit $(cat "$out/f30-missing.code"))" $ok

run_quick f30-slideshow
ok=1
ran_clean f30-slideshow && grep -q "Found 3 images in directory /home/tester/Pictures" "$out/f30-slideshow.log" \
    && grep -q 'Background set up: Slideshow' "$out/f30-slideshow.log" && ok=0
result "a slideshow finds its three images, leaving out a hidden picture and a text file (exit $(cat "$out/f30-slideshow.code"))" $ok

# Upper-case extensions (a camera's DSC_0001.JPG) are images, as the settings' browser says
run_quick f30-upper
ok=1
ran_clean f30-upper && grep -q "Found 2 images in directory /home/tester/upper" "$out/f30-upper.log" \
    && grep -q 'Background set up: Slideshow' "$out/f30-upper.log" && ok=0
result "a slideshow finds images whose extensions are upper-case (exit $(cat "$out/f30-upper.code"))" $ok

run_quick f30-one
ok=1
ran_clean f30-one && grep -q "Only one image found" "$out/f30-one.log" \
    && grep -A4 'Background ===' "$out/f30-one.log" | grep -qE 'Image:\s+\(null\)$' && ok=0
result "a one-image slideshow shows the image without rewriting the Image setting (exit $(cat "$out/f30-one.code"))" $ok

run_quick f30-empty
ok=1
ran_clean f30-empty && grep -q "No images found in slideshow directory" "$out/f30-empty.log" \
    && grep -q 'Background set up: Color' "$out/f30-empty.log" && ok=0
result "an empty slideshow folder falls back to the colour (exit $(cat "$out/f30-empty.code"))" $ok

run_quick f30-nodir
ok=1
ran_clean f30-nodir && grep -q "does not exist" "$out/f30-nodir.log" && ok=0
result "Mode=Slideshow with no SlideshowDirectory falls back instead of crashing (exit $(cat "$out/f30-nodir.code"))" $ok

# The slideshow loader. It runs on its own thread, so when its folder stops giving it two pictures
# it only reports, and the main thread falls back: to the one picture that still loads, or to the
# colour. The Mode setting stays Slideshow. run_slideshow (run.sh) waits for the verdict's line.

# At startup: no file in the folder loads
run_quick f30-broken
ok=1
ran_clean f30-broken \
    && grep -q "Could not load any image from slideshow directory /home/tester/broken" "$out/f30-broken.log" \
    && grep -q "Background set up: Color" "$out/f30-broken.log" && ok=0
result "a slideshow whose files all fail to load falls back to the colour instead of hanging (exit $(cat "$out/f30-broken.code"))" $ok

# While running: the only picture that loads is the one on show
run_slideshow f30-mixed 'Could only load one image from slideshow directory /home/tester/mixed, showing it as a single image' true
ok=1
ran_clean f30-mixed \
    && grep -q "Could only load one image from slideshow directory /home/tester/mixed, showing it as a single image" "$out/f30-mixed.log" \
    && ok=0
result "a running slideshow left with one picture shows it as a single image (exit $(cat "$out/f30-mixed.code"))" $ok

# While running: the pictures vanish (a network share dropping, say)
mkdir -p "$TESTER_HOME/vanish"
cp "$TESTER_HOME/Pictures/blue.png" "$TESTER_HOME/Pictures/green.png" "$TESTER_HOME/vanish/"
chown -R tester:tester "$TESTER_HOME/vanish"
run_slideshow f30-vanish 'Could not load any image from slideshow directory /home/tester/vanish' \
    rm -f "$TESTER_HOME/vanish/blue.png" "$TESTER_HOME/vanish/green.png"
ok=1
ran_clean f30-vanish \
    && grep -q "Could not load any image from slideshow directory /home/tester/vanish" "$out/f30-vanish.log" && ok=0
result "a running slideshow whose pictures vanish falls back to the colour (exit $(cat "$out/f30-vanish.code"))" $ok
grep -m2 -E 'runtime error|AddressSanitizer' "$out/f30-vanish.err" | sed 's/^/      /'

# Pipes that nothing writes, named as the background image and as two icons (a raster one and an
# SVG): a read of any would wait for good, so none is opened. Each is refused as a file that will
# not load is, with its path in the log, and the launcher starts and quits by itself.
rm -rf "$TESTER_HOME/pipes"
mkdir -p "$TESTER_HOME/pipes"
mkfifo "$TESTER_HOME/pipes/bg.png" "$TESTER_HOME/pipes/icon.png" "$TESTER_HOME/pipes/icon.svg"
chown -R tester:tester "$TESTER_HOME/pipes"
run_quick f30-pipes
ok=1
ran_clean f30-pipes && grep -q "Couldn't load background image" "$out/f30-pipes.log" \
    && grep -A1 'Could not load image /home/tester/pipes/bg.png' "$out/f30-pipes.log" | grep -q 'not a regular file' \
    && grep -A1 'Could not load image /home/tester/pipes/icon.png' "$out/f30-pipes.log" | grep -q 'not a regular file' \
    && grep -A1 'Could not load image /home/tester/pipes/icon.svg' "$out/f30-pipes.log" | grep -q 'not a regular file' \
    && ok=0
result "a pipe named as the background image or an icon is never opened, and the launcher starts (exit $(cat "$out/f30-pipes.code"))" $ok
grep -A1 'Could not load image' "$out/f30-pipes.log" | sed 's/^/      /'
