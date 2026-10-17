#!/bin/bash
# The no-OS-keyboard rule (spec, "No OS keyboard, ever"): StreamFlex never starts SDL's text input, which is
# what brings up GNOME's, KDE's and Windows' screen keyboards, and so has no handler for the text events only
# text input sends. This fails when any of them appears anywhere under src/ outside a comment line (one that
# starts with //, /*, or a * followed by a space, a / or the line's end, which may name the calls it rules
# out; a line such as *started = SDL_StartTextInput(w); is code), and when there is no src/ to read.
#   bash tests/check-no-text-input.sh [ROOT]   ROOT: the source tree; this file's parent folder by default
set -u
root=${1:-$(dirname "$0")/..}
if [ ! -d "$root/src" ]; then
    echo "NO SOURCE TREE: $root/src is not a folder, so nothing was checked"
    exit 1
fi
found=$(grep -rnE 'SDL_StartTextInput|SDL_EVENT_TEXT_INPUT|SDL_EVENT_TEXT_EDITING' "$root/src" \
        | grep -vE '^[^:]+:[0-9]+:[[:space:]]*(//|/\*|\*([[:space:]/]|$))')
if [ -n "$found" ]; then
    echo "SDL TEXT INPUT FOUND: StreamFlex must never start text input (spec, \"No OS keyboard, ever\")"
    echo "$found"
    exit 1
fi
echo "No SDL text input in src/"
