#!/bin/bash
# The unit tests as CI's "Unit tests (ASan, UBSan)" job runs them: that job (unit_sanitizers in
# .github/workflows/build.yml) runs this file in the Debian image, and it runs in either harness
# image (streamflex-test or streamflex-test-fedora): a copy of /src built with ASan, UBSan and the
# extra warnings, as the user tester, then ctest. The repository is mounted read-only and never
# written; everything happens in the copy at /work.
#   bash /src/tests/unit.sh [CTEST-REGEX]   CTEST-REGEX: ctest's -R filter; none runs every test
# UNIT_TARGETS, when set, builds only those targets (space-separated), as before the launcher builds.
# MUTANT_FILE and MUTANT_SED, when set, apply one sed program to that file in the copy only: a unit
# mutant. MUTANT_FIND and MUTANT_REPLACE, set in MUTANT_SED's place, replace that exact text, which
# must be in the file MUTANT_COUNT times (once when unset), as tests/mutants/mutants.py does. A mutant
# that changes nothing, or a configure or build that fails, stops the run before ctest (exit 2), so
# none counts as a kill. A warning outside src/external stops the run too (exit 1), but in a mutant's
# build it is printed and ctest runs: a mutant that leaves a variable unused is still judged by its
# tests. It runs tests/check-no-text-input.sh on the copy first, which stops the run (exit 1) when
# src/ starts SDL's text input.
set -u
rm -rf /work; mkdir -p /work
tar -C /src --exclude=./build --exclude=./.git --exclude=./headless-out --exclude=./.superpowers -cf - . | tar -C /work -xf -
if [ -n "${MUTANT_FILE:-}" ]; then
    if [ -n "${MUTANT_FIND:-}" ]; then
        python3 - "/work/$MUTANT_FILE" <<'EOF' || exit 2
import os
import sys

path = sys.argv[1]
text = open(path, encoding="utf-8").read()
want = int(os.environ.get("MUTANT_COUNT") or "1")
found = text.count(os.environ["MUTANT_FIND"])
if found != want:
    print(f"MUTANT NOT APPLIED to {os.environ['MUTANT_FILE']}: the text is there {found} times, not {want}")
    sys.exit(2)
open(path, "w", encoding="utf-8", newline="\n").write(text.replace(os.environ["MUTANT_FIND"], os.environ.get("MUTANT_REPLACE", "")))
EOF
    else
        sed -i -e "$MUTANT_SED" "/work/$MUTANT_FILE"
    fi
    cmp -s "/src/$MUTANT_FILE" "/work/$MUTANT_FILE" && { echo "MUTANT NOT APPLIED to $MUTANT_FILE"; exit 2; }
    echo "MUTANT applied to $MUTANT_FILE:"; diff "/src/$MUTANT_FILE" "/work/$MUTANT_FILE"
fi
# The no-OS-keyboard rule, on the copy (a mutant's change included): CI's unit job runs this file
bash /work/tests/check-no-text-input.sh /work || exit 1
chown -R tester /work
flags="-fsanitize=address,undefined -fno-omit-frame-pointer"
su tester -c "cmake -S /work -B /work/build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_FLAGS='$flags' -DCMAKE_EXE_LINKER_FLAGS='-fsanitize=address,undefined' -DEXTRA_WARNINGS=ON" > /tmp/configure.log 2>&1 \
    || { echo "CONFIGURE FAILED"; tail -30 /tmp/configure.log; exit 2; }
targets=
[ -z "${UNIT_TARGETS:-}" ] || targets="--target $UNIT_TARGETS"
su tester -c "cmake --build /work/build -j$(nproc) $targets" > /tmp/build.log 2>&1 \
    || { echo "BUILD FAILED"; grep -E 'error' /tmp/build.log | head -30; tail -10 /tmp/build.log; exit 2; }
ours=$(grep -iE 'warning:' /tmp/build.log | grep -vE '^g?make(\[[0-9]+\])?: ' | grep -v '^/work/src/external/')
if [ -n "$ours" ]; then
    echo "WARNINGS OUTSIDE src/external"; echo "$ours"
    [ -n "${MUTANT_FILE:-}" ] || exit 1
    echo "MUTANT: the warnings above are the mutant's build's; its tests run"
fi
filter=
[ -z "${1:-}" ] || filter="-R '$1'"
su tester -c "UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 setarch $(uname -m) -R ctest --test-dir /work/build --output-on-failure $filter"
