#!/usr/bin/env bash
# The Phase 0 probe kit's Linux check: builds tools/overlay-phase0 with the system SDL3 (no vcpkg) and -Wall -Wextra,
# prints every warning, then runs the whole tests folder against the built binaries. It runs inside the image
# Dockerfile.linux makes, with the repo mounted read-only at /src, from the repo root on the host:
#
#   docker build -t cptk-overlay-p0-linux:local -f tools/overlay-phase0/ci/Dockerfile.linux tools/overlay-phase0/ci
#   docker run --rm -v "$PWD:/src:ro" cptk-overlay-p0-linux:local bash /src/tools/overlay-phase0/ci/linux-check.sh
#
# A plain container has no /dev/uinput, no evdev devices and no display, and the check grants none: a test that needs
# one skips or fails, and the summary says which. Exits 0 only when the build and every suite pass.
set -u -o pipefail

kit=/tmp/kit
build=/tmp/kit-build
rm -rf "$kit" "$build"
cp -a /src/. "$kit"

echo "== toolchain"
cc --version | head -n 1
cmake --version | head -n 1
echo "SDL3 $(pkg-config --modversion sdl3)"
python3 --version
echo "node $(node --version)"

echo "== configure"
cmake -S "$kit/tools/overlay-phase0" -B "$build" -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_FLAGS="-Wall -Wextra" \
    || { echo "RESULT: configure failed"; exit 1; }

echo "== build"
# One job, and -k to go on past an error, so every warning and error of every file is in the log, in order.
cmake --build "$build" -- -k -j1 2>&1 | tee /tmp/build.log
build_rc=$?

echo "== warnings"
grep -E ': warning: ' /tmp/build.log || echo "(none)"
warnings=$(grep -cE ': warning: ' /tmp/build.log)
errors=$(grep -cE ': (fatal )?error: ' /tmp/build.log)
echo "BUILD: exit $build_rc, $errors error(s), $warnings warning(s)"
if [ "$build_rc" -ne 0 ] || [ ! -x "$build/probe_host" ] || [ ! -x "$build/probe_spawn" ]; then
    echo "RESULT: build failed; the tests need both binaries"
    exit 1
fi

echo "== tests"
PROBE_HOST="$build/probe_host" PROBE_SPAWN="$build/probe_spawn" \
    python3 -m unittest discover -s "$kit/tools/overlay-phase0/tests" -v 2>&1 | tee /tmp/tests.log
tests_rc=$?

echo "== skipped"
grep -E '\.\.\. skipped' /tmp/tests.log || echo "(none)"
echo "== summary"
grep -E '^(Ran [0-9]+ tests|OK|FAILED)' /tmp/tests.log
echo "RESULT: build exit $build_rc ($errors error(s), $warnings warning(s)), tests exit $tests_rc"
exit "$tests_rc"
