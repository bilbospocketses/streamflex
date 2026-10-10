#!/bin/bash
# ubuntu-guest.sh: the guest-side steps of StreamFlex's Ubuntu hands-on check.
#
# PURPOSE
#   Everything the Ubuntu checklist (checklists/ubuntu-hands-on.md) does inside the guest that
#   would be fragile as a one-line ssh command: non-ASCII names, section-scoped config edits,
#   the launch wrapper and the facts the check records first. Copy it to the guest with
#   virtual-gamepad.py and make-test-images.py (all three in one folder) and run it as root.
#
# USAGE (as root; the desktop user is qa unless QA_USER says otherwise)
#   ubuntu-guest.sh facts                 record the unknowns: GPU, resolution, session, XWayland,
#                                         audio, the SDL libraries StreamFlex loads, uinput
#   ubuntu-guest.sh setup                 load uinput; turn the gamepad on in the system config
#                                         (keeping the shipped copy); install the launch wrapper
#                                         and the "StreamFlex Debug" app-menu entry; point
#                                         Pictures at ~/Bilder through user-dirs.dirs; make the
#                                         picture folder ~/Bilder/Été and ~/Été/café.txt
#   ubuntu-guest.sh reset-pass wayland|x11|default
#                                         start a pass afresh: delete the user config and the
#                                         kept logs, and choose SDL_VIDEODRIVER for sf-run.sh
#                                         (default: leave it unset, so SDL picks)
#   ubuntu-guest.sh add-cafe-entry        add Entry5=Café to [Main] of the user config
#   ubuntu-guest.sh pad-node              print the virtual pad's /dev/input node, its udev
#                                         properties and its ACL (is it the desktop user's?)
#
# REQUIREMENTS
#   Ubuntu 26.04 with the StreamFlex .deb installed (setup needs it), python3, root.
set -u

QA_USER=${QA_USER:-qa}
QA_HOME=$(getent passwd "$QA_USER" | cut -d: -f6)
QA_GROUP=$(id -gn "$QA_USER" 2>/dev/null)
HERE=$(cd "$(dirname "$0")" && pwd)
SYSTEM_CONFIG=/usr/share/streamflex/config.ini
E=$(printf '\xc3\x89t\xc3\xa9')        # Été
CAFE=$(printf 'caf\xc3\xa9')           # café

die() { echo "ubuntu-guest: $*" >&2; exit 1; }
[ "$(id -u)" = 0 ] || die "run this as root"
[ -n "$QA_HOME" ] || die "no user $QA_USER"
as_qa() { runuser -u "$QA_USER" -- "$@"; }
# chown to the user and their group, spelled out: "user:" alone is not honored by every coreutils
give() { chown "$QA_USER:$QA_GROUP" "$@"; }

facts() {
    echo "== OS";           grep PRETTY_NAME /etc/os-release; uname -r
    echo "== GPU";          lspci -nnk 2>/dev/null | grep -A3 -Ei 'vga|3d|display' || echo "(lspci missing)"
    for d in /sys/class/drm/card*; do
        [ -e "$d/device/driver" ] && echo "$(basename "$d"): driver $(basename "$(readlink "$d/device/driver")")"
    done
    echo "== Connected outputs and their modes"
    for s in /sys/class/drm/card*-*/status; do
        [ -e "$s" ] || continue
        [ "$(cat "$s")" = connected ] && echo "$(basename "$(dirname "$s")"): $(head -n 1 "$(dirname "$s")/modes")"
    done
    echo "== The desktop user's session"
    loginctl list-sessions --no-legend 2>/dev/null
    for id in $(loginctl list-sessions --no-legend 2>/dev/null | awk -v u="$QA_USER" '$3 == u {print $1}'); do
        loginctl show-session "$id" -p Type -p Desktop -p Seat -p Active 2>/dev/null
    done
    echo "== The user manager's environment (what systemd-run --user and the app menu pass on)"
    as_qa env XDG_RUNTIME_DIR="/run/user/$(id -u "$QA_USER")" systemctl --user show-environment 2>/dev/null \
        | grep -E '^(WAYLAND_DISPLAY|DISPLAY|XDG_SESSION_TYPE|XDG_CURRENT_DESKTOP|XDG_PICTURES_DIR|SDL_)' \
        || echo "(could not read it)"
    echo "== XWayland";      pgrep -a Xwayland || echo "not running now (GNOME may start it on demand)"
    ls -l /tmp/.X11-unix/ 2>/dev/null
    echo "== GL renderer (for the report; StreamFlex's log names only the video driver)"
    if command -v glxinfo >/dev/null; then
        local xauth
        xauth=$(ls /run/user/"$(id -u "$QA_USER")"/.mutter-Xwaylandauth.* 2>/dev/null | head -n 1)
        as_qa env DISPLAY=:0 XAUTHORITY="$xauth" glxinfo -B 2>&1 | grep -E 'OpenGL renderer|OpenGL version' \
            || echo "(glxinfo could not reach :0)"
    else
        echo "(glxinfo not installed: mesa-utils)"
    fi
    echo "== Audio"
    as_qa env XDG_RUNTIME_DIR="/run/user/$(id -u "$QA_USER")" wpctl status 2>/dev/null | sed -n '1,12p' \
        || echo "(wpctl not available)"
    echo "== SDL libraries StreamFlex loads"
    if [ -x /usr/bin/streamflex ]; then
        ldd /usr/bin/streamflex | grep -E 'SDL|inih|not found'
        for lib in $(ldd /usr/bin/streamflex | awk '/SDL2/ {print $3}'); do
            echo "$lib -> $(readlink -f "$lib") ($(dpkg -S "$(readlink -f "$lib")" 2>/dev/null | cut -d: -f1))"
        done
    else
        echo "(streamflex is not installed yet)"
    fi
    echo "== uinput and evdev"
    modinfo -F filename uinput 2>/dev/null || echo "uinput: no module file (built in, or missing)"
    ls -l /dev/uinput 2>&1
    grep -E '^(uinput|evdev) ' /proc/modules || echo "(neither listed as a module: built in, or not loaded)"
}

