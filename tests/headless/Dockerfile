# The StreamFlex headless test image: CI's Debian build dependencies (SDL3 at the floor, Debian trixie),
# plus a virtual X display (Xvfb) with Mesa's software drivers, so the Vulkan renderer runs on lavapipe
# and OpenGL on softpipe without a screen; xdotool to press keys, xwd (x11-apps) to take the screenshots
# pixels.py reads, xrandr (x11-xserver-utils) to read the display's refresh rate, and setxkbmap
# (x11-xkb-utils), which check 84 uses to type under a German layout. The Noto CJK and
# color emoji fonts and fontconfig's development files serve the fallback-font checks, and Weston (14,
# which has no fifo-v1) the Wayland leg. The launcher runs as `tester`, because root ignores file
# permissions. See run.sh for how it is used.
# Through mirror.gcr.io, Google's public Docker Hub mirror: Docker Hub rate-limits the unauthenticated
# pulls of GitHub's runners. The digest is the same image's.
# The debug symbols of Mesa's GLX library and Vulkan drivers (lavapipe), SDL3 and fontconfig come from
# Debian's debug archives: with them LeakSanitizer names the functions in those libraries, which the
# leak pass's suppressions (lsan-debian.supp, named in LEAK_SUPPRESSIONS) match one function at a time,
# and ThreadSanitizer names lavapipe's, which the tsan mode's suppressions (tsan.supp) match the same
# way. Without them a sanitizer gives only a library and an offset, and a suppression could only name a
# whole library. A -dbgsym package needs its library's exact version, and the image's apt sources also
# hold trixie-updates and trixie-security, which can move a library ahead of trixie-debug: so the debug
# suites of all three are listed (an update's symbols are in trixie-proposed-updates-debug, a security
# fix's in trixie-security-debug), the libraries are installed first, and each -dbgsym is asked for at
# the version installed. Should the debug archive not have that version yet, the build stops and says
# so (CONTRIBUTING.md, the headless checks, says what to do).
FROM mirror.gcr.io/library/debian:trixie@sha256:4c25275446ca371766b8b093c696782eabce06263fccacb8d5d813b3dc44b308
RUN printf '%s\n' \
      'deb http://deb.debian.org/debian-debug trixie-debug main' \
      'deb http://deb.debian.org/debian-debug trixie-proposed-updates-debug main' \
      'deb http://deb.debian.org/debian-security-debug trixie-security-debug main' \
      > /etc/apt/sources.list.d/debian-debug.list \
 && apt-get update \
 && apt-get install -y --no-install-recommends \
      build-essential file git cmake pkg-config \
      libsdl3-dev libsdl3-image-dev libsdl3-ttf-dev libfontconfig-dev libinih-dev libdbus-1-dev \
      xvfb xauth xdotool x11-xkb-utils x11-apps x11-xserver-utils libgl1-mesa-dri mesa-vulkan-drivers libvulkan1 \
      fonts-noto-cjk fonts-noto-color-emoji weston python3 \
 && dbgsym= \
 && for lib in libglx-mesa0 mesa-vulkan-drivers libsdl3-0 libfontconfig1; do \
      dbgsym="$dbgsym $lib-dbgsym=$(dpkg-query -W -f='${Version}' "$lib")" || exit 1; \
    done \
 && { apt-get install -y --no-install-recommends $dbgsym \
      || { echo "DEBUG SYMBOLS MISSING: Debian's debug archives have no$dbgsym yet; see CONTRIBUTING.md's headless checks" >&2; exit 1; }; } \
 && rm -rf /var/lib/apt/lists/* \
 && useradd --create-home tester
ENV LEAK_SUPPRESSIONS=lsan-debian.supp
