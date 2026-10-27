# Streaming overlay Phase 0 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or
> superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the throwaway probe kit Phase 0 measures with, publish the probe extension as its own listing, CPTK
Overlay Probe 0.0.1 (unlisted), in all three stores, and write the guest protocol (G1-G16) and the user's run sheet
(H1-H4), so that qa-harness-sf and the user can produce every result the spec's Phase 0 asks for. The real CPTK
Overlay listing is not created until R1.

**Architecture:** A self-contained kit under `tools/overlay-phase0/`, never built by the root `CMakeLists.txt` and
never shipped: a C native-messaging probe host, a C spawner (process supervision, key injection, key logging, SDL3
gamepad and focus probes), a plain-JavaScript probe extension in both flavors with a Python build script, two probe
pages, and setup scripts that write the policy, host registration and Firefox profile the spec describes. The
measurements themselves run later, in qa-harness guests and on the user's HTPC; this plan ends with their protocol,
their run sheet and the results template.

**Tech Stack:** C17, SDL3 (floor 3.2.10), CMake 3.18+, Python 3 (`unittest`), PowerShell 5.1-compatible scripts for
Windows, WebExtensions MV3, `web-ext` for AMO signing.

**Spec:** `design/specs/2026-10-27-streaming-overlay-design.md`, section **Phase 0: measure before building**.

## Global Constraints

- **No product code:** nothing under `src/`, `assets/`, `config/` or the root `CMakeLists.txt` changes. The kit is
  `tools/overlay-phase0/` only, plus documents in `design/research/overlay/phase0/`.
- **Names are final from day one:** the probe is published as its own listing, extension name `CPTK Overlay Probe`,
  gecko id `overlay-probe@cptk.app`, version `0.0.1`. The real CPTK Overlay listing (gecko id `overlay@cptk.app`)
  is not created until R1. The native host `app.cptk.overlay` is unchanged.
- **Store update URLs:** Chrome `https://clients2.google.com/service/update2/crx`; Edge
  `https://edge.microsoft.com/extensionwebstorebase/v1/crx`.
- **The launch token** is 32 lowercase hex characters in the first URL's fragment as `cptk=<token>`.
- **Native Messaging framing:** a 32-bit native-endian length, then UTF-8 JSON; extension-to-host frames are capped
  at 64 MiB.
- **SDL3 floor 3.2.10**; C17; MSVC and GCC/Clang both build the kit.
- **Windows scripts run in Windows PowerShell 5.1** (a guest may have no PowerShell 7). Linux scripts are Python 3.
- **American spelling** everywhere (the pre-push gate checks added lines and commit messages).
- **Test hang guards are generous** (120 s or more) and a timeout names what hung; no test asserts a duration.
- **Heavy work goes through the box queue:** building the kit is light, except its first configure, which installs
  the root manifest's vcpkg packages and is declared first (Task 1 Step 3); any VM or guest run is qa-harness-sf's,
  booked by it.

## Review Focus

1. **A truncated frame** (the browser dies mid-message): the host must log the end and exit 0, never hang
   (Task 1 test `test_truncated_frame_ends_cleanly`).
2. **A chord with one unknown part** (`Alt+Foo`): the spawner must refuse the whole command before injecting
   anything, so a guest never receives half a chord (Task 3 test `test_unknown_chord_part_injects_nothing`).
3. **Setup run twice, or beside another product's force-install entry:** no duplicate entry, and the other entry
   survives (Task 6 tests `test_setup_is_idempotent_and_keeps_foreign_entries` and the Windows twin).
4. **A `user.js` the user already wrote:** profile preparation keeps every line that is not its own
   (Task 6 test `test_profile_keeps_user_lines`, in `FirefoxProfileTest` and its PowerShell twin
   `FirefoxProfilePsTest`).
5. **The child exits before any `close`:** the spawner reports the exit with `ms_since_close` of -1, not a garbage
   interval (Task 2 test `test_run_reports_exit_code`).

---

### Task 1: The kit's build and the probe host

