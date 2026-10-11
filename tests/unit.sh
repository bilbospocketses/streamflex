#!/bin/bash
# The unit tests as CI's "Unit tests (ASan, UBSan)" job runs them (.github/workflows/build.yml,
# unit_sanitizers), in either harness image (streamflex-test or streamflex-test-fedora): a copy of
# /src built with ASan, UBSan and the extra warnings, as the user tester, then ctest. The repository
# is mounted read-only and never written; everything happens in the copy at /work.
#   bash /src/tests/unit.sh [CTEST-REGEX]   CTEST-REGEX: ctest's -R filter; none runs every test
# UNIT_TARGETS, when set, builds only those targets (space-separated), as before the launcher builds.
# MUTANT_FILE and MUTANT_SED, when set, apply one sed program to that file in the copy only: a unit
# mutant. A mutant that changes nothing, a configure or build that fails, or a warning outside
# src/external stops the run before ctest (exit 2, or 1 for the warnings), so none counts as a kill.
set -u
rm -rf /work; mkdir -p /work
tar -C /src --exclude=./build --exclude=./.git --exclude=./headless-out --exclude=./.superpowers -cf - . | tar -C /work -xf -
if [ -n "${MUTANT_FILE:-}" ]; then
    sed -i -e "$MUTANT_SED" "/work/$MUTANT_FILE"
    cmp -s "/src/$MUTANT_FILE" "/work/$MUTANT_FILE" && { echo "MUTANT NOT APPLIED to $MUTANT_FILE"; exit 2; }
    echo "MUTANT applied to $MUTANT_FILE:"; diff "/src/$MUTANT_FILE" "/work/$MUTANT_FILE"
fi
chown -R tester /work
flags="-fsanitize=address,undefined -fno-omit-frame-pointer"
su tester -c "cmake -S /work -B /work/build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_FLAGS='$flags' -DCMAKE_EXE_LINKER_FLAGS='-fsanitize=address,undefined' -DEXTRA_WARNINGS=ON" > /tmp/configure.log 2>&1 \
    || { echo "CONFIGURE FAILED"; tail -30 /tmp/configure.log; exit 2; }
targets=
[ -z "${UNIT_TARGETS:-}" ] || targets="--target $UNIT_TARGETS"
su tester -c "cmake --build /work/build -j$(nproc) $targets" > /tmp/build.log 2>&1 \
    || { echo "BUILD FAILED"; grep -E 'error' /tmp/build.log | head -30; tail -10 /tmp/build.log; exit 2; }
ours=$(grep -iE 'warning:' /tmp/build.log | grep -vE '^g?make(\[[0-9]+\])?: ' | grep -v '^/work/src/external/')
[ -z "$ours" ] || { echo "WARNINGS OUTSIDE src/external"; echo "$ours"; exit 1; }
filter=
[ -z "${1:-}" ] || filter="-R '$1'"
su tester -c "UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 setarch $(uname -m) -R ctest --test-dir /work/build --output-on-failure $filter"
