# The StreamFlex headless test image: CI's Debian build dependencies, plus a virtual X display
# (Xvfb) with Mesa's software OpenGL so the accelerated renderer works without a screen, xdotool
# to press keys, xwd (x11-apps) to take the screenshots pixels.py reads, and xrandr
# (x11-xserver-utils) to read the display's refresh rate. The launcher runs as `tester`, because
# root ignores file permissions. See run.sh for how it is used.
FROM debian:bookworm@sha256:2c037a04925515fdd6ea85ea14a682d0e79931f5e9f5d07b6dbfc6ba12f9e858
RUN apt-get update \
 && apt-get install -y --no-install-recommends \
      build-essential file git cmake pkg-config \
      libsdl2-dev libsdl2-image-dev libsdl2-ttf-dev libinih-dev \
      xvfb xauth xdotool x11-apps x11-xserver-utils libgl1-mesa-dri python3 \
 && rm -rf /var/lib/apt/lists/* \
 && useradd --create-home tester