**Files:**
- Create: `tools/overlay-phase0/CMakeLists.txt`
- Create: `tools/overlay-phase0/src/common.h`, `tools/overlay-phase0/src/common.c`
- Create: `tools/overlay-phase0/src/probe_host.c`
- Create: `tools/overlay-phase0/tests/test_host.py`
- Create: `tools/overlay-phase0/README.md`
- Modify: `.gitignore` (the kit's build tree, extension packages and Python caches)

**Interfaces:**
- Produces: `int64_t probe_now_ms(void)` (wall clock, ms since the epoch); `void probe_emit(const char *fields)`
  (prints `{"t":<ms>,<fields>}` and a newline to stdout, flushed); `void probe_json_string(char *out, size_t cap,
  const char *s)` (writes `s` into `out` as a quoted JSON string; a NULL `s` is `""`; it stops early rather than
  overflow, and `out` is always a whole quoted string). Every string the kit prints that it did not write itself
  goes through `probe_json_string`. The executable `probe_host`, which logs to
  `<dir>/probe.log`, where `<dir>` is `CPTK_PROBE_LOG_DIR` if set, else `%LOCALAPPDATA%\cptk-probe` (Windows) or
  `~/.cptk-probe` (Linux). Log lines: `{"t","pid","event":"start","argv":[...]}`, `{"t","pid","event":"msg","seq",
  "body":<the message>}`, `{"t","pid","event":"oversize","len"}`, `{"t","pid","event":"empty"}` (a zero-length
  frame), `{"t","pid","event":"end"}`. After `oversize` or `empty` the host exits 1. Reply per message:
  `{"type":"ack","seq":<n>,"pid":<pid>}`.

- [ ] **Step 1: Write the failing test**

`tools/overlay-phase0/tests/test_host.py`:

```python
"""Tests for the Phase 0 probe host. PROBE_HOST names the built executable."""
import json
import os
import struct
import subprocess
import tempfile
import unittest

HOST = os.environ.get("PROBE_HOST", "")


def frame(obj):
    body = json.dumps(obj).encode("utf-8")
    return struct.pack("=I", len(body)) + body


def unframe(data):
    out = []
    while data:
        (n,) = struct.unpack("=I", data[:4])
        out.append(json.loads(data[4 : 4 + n]))
        data = data[4 + n :]
    return out


@unittest.skipUnless(HOST, "set PROBE_HOST to the built probe_host")
class HostTest(unittest.TestCase):
    def run_host(self, data):
        log_dir = tempfile.mkdtemp()
        env = dict(os.environ, CPTK_PROBE_LOG_DIR=log_dir)
        try:
            proc = subprocess.run(
                [HOST, "chrome-extension://abc/"], input=data, capture_output=True, env=env, timeout=120
            )
        except subprocess.TimeoutExpired:
            self.fail("HUNG: probe_host did not exit after its stdin closed")
        with open(os.path.join(log_dir, "probe.log"), encoding="utf-8") as f:
            lines = [json.loads(line) for line in f]
        return proc, lines

    def test_acks_each_message_and_logs_it(self):
        proc, lines = self.run_host(
            frame({"type": "report", "event": "start"}) + frame({"type": "report", "event": "key"})
        )
        self.assertEqual(proc.returncode, 0)
        self.assertEqual([r["seq"] for r in unframe(proc.stdout)], [1, 2])
        self.assertEqual([line["event"] for line in lines], ["start", "msg", "msg", "end"])
        self.assertEqual(lines[0]["argv"][1], "chrome-extension://abc/")
        self.assertEqual(lines[2]["body"]["event"], "key")

    def test_refuses_an_oversize_frame(self):
        proc, lines = self.run_host(struct.pack("=I", 0x7FFFFFFF))
        self.assertEqual(proc.returncode, 1)
        self.assertEqual(lines[-1]["event"], "oversize")
        self.assertEqual(lines[-1]["len"], 0x7FFFFFFF)

    def test_refuses_an_empty_frame(self):
        proc, lines = self.run_host(struct.pack("=I", 0))
        self.assertEqual(proc.returncode, 1)
        self.assertEqual(lines[-1]["event"], "empty")
        self.assertNotIn("len", lines[-1])

    def test_truncated_frame_ends_cleanly(self):
        proc, lines = self.run_host(struct.pack("=I", 100) + b'{"type":')
        self.assertEqual(proc.returncode, 0)
        self.assertEqual([line["event"] for line in lines], ["start", "end"])


if __name__ == "__main__":
    unittest.main()
```

- [ ] **Step 2: Write the build files and the shared helpers**

`tools/overlay-phase0/CMakeLists.txt`:

```cmake
# The Phase 0 probe kit (design/plans/2026-10-27-overlay-phase0.md). Not part of StreamFlex's build.
cmake_minimum_required(VERSION 3.18)
project(cptk_overlay_phase0 LANGUAGES C)
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
if (MSVC)
  add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
endif ()

add_library(probe_common STATIC src/common.c)
target_include_directories(probe_common PUBLIC src)

add_executable(probe_host src/probe_host.c)
target_link_libraries(probe_host PRIVATE probe_common)
```

`tools/overlay-phase0/src/common.h`:

```c
/* Shared helpers for the Phase 0 probe kit. */
#ifndef PROBE_COMMON_H
#define PROBE_COMMON_H

#include <stddef.h>
#include <stdint.h>

/* Wall-clock time in milliseconds since the Unix epoch. */
int64_t probe_now_ms(void);

/* Print one JSON event line, {"t":<now>,<fields>}, to stdout and flush it. */
void probe_emit(const char *fields);

/* Write s into out as a quoted JSON string (a NULL s is ""). Stops early rather than overflow, so out is always a
   whole quoted string; cap must be at least 3. */
void probe_json_string(char *out, size_t cap, const char *s);

#endif
```

`tools/overlay-phase0/src/common.c`:

```c
#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif
#include "common.h"

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>

int64_t probe_now_ms(void)
{
    FILETIME ft;
    ULARGE_INTEGER u;
    GetSystemTimeAsFileTime(&ft);
    u.LowPart = ft.dwLowDateTime;
    u.HighPart = ft.dwHighDateTime;
    return (int64_t)((u.QuadPart - 116444736000000000ULL) / 10000ULL);
}
#else
#include <time.h>

int64_t probe_now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}
#endif

void probe_emit(const char *fields)
{
    printf("{\"t\":%lld,%s}\n", (long long)probe_now_ms(), fields);
    fflush(stdout);
}

void probe_json_string(char *out, size_t cap, const char *s)
{
    size_t n = 0;
    if (cap < 3) {
        if (cap)
            out[0] = '\0';
        return;
    }
    out[n++] = '"';
    for (const unsigned char *p = (const unsigned char *)(s ? s : ""); *p; p++) {
        char esc[8];
        size_t len = 1;
        if (*p == '"' || *p == '\\') {
            esc[0] = '\\';
            esc[1] = (char)*p;
            len = 2;
        } else if (*p < 0x20) {
            len = (size_t)snprintf(esc, sizeof esc, "\\u%04x", *p);
        } else {
            esc[0] = (char)*p;
        }
        if (n + len + 2 > cap)
            break; /* room for the closing quote and the NUL */
        memcpy(out + n, esc, len);
        n += len;
    }
    out[n++] = '"';
    out[n] = '\0';
}
```

Append to the repo root's `.gitignore`:

```
# The Phase 0 probe kit's build tree, extension packages and Python caches
/build-phase0/
tools/overlay-phase0/extension/dist/
tools/overlay-phase0/tests/__pycache__/
tools/overlay-phase0/setup/__pycache__/
```

- [ ] **Step 3: Declare the first configure to the box queue**

The first configure runs vcpkg in manifest mode on the repo root's `vcpkg.json`, so it installs every package there
(SDL3 among them) for the default `x64-windows` triplet. That can take 10 minutes or more, which the box queue counts
as BIG. Declare it before Step 4 runs it, with this session's id:

```powershell
python C:/Users/jscha/.claude/hooks/box_queue.py request --session <session id> --work "vcpkg configure of the overlay Phase 0 kit (first install of the root manifest)" --overlap ok --touches workdir:C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec --big
```

Expected: `START`. A `WAIT` answer holds a place in line: run the same command with `--wait` added, alone and in the
background, and run Step 4's configure in the next call once it answers `START`. Later configures reuse the installed
packages and need no declaration.

- [ ] **Step 4: Configure, to verify the build fails**

Windows (the one configure command this plan uses; Step 6 repeats it, and Tasks 2-4 only rebuild):

```powershell
$env:Path += ";C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin"
cmake -S C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec/tools/overlay-phase0 -B C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec/build-phase0 -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" -DVCPKG_MANIFEST_DIR=C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec
```

Expected: vcpkg installs the root manifest's packages (the first time only), then FAIL at configure:
`Cannot find source file: src/probe_host.c`.

- [ ] **Step 5: Write the host**

`tools/overlay-phase0/src/probe_host.c`:

```c
/* Phase 0 probe native-messaging host: logs every frame as one JSON line and acknowledges it. */
#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#define PROBE_PID ((unsigned long)GetCurrentProcessId())
#define PROBE_SEP "\\"
#else
#include <sys/stat.h>
#include <unistd.h>
#define PROBE_PID ((unsigned long)getpid())
#define PROBE_SEP "/"
#endif

#define PROBE_MAX_IN (64u * 1024u * 1024u)

static FILE *open_log(void)
{
    char dir[1024];
    char path[1100];
    const char *override = getenv("CPTK_PROBE_LOG_DIR");
#ifdef _WIN32
    const char *base = getenv("LOCALAPPDATA");
    if (override)
        snprintf(dir, sizeof dir, "%s", override);
    else
        snprintf(dir, sizeof dir, "%s\\cptk-probe", base ? base : ".");
    CreateDirectoryA(dir, NULL);
#else
    const char *base = getenv("HOME");
    if (override)
        snprintf(dir, sizeof dir, "%s", override);
    else
        snprintf(dir, sizeof dir, "%s/.cptk-probe", base ? base : ".");
    mkdir(dir, 0700);
#endif
    snprintf(path, sizeof path, "%s" PROBE_SEP "probe.log", dir);
    return fopen(path, "ab");
}

static void log_start(FILE *f, const char *event)
{
    fprintf(f, "{\"t\":%lld,\"pid\":%lu,\"event\":\"%s\"", (long long)probe_now_ms(), PROBE_PID, event);
}

int main(int argc, char **argv)
{
#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif
    FILE *log = open_log();
    if (!log)
        return 2;

    log_start(log, "start");
    fputs(",\"argv\":[", log);
    for (int i = 0; i < argc; i++) {
        char arg[4096];
        probe_json_string(arg, sizeof arg, argv[i]);
        if (i)
            fputc(',', log);
        fputs(arg, log);
    }
    fputs("]}\n", log);
    fflush(log);

    unsigned long seq = 0;
    for (;;) {
        uint32_t len;
        if (fread(&len, sizeof len, 1, stdin) != 1)
            break; /* the port closed */
        if (len == 0 || len > PROBE_MAX_IN) {
            log_start(log, len == 0 ? "empty" : "oversize");
            if (len)
                fprintf(log, ",\"len\":%lu", (unsigned long)len);
            fputs("}\n", log);
            fclose(log);
            return 1;
        }
        char *buf = malloc(len);
        if (!buf || fread(buf, 1, len, stdin) != len) {
            free(buf);
            break; /* truncated: the browser went away mid-frame */
        }
        seq++;
        log_start(log, "msg");
        fprintf(log, ",\"seq\":%lu,\"body\":", seq);
        fwrite(buf, 1, len, log);
        fputs("}\n", log);
        fflush(log);
        free(buf);

        char reply[96];
        int n = snprintf(reply, sizeof reply, "{\"type\":\"ack\",\"seq\":%lu,\"pid\":%lu}", seq, PROBE_PID);
        uint32_t rlen = (uint32_t)n;
        fwrite(&rlen, sizeof rlen, 1, stdout);
        fwrite(reply, 1, (size_t)n, stdout);
        fflush(stdout);
    }
    log_start(log, "end");
    fputs("}\n", log);
    fclose(log);
    return 0;
}
```

- [ ] **Step 6: Build and run the tests to verify they pass**

Windows (Visual Studio generator; Step 4's configure command, unchanged):

```powershell
$env:Path += ";C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin"
cmake -S C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec/tools/overlay-phase0 -B C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec/build-phase0 -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" -DVCPKG_MANIFEST_DIR=C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec
cmake --build C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec/build-phase0 --config Release --target probe_host
$env:PROBE_HOST = 'C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec/build-phase0/Release/probe_host.exe'
python -m unittest discover -s C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec/tools/overlay-phase0/tests -p test_host.py -v
```

Run all five lines in one call: each call starts a new shell, and without `PROBE_HOST` every test skips.
Expected: `Ran 4 tests`, then `OK` with no `skipped=` (nothing skips on Windows).

- [ ] **Step 7: Write the kit's README**

`tools/overlay-phase0/README.md`:

````markdown
# Streaming overlay Phase 0 probe kit

Throwaway tools for the measurements in `design/specs/2026-10-27-streaming-overlay-design.md` (Phase 0). Nothing
here ships, and StreamFlex's own build never compiles it. The protocol that uses it is
`design/research/overlay/phase0/guest-protocol.md`; the user's run sheet is `design/research/overlay/phase0/run-sheet.md`.

## Build

Windows (vcpkg; the repo root's `vcpkg.json` supplies SDL3):

```powershell
cmake -S tools/overlay-phase0 -B build-phase0 -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" -DVCPKG_MANIFEST_DIR="$PWD"
cmake --build build-phase0 --config Release
```

Linux (the distro's SDL3 development package):

```sh
cmake -S tools/overlay-phase0 -B build-phase0 -DCMAKE_BUILD_TYPE=Release
cmake --build build-phase0
```

## Test

```sh
PROBE_HOST=build-phase0/probe_host PROBE_SPAWN=build-phase0/probe_spawn python3 -m unittest discover -s tools/overlay-phase0/tests -v
```

## What is here

| Piece | Job |
|---|---|
| `probe_host` | The native-messaging host `app.cptk.overlay`: logs every message to `probe.log` and acknowledges it. |
| `probe_spawn` | `run` (supervise a browser), `keys` (inject keys), `keylog` (log raw keys), `pad` (gamepad in the background), `focus` (does the launcher get focus back). |
| `extension/` | CPTK Overlay Probe 0.0.1, the probe extension, and `build.py`, which makes the Chromium and Firefox packages. |
| `pages/` | `probe.html` (fullscreen and media-start targets), `keylog.html` (what a page sees), `serve.ps1` (a page server for Windows guests). |
| `setup/` | `windows.ps1`, `linux_setup.py`: policy and host registration. `firefox-profile.ps1`, `firefox_profile.py`: a Firefox profile with the sideloaded XPI. |
| `ids.json` | The three store ids, recorded when the probe is published. |
````

- [ ] **Step 8: Commit**

```powershell
git -C C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec add .gitignore tools/overlay-phase0/CMakeLists.txt tools/overlay-phase0/src/common.h tools/overlay-phase0/src/common.c tools/overlay-phase0/src/probe_host.c tools/overlay-phase0/tests/test_host.py tools/overlay-phase0/README.md
git -C C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec commit -m "tools: add the Phase 0 probe host"
```

---

### Task 2: The spawner's `run` command (process supervision)

**Files:**
- Create: `tools/overlay-phase0/src/spawn.h`, `tools/overlay-phase0/src/spawn_main.c`
- Create: `tools/overlay-phase0/src/proc_win32.c`, `tools/overlay-phase0/src/proc_linux.c`
- Modify: `tools/overlay-phase0/CMakeLists.txt`
- Create: `tools/overlay-phase0/tests/test_spawn.py`

**Interfaces:**
- Consumes: `probe_now_ms`, `probe_emit` (Task 1).
- Produces: `int probe_run(int argc, char **argv)`, where `argv[0]` is the absolute path of the program and the rest
  its arguments. It prints `{"t","event":"started","pid"}`, reads `close` and `kill` lines from stdin, prints
  `close_sent` / `kill_sent`, and ends with `{"t","event":"exited","code","ms_since_close"}` once the program **and
  every process it started** are gone (Windows: the Job Object's active-process-zero; Linux: the leader reaped and
  the process group empty). `ms_since_close` is -1 when no `close` came. A Windows `close` posts `WM_CLOSE` to every
  visible top-level window of the job's processes; a Linux `close` sends `SIGTERM` to the group. `kill` ends the job
  (exit code 137) or sends `SIGKILL` to the group. `spawn.h` also declares the commands Tasks 3 and 4 add.

- [ ] **Step 1: Write the failing test**

`tools/overlay-phase0/tests/test_spawn.py`:

```python
"""Tests for probe_spawn. PROBE_SPAWN names the built executable."""
import json
import os
import signal
import subprocess
import sys
import threading
import unittest

SPAWN = os.environ.get("PROBE_SPAWN", "")


def events(text):
    return [json.loads(line) for line in text.splitlines() if line.startswith("{")]


@unittest.skipUnless(SPAWN, "set PROBE_SPAWN to the built probe_spawn")
class RunTest(unittest.TestCase):
    def test_run_reports_exit_code(self):
        proc = subprocess.run(
            [SPAWN, "run", sys.executable, "-c", "import sys; sys.exit(3)"],
            input="", capture_output=True, text=True, timeout=120,
        )
        evs = events(proc.stdout)
        self.assertEqual([e["event"] for e in evs], ["started", "exited"])
        self.assertEqual(evs[1]["code"], 3)
        self.assertEqual(evs[1]["ms_since_close"], -1)

    def test_kill_ends_the_child(self):
        proc = subprocess.Popen(
            [SPAWN, "run", sys.executable, "-c", "import time; time.sleep(600)"],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True,
        )
        child = None
        try:
            first = []
            reader = threading.Thread(target=lambda: first.append(proc.stdout.readline()), daemon=True)
            reader.start()
            reader.join(120)
            if reader.is_alive():
                self.fail("HUNG: probe_spawn run printed no started line")
            started = json.loads(first[0])
            self.assertEqual(started["event"], "started")
            child = started["pid"]
            proc.stdin.write("kill\n")
            proc.stdin.flush()
            try:
                out, _ = proc.communicate(timeout=120)
            except subprocess.TimeoutExpired:
                self.fail("HUNG: probe_spawn run did not report the killed child's exit")
            evs = events(out)
            self.assertEqual([e["event"] for e in evs], ["kill_sent", "exited"])
            self.assertEqual(evs[1]["code"], 137)
        finally:
            if proc.poll() is None:
                proc.kill()  # on Windows the job closes with it and ends the child
                proc.wait(timeout=120)
            if child and os.name == "posix":
                try:
                    os.killpg(child, signal.SIGKILL)
                except ProcessLookupError:
                    pass

    def test_run_needs_a_program(self):
        proc = subprocess.run([SPAWN, "run"], capture_output=True, text=True, timeout=120)
        self.assertEqual(proc.returncode, 2)


if __name__ == "__main__":
    unittest.main()
```

- [ ] **Step 2: Run it to verify it fails**

```powershell
$env:PROBE_SPAWN = 'C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec/build-phase0/Release/probe_spawn.exe'
python -m unittest discover -s C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec/tools/overlay-phase0/tests -p test_spawn.py -v
```

Run both lines in one call. Expected: `Ran 3 tests`, then `FAILED (errors=3)`: each test ERRORS with
`FileNotFoundError`, because the executable does not exist yet. No `skipped=`.

- [ ] **Step 3: Write the header and the command dispatcher**

`tools/overlay-phase0/src/spawn.h`:

```c
/* probe_spawn's commands. Each takes the arguments after its own name and returns the process exit code. */
#ifndef PROBE_SPAWN_H
#define PROBE_SPAWN_H

int probe_run(int argc, char **argv);    /* run <program> [args...]; stdin lines: close | kill */
int probe_keys(int argc, char **argv);   /* keys [--dry-run] <delay_ms> <key>[+<key>...] ... */
int probe_keylog(int argc, char **argv); /* keylog <seconds> */
int probe_pad(int argc, char **argv);    /* pad <seconds> <default|xinput|rawinput|wgi> */
int probe_focus(int argc, char **argv);  /* focus <program> [args...] */

#endif
```

`tools/overlay-phase0/src/spawn_main.c`:

```c
/* probe_spawn: the Phase 0 spawner. One command per run; every result is a JSON line on stdout. */
#include <stdio.h>
#include <string.h>

#include "spawn.h"

static int usage(void)
{
    fputs("usage: probe_spawn run|keys|keylog|pad|focus ...\n", stderr);
    return 2;
}

int main(int argc, char **argv)
{
    if (argc < 2)
        return usage();
    const char *cmd = argv[1];
    if (strcmp(cmd, "run") == 0)
        return probe_run(argc - 2, argv + 2);
    if (strcmp(cmd, "keys") == 0)
        return probe_keys(argc - 2, argv + 2);
    if (strcmp(cmd, "keylog") == 0)
        return probe_keylog(argc - 2, argv + 2);
    if (strcmp(cmd, "pad") == 0)
        return probe_pad(argc - 2, argv + 2);
    if (strcmp(cmd, "focus") == 0)
        return probe_focus(argc - 2, argv + 2);
    return usage();
}
```

Until Tasks 3 and 4 land, add temporary stubs at the end of `spawn_main.c` so the executable links (Task 3 Step 3
and Task 4 Step 3 delete them):

```c
int probe_keys(int argc, char **argv) { (void)argc; (void)argv; return 2; }
int probe_keylog(int argc, char **argv) { (void)argc; (void)argv; return 2; }
int probe_pad(int argc, char **argv) { (void)argc; (void)argv; return 2; }
int probe_focus(int argc, char **argv) { (void)argc; (void)argv; return 2; }
```

- [ ] **Step 4: Write the Windows supervisor**

`tools/overlay-phase0/src/proc_win32.c`:

```c
/* probe_spawn run, Windows: a Job Object with a completion port around a suspended start. */
#include <windows.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "spawn.h"

static HANDLE g_job;
static volatile LONG g_cmd; /* 0 none, 1 close, 2 kill */

static DWORD WINAPI stdin_thread(LPVOID unused)
{
    char line[64];
    (void)unused;
    while (fgets(line, sizeof line, stdin)) {
        if (strncmp(line, "close", 5) == 0)
            InterlockedExchange(&g_cmd, 1);
        else if (strncmp(line, "kill", 4) == 0)
            InterlockedExchange(&g_cmd, 2);
    }
    return 0;
}

static BOOL CALLBACK close_if_in_job(HWND hwnd, LPARAM unused)
{
    DWORD pid = 0;
    (void)unused;
    if (!IsWindowVisible(hwnd))
        return TRUE;
    GetWindowThreadProcessId(hwnd, &pid);
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (h) {
        BOOL in = FALSE;
        if (IsProcessInJob(h, g_job, &in) && in)
            PostMessageW(hwnd, WM_CLOSE, 0, 0);
        CloseHandle(h);
    }
    return TRUE;
}

/* Appends one argument to the command line, quoted by the rules CommandLineToArgvW reverses. */
static void append_arg(wchar_t *out, size_t cap, const char *utf8)
{
    wchar_t w[4096];
    if (!MultiByteToWideChar(CP_UTF8, 0, utf8, -1, w, 4096))
        w[0] = L'\0';
    size_t len = wcslen(out);
    if (len && len + 1 < cap)
        out[len++] = L' ';
    if (len + 1 < cap)
        out[len++] = L'"';
    size_t slashes = 0;
    for (const wchar_t *p = w; *p && len + 3 < cap; p++) {
        if (*p == L'\\') {
            slashes++;
            out[len++] = L'\\';
            continue;
        }
        if (*p == L'"') {
            for (size_t i = 0; i < slashes + 1 && len + 2 < cap; i++)
                out[len++] = L'\\';
        }
        slashes = 0;
        out[len++] = *p;
    }
    for (size_t i = 0; i < slashes && len + 2 < cap; i++)
        out[len++] = L'\\';
    if (len + 1 < cap)
        out[len++] = L'"';
    out[len] = L'\0';
}

int probe_run(int argc, char **argv)
{
    if (argc < 1) {
        fputs("usage: probe_spawn run <program> [args...]\n", stderr);
        return 2;
    }
    static wchar_t cmdline[32768];
    cmdline[0] = L'\0';
    for (int i = 0; i < argc; i++)
        append_arg(cmdline, sizeof cmdline / sizeof cmdline[0], argv[i]);

    g_job = CreateJobObjectW(NULL, NULL);
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION lim;
    memset(&lim, 0, sizeof lim);
    lim.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    SetInformationJobObject(g_job, JobObjectExtendedLimitInformation, &lim, sizeof lim);
    HANDLE port = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 1);
    JOBOBJECT_ASSOCIATE_COMPLETION_PORT acp;
    acp.CompletionKey = (PVOID)1;
    acp.CompletionPort = port;
    SetInformationJobObject(g_job, JobObjectAssociateCompletionPortInformation, &acp, sizeof acp);

    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    memset(&si, 0, sizeof si);
    si.cb = sizeof si;
    char fields[160];
    if (!CreateProcessW(NULL, cmdline, NULL, NULL, FALSE, CREATE_SUSPENDED, NULL, NULL, &si, &pi)) {
        snprintf(fields, sizeof fields, "\"event\":\"spawn_failed\",\"error\":%lu", GetLastError());
        probe_emit(fields);
        return 1;
    }
    AssignProcessToJobObject(g_job, pi.hProcess);
    ResumeThread(pi.hThread);
    CloseHandle(pi.hThread);
    snprintf(fields, sizeof fields, "\"event\":\"started\",\"pid\":%lu", pi.dwProcessId);
    probe_emit(fields);
    CreateThread(NULL, 0, stdin_thread, NULL, 0, NULL);

    int64_t t_close = 0;
    for (;;) {
        DWORD code;
        ULONG_PTR key;
        LPOVERLAPPED ov;
        if (GetQueuedCompletionStatus(port, &code, &key, &ov, 50) && code == JOB_OBJECT_MSG_ACTIVE_PROCESS_ZERO)
            break;
        LONG cmd = InterlockedExchange(&g_cmd, 0);
        if (cmd == 1) {
            t_close = probe_now_ms();
            EnumWindows(close_if_in_job, 0);
            probe_emit("\"event\":\"close_sent\"");
        } else if (cmd == 2) {
            TerminateJobObject(g_job, 137);
            probe_emit("\"event\":\"kill_sent\"");
        }
    }
    DWORD exit_code = 0;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    long long since = t_close ? (long long)(probe_now_ms() - t_close) : -1;
    snprintf(fields, sizeof fields, "\"event\":\"exited\",\"code\":%lu,\"ms_since_close\":%lld", exit_code, since);
    probe_emit(fields);
    return 0;
}
```

- [ ] **Step 5: Write the Linux supervisor**

`tools/overlay-phase0/src/proc_linux.c`:

```c
/* probe_spawn run, Linux: a process group, waitpid on the leader, then the group must be empty. */
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "common.h"
#include "spawn.h"

int probe_run(int argc, char **argv)
{
    if (argc < 1) {
        fputs("usage: probe_spawn run <program> [args...]\n", stderr);
        return 2;
    }
    char fields[160];
    pid_t pid = fork();
    if (pid < 0) {
        snprintf(fields, sizeof fields, "\"event\":\"spawn_failed\",\"error\":%d", errno);
        probe_emit(fields);
        return 1;
    }
    if (pid == 0) {
        setpgid(0, 0);
        execv(argv[0], argv);
        _exit(127);
    }
    setpgid(pid, pid);
    snprintf(fields, sizeof fields, "\"event\":\"started\",\"pid\":%d", (int)pid);
    probe_emit(fields);

    int64_t t_close = 0;
    int status = 0;
    int leader_done = 0;
    int stdin_open = 1;
    for (;;) {
        if (!leader_done && waitpid(pid, &status, WNOHANG) == pid)
            leader_done = 1;
        if (leader_done && kill(-pid, 0) == -1 && errno == ESRCH)
            break;
        struct pollfd pfd = { STDIN_FILENO, POLLIN, 0 };
        if (!stdin_open) {
            poll(NULL, 0, 50);
            continue;
        }
        if (poll(&pfd, 1, 50) <= 0)
            continue;
        char buf[64];
        ssize_t n = read(STDIN_FILENO, buf, sizeof buf - 1);
        if (n <= 0) {
            stdin_open = 0;
            continue;
        }
        buf[n] = '\0';
        if (strstr(buf, "close")) {
            t_close = probe_now_ms();
            kill(-pid, SIGTERM);
            probe_emit("\"event\":\"close_sent\"");
        } else if (strstr(buf, "kill")) {
            kill(-pid, SIGKILL);
            probe_emit("\"event\":\"kill_sent\"");
        }
    }
    int code = WIFEXITED(status) ? WEXITSTATUS(status) : WIFSIGNALED(status) ? 128 + WTERMSIG(status) : -1;
    long long since = t_close ? (long long)(probe_now_ms() - t_close) : -1;
    snprintf(fields, sizeof fields, "\"event\":\"exited\",\"code\":%d,\"ms_since_close\":%lld", code, since);
    probe_emit(fields);
    return 0;
}
```

- [ ] **Step 6: Add the executable to the build**

Append to `tools/overlay-phase0/CMakeLists.txt`:

```cmake
add_executable(probe_spawn src/spawn_main.c)
if (WIN32)
  target_sources(probe_spawn PRIVATE src/proc_win32.c)
  target_link_libraries(probe_spawn PRIVATE user32)
else ()
  target_sources(probe_spawn PRIVATE src/proc_linux.c)
endif ()
target_link_libraries(probe_spawn PRIVATE probe_common)
```

- [ ] **Step 7: Build and run the tests to verify they pass**

```powershell
$env:Path += ";C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin"
cmake --build C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec/build-phase0 --config Release
$env:PROBE_SPAWN = 'C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec/build-phase0/Release/probe_spawn.exe'
python -m unittest discover -s C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec/tools/overlay-phase0/tests -p test_spawn.py -v
```

Run all four lines in one call. Expected: `Ran 3 tests`, then `OK` with no `skipped=`. On Linux the same suite runs
with `PROBE_SPAWN` set to the absolute path of `build-phase0/probe_spawn` (the kill test's code is `128 + 9 = 137`
there).

- [ ] **Step 8: Commit**

```powershell
git -C C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec add tools/overlay-phase0/CMakeLists.txt tools/overlay-phase0/src/spawn.h tools/overlay-phase0/src/spawn_main.c tools/overlay-phase0/src/proc_win32.c tools/overlay-phase0/src/proc_linux.c tools/overlay-phase0/tests/test_spawn.py
git -C C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec commit -m "tools: add probe_spawn run (job object and process group supervision)"
```

---

### Task 3: Key injection and the key logger

**Files:**
- Create: `tools/overlay-phase0/src/keys.h`, `tools/overlay-phase0/src/keys.c`
- Create: `tools/overlay-phase0/src/inject_win32.c`, `tools/overlay-phase0/src/inject_linux.c`
- Modify: `tools/overlay-phase0/src/spawn_main.c` (delete the `probe_keys` and `probe_keylog` stubs)
- Modify: `tools/overlay-phase0/CMakeLists.txt`
- Modify: `tools/overlay-phase0/tests/test_spawn.py`

**Interfaces:**
- Consumes: `probe_emit`, `probe_now_ms`, `probe_json_string` (Task 1); `spawn.h` (Task 2).
- Produces: `typedef struct { const char *name; unsigned short vk; unsigned short evdev; int extended; } probe_key;`
  `const probe_key *probe_key_find(const char *name);` `extern const probe_key probe_keys_table[];`
  `extern const unsigned probe_keys_count;` `PROBE_MAX_CHORD` (4); `int probe_parse_chord(const char *arg,
  const probe_key **out, char *bad, size_t bad_cap);` `typedef struct { int (*open)(void); void (*send)(const
  probe_key *k, int up); void (*sleep_ms)(long ms); void (*close)(void); } probe_key_sender;` and `int
  probe_keys_main(int argc, char **argv, const probe_key_sender *sender);`, which holds the whole `keys` command
  (arguments, the check of every chord, the key lines) so each platform supplies only its sender. Commands:
  `keys [--dry-run] <delay_ms> <chord>...` (a chord is names
  joined by `+`, pressed in order and released in reverse; every chord is validated before anything is sent; an
  unknown or empty part prints `{"event":"unknown_key","name"}` and exits 2; prints
  one `{"event":"key","name","vk","evdev","dry"}` per key pressed) and `keylog <seconds>` (prints one line per raw key
  or consumer-control report until the time is up, then `{"event":"keylog_end"}`).

- [ ] **Step 1: Write the failing tests**

Insert this new class into `tools/overlay-phase0/tests/test_spawn.py`, above the `if __name__ == "__main__":` guard:

```python
@unittest.skipUnless(SPAWN, "set PROBE_SPAWN to the built probe_spawn")
class KeysTest(unittest.TestCase):
    def test_dry_run_names_every_key_of_a_chord(self):
        proc = subprocess.run(
            [SPAWN, "keys", "--dry-run", "0", "Up", "Alt+F4"], capture_output=True, text=True, timeout=120
        )
        self.assertEqual(proc.returncode, 0)
        evs = events(proc.stdout)
        self.assertEqual([e["name"] for e in evs], ["Up", "Alt", "F4"])
        self.assertEqual(evs[0]["vk"], 0x26)
        self.assertEqual(evs[0]["evdev"], 103)
        self.assertTrue(all(e["dry"] for e in evs))

    def test_unknown_chord_part_injects_nothing(self):
        proc = subprocess.run(
            [SPAWN, "keys", "--dry-run", "0", "Up", "Alt+Foo"], capture_output=True, text=True, timeout=120
        )
        self.assertEqual(proc.returncode, 2)
        self.assertEqual([e["event"] for e in events(proc.stdout)], ["unknown_key"])

    def test_unknown_key_name_is_json_escaped(self):
        proc = subprocess.run(
            [SPAWN, "keys", "--dry-run", "0", 'Up+Fo"o\\'], capture_output=True, text=True, timeout=120
        )
        self.assertEqual(proc.returncode, 2)
        self.assertEqual([e.get("name") for e in events(proc.stdout)], ['Fo"o\\'])

    def test_keylog_ends_on_time(self):
        proc = subprocess.run([SPAWN, "keylog", "1"], capture_output=True, text=True, timeout=120)
        self.assertEqual(events(proc.stdout)[-1]["event"], "keylog_end")
```

- [ ] **Step 2: Run them to verify they fail**

```powershell
$env:PROBE_SPAWN = 'C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec/build-phase0/Release/probe_spawn.exe'
python -m unittest discover -s C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec/tools/overlay-phase0/tests -p test_spawn.py -v
```

Run both lines in one call. Expected: `Ran 7 tests`, then `FAILED (failures=3, errors=1)`, no `skipped=`. The three
`RunTest` tests pass; the four new ones FAIL or ERROR, because the stubs return 2 and print nothing
(`test_keylog_ends_on_time` ERRORS on the empty list).

- [ ] **Step 3: Write the key table, then delete the two stubs from `spawn_main.c`**

`tools/overlay-phase0/src/keys.h`:

```c
/* The keys the probe can send: one name, its Windows virtual key and its Linux evdev code. */
#ifndef PROBE_KEYS_H
#define PROBE_KEYS_H

#include <stddef.h>

#define PROBE_MAX_CHORD 4

typedef struct {
    const char *name;
    unsigned short vk;
    unsigned short evdev;
    int extended; /* Windows: send with KEYEVENTF_EXTENDEDKEY */
} probe_key;

/* What a platform gives probe_keys_main. open runs only for a real send, after every chord has been checked; it
   returns 0, or prints its own failure line and returns the exit code. open and close may be NULL. */
typedef struct {
    int (*open)(void);
    void (*send)(const probe_key *k, int up);
    void (*sleep_ms)(long ms);
    void (*close)(void);
} probe_key_sender;

extern const probe_key probe_keys_table[];
extern const unsigned probe_keys_count;

const probe_key *probe_key_find(const char *name);

/* Splits "Alt+F4" into keys; returns the count, or -1 with bad set to the unknown (or empty, or fifth) part. */
int probe_parse_chord(const char *arg, const probe_key **out, char *bad, size_t bad_cap);

/* The keys command, [--dry-run] <delay_ms> <chord>...: refuses the whole command before sending anything when any
   chord has an unknown part. */
int probe_keys_main(int argc, char **argv, const probe_key_sender *sender);

#endif
```

`tools/overlay-phase0/src/keys.c`:

```c
#include "keys.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

const probe_key probe_keys_table[] = {
    { "Up", 0x26, 103, 1 },          { "Down", 0x28, 108, 1 },     { "Left", 0x25, 105, 1 },
    { "Right", 0x27, 106, 1 },       { "Enter", 0x0D, 28, 0 },     { "Backspace", 0x08, 14, 0 },
    { "Escape", 0x1B, 1, 0 },        { "Space", 0x20, 57, 0 },     { "Tab", 0x09, 15, 0 },
    { "F", 0x46, 33, 0 },            { "P", 0x50, 25, 0 },         { "Alt", 0x12, 56, 0 },
    { "F4", 0x73, 62, 0 },           { "Apps", 0x5D, 127, 1 },     { "BrowserHome", 0xAC, 172, 1 },
    { "MediaPlayPause", 0xB3, 164, 1 },
};
const unsigned probe_keys_count = sizeof probe_keys_table / sizeof probe_keys_table[0];

const probe_key *probe_key_find(const char *name)
{
    for (unsigned i = 0; i < probe_keys_count; i++) {
        if (strcmp(probe_keys_table[i].name, name) == 0)
            return &probe_keys_table[i];
    }
    return NULL;
}

int probe_parse_chord(const char *arg, const probe_key **out, char *bad, size_t bad_cap)
{
    int n = 0;
    const char *part = arg;
    for (;;) {
        const char *plus = strchr(part, '+');
        size_t len = plus ? (size_t)(plus - part) : strlen(part);
        char name[64];
        snprintf(name, sizeof name, "%.*s", (int)len, part);
        const probe_key *k = probe_key_find(name);
        if (!k || n == PROBE_MAX_CHORD) {
            snprintf(bad, bad_cap, "%s", name);
            return -1;
        }
        out[n++] = k;
        if (!plus)
            return n;
        part = plus + 1;
    }
}

int probe_keys_main(int argc, char **argv, const probe_key_sender *sender)
{
    int i = 0, dry = 0;
    if (argc > 0 && strcmp(argv[0], "--dry-run") == 0) {
        dry = 1;
        i = 1;
    }
    if (argc - i < 2) {
        fputs("usage: probe_spawn keys [--dry-run] <delay_ms> <chord>...\n", stderr);
        return 2;
    }
    long delay = atol(argv[i++]);
    const probe_key *chord[PROBE_MAX_CHORD];
    char bad[64], name[400], fields[512];
    for (int a = i; a < argc; a++) {
        if (probe_parse_chord(argv[a], chord, bad, sizeof bad) < 0) {
            probe_json_string(name, sizeof name, bad);
            snprintf(fields, sizeof fields, "\"event\":\"unknown_key\",\"name\":%s", name);
            probe_emit(fields);
            return 2;
        }
    }
    if (!dry) {
        int rc = sender->open ? sender->open() : 0;
        if (rc)
            return rc;
        sender->sleep_ms(delay);
    }
    for (int a = i; a < argc; a++) {
        int n = probe_parse_chord(argv[a], chord, bad, sizeof bad);
        for (int k = 0; k < n; k++) {
            if (!dry)
                sender->send(chord[k], 0);
            snprintf(fields, sizeof fields, "\"event\":\"key\",\"name\":\"%s\",\"vk\":%u,\"evdev\":%u,\"dry\":%s",
                     chord[k]->name, chord[k]->vk, chord[k]->evdev, dry ? "true" : "false");
            probe_emit(fields);
        }
        for (int k = n - 1; k >= 0; k--) {
            if (!dry)
                sender->send(chord[k], 1);
        }
        if (!dry)
            sender->sleep_ms(50);
    }
    if (!dry && sender->close)
        sender->close();
    return 0;
}
```

Delete these two lines from `spawn_main.c`:

```c
int probe_keys(int argc, char **argv) { (void)argc; (void)argv; return 2; }
int probe_keylog(int argc, char **argv) { (void)argc; (void)argv; return 2; }
```

- [ ] **Step 4: Write the Windows injector and key logger**

`tools/overlay-phase0/src/inject_win32.c`:

```c
/* probe_spawn keys and keylog, Windows: SendInput, and raw input from a message-only window. */
#include <windows.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "keys.h"
#include "spawn.h"

static void send_key(const probe_key *k, int up)
{
    INPUT in;
    memset(&in, 0, sizeof in);
    in.type = INPUT_KEYBOARD;
    in.ki.wVk = k->vk;
    in.ki.wScan = (WORD)MapVirtualKeyW(k->vk, MAPVK_VK_TO_VSC);
    in.ki.dwFlags = (k->extended ? KEYEVENTF_EXTENDEDKEY : 0) | (up ? KEYEVENTF_KEYUP : 0);
    SendInput(1, &in, sizeof in);
}

static void sleep_ms(long ms)
{
    Sleep((DWORD)ms);
}

static const probe_key_sender sender = { NULL, send_key, sleep_ms, NULL };

int probe_keys(int argc, char **argv)
{
    return probe_keys_main(argc, argv, &sender);
}

static LRESULT CALLBACK keylog_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_TIMER) {
        PostQuitMessage(0);
        return 0;
    }
    if (msg != WM_INPUT)
        return DefWindowProcW(hwnd, msg, wp, lp);
    UINT size = 0;
    GetRawInputData((HRAWINPUT)lp, RID_INPUT, NULL, &size, sizeof(RAWINPUTHEADER));
    RAWINPUT *ri = malloc(size);
    if (!ri)
        return 0;
    char fields[512];
    if (GetRawInputData((HRAWINPUT)lp, RID_INPUT, ri, &size, sizeof(RAWINPUTHEADER)) == size) {
        if (ri->header.dwType == RIM_TYPEKEYBOARD) {
            snprintf(fields, sizeof fields, "\"event\":\"raw_key\",\"vk\":%u,\"scan\":%u,\"flags\":%u,\"message\":%u",
                     ri->data.keyboard.VKey, ri->data.keyboard.MakeCode, ri->data.keyboard.Flags,
                     ri->data.keyboard.Message);
            probe_emit(fields);
        } else if (ri->header.dwType == RIM_TYPEHID) {
            char hex[200];
            DWORD bytes = ri->data.hid.dwSizeHid * ri->data.hid.dwCount;
            DWORD shown = bytes < 64 ? bytes : 64;
            for (DWORD b = 0; b < shown; b++)
                snprintf(hex + b * 2, 3, "%02x", ri->data.hid.bRawData[b]);
            hex[shown * 2] = '\0';
            snprintf(fields, sizeof fields, "\"event\":\"raw_hid\",\"bytes\":\"%s\"", hex);
            probe_emit(fields);
        }
    }
    free(ri);
    return 0;
}

int probe_keylog(int argc, char **argv)
{
    if (argc < 1) {
        fputs("usage: probe_spawn keylog <seconds>\n", stderr);
        return 2;
    }
    int seconds = atoi(argv[0]);
    WNDCLASSW wc;
    memset(&wc, 0, sizeof wc);
    wc.lpfnWndProc = keylog_proc;
    wc.hInstance = GetModuleHandleW(NULL);
    wc.lpszClassName = L"cptk_probe_keylog";
    RegisterClassW(&wc);
    HWND hwnd = CreateWindowExW(0, wc.lpszClassName, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, wc.hInstance, NULL);
    RAWINPUTDEVICE rid[2];
    rid[0].usUsagePage = 0x01; /* generic desktop */
    rid[0].usUsage = 0x06;     /* keyboard */
    rid[0].dwFlags = RIDEV_INPUTSINK;
    rid[0].hwndTarget = hwnd;
    rid[1].usUsagePage = 0x0C; /* consumer */
    rid[1].usUsage = 0x01;     /* consumer control */
    rid[1].dwFlags = RIDEV_INPUTSINK;
    rid[1].hwndTarget = hwnd;
    if (!RegisterRawInputDevices(rid, 2, sizeof rid[0])) {
        char fields[96];
        snprintf(fields, sizeof fields, "\"event\":\"keylog_failed\",\"error\":%lu", GetLastError());
        probe_emit(fields);
        return 1;
    }
    SetTimer(hwnd, 1, (UINT)seconds * 1000u, NULL);
    MSG m;
    while (GetMessageW(&m, NULL, 0, 0) > 0)
        DispatchMessageW(&m);
    probe_emit("\"event\":\"keylog_end\"");
    return 0;
}
```

- [ ] **Step 5: Write the Linux injector and key logger**

`tools/overlay-phase0/src/inject_linux.c`:

```c
/* probe_spawn keys and keylog, Linux: one uinput keyboard, and every readable evdev device. */
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

#include "common.h"
#include "keys.h"
#include "spawn.h"

static int g_fd = -1;

static void sleep_ms(long ms)
{
    struct timespec ts = { ms / 1000, (ms % 1000) * 1000000L };
    nanosleep(&ts, NULL);
}

static void emit_ev(int fd, int type, int code, int value)
{
    struct input_event ie;
    memset(&ie, 0, sizeof ie);
    ie.type = (unsigned short)type;
    ie.code = (unsigned short)code;
    ie.value = value;
    if (write(fd, &ie, sizeof ie) != (ssize_t)sizeof ie)
        perror("uinput write");
}

static int uinput_open(void)
{
    g_fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (g_fd < 0) {
        char fields[96];
        snprintf(fields, sizeof fields, "\"event\":\"uinput_failed\",\"error\":%d", errno);
        probe_emit(fields);
        return 1;
    }
    ioctl(g_fd, UI_SET_EVBIT, EV_KEY);
    for (unsigned i = 0; i < probe_keys_count; i++)
        ioctl(g_fd, UI_SET_KEYBIT, probe_keys_table[i].evdev);
    struct uinput_setup us;
    memset(&us, 0, sizeof us);
    us.id.bustype = BUS_USB;
    us.id.vendor = 0x1209;
    us.id.product = 0x0001;
    snprintf(us.name, sizeof us.name, "CPTK probe keyboard");
    ioctl(g_fd, UI_DEV_SETUP, &us);
    ioctl(g_fd, UI_DEV_CREATE);
    sleep_ms(1000); /* let the desktop pick up the new device */
    return 0;
}

static void send_key(const probe_key *k, int up)
{
    emit_ev(g_fd, EV_KEY, k->evdev, up ? 0 : 1);
    emit_ev(g_fd, EV_SYN, SYN_REPORT, 0);
}

static void uinput_close(void)
{
    ioctl(g_fd, UI_DEV_DESTROY);
    close(g_fd);
    g_fd = -1;
}

static const probe_key_sender sender = { uinput_open, send_key, sleep_ms, uinput_close };

int probe_keys(int argc, char **argv)
{
    return probe_keys_main(argc, argv, &sender);
}

int probe_keylog(int argc, char **argv)
{
    if (argc < 1) {
        fputs("usage: probe_spawn keylog <seconds>\n", stderr);
        return 2;
    }
    struct pollfd fds[64];
    char names[64][32];
    nfds_t n = 0;
    for (int d = 0; d < 64; d++) {
        char path[32];
        snprintf(path, sizeof path, "/dev/input/event%d", d);
        int fd = open(path, O_RDONLY | O_NONBLOCK);
        if (fd < 0)
            continue;
        fds[n].fd = fd;
        fds[n].events = POLLIN;
        snprintf(names[n], sizeof names[n], "%s", path);
        n++;
    }
    char fields[160];
    snprintf(fields, sizeof fields, "\"event\":\"keylog_devices\",\"count\":%u", (unsigned)n);
    probe_emit(fields);
    int64_t end = probe_now_ms() + (int64_t)atoi(argv[0]) * 1000;
    while (probe_now_ms() < end) {
        if (n == 0) {
            sleep_ms(50);
            continue;
        }
        if (poll(fds, n, 50) <= 0)
            continue;
        for (nfds_t f = 0; f < n; f++) {
            if (!(fds[f].revents & POLLIN))
                continue;
            struct input_event ie;
            while (read(fds[f].fd, &ie, sizeof ie) == (ssize_t)sizeof ie) {
                if (ie.type != EV_KEY)
                    continue;
                snprintf(fields, sizeof fields, "\"event\":\"raw_key\",\"device\":\"%s\",\"code\":%u,\"value\":%d",
                         names[f], ie.code, ie.value);
                probe_emit(fields);
            }
        }
    }
    for (nfds_t f = 0; f < n; f++)
        close(fds[f].fd);
    probe_emit("\"event\":\"keylog_end\"");
    return 0;
}
```

- [ ] **Step 6: Add the sources to the build**

In `tools/overlay-phase0/CMakeLists.txt`, change the `probe_common` and `probe_spawn` lines to:

```cmake
add_library(probe_common STATIC src/common.c src/keys.c)
```

and inside the `if (WIN32)` block of `probe_spawn`:

```cmake
  target_sources(probe_spawn PRIVATE src/proc_win32.c src/inject_win32.c)
```

and in its `else ()` block:

```cmake
  target_sources(probe_spawn PRIVATE src/proc_linux.c src/inject_linux.c)
```

- [ ] **Step 7: Build and run the tests to verify they pass**

```powershell
$env:Path += ";C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin"
cmake --build C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec/build-phase0 --config Release
$env:PROBE_SPAWN = 'C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec/build-phase0/Release/probe_spawn.exe'
python -m unittest discover -s C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec/tools/overlay-phase0/tests -p test_spawn.py -v
```

Run all four lines in one call. Expected: `Ran 7 tests`, then `OK` with no `skipped=`. (The key logger test only
checks that the command ends on time; what it logs is measured in the guests.)

- [ ] **Step 8: Commit**

```powershell
git -C C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec add tools/overlay-phase0/CMakeLists.txt tools/overlay-phase0/src/keys.h tools/overlay-phase0/src/keys.c tools/overlay-phase0/src/inject_win32.c tools/overlay-phase0/src/inject_linux.c tools/overlay-phase0/src/spawn_main.c tools/overlay-phase0/tests/test_spawn.py
git -C C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec commit -m "tools: add probe_spawn keys and keylog"
```

---

### Task 4: The SDL3 gamepad and focus probes

**Files:**
- Create: `tools/overlay-phase0/src/sdlprobe.c`
- Modify: `tools/overlay-phase0/src/spawn_main.c` (delete the `probe_pad` and `probe_focus` stubs)
- Modify: `tools/overlay-phase0/CMakeLists.txt`
- Modify: `tools/overlay-phase0/tests/test_spawn.py`

**Interfaces:**
- Consumes: `probe_emit`, `probe_now_ms`, `probe_json_string` (Task 1).
- Produces: `pad <seconds> <default|xinput|rawinput|wgi>`: a 320 x 240 window, background gamepad events on, the
  Windows backend chosen by hints; prints `pad_added`, `focus` (with `focused`), and `button` lines carrying
  `focused`, then `pad_end`. `focus <program> [args...]`: a fullscreen window, starts the program with
  `SDL_CreateProcess`, waits for it to exit, calls `SDL_RaiseWindow`, then prints `focus_after_exit` with
  `gained` and `ms`, and `key_after_exit` with `ms` (or `-1` after 10 s with no key). When SDL cannot start, either
  command prints `{"event":"sdl_failed","error"}` and exits 1; `focus` does the same when its window or renderer
  cannot be created, so it never prints focus results nothing measured.

- [ ] **Step 1: Write the failing test**

Insert this new class into `tools/overlay-phase0/tests/test_spawn.py`, above the `if __name__ == "__main__":` guard:

```python
@unittest.skipUnless(SPAWN, "set PROBE_SPAWN to the built probe_spawn")
class SdlProbeTest(unittest.TestCase):
    def test_pad_runs_without_a_display(self):
        env = dict(os.environ, SDL_VIDEO_DRIVER="dummy")
        proc = subprocess.run([SPAWN, "pad", "1", "default"], capture_output=True, text=True, env=env, timeout=120)
        self.assertEqual(proc.returncode, 0)
        self.assertEqual(events(proc.stdout)[-1]["event"], "pad_end")

    def test_pad_refuses_an_unknown_backend(self):
        proc = subprocess.run([SPAWN, "pad", "1", "dinput"], capture_output=True, text=True, timeout=120)
        self.assertEqual(proc.returncode, 2)
```

- [ ] **Step 2: Run it to verify it fails**

```powershell
$env:PROBE_SPAWN = 'C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec/build-phase0/Release/probe_spawn.exe'
python -m unittest discover -s C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec/tools/overlay-phase0/tests -p test_spawn.py -v
```

Run both lines in one call. Expected: `Ran 9 tests`, then `FAILED (failures=1)`, no `skipped=`.
`test_pad_runs_without_a_display` FAILS (the stub returns 2 and prints nothing); `test_pad_refuses_an_unknown_backend`
passes by accident (the stub returns 2), which is fine.

- [ ] **Step 3: Write the probes, then delete the two remaining stubs from `spawn_main.c`**

`tools/overlay-phase0/src/sdlprobe.c`:

```c
/* probe_spawn pad and focus: SDL3 gamepad input while another window has focus, and focus after a browser exits. */
#include <SDL3/SDL.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "spawn.h"

/* Prints {"event":<event>,"error":<SDL's last error>} and returns 1. */
static int emit_sdl_error(const char *event)
{
    char error[256], fields[320];
    probe_json_string(error, sizeof error, SDL_GetError());
    snprintf(fields, sizeof fields, "\"event\":\"%s\",\"error\":%s", event, error);
    probe_emit(fields);
    return 1;
}

static int set_backend(const char *backend)
{
    if (strcmp(backend, "default") == 0)
        return 0;
    if (strcmp(backend, "xinput") == 0) {
        SDL_SetHint(SDL_HINT_JOYSTICK_RAWINPUT, "0");
        SDL_SetHint(SDL_HINT_JOYSTICK_WGI, "0");
        SDL_SetHint(SDL_HINT_XINPUT_ENABLED, "1");
        return 0;
    }
    if (strcmp(backend, "rawinput") == 0) {
        SDL_SetHint(SDL_HINT_JOYSTICK_RAWINPUT, "1");
        SDL_SetHint(SDL_HINT_JOYSTICK_WGI, "0");
        SDL_SetHint(SDL_HINT_XINPUT_ENABLED, "0");
        return 0;
    }
    if (strcmp(backend, "wgi") == 0) {
        SDL_SetHint(SDL_HINT_JOYSTICK_WGI, "1");
        SDL_SetHint(SDL_HINT_JOYSTICK_RAWINPUT, "0");
        SDL_SetHint(SDL_HINT_XINPUT_ENABLED, "0");
        return 0;
    }
    return -1;
}

int probe_pad(int argc, char **argv)
{
    if (argc < 2 || set_backend(argv[1]) < 0) {
        fputs("usage: probe_spawn pad <seconds> <default|xinput|rawinput|wgi>\n", stderr);
        return 2;
    }
    char fields[512], text[256];
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD))
        return emit_sdl_error("sdl_failed");
    SDL_Window *w = SDL_CreateWindow("CPTK probe pad", 320, 240, 0);
    int focused = w && (SDL_GetWindowFlags(w) & SDL_WINDOW_INPUT_FOCUS) != 0;
    Uint64 end = SDL_GetTicks() + (Uint64)atoi(argv[0]) * 1000u;
    while (SDL_GetTicks() < end) {
        SDL_Event e;
        if (!SDL_WaitEventTimeout(&e, 50))
            continue;
        switch (e.type) {
        case SDL_EVENT_GAMEPAD_ADDED: {
            SDL_Gamepad *pad = SDL_OpenGamepad(e.gdevice.which);
            probe_json_string(text, sizeof text, pad ? SDL_GetGamepadName(pad) : "?");
            snprintf(fields, sizeof fields, "\"event\":\"pad_added\",\"name\":%s", text);
            probe_emit(fields);
            break;
        }
        case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
            probe_json_string(text, sizeof text, SDL_GetGamepadStringForButton((SDL_GamepadButton)e.gbutton.button));
            snprintf(fields, sizeof fields, "\"event\":\"button\",\"button\":%s,\"focused\":%s", text,
                     focused ? "true" : "false");
            probe_emit(fields);
            break;
        case SDL_EVENT_WINDOW_FOCUS_GAINED:
        case SDL_EVENT_WINDOW_FOCUS_LOST:
            focused = e.type == SDL_EVENT_WINDOW_FOCUS_GAINED;
            snprintf(fields, sizeof fields, "\"event\":\"focus\",\"focused\":%s", focused ? "true" : "false");
            probe_emit(fields);
            break;
        case SDL_EVENT_QUIT:
            end = 0;
            break;
        default:
            break;
        }
    }
    probe_emit("\"event\":\"pad_end\"");
    SDL_DestroyWindow(w);
    SDL_Quit();
    return 0;
}

int probe_focus(int argc, char **argv)
{
    if (argc < 1) {
        fputs("usage: probe_spawn focus <program> [args...]\n", stderr);
        return 2;
    }
    char fields[256];
    if (!SDL_Init(SDL_INIT_VIDEO))
        return emit_sdl_error("sdl_failed");
    SDL_Window *w = SDL_CreateWindow("CPTK probe focus", 0, 0, SDL_WINDOW_FULLSCREEN);
    SDL_Renderer *r = w ? SDL_CreateRenderer(w, NULL) : NULL;
    if (!r) {
        /* Report it rather than print focus results nothing measured. */
        int rc = emit_sdl_error("sdl_failed");
        if (w)
            SDL_DestroyWindow(w);
        SDL_Quit();
        return rc;
    }
    SDL_SetRenderDrawColor(r, 32, 32, 32, 255);
    SDL_RenderClear(r);
    SDL_RenderPresent(r);

    /* argv is the tail of main's argv, so argv[argc] is the NULL that SDL_CreateProcess needs. */
    SDL_Process *p = SDL_CreateProcess((const char *const *)argv, false);
    if (!p)
        return emit_sdl_error("spawn_failed");
    probe_emit("\"event\":\"started\"");
    int code = 0;
    while (!SDL_WaitProcess(p, false, &code)) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
        }
        SDL_RenderClear(r);
        SDL_RenderPresent(r);
        SDL_Delay(20);
    }
    int64_t t_exit = probe_now_ms();
    snprintf(fields, sizeof fields, "\"event\":\"exited\",\"code\":%d", code);
    probe_emit(fields);
    SDL_DestroyProcess(p);

    SDL_RaiseWindow(w);
    long long gained_ms = (SDL_GetWindowFlags(w) & SDL_WINDOW_INPUT_FOCUS) ? 0 : -1;
    long long key_ms = -1;
    while (probe_now_ms() - t_exit < 10000 && key_ms < 0) {
        SDL_Event e;
        if (!SDL_WaitEventTimeout(&e, 20))
            continue;
        if (e.type == SDL_EVENT_WINDOW_FOCUS_GAINED && gained_ms < 0)
            gained_ms = probe_now_ms() - t_exit;
        else if (e.type == SDL_EVENT_KEY_DOWN)
            key_ms = probe_now_ms() - t_exit;
    }
    snprintf(fields, sizeof fields, "\"event\":\"focus_after_exit\",\"gained\":%s,\"ms\":%lld",
             (gained_ms >= 0 && gained_ms <= 1000) ? "true" : "false", gained_ms);
    probe_emit(fields);
    snprintf(fields, sizeof fields, "\"event\":\"key_after_exit\",\"ms\":%lld", key_ms);
    probe_emit(fields);
    SDL_DestroyRenderer(r);
    SDL_DestroyWindow(w);
    SDL_Quit();
    return 0;
}
```

Delete these two lines from `spawn_main.c`:

```c
int probe_pad(int argc, char **argv) { (void)argc; (void)argv; return 2; }
int probe_focus(int argc, char **argv) { (void)argc; (void)argv; return 2; }
```

- [ ] **Step 4: Add SDL3 to the build**

In `tools/overlay-phase0/CMakeLists.txt`, after `project(...)` add:

```cmake
find_package(SDL3 3.2.10 REQUIRED CONFIG)
```

change `add_executable(probe_spawn src/spawn_main.c)` to:

```cmake
add_executable(probe_spawn src/spawn_main.c src/sdlprobe.c)
```

and change its last line to:

```cmake
target_link_libraries(probe_spawn PRIVATE probe_common SDL3::SDL3)
```

No reconfigure is needed: `build-phase0` was configured with the vcpkg toolchain in Task 1 Step 4, which already
installed SDL3, and the build below re-runs CMake on its own because `CMakeLists.txt` changed. The SDL3 DLL lands
beside `probe_spawn.exe` through vcpkg's app-local copy.

- [ ] **Step 5: Build and run the tests to verify they pass**

```powershell
$env:Path += ";C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin"
cmake --build C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec/build-phase0 --config Release
$env:PROBE_HOST = 'C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec/build-phase0/Release/probe_host.exe'
$env:PROBE_SPAWN = 'C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec/build-phase0/Release/probe_spawn.exe'
python -m unittest discover -s C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec/tools/overlay-phase0/tests -v
```

Run all five lines in one call. Expected: `Ran 13 tests` (4 in `test_host.py`, 9 in `test_spawn.py`), then `OK`
with no `skipped=`.

- [ ] **Step 6: Commit**

```powershell
git -C C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec add tools/overlay-phase0/CMakeLists.txt tools/overlay-phase0/src/sdlprobe.c tools/overlay-phase0/src/spawn_main.c tools/overlay-phase0/tests/test_spawn.py
git -C C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec commit -m "tools: add the SDL3 pad and focus probes"
```

---

### Task 5: The probe extension and the probe pages

**Files:**
- Create: `tools/overlay-phase0/extension/src/content.js`
- Create: `tools/overlay-phase0/extension/src/background.js`
- Create: `tools/overlay-phase0/extension/build.py`
- Create: `tools/overlay-phase0/pages/probe.html`, `tools/overlay-phase0/pages/keylog.html`,
  `tools/overlay-phase0/pages/serve.ps1`
- Create: `tools/overlay-phase0/tests/test_extension_build.py`

**Interfaces:**
- Consumes: the host name `app.cptk.overlay` and the host's ack (Task 1).
- Produces: `build.py` writes `extension/dist/chromium/`, `extension/dist/firefox/` and
  `extension/dist/cptk-overlay-<flavor>-0.0.1.zip`. Messages the extension sends to the host (all
  `{"type":"report","event","data":{...,"origin","version"},"tab","frame","t"}`): `start` (`webdriver`,
  `tokenFromUrl`, `allUrls`), `key` (`key`, `code`, `trusted`, `activation`), `fullscreen` (`ok`, `error`), `play`
  (`ok`, `error`). The probe pages: `probe.html` has a `<video>` fed by a canvas and an oscillator; F asks for
  fullscreen and P starts the video with sound.

- [ ] **Step 1: Write the failing test**

`tools/overlay-phase0/tests/test_extension_build.py`:

```python
"""Tests for the probe extension's build script."""
import json
import pathlib
import subprocess
import sys
import unittest
import zipfile

KIT = pathlib.Path(__file__).resolve().parent.parent
DIST = KIT / "extension" / "dist"


class ExtensionBuildTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        subprocess.run([sys.executable, str(KIT / "extension" / "build.py")], check=True, timeout=120)

    def manifest(self, flavor):
        return json.loads((DIST / flavor / "manifest.json").read_text(encoding="utf-8"))

    def test_both_flavors_share_the_listing_fields(self):
        for flavor in ("chromium", "firefox"):
            m = self.manifest(flavor)
            self.assertEqual(m["manifest_version"], 3)
            self.assertEqual(m["name"], "CPTK Overlay Probe")
            self.assertEqual(m["version"], "0.0.1")
            self.assertEqual(m["permissions"], ["nativeMessaging", "storage"])
            self.assertEqual(m["host_permissions"], ["<all_urls>"])
            self.assertEqual(m["content_scripts"][0]["run_at"], "document_start")
            self.assertTrue((DIST / flavor / "icon128.png").read_bytes().startswith(b"\x89PNG"))

    def test_flavor_specific_fields(self):
        self.assertEqual(self.manifest("chromium")["background"], {"service_worker": "background.js"})
        ff = self.manifest("firefox")
        self.assertEqual(ff["background"], {"scripts": ["background.js"]})
        gecko = ff["browser_specific_settings"]["gecko"]
        self.assertEqual(gecko["id"], "overlay-probe@cptk.app")
        self.assertEqual(gecko["strict_min_version"], "140.0")
        self.assertEqual(gecko["data_collection_permissions"], {"required": ["none"]})

    def test_zips_hold_the_manifest_at_the_root(self):
        for flavor in ("chromium", "firefox"):
            with zipfile.ZipFile(DIST / f"cptk-overlay-probe-{flavor}-0.0.1.zip") as z:
                self.assertIn("manifest.json", z.namelist())
                self.assertIn("content.js", z.namelist())

    def test_no_dynamic_code(self):
        for name in ("content.js", "background.js"):
            text = (KIT / "extension" / "src" / name).read_text(encoding="utf-8")
            self.assertNotIn("eval(", text)
            self.assertNotIn("new Function", text)


if __name__ == "__main__":
    unittest.main()
```

- [ ] **Step 2: Run it to verify it fails**

Run: `python -m unittest discover -s C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec/tools/overlay-phase0/tests -p test_extension_build.py -v`
Expected: `Ran 0 tests`, then `FAILED (errors=1)`: ERROR in `setUpClass` (`build.py` does not exist), so none of
the 4 tests runs.

- [ ] **Step 3: Write the extension's scripts**

`tools/overlay-phase0/extension/src/content.js`:

```js
// CPTK Overlay Probe 0.0.1, the Phase 0 probe. It does nothing unless this browser profile was started with a CPTK token.
(() => {
  "use strict";
  const api = globalThis.browser ?? globalThis.chrome;
  const VERSION = api.runtime.getManifest().version;
  const lines = [];
  let box = null;

  function show(text) {
    lines.push(text);
    if (lines.length > 8) lines.shift();
    if (!box && document.body) {
      box = document.createElement("div");
      box.style.cssText =
        "position:fixed;left:8px;bottom:8px;z-index:2147483647;background:#000c;color:#fff;" +
        "font:16px monospace;padding:8px;white-space:pre;pointer-events:none";
      document.body.appendChild(box);
    }
    if (box) box.textContent = `CPTK Overlay Probe ${VERSION}\n` + lines.join("\n");
  }

  function report(event, data) {
    const message = { type: "report", event, data: { ...data, origin: location.origin, version: VERSION } };
    Promise.resolve(api.runtime.sendMessage(message)).catch(() => {});
    show(`${event} ${JSON.stringify(data)}`);
  }

  async function onKey(e) {
    report("key", {
      key: e.key,
      code: e.code,
      trusted: e.isTrusted,
      activation: navigator.userActivation ? navigator.userActivation.isActive : null,
    });
    if (e.code === "KeyF") {
      try {
        await document.documentElement.requestFullscreen();
        report("fullscreen", { ok: true });
      } catch (err) {
        report("fullscreen", { ok: false, error: String(err && err.name) });
      }
    } else if (e.code === "KeyP") {
      const video = document.querySelector("video");
      if (!video) {
        report("play", { ok: false, error: "no video" });
        return;
      }
      video.muted = false;
      try {
        await video.play();
        report("play", { ok: true });
      } catch (err) {
        report("play", { ok: false, error: String(err && err.name) });
      }
    }
  }

  async function main() {
    const m = location.hash.match(/(?:^#|&)cptk=([0-9a-f]{32})(?=&|$)/);
    if (m) {
      await api.storage.local.set({ cptkToken: m[1] });
      const cleaned = location.href.replace(/([#&])cptk=[0-9a-f]{32}&?/, "$1").replace(/[#&]$/, "");
      history.replaceState(history.state, "", cleaned);
    }
    const stored = await api.storage.local.get(["cptkToken", "cptkAllUrlsGranted"]);
    if (!stored.cptkToken) return;
    report("start", {
      webdriver: navigator.webdriver === true,
      tokenFromUrl: Boolean(m),
      allUrls: stored.cptkAllUrlsGranted ?? null,
    });
    window.addEventListener("keydown", onKey, true);
    document.addEventListener("DOMContentLoaded", () => show("page ready"));
  }

  main().catch((err) => show(`error ${err}`));
})();
```

`tools/overlay-phase0/extension/src/background.js`:

```js
// CPTK Overlay Probe 0.0.1, the Phase 0 probe: relays the content script's reports to the native host.
const api = globalThis.browser ?? globalThis.chrome;
let port = null;

function hostPort() {
  if (port) return port;
  port = api.runtime.connectNative("app.cptk.overlay");
  port.onDisconnect.addListener(() => {
    port = null;
  });
  port.onMessage.addListener(() => {});
  return port;
}

api.runtime.onMessage.addListener((msg, sender) => {
  if (!msg || msg.type !== "report") return;
  hostPort().postMessage({
    ...msg,
    tab: sender.tab ? sender.tab.id : null,
    frame: sender.frameId ?? null,
    t: Date.now(),
  });
});

// Recorded for the content script's "start" report; the background never opens the port on its own.
api.permissions.contains({ origins: ["<all_urls>"] }).then((granted) => {
  api.storage.local.set({ cptkAllUrlsGranted: granted });
});
```

- [ ] **Step 4: Write the build script**

`tools/overlay-phase0/extension/build.py`:

```python
#!/usr/bin/env python3
"""Builds CPTK Overlay Probe 0.0.1 (the Phase 0 probe) for Chromium (Chrome, Edge) and Firefox."""
import json
import pathlib
import shutil
import struct
import zipfile
import zlib

HERE = pathlib.Path(__file__).resolve().parent
VERSION = "0.0.1"

BASE = {
    "manifest_version": 3,
    "name": "CPTK Overlay Probe",
    "version": VERSION,
    "description": (
        "Drives streaming sites with a TV remote inside CPTK's own browser profiles. "
        "It does nothing anywhere else."
    ),
    "permissions": ["nativeMessaging", "storage"],
    "host_permissions": ["<all_urls>"],
    "content_scripts": [{"matches": ["<all_urls>"], "js": ["content.js"], "run_at": "document_start"}],
    "icons": {"128": "icon128.png"},
}

FLAVORS = {
    "chromium": {"background": {"service_worker": "background.js"}},
    "firefox": {
        "background": {"scripts": ["background.js"]},
        "browser_specific_settings": {
            "gecko": {
                "id": "overlay-probe@cptk.app",
                "strict_min_version": "140.0",
                "data_collection_permissions": {"required": ["none"]},
            }
        },
    },
}


def icon_png(size=128, rgb=(0x1F, 0x6F, 0xB2)):
    """A plain square icon, written without any imaging library."""
    row = b"\x00" + bytes(rgb) * size
    raw = row * size

    def chunk(kind, data):
        body = kind + data
        return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body) & 0xFFFFFFFF)

    header = struct.pack(">IIBBBBB", size, size, 8, 2, 0, 0, 0)
    return b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header) + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b"")


def build():
    dist = HERE / "dist"
    for flavor, extra in FLAVORS.items():
        out = dist / flavor
        if out.exists():
            shutil.rmtree(out)
        out.mkdir(parents=True)
        for name in ("content.js", "background.js"):
            shutil.copyfile(HERE / "src" / name, out / name)
        (out / "icon128.png").write_bytes(icon_png())
        manifest = {**BASE, **extra}
        (out / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
        archive = dist / f"cptk-overlay-probe-{flavor}-{VERSION}.zip"
        with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED) as z:
            for path in sorted(out.iterdir()):
                z.write(path, path.name)
        print(f"built {archive}")


if __name__ == "__main__":
    build()
```

- [ ] **Step 5: Write the probe pages and the Windows page server**

`tools/overlay-phase0/pages/probe.html`:

```html
<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<title>CPTK probe page</title>
<style>
  body { background: #202020; color: #eeeeee; font: 20px sans-serif; margin: 40px; }
  video { width: 640px; height: 360px; background: #000000; }
</style>
</head>
<body>
<h1>CPTK probe page</h1>
<p>F asks for fullscreen. P starts the video with sound. The probe extension reports both.</p>
<video playsinline></video>
<script>
  const canvas = document.createElement("canvas");
  canvas.width = 640;
  canvas.height = 360;
  const ctx = canvas.getContext("2d");
  let hue = 0;
  setInterval(() => {
    hue = (hue + 3) % 360;
    ctx.fillStyle = `hsl(${hue} 60% 40%)`;
    ctx.fillRect(0, 0, 640, 360);
  }, 100);
  const audio = new AudioContext();
  const tone = audio.createOscillator();
  const sink = audio.createMediaStreamDestination();
  tone.connect(sink);
  tone.start();
  const stream = canvas.captureStream(10);
  stream.addTrack(sink.stream.getAudioTracks()[0]);
  document.querySelector("video").srcObject = stream;
</script>
</body>
</html>
```

`tools/overlay-phase0/pages/keylog.html`:

```html
<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<title>CPTK key logger</title>
<style>
  body { background: #202020; color: #eeeeee; font: 18px monospace; margin: 24px; }
  li { margin: 2px 0; }
</style>
</head>
<body>
<h1>CPTK key logger</h1>
<p>Press every button on the remote, short and long. Gamepads show after their first press.</p>
<ol id="log"></ol>
<script>
  const log = document.getElementById("log");
  function add(text) {
    const li = document.createElement("li");
    li.textContent = `${new Date().toISOString()} ${text}`;
    log.prepend(li);
  }
  for (const type of ["keydown", "keyup"]) {
    window.addEventListener(type, (e) => {
      add(`${type} key=${e.key} code=${e.code} keyCode=${e.keyCode} repeat=${e.repeat} trusted=${e.isTrusted}`);
      e.preventDefault();
    }, true);
  }
  const held = new Map();
  function poll() {
    for (const pad of navigator.getGamepads()) {
      if (!pad) continue;
      pad.buttons.forEach((b, i) => {
        const key = `${pad.index}:${i}`;
        if (b.pressed !== Boolean(held.get(key))) {
          held.set(key, b.pressed);
          add(`gamepad ${pad.index} "${pad.id}" button ${i} ${b.pressed ? "down" : "up"}`);
        }
      });
    }
    requestAnimationFrame(poll);
  }
  requestAnimationFrame(poll);
</script>
</body>
</html>
```

`tools/overlay-phase0/pages/serve.ps1`:

```powershell
<#
Serves tools/overlay-phase0/pages at http://127.0.0.1:8765/ for a Windows guest with no Python.
Stop it with Ctrl+C. Linux guests use: python3 -m http.server 8765 --bind 127.0.0.1 --directory <pages>
#>
param([int]$Port = 8765)
$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
$listener = New-Object System.Net.HttpListener
$listener.Prefixes.Add("http://127.0.0.1:$Port/")
$listener.Start()
Write-Output "serving $root at http://127.0.0.1:$Port/"
try {
    while ($listener.IsListening) {
        $ctx = $listener.GetContext()
        $name = [IO.Path]::GetFileName($ctx.Request.Url.AbsolutePath)
        if (-not $name) { $name = 'probe.html' }
        $file = Join-Path $root $name
        if ($name -like '*.html' -and (Test-Path -LiteralPath $file)) {
            $bytes = [IO.File]::ReadAllBytes($file)
            $ctx.Response.ContentType = 'text/html; charset=utf-8'
            $ctx.Response.OutputStream.Write($bytes, 0, $bytes.Length)
        } else {
            $ctx.Response.StatusCode = 404
        }
        $ctx.Response.Close()
    }
} finally {
    $listener.Stop()
}
```

`extension/dist/` is already ignored (Task 1 Step 2).

- [ ] **Step 6: Run the tests to verify they pass**

Run: `python -m unittest discover -s C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec/tools/overlay-phase0/tests -p test_extension_build.py -v`
Expected: `Ran 4 tests`, then `OK` with no `skipped=`.

Whether a browser accepts the unpacked Chromium build is checked by guest protocol G16 (Task 8), not here.

- [ ] **Step 7: Commit**

```powershell
git -C C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec add tools/overlay-phase0/extension/src/content.js tools/overlay-phase0/extension/src/background.js tools/overlay-phase0/extension/build.py tools/overlay-phase0/pages/probe.html tools/overlay-phase0/pages/keylog.html tools/overlay-phase0/pages/serve.ps1 tools/overlay-phase0/tests/test_extension_build.py
git -C C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec commit -m "tools: add the probe extension, its build and the probe pages"
```

---

### Task 6: The setup scripts (policy, host registration, Firefox profile)

**Files:**
- Create: `tools/overlay-phase0/setup/windows.ps1`
- Create: `tools/overlay-phase0/setup/firefox-profile.ps1`
- Create: `tools/overlay-phase0/setup/linux_setup.py`
- Create: `tools/overlay-phase0/setup/firefox_profile.py`
- Create: `tools/overlay-phase0/tests/test_setup.py`

**Interfaces:**
- Consumes: an ids file (`{"chrome_web_store": "<32 a-p>", "edge_addons": "<32 a-p>", "gecko": "overlay-probe@cptk.app"}`;
  the real one is Task 7's `tools/overlay-phase0/ids.json`), the absolute path of `probe_host`, the signed XPI.
- Produces:
  - `windows.ps1 -IdsFile <f> -HostPath <abs> -ManifestDir <dir> [-Scope HKCU|HKLM] [-RegistryRoot <PS path>]
    [-Remove]`: adds one `ExtensionInstallForcelist` value per browser beside any others (never twice), the Edge
    hygiene DWORDs, two host manifests, and the three host keys (always under `HKCU:\Software` unless
    `-RegistryRoot` is given). `-RegistryRoot` replaces `<Scope>:\Software` for tests. `-Remove` takes out only what
    it added (a foreign forcelist entry stays) and prunes the Chrome and Edge policy keys it leaves empty.
  - `linux_setup.py --ids <f> --host <abs> [--root /] [--home <dir>] [--remove]`: five JSON files (the two policy
    files, the two Chromium host manifests, the per-user Firefox host manifest); prints any other managed file that
    sets `ExtensionInstallForcelist`.
  - `firefox_profile.py --profile <dir> --xpi <file> [--no-scan-scopes]` and
    `firefox-profile.ps1 -ProfileDir <dir> -Xpi <file> [-NoScanScopes]`: write the CPTK lines of `user.js` (each ends
    with `// cptk-overlay`; other lines are kept; the file is read and written as UTF-8) and copy the XPI to `extensions/overlay-probe@cptk.app.xpi` when it
    differs. Both print `copied` or `unchanged`.

- [ ] **Step 1: Write the failing tests**

`tools/overlay-phase0/tests/test_setup.py`:

```python
"""Tests for the Phase 0 setup scripts."""
import json
import os
import pathlib
import shutil
import subprocess
import sys
import tempfile
import unittest

KIT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(KIT / "setup"))
import firefox_profile  # noqa: E402
import linux_setup  # noqa: E402

IDS = {"chrome_web_store": "a" * 32, "edge_addons": "b" * 32, "gecko": "overlay-probe@cptk.app"}
# A user line with a non-ASCII character: the profile scripts must keep it byte for byte.
USER_LINE = 'user_pref("browser.startup.homepage", "https://example.org/café");'


def write_ids(folder):
    path = pathlib.Path(folder) / "ids.json"
    path.write_text(json.dumps(IDS))
    return path


class LinuxSetupTest(unittest.TestCase):
    def setUp(self):
        self.root = pathlib.Path(tempfile.mkdtemp())
        self.home = pathlib.Path(tempfile.mkdtemp())
        self.ids = write_ids(tempfile.mkdtemp())

    def run_setup(self, *extra):
        return linux_setup.main(
            ["--ids", str(self.ids), "--host", "/usr/libexec/cptk/probe_host", "--root", str(self.root),
             "--home", str(self.home), *extra]
        )

    def test_writes_policy_and_host_files(self):
        self.assertEqual(self.run_setup(), 0)
        chrome = json.loads((self.root / "etc/opt/chrome/policies/managed/cptk-overlay.json").read_text())
        self.assertEqual(chrome["ExtensionInstallForcelist"], ["a" * 32 + ";https://clients2.google.com/service/update2/crx"])
        edge = json.loads((self.root / "etc/opt/edge/policies/managed/cptk-overlay.json").read_text())
        self.assertFalse(edge["StartupBoostEnabled"])
        host = json.loads((self.root / "etc/opt/edge/native-messaging-hosts/app.cptk.overlay.json").read_text())
        self.assertEqual(host["allowed_origins"], ["chrome-extension://" + "a" * 32 + "/", "chrome-extension://" + "b" * 32 + "/"])
        ff = json.loads((self.home / ".mozilla/native-messaging-hosts/app.cptk.overlay.json").read_text())
        self.assertEqual(ff["allowed_extensions"], ["overlay-probe@cptk.app"])
        self.assertEqual(ff["path"], "/usr/libexec/cptk/probe_host")

    def test_setup_is_idempotent_and_keeps_foreign_entries(self):
        other = self.root / "etc/opt/chrome/policies/managed/zz-it.json"
        other.parent.mkdir(parents=True)
        other.write_text(json.dumps({"ExtensionInstallForcelist": ["c" * 32 + ";https://x"]}))
        self.run_setup()
        first = (self.root / "etc/opt/chrome/policies/managed/cptk-overlay.json").read_text()
        self.run_setup()
        self.assertEqual((self.root / "etc/opt/chrome/policies/managed/cptk-overlay.json").read_text(), first)
        self.assertIn("c" * 32, other.read_text())
        self.assertEqual(linux_setup.conflicts(self.root, "cptk-overlay.json"), [str(other)])

    def test_remove_deletes_only_its_files(self):
        other = self.root / "etc/opt/chrome/policies/managed/zz-it.json"
        other.parent.mkdir(parents=True)
        other.write_text(json.dumps({"ExtensionInstallForcelist": ["c" * 32 + ";https://x"]}))
        self.run_setup()
        self.run_setup("--remove")
        self.assertFalse((self.root / "etc/opt/chrome/policies/managed/cptk-overlay.json").exists())
        self.assertFalse((self.home / ".mozilla/native-messaging-hosts/app.cptk.overlay.json").exists())
        self.assertTrue(other.exists())

    def test_lists_the_folders_it_will_create(self):
        target = self.home / ".mozilla/native-messaging-hosts"
        self.assertEqual(linux_setup.missing_dirs(target), [self.home / ".mozilla", target])
        target.mkdir(parents=True)
        self.assertEqual(linux_setup.missing_dirs(target), [])

    def test_refuses_a_relative_host_path(self):
        with self.assertRaises(SystemExit):
            linux_setup.main(["--ids", str(self.ids), "--host", "probe_host", "--root", str(self.root),
                              "--home", str(self.home)])


class FirefoxProfileTest(unittest.TestCase):
    def setUp(self):
        self.profile = pathlib.Path(tempfile.mkdtemp())
        self.xpi = pathlib.Path(tempfile.mkdtemp()) / "probe.xpi"
        self.xpi.write_bytes(b"version-1")

    def test_profile_keeps_user_lines(self):
        (self.profile / "user.js").write_text(USER_LINE + "\n", encoding="utf-8")
        firefox_profile.prepare(self.profile, self.xpi)
        text = (self.profile / "user.js").read_text(encoding="utf-8")
        self.assertIn(USER_LINE, text)
        self.assertIn('user_pref("extensions.autoDisableScopes", 0); // cptk-overlay', text)
        self.assertIn('user_pref("extensions.startupScanScopes", 1); // cptk-overlay', text)
        firefox_profile.prepare(self.profile, self.xpi)
        self.assertEqual((self.profile / "user.js").read_text(encoding="utf-8"), text)

    def test_copies_only_when_the_xpi_differs(self):
        self.assertTrue(firefox_profile.prepare(self.profile, self.xpi))
        self.assertFalse(firefox_profile.prepare(self.profile, self.xpi))
        self.xpi.write_bytes(b"version-2")
        self.assertTrue(firefox_profile.prepare(self.profile, self.xpi))
        self.assertEqual((self.profile / "extensions/overlay-probe@cptk.app.xpi").read_bytes(), b"version-2")

    def test_no_scan_scopes_variant(self):
        firefox_profile.prepare(self.profile, self.xpi, scan_scopes=False)
        self.assertNotIn("startupScanScopes", (self.profile / "user.js").read_text(encoding="utf-8"))


@unittest.skipUnless(os.name == "nt" and shutil.which("powershell"), "Windows PowerShell only")
class FirefoxProfilePsTest(unittest.TestCase):
    """The Windows twin of FirefoxProfileTest: setup/firefox-profile.ps1, for guests with no Python."""

    def setUp(self):
        self.profile = pathlib.Path(tempfile.mkdtemp())
        self.xpi = pathlib.Path(tempfile.mkdtemp()) / "probe.xpi"
        self.xpi.write_bytes(b"version-1")

    def prepare(self, *extra):
        cmd = ["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File",
               str(KIT / "setup" / "firefox-profile.ps1"), "-ProfileDir", str(self.profile), "-Xpi", str(self.xpi),
               *extra]
        proc = subprocess.run(cmd, capture_output=True, text=True, timeout=120)
        self.assertEqual(proc.returncode, 0, proc.stderr)
        return proc.stdout.strip()

    def test_profile_keeps_user_lines(self):
        (self.profile / "user.js").write_text(USER_LINE + "\n", encoding="utf-8")
        self.prepare()
        text = (self.profile / "user.js").read_text(encoding="utf-8")
        self.assertIn(USER_LINE, text)
        self.assertIn('user_pref("extensions.autoDisableScopes", 0); // cptk-overlay', text)
        self.assertIn('user_pref("extensions.startupScanScopes", 1); // cptk-overlay', text)
        self.prepare()
        self.assertEqual((self.profile / "user.js").read_text(encoding="utf-8"), text)

    def test_copies_only_when_the_xpi_differs(self):
        self.assertEqual(self.prepare(), "copied")
        self.assertEqual(self.prepare(), "unchanged")
        self.xpi.write_bytes(b"version-2")
        self.assertEqual(self.prepare(), "copied")
        self.assertEqual((self.profile / "extensions/overlay-probe@cptk.app.xpi").read_bytes(), b"version-2")

    def test_no_scan_scopes_variant(self):
        self.prepare("-NoScanScopes")
        self.assertNotIn("startupScanScopes", (self.profile / "user.js").read_text(encoding="utf-8"))


@unittest.skipUnless(os.name == "nt" and shutil.which("powershell"), "Windows PowerShell only")
class WindowsSetupTest(unittest.TestCase):
    TEST_ROOT = r"HKCU:\Software\CPTK-Phase0-Test"

    def ps(self, *args):
        cmd = ["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", str(KIT / "setup" / "windows.ps1"), *args]
        return subprocess.run(cmd, capture_output=True, text=True, timeout=120)

    def tearDown(self):
        subprocess.run(["powershell", "-NoProfile", "-Command",
                        f"Remove-Item -LiteralPath '{self.TEST_ROOT}' -Recurse -ErrorAction SilentlyContinue"],
                       timeout=120)

    def test_setup_is_idempotent_and_keeps_foreign_entries(self):
        import winreg
        ids = write_ids(tempfile.mkdtemp())
        manifests = tempfile.mkdtemp()
        key = r"Software\CPTK-Phase0-Test\Policies\Google\Chrome\ExtensionInstallForcelist"
        with winreg.CreateKey(winreg.HKEY_CURRENT_USER, key) as k:
            winreg.SetValueEx(k, "1", 0, winreg.REG_SZ, "c" * 32 + ";https://x")
        args = ["-IdsFile", str(ids), "-HostPath", r"C:\cptk\probe_host.exe", "-ManifestDir", manifests,
                "-RegistryRoot", self.TEST_ROOT]
        self.assertEqual(self.ps(*args).returncode, 0)
        self.assertEqual(self.ps(*args).returncode, 0)
        with winreg.OpenKey(winreg.HKEY_CURRENT_USER, key) as k:
            values = {winreg.EnumValue(k, i)[0]: winreg.EnumValue(k, i)[1] for i in range(winreg.QueryInfoKey(k)[1])}
        self.assertEqual(values, {"1": "c" * 32 + ";https://x", "2": "a" * 32 + ";https://clients2.google.com/service/update2/crx"})
        host = r"Software\CPTK-Phase0-Test\Microsoft\Edge\NativeMessagingHosts\app.cptk.overlay"
        with winreg.OpenKey(winreg.HKEY_CURRENT_USER, host) as k:
            manifest_path = winreg.QueryValue(k, None)
        self.assertEqual(json.loads(pathlib.Path(manifest_path).read_text(encoding="utf-8-sig"))["path"], r"C:\cptk\probe_host.exe")
        self.assertEqual(self.ps(*args, "-Remove").returncode, 0)
        with winreg.OpenKey(winreg.HKEY_CURRENT_USER, key) as k:
            self.assertEqual(winreg.QueryInfoKey(k)[1], 1)

    def test_remove_leaves_no_policy_keys_behind(self):
        import winreg
        ids = write_ids(tempfile.mkdtemp())
        args = ["-IdsFile", str(ids), "-HostPath", r"C:\cptk\probe_host.exe", "-ManifestDir", tempfile.mkdtemp(),
                "-RegistryRoot", self.TEST_ROOT]
        self.assertEqual(self.ps(*args).returncode, 0)
        self.assertEqual(self.ps(*args, "-Remove").returncode, 0)
        for gone in (r"Policies\Google", r"Policies\Microsoft\Edge",
                     r"Microsoft\Edge\NativeMessagingHosts\app.cptk.overlay"):
            with self.assertRaises(FileNotFoundError, msg=gone):
                winreg.OpenKey(winreg.HKEY_CURRENT_USER, rf"Software\CPTK-Phase0-Test\{gone}")


if __name__ == "__main__":
    unittest.main()
```

- [ ] **Step 2: Run them to verify they fail**

Run: `python -m unittest discover -s C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec/tools/overlay-phase0/tests -p test_setup.py -v`
Expected: `Ran 1 test`, then `FAILED (errors=1)`: the module fails at import (`No module named 'firefox_profile'`),
which unittest reports as one failed test, so none of its 13 tests runs.

- [ ] **Step 3: Write the Linux setup and the Firefox profile scripts**

`tools/overlay-phase0/setup/linux_setup.py`:

```python
#!/usr/bin/env python3
"""Phase 0 setup on Linux: Chrome and Edge policy files and the native-messaging host manifests. Run with sudo."""
import argparse
import json
import os
import pathlib
import posixpath
import sys

CWS_UPDATE = "https://clients2.google.com/service/update2/crx"
EDGE_UPDATE = "https://edge.microsoft.com/extensionwebstorebase/v1/crx"
HOST = "app.cptk.overlay"
OURS = "cptk-overlay.json"


def plan(ids, host_path, root, home):
    r = pathlib.Path(root)
    origins = [f"chrome-extension://{ids['chrome_web_store']}/", f"chrome-extension://{ids['edge_addons']}/"]
    base = {"name": HOST, "description": "CPTK Overlay Probe host", "path": host_path, "type": "stdio"}
    return {
        r / "etc/opt/chrome/policies/managed" / OURS: {
            "ExtensionInstallForcelist": [f"{ids['chrome_web_store']};{CWS_UPDATE}"],
        },
        r / "etc/opt/edge/policies/managed" / OURS: {
            "ExtensionInstallForcelist": [f"{ids['edge_addons']};{EDGE_UPDATE}"],
            "StartupBoostEnabled": False,
            "BackgroundModeEnabled": False,
        },
        r / f"etc/opt/chrome/native-messaging-hosts/{HOST}.json": {**base, "allowed_origins": origins},
        r / f"etc/opt/edge/native-messaging-hosts/{HOST}.json": {**base, "allowed_origins": origins},
        pathlib.Path(home) / f".mozilla/native-messaging-hosts/{HOST}.json": {**base, "allowed_extensions": [ids["gecko"]]},
    }


def conflicts(root, ours):
    """Other managed files that also set ExtensionInstallForcelist: Chromium takes the last file's list."""
    found = []
    for folder in ("etc/opt/chrome/policies/managed", "etc/opt/edge/policies/managed"):
        path = pathlib.Path(root) / folder
        if not path.is_dir():
            continue
        for f in sorted(path.glob("*.json")):
            if f.name == ours:
                continue
            try:
                data = json.loads(f.read_text())
            except (OSError, ValueError):
                continue
            if "ExtensionInstallForcelist" in data:
                found.append(str(f))
    return found


def missing_dirs(folder):
    """The folders mkdir(parents=True) would create for folder, outermost first."""
    missing = []
    folder = pathlib.Path(folder)
    while not folder.exists():
        missing.append(folder)
        folder = folder.parent
    return list(reversed(missing))


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--ids", required=True)
    ap.add_argument("--host", required=True)
    ap.add_argument("--root", default="/")
    ap.add_argument("--home", default=None)
    ap.add_argument("--remove", action="store_true")
    args = ap.parse_args(argv)
    if not posixpath.isabs(args.host):  # a Linux path, even when the tests run on Windows
        ap.error("--host must be an absolute path")
    ids = json.loads(pathlib.Path(args.ids).read_text())
    owner = None
    home = args.home
    if home is None:
        sudo_user = os.environ.get("SUDO_USER")
        if sudo_user:
            import pwd

            entry = pwd.getpwnam(sudo_user)
            home, owner = entry.pw_dir, (entry.pw_uid, entry.pw_gid)
        else:
            home = os.path.expanduser("~")
    files = plan(ids, args.host, args.root, home)
    for path, content in files.items():
        if args.remove:
            if path.exists():
                path.unlink()
            continue
        made = missing_dirs(path.parent)
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(json.dumps(content, indent=2) + "\n")
        os.chmod(path, 0o644)
        if owner and str(path).startswith(str(home)):
            # Under sudo every folder mkdir made is root's; give the user back each one it made in their home.
            for folder in made:
                os.chown(folder, *owner)
            os.chown(path, *owner)
    for other in conflicts(args.root, OURS):
        print(f"setup: another policy file also sets ExtensionInstallForcelist: {other}")
    print(f"setup: {'removed' if args.remove else 'wrote'} {len(files)} files")
    return 0


if __name__ == "__main__":
    sys.exit(main())
```

`tools/overlay-phase0/setup/firefox_profile.py`:

```python
#!/usr/bin/env python3
"""Prepares a Firefox profile for the probe: CPTK's user.js lines and the sideloaded XPI. Run with Firefox closed."""
import argparse
import pathlib
import shutil
import sys

MARK = "// cptk-overlay"
PREFS = [
    ("extensions.autoDisableScopes", "0"),
    ("extensions.startupScanScopes", "1"),
    ("media.autoplay.default", "0"),
]


def prepare(profile, xpi, scan_scopes=True):
    """Returns True when the XPI was copied, False when the profile already had the same file."""
    profile = pathlib.Path(profile)
    profile.mkdir(parents=True, exist_ok=True)
    userjs = profile / "user.js"
    old = userjs.read_text(encoding="utf-8").splitlines() if userjs.exists() else []
    kept = [line for line in old if not line.rstrip().endswith(MARK)]
    ours = [
        f'user_pref("{name}", {value}); {MARK}'
        for name, value in PREFS
        if scan_scopes or name != "extensions.startupScanScopes"
    ]
    userjs.write_text("\n".join(kept + ours) + "\n", encoding="utf-8")
    extensions = profile / "extensions"
    extensions.mkdir(exist_ok=True)
    dest = extensions / "overlay-probe@cptk.app.xpi"
    data = pathlib.Path(xpi).read_bytes()
    if dest.exists() and dest.read_bytes() == data:
        return False
    shutil.copyfile(xpi, dest)
    return True


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--profile", required=True)
    ap.add_argument("--xpi", required=True)
    ap.add_argument("--no-scan-scopes", action="store_true")
    args = ap.parse_args(argv)
    print("copied" if prepare(args.profile, args.xpi, not args.no_scan_scopes) else "unchanged")
    return 0


if __name__ == "__main__":
    sys.exit(main())
```

- [ ] **Step 4: Write the Windows scripts**

`tools/overlay-phase0/setup/windows.ps1`:

```powershell
<#
Phase 0 setup on Windows: force-install policy for Chrome and Edge, the Edge hygiene policies, and the
native-messaging host registration for all three browsers. Policy goes under -Scope (HKCU by default; HKLM needs
an elevated shell); host keys always go under HKCU. -RegistryRoot replaces "<Scope>:\Software" for tests.
Windows PowerShell 5.1 compatible.
#>
[CmdletBinding()]
param(
    [ValidateSet('HKCU', 'HKLM')][string]$Scope = 'HKCU',
    [Parameter(Mandatory = $true)][string]$IdsFile,
    [Parameter(Mandatory = $true)][string]$HostPath,
    [Parameter(Mandatory = $true)][string]$ManifestDir,
    [string]$RegistryRoot,
    [switch]$Remove
)
$ErrorActionPreference = 'Stop'
$ids = Get-Content -Raw -LiteralPath $IdsFile | ConvertFrom-Json
$policyRoot = if ($RegistryRoot) { $RegistryRoot } else { "$($Scope):\Software" }
$hostRoot = if ($RegistryRoot) { $RegistryRoot } else { 'HKCU:\Software' }
$cwsUpdate = 'https://clients2.google.com/service/update2/crx'
$edgeUpdate = 'https://edge.microsoft.com/extensionwebstorebase/v1/crx'

function Ensure-Key([string]$Key) {
    if (-not (Test-Path -LiteralPath $Key)) { New-Item -Path $Key -Force | Out-Null }
}

function Set-ForcelistEntry([string]$Key, [string]$Id, [string]$UpdateUrl) {
    if (-not (Test-Path -LiteralPath $Key)) {
        if ($Remove) { return }
        Ensure-Key $Key
    }
    $item = Get-Item -LiteralPath $Key
    $names = @($item.GetValueNames())
    foreach ($n in $names) {
        if ([string]$item.GetValue($n) -like "$Id;*") {
            if ($Remove) { Remove-ItemProperty -LiteralPath $Key -Name $n }
            return
        }
    }
    if ($Remove) { return }
    $i = 1
    while ($names -contains [string]$i) { $i++ }
    New-ItemProperty -LiteralPath $Key -Name ([string]$i) -Value "$Id;$UpdateUrl" -PropertyType String | Out-Null
}

function Set-DwordPolicy([string]$Key, [string]$Name, [int]$Value) {
    if ($Remove) {
        if (Test-Path -LiteralPath $Key) { Remove-ItemProperty -LiteralPath $Key -Name $Name -ErrorAction SilentlyContinue }
        return
    }
    Ensure-Key $Key
    New-ItemProperty -LiteralPath $Key -Name $Name -Value $Value -PropertyType DWord -Force | Out-Null
}

function Set-HostKey([string]$Key, [string]$ManifestPath) {
    if ($Remove) {
        if (Test-Path -LiteralPath $Key) { Remove-Item -LiteralPath $Key -Recurse }
        return
    }
    Ensure-Key $Key
    Set-Item -LiteralPath $Key -Value $ManifestPath
}

$chromiumManifest = Join-Path $ManifestDir 'app.cptk.overlay.chromium.json'
$firefoxManifest = Join-Path $ManifestDir 'app.cptk.overlay.firefox.json'
if ($Remove) {
    Remove-Item -LiteralPath $chromiumManifest, $firefoxManifest -ErrorAction SilentlyContinue
} else {
    New-Item -ItemType Directory -Path $ManifestDir -Force | Out-Null
    $utf8 = New-Object System.Text.UTF8Encoding $false
    $chromium = [ordered]@{
        name = 'app.cptk.overlay'; description = 'CPTK Overlay Probe host'; path = $HostPath; type = 'stdio'
        allowed_origins = @("chrome-extension://$($ids.chrome_web_store)/", "chrome-extension://$($ids.edge_addons)/")
    }
    $firefox = [ordered]@{
        name = 'app.cptk.overlay'; description = 'CPTK Overlay Probe host'; path = $HostPath; type = 'stdio'
        allowed_extensions = @($ids.gecko)
    }
    [IO.File]::WriteAllText($chromiumManifest, ($chromium | ConvertTo-Json), $utf8)
    [IO.File]::WriteAllText($firefoxManifest, ($firefox | ConvertTo-Json), $utf8)
}

$pol = "$policyRoot\Policies"
Set-ForcelistEntry "$pol\Google\Chrome\ExtensionInstallForcelist" $ids.chrome_web_store $cwsUpdate
Set-ForcelistEntry "$pol\Microsoft\Edge\ExtensionInstallForcelist" $ids.edge_addons $edgeUpdate
Set-DwordPolicy "$pol\Microsoft\Edge" 'StartupBoostEnabled' 0
Set-DwordPolicy "$pol\Microsoft\Edge" 'BackgroundModeEnabled' 0
Set-HostKey "$hostRoot\Google\Chrome\NativeMessagingHosts\app.cptk.overlay" $chromiumManifest
Set-HostKey "$hostRoot\Microsoft\Edge\NativeMessagingHosts\app.cptk.overlay" $chromiumManifest
Set-HostKey "$hostRoot\Mozilla\NativeMessagingHosts\app.cptk.overlay" $firefoxManifest
if ($Remove) {
    # An empty policy key left behind can keep a browser "Managed by your organization"; prune the ones this script
    # creates once they hold nothing, deepest first. Policies and Policies\Microsoft are shared and never pruned.
    foreach ($k in @("$pol\Google\Chrome\ExtensionInstallForcelist", "$pol\Google\Chrome", "$pol\Google",
                     "$pol\Microsoft\Edge\ExtensionInstallForcelist", "$pol\Microsoft\Edge")) {
        if (Test-Path -LiteralPath $k) {
            $item = Get-Item -LiteralPath $k
            if ($item.ValueCount -eq 0 -and $item.SubKeyCount -eq 0) { Remove-Item -LiteralPath $k }
        }
    }
}
Write-Output ("setup: {0} (policy under {1}, host keys under {2})" -f ($(if ($Remove) { 'removed' } else { 'wrote' })), $policyRoot, $hostRoot)
```

`tools/overlay-phase0/setup/firefox-profile.ps1`:

```powershell
<#
Prepares a Firefox profile for the probe on Windows: CPTK's user.js lines and the sideloaded XPI.
Run with Firefox closed. Windows PowerShell 5.1 compatible. The Linux twin is firefox_profile.py.
#>
param(
    [Parameter(Mandatory = $true)][string]$ProfileDir,
    [Parameter(Mandatory = $true)][string]$Xpi,
    [switch]$NoScanScopes
)
$ErrorActionPreference = 'Stop'
$mark = '// cptk-overlay'
New-Item -ItemType Directory -Path $ProfileDir -Force | Out-Null
$userJs = Join-Path $ProfileDir 'user.js'
$kept = @()
if (Test-Path -LiteralPath $userJs) {
    # -Encoding UTF8: Windows PowerShell 5.1 would read the file as ANSI and change a non-ASCII line on write.
    $kept = @(Get-Content -LiteralPath $userJs -Encoding UTF8 | Where-Object { -not $_.TrimEnd().EndsWith($mark) })
}
$ours = @(
    "user_pref(""extensions.autoDisableScopes"", 0); $mark"
)
if (-not $NoScanScopes) { $ours += "user_pref(""extensions.startupScanScopes"", 1); $mark" }
$ours += "user_pref(""media.autoplay.default"", 0); $mark"
$utf8 = New-Object System.Text.UTF8Encoding $false
[IO.File]::WriteAllText($userJs, (($kept + $ours) -join "`n") + "`n", $utf8)
$extensions = Join-Path $ProfileDir 'extensions'
New-Item -ItemType Directory -Path $extensions -Force | Out-Null
$dest = Join-Path $extensions 'overlay-probe@cptk.app.xpi'
# Compare the bytes, as firefox_profile.py does. Not Get-FileHash: 5.1 defines it in a script module, and a
# PSModulePath that lists PowerShell 7's modules first hides it.
$new = [IO.File]::ReadAllBytes((Resolve-Path -LiteralPath $Xpi).ProviderPath)
$same = (Test-Path -LiteralPath $dest) -and
    ([Convert]::ToBase64String([IO.File]::ReadAllBytes((Resolve-Path -LiteralPath $dest).ProviderPath)) -eq [Convert]::ToBase64String($new))
if ($same) { Write-Output 'unchanged' } else { Copy-Item -LiteralPath $Xpi -Destination $dest -Force; Write-Output 'copied' }
```

- [ ] **Step 5: Run the tests to verify they pass**

Run: `python -m unittest discover -s C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec/tools/overlay-phase0/tests -p test_setup.py -v`
Expected on Windows: `Ran 13 tests` (5 Linux setup, 3 Firefox profile, 3 Firefox profile through PowerShell, 2
Windows setup), then `OK` with no `skipped=`. On Linux the 5 Windows tests are skipped.

- [ ] **Step 6: Commit**

```powershell
git -C C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec add tools/overlay-phase0/setup/windows.ps1 tools/overlay-phase0/setup/firefox-profile.ps1 tools/overlay-phase0/setup/linux_setup.py tools/overlay-phase0/setup/firefox_profile.py tools/overlay-phase0/tests/test_setup.py
git -C C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec commit -m "tools: add the Phase 0 setup scripts"
```

---

### Task 7: The store accounts and CPTK Overlay Probe 0.0.1 (the user's steps, then the record)

This task is mostly the user's: three accounts, three uploads. Start it as soon as Task 5 is done, because store
review can take days and G1, G10 and G13 cannot run before the listings are live.

**Files:**
- Create: `tools/overlay-phase0/ids.json`
- Create: `tools/overlay-phase0/listing.md` (the listing text, so all three stores say the same thing)

**Interfaces:**
- Consumes: `extension/dist/cptk-overlay-probe-chromium-0.0.1.zip`, `extension/dist/firefox/` (Task 5).
- Produces: `ids.json` with `chrome_web_store`, `edge_addons` and `gecko`; the signed XPI at
  `C:/Users/jscha/ClaudeScratch/streamflex-handoff/overlay-phase0/cptk-overlay-probe-0.0.1.xpi`.

- [ ] **Step 1: Write the listing text**

`tools/overlay-phase0/listing.md`:

```markdown
# CPTK Overlay Probe: store listing text

**Name:** CPTK Overlay Probe
**Summary (132 characters max):** Drives streaming sites with a TV remote inside CPTK's own browser profiles. It does
nothing anywhere else.
**Visibility:** Unlisted (Chrome Web Store), Hidden (Edge Add-ons), unlisted self-distribution (AMO).
**Category:** Accessibility.

**Description:**
CPTK Overlay Probe is part of CPTK, a 10-foot launcher for living-room PCs. CPTK opens streaming sites in a browser
profile of its own and installs this extension there, so a TV remote or a gamepad can move around the site.

The extension stays inactive in every other profile, including your everyday browsing. It talks only to the CPTK
program on the same computer, through the browser's native messaging, and sends nothing over the network.

**Single purpose:** Lets a TV remote or gamepad navigate web pages in browser profiles that CPTK starts.

**Permission justifications:**
- *nativeMessaging:* to talk to the CPTK program on the same computer, which starts the browser and handles the
  remote's Home button.
- *storage:* to remember, per browser profile, that the profile belongs to CPTK, so the extension stays inactive
  everywhere else.
- *Host permissions on all sites:* CPTK users add their own media servers (Jellyfin, Emby) at addresses that are
  not known in advance; the extension does nothing on a page until CPTK has started that profile.

**Data use:** collects no user data and sends nothing off the computer.
```

- [ ] **Step 2: (The user) Create the three accounts**

1. **Chrome Web Store:** sign in at `https://chrome.google.com/webstore/devconsole` with the Google account that
   will own CPTK's listings, accept the developer agreement, pay the one-time registration fee, and set the
   publisher display name to `CPTK`.
2. **Edge Add-ons:** sign in at `https://partner.microsoft.com/dashboard/microsoftedge/overview` with the Microsoft
   account that will own them, and register for the Microsoft Edge program (no fee). Publisher display name `CPTK`.
3. **AMO:** sign in at `https://addons.mozilla.org/developers/` with a Mozilla account, accept the distribution
   agreement, then create API credentials at `https://addons.mozilla.org/developers/addon/api/key/` (a JWT issuer
   and secret). Keep both in the password manager.

- [ ] **Step 3: Build the packages**

```powershell
python C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec/tools/overlay-phase0/extension/build.py
```

Expected: `built ...cptk-overlay-probe-chromium-0.0.1.zip` and `built ...cptk-overlay-probe-firefox-0.0.1.zip`.

- [ ] **Step 4: (The user) Upload to the Chrome Web Store**

In the developer console: *New item*, upload `cptk-overlay-probe-chromium-0.0.1.zip`, fill the listing from
`listing.md`, set **Visibility: Unlisted**, fill *Privacy practices* from its single-purpose, permission and
data-use lines, and submit for review. The item id (32 characters, `a` to `p`) shows on the item's page at once.

- [ ] **Step 5: (The user) Upload to Edge Add-ons**

In Partner Center: *Create new extension*, upload the same zip, set **Visibility: Hidden**, fill the listing and
the permission justifications from `listing.md`, and submit. Its id shows on the extension's overview page.

- [ ] **Step 6: Sign the Firefox build on AMO**

With the user's AMO credentials in the environment (never written to a file):

```powershell
$env:WEB_EXT_API_KEY = '<the JWT issuer>'
$env:WEB_EXT_API_SECRET = '<the JWT secret>'
npx --yes web-ext@8 sign --source-dir C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec/tools/overlay-phase0/extension/dist/firefox --channel unlisted --artifacts-dir C:/Users/jscha/ClaudeScratch/streamflex-handoff/overlay-phase0
```

Expected: a signed `.xpi` in that folder. Rename it to `cptk-overlay-probe-0.0.1.xpi`.

- [ ] **Step 7: Record the ids**

`tools/overlay-phase0/ids.json` (the two store ids are the real ones from Steps 4 and 5):

```json
{
  "chrome_web_store": "<the 32-character id from the Chrome Web Store>",
  "edge_addons": "<the 32-character id from Edge Add-ons>",
  "gecko": "overlay-probe@cptk.app"
}
```

Check both ids match `^[a-p]{32}$` before committing:

```powershell
$ids = Get-Content -Raw C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec/tools/overlay-phase0/ids.json | ConvertFrom-Json
$ids.chrome_web_store -match '^[a-p]{32}$' -and $ids.edge_addons -match '^[a-p]{32}$'
```

Expected: `True`.

- [ ] **Step 8: Wait for both reviews**

Each store emails when the item is published. G1, G10 and G13 need both live; the other G items do not.

- [ ] **Step 9: Commit**

```powershell
git -C C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec add tools/overlay-phase0/ids.json tools/overlay-phase0/listing.md
git -C C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec commit -m "tools: record CPTK Overlay Probe's store ids and listing text"
```

---

### Task 8: The guest protocol and the hand-off to qa-harness-sf

**Files:**
- Create: `design/research/overlay/phase0/guest-protocol.md`

**Interfaces:**
- Consumes: every kit command (Tasks 1-6), `ids.json` and the signed XPI (Task 7).
- Produces: the protocol qa-harness-sf automates in its own repo, and the results format Task 10 collects.

- [ ] **Step 1: Write the protocol**

`design/research/overlay/phase0/guest-protocol.md`:

````markdown
# Streaming overlay Phase 0: the guest protocol

For qa-harness-sf. Each item below is one measurement from the spec's Phase 0
(`design/specs/2026-10-27-streaming-overlay-design.md`). qa-harness automates them in its own repo against its own
guests; this file says what to set up, what to do, what to read, and what passes. Every result goes into
`results.md` beside this file, one row per item, with the evidence path.

## Common setup

- **The kit:** build `tools/overlay-phase0/` in the guest (README there) or copy the built `probe_host`,
  `probe_spawn` (with its SDL3 library), `pages/`, `setup/` and `ids.json` in. The signed XPI is in
  `C:/Users/jscha/ClaudeScratch/streamflex-handoff/overlay-phase0/cptk-overlay-probe-0.0.1.xpi` on the host.
- **The page server:** Windows `powershell -File pages\serve.ps1`; Linux
  `python3 -m http.server 8765 --bind 127.0.0.1 --directory pages`. The probe page is
  `http://127.0.0.1:8765/probe.html`.
- **A launch token** is any 32 lowercase hex characters, for example `0123456789abcdef0123456789abcdef`, appended
  as `#cptk=<token>`.
- **Evidence:** the probe host's `probe.log` (`%LOCALAPPDATA%\cptk-probe\probe.log`, `~/.cptk-probe/probe.log`),
  `probe_spawn`'s JSON lines, and a screenshot where a line says so.
- **Browsers:** Windows Edge `C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe`, Chrome
  `C:\Program Files\Google\Chrome\Application\chrome.exe`, Firefox `C:\Program Files\Mozilla Firefox\firefox.exe`;
  Linux `/opt/microsoft/msedge/msedge`, `/opt/google/chrome/chrome`, and the full Firefox's own path. Record each
  browser's version with every result.
- **Fresh profiles:** every run uses a new, empty profile directory unless the item says otherwise.
- **Chromium launch line:** `<exe> --user-data-dir=<profile> --app=<url> --start-fullscreen --no-first-run
  --disable-session-crashed-bubble`. **Firefox:** run `setup/firefox-profile.ps1` (Windows) or
  `setup/firefox_profile.py` (Linux) on the profile first, then `<exe> -kiosk <url> -profile <profile> -no-remote`
  (Windows adds `-wait-for-browser`).

## The items

### G1. Store force-install from HKCU policy (Windows 11 Home)

1. As the standard user: `powershell -File setup\windows.ps1 -IdsFile ids.json -HostPath <abs probe_host.exe>
   -ManifestDir %LOCALAPPDATA%\cptk-probe\hosts`.
2. Start Edge with the Chromium launch line on `http://127.0.0.1:8765/probe.html#cptk=<token>`; wait 60 s.
3. Repeat for Chrome.
4. Repeat 1-3 with `-Scope HKLM` from an elevated shell, after `-Remove` of the HKCU setup.

**Read:** `probe.log` has a `start` line from each browser's origin; `edge://policy` / `chrome://policy` list the
forcelist with status OK (screenshot). **Pass:** both browsers, HKCU, first launch, no prompt.

### G2. A standard user writes HKCU\Software\Policies

As a standard (non-admin) user: `reg add "HKCU\Software\Policies\Google\Chrome" /v CptkProbe /t REG_DWORD /d 1`,
then delete it. **Pass:** exit code 0. (If G1's HKCU step succeeded, G2 passed with it.)

### G3. UAC by remote

As an administrator, start `powershell -Command "Start-Process powershell -Verb RunAs"` and answer the consent
prompt with keys only: Left, Right, Tab and Enter, sent through the guest's keyboard. Record which button has focus
first and which keys reach Yes. **Pass:** the prompt can be answered Yes without a mouse.

### G4. Exit detection (all three browsers, both OSes)

`probe_spawn run <browser launch line>`; after 20 s close the window with Alt+F4 through the guest's keyboard,
and note the time sent. Then repeat and send `close` on `probe_spawn`'s stdin instead. **Read:** the `exited` line's
`t` minus the Alt+F4 time, and `ms_since_close`. Edge runs with G1's hygiene policies in place. **Pass:** each exit
seen within 2000 ms. Record Edge without the hygiene policies too, as information.

### G5. Trusted injection

With the probe page open in each browser (setup as G1 on Windows, `linux_setup.py` on Linux), run
`probe_spawn keys 3000 F` and, after reloading, `probe_spawn keys 3000 P`. On Linux run it on GNOME Wayland and KDE
Wayland. **Read:** `probe.log` `key` lines (`trusted`, `activation`), `fullscreen` and `play` lines. **Pass:**
`trusted: true`, `fullscreen ok`, `play ok`, on every browser and desktop.

### G6. Focus after the browser closes

`probe_spawn focus <browser launch line on probe.html>`; after 15 s send Alt+F4, then 2 s later press Enter through
the guest's keyboard. Twenty times per OS and desktop (Windows 11, GNOME Wayland, KDE, an X11 session). **Read:**
`focus_after_exit.gained` and `key_after_exit.ms`. **Pass:** 20 of 20 with `gained: true` and a key seen.

### G7. Key codes through the launcher's paths

`probe_spawn keylog 60` while pressing, through the guest's keyboard and a USB-forwarded or emulated consumer-control
device where the guest has one: Home (Browser Home), Back (Browser Back), Menu (Apps), Play/Pause, the arrows,
Enter, Backspace. **Read:** the `raw_key` / `raw_hid` lines. **Pass:** every key appears. (The user's real remote
is run sheet H2.)

### G8. Gamepads in the background

Windows guest with an emulated Xbox pad (ViGEmBus); Linux guest with a uinput gamepad. Start
`probe_spawn pad 60 <backend>`, then give the browser focus (probe page open), then press A, B and the D-pad.
Backends: `default`, `xinput`, `rawinput`, `wgi` (Windows); `default` (Linux). **Read:** `button` lines with
`focused: false`. **Pass:** at least one Windows backend, and Linux, report buttons while unfocused. Record every
backend's result.

### G9. Windows Firefox native messaging and exit

Profile prepared with `firefox-profile.ps1`; `windows.ps1` run for the host keys. `probe_spawn run <firefox launch
line on probe.html>`; press F once; then Alt+F4. **Read:** `probe.log` `start` from Firefox; `exited` seen.
**Pass:** both.

### G10. Linux Chrome and Edge policy

`sudo python3 setup/linux_setup.py --ids ids.json --host <abs probe_host>`, then start each browser on the probe
page. Add a second managed file `zz-other.json` that sets `ExtensionInstallForcelist` to another id and start
again. **Read:** `chrome://policy`, `edge://policy` (screenshots), `probe.log` `start` lines, the script's conflict
line. **Pass:** both browsers install and report on the first launch; with `zz-other.json` the extension is gone
and the script names the file.

### G11. Linux browser discovery

On each of: Ubuntu default (the snap Firefox), Ubuntu with Mozilla's deb (pinned as Mozilla documents), Debian with
`firefox-esr`, Fedora, Arch, a `/opt/firefox` tarball, and a Flatpak-only box, record for each candidate path in the
spec's **Browser discovery** list whether it exists, is an ELF, has `application.ini` or `omni.ja` beside it, and
where `realpath $(command -v firefox)` points. **Pass:** the spec's rules classify every box correctly (full
Firefox found only where one is installed natively).

### G12. Linux Edge and Widevine

Stock Edge on Ubuntu and Fedora; open `edge://components` (screenshot: is a Widevine CDM listed?) and play the
Shaka Player demo's Widevine stream (`https://shaka-player-demo.appspot.com/`, asset "Angel One (multicodec,
multilingual, Widevine)"). **Pass:** it plays without `--enable-features=msWidevinePlatform`. Record the result
with the flag too, as information.

### G13. The Firefox profile sideload

Windows and Linux, current release Firefox, with an everyday Firefox installed:
1. Prepare a fresh profile; start Firefox `-kiosk` on the probe page. **Read:** `probe.log` `start` with
   `allUrls: true`.
2. Close it, rebuild the XPI as version 0.0.2 (change `VERSION` in `build.py` locally, sign it the same way), copy
   it in with the profile script, start again. **Read:** the `start` line's `version` is 0.0.2.
3. Repeat 2 with `--no-scan-scopes` (`-NoScanScopes`), on a profile that already ran 0.0.1.
4. Open the everyday Firefox: `about:addons` and `about:policies` (screenshots).

**Pass:** 1 runs with no prompt; 2 shows 0.0.2; the everyday Firefox has no CPTK add-on and no CPTK policy. Step 3's
result says whether `startupScanScopes` is needed.

### G14. Widevine in a fresh Firefox profile

A fresh prepared profile on the Shaka demo's Widevine stream; time from launch to playback, and whether
`gmp-widevinecdm` appeared in the profile. **Pass:** it plays; record the time.

### G15. A one-hour soak

Each browser, prepared as above, on the Shaka demo's Widevine stream for 60 minutes with the probe active, Space
sent through `probe_spawn keys` every 5 minutes. **Pass:** no playback error, no reload.

### G16. Chrome and Edge `Extensions.loadUnpacked` (the development loop)

Start each browser with `--user-data-dir=<fresh> --remote-debugging-pipe --enable-unsafe-extension-debugging` from a
small CDP client and send `Extensions.loadUnpacked` with `extension/dist/chromium`. **Read:** the extension list
(screenshot), any banner, `navigator.webdriver` in the `start` line. **Pass:** it loads. (Development only.)

## Results format

`results.md`, one row per item: `| G<n> | pass / fail / partial | browsers and versions | evidence path | note |`.
````

- [ ] **Step 2: Commit**

```powershell
git -C C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec add design/research/overlay/phase0/guest-protocol.md
git -C C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec commit -m "docs: write the Phase 0 guest protocol"
```

- [ ] **Step 3: Hand it to qa-harness-sf (after v0.6.0 has shipped)**

Write `C:/Users/jscha/ClaudeScratch/streamflex-handoff/overlay-phase0/README.md` naming: the protocol's path on
`master` (after this branch merges), the built kit's location, `ids.json`, the signed XPI, and which items wait
for the store reviews (G1, G10, G13). Then send qa-harness-sf one message starting
`From the streamflex tab: Phase 0 of the streaming overlay is ready for your guests`, with that folder's path. Its
plan, bookings and guests are its own.

---

### Task 9: The user's run sheet

**Files:**
- Create: `design/research/overlay/phase0/run-sheet.md`

- [ ] **Step 1: Write the run sheet**

`design/research/overlay/phase0/run-sheet.md`:

````markdown
# Streaming overlay Phase 0: your run sheet (about an hour)

Four checks only a real GPU, display chain and remote can answer. Use your HTPC, your TV and AVR as they are wired
for watching, and your real remote. Write each answer in the table at the end. Nothing here needs a keyboard
after setup except where it says so.

**Before you start:** have the Netflix, Disney+ and Prime Video logins ready. The setup comes partway through H1, not
here: it installs the probe into every Chrome and Edge profile, so the rows before it must run without it.

## H1. Does anything we add lower the picture or sound? (about 35 minutes)

Netflix's stats overlay is **Ctrl+Shift+Alt+D** while a title plays. Use one 4K HDR title with Atmos for every row
(any Netflix original marked "Ultra HD", "Dolby Vision" or "HDR", and "Dolby Atmos"). Let it play 60 seconds before
reading, and read: **Playing bitrate (video)** resolution, the **DRM** line, whether it says **HDR** or **Dolby
Vision**, and the **audio** codec and channels.

**Part A: without the probe.** Run these rows first, in this order.

| Row | Browser | How to start it | Fill in |
|---|---|---|---|
| H1.1 | Edge | normally, your usual profile | baseline |
| H1.2 | Edge | `msedge.exe --user-data-dir=%LOCALAPPDATA%\cptk-probe\p1 --no-first-run https://www.netflix.com` (sign in once) | |
| H1.3 | Edge | `msedge.exe --user-data-dir=%LOCALAPPDATA%\cptk-probe\p1 --app=https://www.netflix.com#cptk=0123456789abcdef0123456789abcdef --start-fullscreen --no-first-run` | |
| H1.5 | Chrome | normally | baseline |
| H1.6a | Chrome | as H1.3, with `chrome.exe` and `--user-data-dir=%LOCALAPPDATA%\cptk-probe\c1` (sign in once) | |
| H1.7 | Firefox | normally | baseline (1080p expected) |

**Setup** (with a keyboard, once, after Part A):
1. Close every browser window.
2. Run the setup, as yourself, in a PowerShell window:
   `powershell -File <kit>\setup\windows.ps1 -IdsFile <kit>\ids.json -HostPath <kit>\probe_host.exe -ManifestDir $env:LOCALAPPDATA\cptk-probe\hosts`

**Part B: with the probe.** Check that the probe box shows in the bottom-left corner on each row.

| Row | Browser | How to start it | Fill in |
|---|---|---|---|
| H1.4 | Edge | as H1.3 | |
| H1.6b | Chrome | as H1.6a | |
| H1.8 | Firefox | prepared profile: `powershell -File <kit>\setup\firefox-profile.ps1 -ProfileDir %LOCALAPPDATA%\cptk-probe\f1 -Xpi <kit>\cptk-overlay-probe-0.0.1.xpi`, then `firefox.exe -kiosk https://www.netflix.com#cptk=0123456789abcdef0123456789abcdef -profile %LOCALAPPDATA%\cptk-probe\f1 -no-remote -wait-for-browser` | |
| H1.9 | Edge | as H1.4, on Disney+ and on Prime Video (1080p expected) | |

## H2. What does your remote send? (about 10 minutes)

1. Start `<kit>\probe_spawn.exe keylog 300` in a PowerShell window, and open
   `http://127.0.0.1:8765/keylog.html` in Edge (start `<kit>\pages\serve.ps1` first).
2. Press every button once, then hold each of OK, Back and Home for two seconds.
3. Save both: the PowerShell window's text and a screenshot of the page.

## H3. Atmos at the receiver (about 5 minutes)

In H1.4's setup, play the Atmos title and read the receiver's display. Note what it shows (Dolby Atmos, Dolby
Digital Plus 5.1, PCM 2.0).

## H4. The display chain and sleep (about 10 minutes)

1. In H1.4's setup, confirm 2160p and HDR in the stats overlay.
2. Put the PC to sleep for one minute with the title paused, wake it, and press play with the remote. Note whether it
   plays, errors or goes black.

## Cleanup: put your browsers back (about 5 minutes)

The setup left your everyday Chrome and Edge "Managed by your organization", with the probe installed and Edge's
Startup Boost and background mode turned off. This puts them back.

1. Close every browser window.
2. Run the setup again with `-Remove`, in a PowerShell window:
   `powershell -File <kit>\setup\windows.ps1 -IdsFile <kit>\ids.json -HostPath <kit>\probe_host.exe -ManifestDir $env:LOCALAPPDATA\cptk-probe\hosts -Remove`
   It prints `setup: removed`.
3. Open Edge and Chrome normally. `edge://policy` and `chrome://policy` list nothing from CPTK, "Managed by your
   organization" is gone from the menu, and CPTK Overlay Probe is gone from the extensions page. If any of it is still
   there, close the browser and open it once more, then note it in the table's Cleanup row.
4. Delete the folder `%LOCALAPPDATA%\cptk-probe` (the test profiles and host files).

## Your answers

| Row | Resolution | DRM line | HDR / DV | Audio | Notes |
|---|---|---|---|---|---|
| H1.1 | | | | | |
| H1.2 | | | | | |
| H1.3 | | | | | |
| H1.4 | | | | | |
| H1.5 | | | | | |
| H1.6a | | | | | |
| H1.6b | | | | | |
| H1.7 | | | | | |
| H1.8 | | | | | |
| H1.9 Disney+ | | | | | |
| H1.9 Prime | | | | | |
| H3 | | | | | |
| H4 | | | | | |
| Cleanup | | | | | |

Send the filled table, the H2 files and any screenshots back to the streamflex tab.
````

- [ ] **Step 2: Commit**

```powershell
git -C C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec add design/research/overlay/phase0/run-sheet.md
git -C C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec commit -m "docs: write the Phase 0 run sheet"
```

---

### Task 10: Collect the results and record the branches taken

Runs after qa-harness-sf's results and the user's run sheet have come back. It closes Phase 0.

**Files:**
- Create: `design/research/overlay/phase0/results.md`

- [ ] **Step 1: Write the results file**

`design/research/overlay/phase0/results.md` holds three sections:
1. **Guest items:** one row per G item in the protocol's results format, copied from qa-harness-sf's report, each
   with its evidence path.
2. **Run sheet:** the user's table from H1-H4, verbatim, plus where the H2 files are kept.
3. **Branches taken:** one line per item that failed or was partial, naming the spec's "If it fails" outcome that
   now applies (for example "G2 failed: setup uses the elevated helper"), and every "back to the user" result as an
   open question.

- [ ] **Step 2: Check every item has a result**

```powershell
$text = Get-Content -Raw C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec/design/research/overlay/phase0/results.md
foreach ($id in (1..16 | ForEach-Object { "G$_" }) + (1..4 | ForEach-Object { "H$_" })) { if ($text -notmatch "\b$id\b") { "missing $id" } }
```

Expected: no output.

- [ ] **Step 3: Commit, and raise any "back to the user" result**

```powershell
git -C C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec add design/research/overlay/phase0/results.md
git -C C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec commit -m "docs: record the Phase 0 results"
```

Any result whose branch says "back to the user" is put to the user with AskUserQuestion before R1's plan is
written. With none, R1's plan (v0.7.0, launch and Home) is next.