setup() {
    [ -f "$SYSTEM_CONFIG" ] || die "$SYSTEM_CONFIG is missing: install the .deb first"
    modprobe uinput || die "modprobe uinput failed"
    [ -c /dev/uinput ] || die "/dev/uinput is still missing after modprobe"

    # The gamepad: the shipped config already says Enabled=true under [Gamepad], so the sed is a
    # no-op kept for an older package; the check after it is what matters
    [ -f /root/config.ini.shipped ] || cp -p "$SYSTEM_CONFIG" /root/config.ini.shipped
    sed -i '/^\[Gamepad\]/,/^\[/ s/^Enabled=false$/Enabled=true/' "$SYSTEM_CONFIG"
    sed -n '/^\[Gamepad\]/,/^\[/p' "$SYSTEM_CONFIG" | grep -qx 'Enabled=true' \
        || die "could not turn the gamepad on in $SYSTEM_CONFIG"
    cp -p "$SYSTEM_CONFIG" "$QA_HOME/baseline-config.ini" && give "$QA_HOME/baseline-config.ini"
    echo "system config: [Gamepad] Enabled=true (the shipped copy is /root/config.ini.shipped;" \
         "the baseline for comparisons is $QA_HOME/baseline-config.ini)"

    # The launch wrapper: -d, the video driver of the pass, the exit code, and a kept log
    cat > "$QA_HOME/sf-run.sh" <<'EOF'
#!/bin/sh
# Starts StreamFlex with -d for the hands-on check (installed by ubuntu-guest.sh setup)
mkdir -p "$HOME/sf-logs"
case "$(cat "$HOME/sf-videodriver" 2>/dev/null)" in
    wayland) SDL_VIDEODRIVER=wayland; export SDL_VIDEODRIVER ;;
    x11) SDL_VIDEODRIVER=x11; export SDL_VIDEODRIVER ;;
esac
echo "started $(date +%T) SDL_VIDEODRIVER=${SDL_VIDEODRIVER:-unset}" > "$HOME/sf-logs/run-info"
/usr/bin/streamflex -d > "$HOME/sf-logs/stdout.txt" 2> "$HOME/sf-logs/stderr.txt"
echo $? > "$HOME/sf-logs/exit-code"
cp "$HOME/.local/share/streamflex/streamflex.log" "$HOME/sf-logs/last.log"
EOF
    chmod 755 "$QA_HOME/sf-run.sh"
    as_qa mkdir -p "$QA_HOME/.local/share/applications" "$QA_HOME/.config" "$QA_HOME/$E" "$QA_HOME/sf-logs" \
        || die "could not make the folders as $QA_USER"
    cat > "$QA_HOME/.local/share/applications/streamflex-debug.desktop" <<EOF
