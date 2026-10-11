# The StreamFlex headless test image: CI's Debian build dependencies (SDL3 at the floor, Debian trixie),
# plus a virtual X display (Xvfb) with Mesa's software drivers, so the Vulkan renderer runs on lavapipe
# and OpenGL on softpipe without a screen; xdotool to press keys, xwd (x11-apps) to take the screenshots
# pixels.py reads, and xrandr (x11-xserver-utils) to read the display's refresh rate. The Noto CJK and
# color emoji fonts and fontconfig's development files serve the fallback-font checks, and Weston (14,
# which has no fifo-v1) the Wayland leg. The launcher runs as `tester`, because root ignores file
# permissions. See run.sh for how it is used.
# Through mirror.gcr.io, Google's public Docker Hub mirror: Docker Hub rate-limits the unauthenticated
# pulls of GitHub's runners. The digest is the same image's.
FROM mirror.gcr.io/library/debian:trixie@sha256:913f6706df59a68922d1dd08f78c2476560a8d367897200a6005b00e5f67c2d5
RUN apt-get update \
 && apt-get install -y --no-install-recommends \
      build-essential file git cmake pkg-config \
      libsdl3-dev libsdl3-image-dev libsdl3-ttf-dev libfontconfig-dev libinih-dev \
      xvfb xauth xdotool x11-apps x11-xserver-utils libgl1-mesa-dri mesa-vulkan-drivers libvulkan1 \
      fonts-noto-cjk fonts-noto-color-emoji weston python3 \
 && rm -rf /var/lib/apt/lists/* \
 && useradd --create-home tester