[Desktop Entry]
Type=Application
Name=StreamFlex Debug
Comment=StreamFlex with -d, for the hands-on check
Exec=$QA_HOME/sf-run.sh
Icon=streamflex
Terminal=false
EOF

    # Pictures through a non-English user-dirs.dirs; the old ~/Pictures stays, so a browser that
    # ignores the file opens somewhere visibly different
    if [ -f "$QA_HOME/.config/user-dirs.dirs" ] && [ ! -f "$QA_HOME/.config/user-dirs.dirs.orig" ]; then
        cp -p "$QA_HOME/.config/user-dirs.dirs" "$QA_HOME/.config/user-dirs.dirs.orig"
    fi
    touch "$QA_HOME/.config/user-dirs.dirs"
    sed -i '/^XDG_PICTURES_DIR=/d' "$QA_HOME/.config/user-dirs.dirs"
    echo 'XDG_PICTURES_DIR="$HOME/Bilder"' >> "$QA_HOME/.config/user-dirs.dirs"
    python3 "$HERE/make-test-images.py" "$QA_HOME/Bilder/$E" --owner "$QA_USER" || die "make-test-images failed"

    # The non-ASCII launch target
    echo "StreamFlex $CAFE test" > "$QA_HOME/$E/$CAFE.txt"

    give "$QA_HOME/sf-run.sh" "$QA_HOME/.local/share/applications/streamflex-debug.desktop" \
        "$QA_HOME/.config/user-dirs.dirs" "$QA_HOME/$E" "$QA_HOME/$E/$CAFE.txt"

    # Nothing may shadow the system config: the working folder and ~/.config come first
    for f in "$QA_HOME/config.ini" "$QA_HOME/.config/streamflex/config.ini"; do
        [ -e "$f" ] && echo "WARNING: $f exists and would be loaded before $SYSTEM_CONFIG"
    done
    echo "editor for the non-ASCII launch: $(command -v gnome-text-editor || command -v gedit || echo 'none found')"
    echo "setup done"
}

reset_pass() {
    case "${1:-}" in wayland|x11|default) ;; *) die "reset-pass needs wayland, x11 or default" ;; esac
    rm -rf "$QA_HOME/.config/streamflex"
    rm -f "$QA_HOME/sf-logs/"*
    echo "$1" > "$QA_HOME/sf-videodriver" && give "$QA_HOME/sf-videodriver"
    cmp -s "$SYSTEM_CONFIG" "$QA_HOME/baseline-config.ini" || echo "WARNING: $SYSTEM_CONFIG differs from the baseline"
    echo "pass $1: user config removed, logs cleared"
}

add_cafe_entry() {
    local cfg="$QA_HOME/.config/streamflex/config.ini"
    [ -f "$cfg" ] || die "$cfg does not exist yet (it is made by the first save)"
    CFG="$cfg" python3 - <<'EOF' || die "could not add the entry"
import os, re
path = os.environ["CFG"]
home = os.path.dirname(os.path.dirname(os.path.dirname(path)))
text = open(path, encoding="utf-8").read()
entry = 'Entry5=Café;apps;%s "%s/Été/café.txt"' % (
    "gnome-text-editor" if os.path.exists("/usr/bin/gnome-text-editor") else "xdg-open", home)
new, count = re.subn(r"(?m)^(Entry4=System;settings;:submenu System)(\r?\n)",
                     lambda m: m.group(1) + m.group(2) + entry + m.group(2), text, count=1)
if count != 1:
    raise SystemExit("Entry4 of [Main] not found")
open(path, "w", encoding="utf-8", newline="").write(new)
print("added: " + entry)
EOF
}

pad_node() {
    local sys
    sys=$(grep -l 'Microsoft X-Box 360 pad' /sys/class/input/input*/name 2>/dev/null | head -n 1)
    [ -n "$sys" ] || die "no virtual pad (is the sf-vgp unit running?)"
    local dir node
    dir=$(dirname "$sys")
    node=/dev/input/$(basename "$(ls -d "$dir"/event* | head -n 1)")
    echo "node: $node"
    udevadm info "$node" | grep -E 'ID_INPUT_JOYSTICK|ID_INPUT=|TAGS|CURRENT_TAGS'
    getfacl -p "$node" 2>/dev/null || ls -l "$node"
}

case "${1:-}" in
    facts) facts ;;
    setup) setup ;;
    reset-pass) reset_pass "${2:-}" ;;
    add-cafe-entry) add_cafe_entry ;;
    pad-node) pad_node ;;
    *) sed -n '2,/^set -u/p' "$0" | sed 's/^# \{0,1\}//' | head -n -1; exit 2 ;;
esac
