# Streaming overlay Phase 0 kit: G17 and G18 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or
> superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Give the Phase 0 kit what G17 step 4 and G18 need: a `probe_spawn pip` watch that reports each new
top-level window of a supervised browser and, when picture-in-picture opens, closes that window and raises the
browser's main window, a
`probe_spawn text` command that types a string as trusted OS keystrokes, and a React text-field probe page; and
cut the kit's Linux setup to Firefox only.

**Architecture:** `pip` reuses `run`'s supervision through a new watch hook (`probe_supervise`) that calls a tick on
every pass of `run`'s loop; a platform-free tracker in `pip.c` turns each tick's window list into `window`,
`window_changed`, `window_gone`, `pip_seen`, `pip_closed`, `pip_gone`, `raised` and `foreground` lines, and a
platform file lists, closes, raises and reads the focus (Windows: the job's processes with `EnumWindows`,
`WM_CLOSE`; Linux: `_NET_CLIENT_LIST` over X11 or XWayland, `_NET_CLOSE_WINDOW`, and a `pip_session` line that says
what a Wayland session cannot see). `text` decodes and checks the string in `text.c`, then a platform file types
each character (Windows: `KEYEVENTF_UNICODE` per UTF-16 code unit; Linux: the key and Shift or AltGr that an
xkbcommon keymap of the session's layout gives it, else a dead-key or compose sequence from xkbcommon's compose
table for the session's locale, sent through uinput). The page is
plain HTML with React 18.3.1's UMD builds vendored beside it, and a Content-Security-Policy that forbids every
network request.

**Tech Stack:** C17 (MSVC `/W4`, GCC `-Wall -Wextra`), Win32 (`user32`, `shell32`), libX11 (optional), libxkbcommon
1.6 or later with its compose API (optional), Linux uinput, CMake 3.18+, Python 3 `unittest`, Windows PowerShell 5.1, React 18.3.1 (UMD).

**Spec:** `design/specs/2026-10-27-streaming-overlay-design.md`: **Picture-in-picture** (under Setup), **The launch
path** step 5, **The channel** (`send_text`), the Phase 0 table rows G17 and G18, and Linux Firefox only (decided
together, 2026-11-03). The protocol entries this plan builds for are `design/research/overlay/phase0/guest-protocol.md`
§G17 and §G18. House conventions come from `design/plans/2026-10-27-overlay-phase0.md` and
`tools/overlay-phase0/README.md`.

All paths below are relative to the worktree `C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec` (branch
`design/streaming-overlay`, PR #73 in `bilbospocketses/streamflex`); every command uses absolute paths.

## Global Constraints

- **Scope:** `tools/overlay-phase0/` only, plus `design/research/overlay/phase0/guest-protocol.md`, two notes in
  `design/plans/2026-10-27-overlay-phase0.md`, the spec's two picture-in-picture fallback sentences (Task 7), and
  one `.gitattributes` line. Nothing under `src/`, `assets/`,
  `config/` or the root `CMakeLists.txt` changes.
- **Linux supports Firefox only** (decided together, 2026-11-03): the kit writes no Linux Chrome or Edge file.
- **`send_text`'s rules, from the spec:** printable characters only (no control characters), at most 256 a message;
  on Windows `SendInput` with `KEYEVENTF_UNICODE`; on Linux uinput, with the key and Shift or AltGr that produce the
  character in the session's keyboard layout; a character that layout cannot produce is not typed and is named back.
- **Never fake events:** typing, closing and raising go through the OS only (`SendInput`, `WM_CLOSE`,
  `SetForegroundWindow`, uinput, `_NET_CLOSE_WINDOW`, `_NET_ACTIVE_WINDOW`). The watch acts on the browser's own
  windows, never on a provider's page (file 30). The page never sets a value or dispatches an event.
- **The page makes no network request:** React is vendored (pinned version, license file, sha256 recorded), and the
  page's policy is `default-src 'none'`.
- **The 2000 ms raise is a protocol pass bar, printed as data** (`foreground.ms`), never a unit-test assertion.
- **No duration limits on correct code:** a test asserts behavior, never `took < N`. A test's hang guard is 120 s
  or more and its failure says HUNG and names what it waited on. A child process the kit itself starts and waits on
  is bounded by no progress (no output and no CPU), never by total time.
- **Unit tests need no real desktop:** the pure parts (picture-in-picture classification, session kind, argument
  parsing, UTF-8 decoding, character-to-key mapping, layout discovery) are reached through `--classify`,
  `--dry-run` and environment variables. The Windows window test follows the existing `test_close_posts_wm_close`
  (a tkinter child) and runs `pip --no-raise`, so it never takes the focus (its close goes only to the test's own
  tkinter window; `--no-close` turns that off too). Real raises, whether the video kept playing, and real typing are
  the guest protocol's.
- **C:** C17; MSVC `/W4` and GCC `-Wall -Wextra` with no new warning; every string the kit did not write goes
  through `probe_json_string`; every result is one JSON line from `probe_emit`. New tests read `probe_spawn`'s
  stdout as UTF-8 (`encoding="utf-8"`).
- **Windows scripts run in Windows PowerShell 5.1.** Linux scripts are Python 3.
- **Heavy work goes through the box queue.** A Docker build or run is BIG: in its own call,
  `python C:/Users/jscha/.claude/hooks/box_queue.py request --session <session id> --work "<what>" --overlap ok --touches workdir:c:/users/jscha/claudescratch/wt-streamflex-overlay-spec --big`,
  then the Docker command in the next call; one request covers one command. The whole tests folder on Windows is
  heavy (the same request without `--big`). One test file, a build and git are light.
- **Git:** `git -C "C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec" ...` always, never `cd`; stage named
  paths only; commits are signed automatically; merges are squash-only, later.
- **American spelling everywhere** (a gate checks added lines and commit messages). **Tier-blind:** no text names a
  paid tier, plan, price or subscription.
- **Model routing:** Opus implements and reviews each task; Sonnet re-reviews fix rounds. Every task's tests run on
  Windows natively and in the Linux container.

## Review Focus

1. **A picture-in-picture window whose title arrives after it is shown** (or a window retitled into one): the watch
   must re-classify it on `window_changed`, close it and raise then, and count `ms` from the window's first
   appearance (Task 2 test `test_pip_closes_a_retitled_window_and_names_the_main_one`).
2. **A string file saved by an editor** (a UTF-8 byte order mark, a trailing CRLF): both dropped; a line break
   inside the text is a control character, refused with nothing typed (Task 4 tests
   `test_a_file_drops_its_bom_and_one_line_ending` and `test_an_inner_line_break_in_a_file_is_refused`).
3. **Malformed UTF-8 in a string file** (an encoded surrogate, an overlong form, a stray byte, a cut sequence):
   refused as `not_utf8` before anything is typed (Task 4 test `test_malformed_utf8_in_a_file_is_refused`).
4. **A layout source that hangs** (a `gsettings` that never answers): stalled after 10 s with no output and no CPU
   use, killed, named, and the next source tried; the command never hangs (Task 5 test
   `test_a_stalled_source_is_named_and_skipped`).
5. **No source can tell the layout** (a bare session; `sudo` stripping the environment): the whole command is
   refused with every source tried named, nothing typed, exit 1 (Task 5 test
   `test_no_source_refuses_and_types_nothing`).
6. **No compose table for the session's locale** (an unusual `LANG`, or none at all): dead-key and compose paths
   are unavailable; the `compose` line says so, the directly typable characters are still typed, and each character
   that needed a sequence is named untyped with that reason, exit 3 (Task 5 test
   `test_no_compose_table_for_the_locale`).

## File Structure

| File | Task | Responsibility |
|---|---|---|
| `setup/linux_setup.py`, `tests/test_setup.py` | 1 | Linux setup writes only the Firefox host manifest |
| `src/spawn.h`, `src/proc_win32.c`, `src/proc_linux.c` | 2 | `probe_supervise` (run with a watch hook) and the child-membership query |
| `src/common.h`, `src/common.c` | 2, 4 | `probe_mono_ms`, `probe_fopen_utf8` |
| `src/pip.h`, `src/pip.c` | 2 | `pip`'s arguments, the picture-in-picture rule, the session kind, the window tracker |
| `src/pip_win32.c` | 2 | the job's windows, the close, the raise, the focus (Windows) |
| `src/pip_x11.c` | 2, 3 | the session report; X11 and XWayland windows, close, raise and focus (Linux) |
| `src/text.h`, `src/text.c` | 4 | `text`'s arguments, UTF-8 decoding, refusals, the plan-then-type loop |
| `src/text_win32.c` | 4 | wide arguments, `KEYEVENTF_UNICODE` (Windows) |
| `src/text_linux.c` | 4, 5 | layout discovery, the xkbcommon key and compose-sequence search, uinput typing (Linux) |
| `src/uinput_linux.h`, `src/inject_linux.c` | 5 | one uinput keyboard shared by `keys` and `text` |
| `src/spawn_main.c`, `CMakeLists.txt`, `ci/Dockerfile.linux`, `ci/linux-check.sh` | 2-5 | dispatch, build, the Linux check image |
| `pages/text.html`, `pages/text-g18.txt`, `pages/vendor/react-18.3.1/*`, `pages/serve.ps1` | 6 | the React probe page, its string, vendored React, serving the vendor folder |
| `tests/test_pip.py`, `tests/test_text.py`, `tests/test_pages.py`, `tests/test_make_kit.py` | 2-6 | tests |
| `README.md`, `make-kit.ps1` (comment), `.gitattributes` | 2-6 | docs and checkout rules |
| `design/research/overlay/phase0/guest-protocol.md`, `design/plans/2026-10-27-overlay-phase0.md` | 1, 7 | the protocol and the earlier plan's notes |
| `design/specs/2026-10-27-streaming-overlay-design.md` | 7 | the launch path's step 5 and the picture-in-picture fallbacks: close, then raise |

## Design decisions (my call, for review)

- **Windows raise:** `SetForegroundWindow` only succeeds for a caller that meets one of its documented conditions;
  the useful one for a background process is *the calling process received the last input event*. The watch first
  sends one empty mouse `INPUT` (no flags: it moves and presses nothing) with `SendInput`, which makes it that
  process, then calls `SetForegroundWindow` (method `input`). If that call fails it falls back to
  `AttachThreadInput` with the foreground window's thread, `BringWindowToTop` and `SetForegroundWindow`, then
  detaches (method `attach`). The `raised` line names the method, so G17 records which one each browser needed.
  Rejected: `AllowSetForegroundWindow` (only the foreground process, the browser, can grant it); a lone Alt press
  (it shows Firefox's menu bar and focuses Chrome's and Edge's app-menu button in the very window the remote then
  drives).
- **Close the picture-in-picture window, then raise** (decided together, 2026-11-03): a picture-in-picture window is
  always on top, so a raise alone leaves it covering the main window. When the watch classifies a window as
  picture-in-picture it first closes it (`WM_CLOSE` posted to it on Windows; a `_NET_CLOSE_WINDOW` client message
  on X11, source 2), which returns the video to the page in every browser, then raises the main window. The lines
  record both steps: `pip_closed` (`call_ok`, `method`, `ms`), `pip_gone` (`ms`, when the window is gone), then
  `raised` and `foreground`; `foreground.ms` stays the protocol's data. Whether the video kept playing in the page
  is G17's to record, not a unit assertion. Closing acts on the browser's window, never on a provider's page
  (file 30). `--no-close` and `--no-raise` turn each step off for the unit tests. Task 7 corrects the spec's
  **The launch path** step 5 and the **Picture-in-picture** fallbacks to match.
- **The picture-in-picture rule:** a window is picture-in-picture when its title, lowercased with hyphens read as
  spaces and outer spaces trimmed, is exactly `picture in picture` (Chrome and Edge: *Picture in picture*; Firefox:
  *Picture-in-Picture*), or, on X11, its `WM_WINDOW_ROLE` is `PictureInPicture` (Firefox). The class is reported but
  not used: the picture-in-picture window's class is its browser's main class (`Chrome_WidgetWin_1`,
  `MozillaWindowClass`, X11 `firefox`), so it cannot tell them apart. Exact match keeps a page titled "Picture in
  picture demo" out. Titles are English: a localized browser, and Chrome's Document picture-in-picture window, whose
  title is UNKNOWN until G17, show up as `pip: false` lines with their titles, which is the data that would extend
  the rule. The main window is the largest live window of the browser that is not picture-in-picture.
- **Linux layout source, in order:** `--layout <layout>[(<variant>)]` when given (source `argument`); in an X11
  session, the root window's `_XKB_RULES_NAMES` with the group from `XkbGetState`; on GNOME,
  `gsettings get org.gnome.desktop.input-sources mru-sources` (first `xkb` entry; `sources` when empty) and
  `xkb-options`; on KDE, `kxkbrc`'s `[Layout]` `LayoutList`, `VariantList`, `Options`, `Model` (first entry; the
  current one is not stored there, so it is assumed); then `localectl status` (the system default). Each attempt is a
  `layout_source` line. The chosen layout is compiled into a one-layout keymap, and each character is searched by
  pressing nothing, Shift, AltGr (`KEY_RIGHTALT`), then Shift and AltGr on an `xkb_state`, keys in code order, so the
  search reads exactly what the compositor does with those presses.
- **Dead keys and compose** (decided together, 2026-11-03): when no key gives a character directly, the search
  walks xkbcommon's compose table for the session's locale (`xkb_compose_table_new_from_locale`, the locale from
  `LC_ALL`, then `LC_CTYPE`, then `LANG`, else `C`; xkbcommon itself honors `XCOMPOSEFILE` and `~/.XCompose`) for
  an entry that produces it, and takes the shortest one whose every keysym the layout can type, a dead-key sequence
  before a `Multi_key` one of the same length (at most 4 keysyms). Each `char` line names its `path`: `direct`,
  `dead` (the sequence starts with a dead key) or `compose` (it starts with `Multi_key`). Only a character with no
  path is untyped and named. A `compose` line says which locale was used and whether a table loaded. Walking the
  table needs `xkb_compose_table_iterator_new`, which arrived in libxkbcommon 1.6.0 (Ubuntu 24.04 ships 1.6.0,
  Debian trixie 1.7.0), so 1.6 is the floor.
- **Run `text` as the desktop user on Linux**, after `setfacl` opens `/dev/uinput` to that user: under `sudo` the
  session's environment and settings are out of reach (the probe prints a `layout_note` when it runs as root).
- **libX11 and libxkbcommon are optional build dependencies** on Linux, found with CMake's `FindX11` (`X11::X11`)
  and `pkg_check_modules(... xkbcommon>=1.6)`. Without libX11, `pip` reports `"x11":"not_built"` and lists nothing;
  without libxkbcommon, `text` refuses with `text_failed` `not_built`. The Linux check image installs both, and its
  tests assert the built behavior, so a missing package fails there rather than skipping.
- **React 18.3.1:** the last release with UMD builds (React 19 ships none), so it runs from two `<script>` tags with
  no bundler. Its controlled-input path (`onChange` from the native `input` event) is the one React 19 keeps.
- **`--file`:** Windows PowerShell 5.1 drops embedded double quotes from a native command's arguments, and the G18
  string holds `"`, `'`, `` ` ``, `$` and `\`, so the protocol passes it as a UTF-8 file (`pages/text-g18.txt`).
- **Exit statuses of `text`:** 0 every character typed; 3 typed what it could and named the rest untyped; 2 refused
  before anything was typed (usage, empty, not UTF-8, more than 256 characters, a control character, an unreadable
  file); 1 a setup or send failure (`not_built`, no layout, an unknown layout, uinput, `SendInput`).

---

### Task 1: Linux setup is Firefox only

**Files:**
- Modify: `tools/overlay-phase0/setup/linux_setup.py` (whole file)
- Modify: `tools/overlay-phase0/tests/test_setup.py:44-130` (the `LinuxSetupTest` class)
- Modify: `tools/overlay-phase0/README.md:59` (the `setup/` row)
- Modify: `design/research/overlay/phase0/guest-protocol.md:59-62` (Browsers) and `:197-198` (G13)
- Modify: `design/plans/2026-10-27-overlay-phase0.md:16-19` (the amendment note)

**Interfaces:**
- Consumes: nothing new.
- Produces: `linux_setup.plan(ids, host_path, home) -> {Path: dict}` with exactly one entry, the Firefox manifest;
  `linux_setup.bad_ids(ids) -> str | None` checking `gecko` only; `linux_setup.main(argv) -> int` with options
  `--ids`, `--host`, `--home`, `--remove` (no `--root`).

- [ ] **Step 1: Write the failing tests**

Replace the whole `LinuxSetupTest` class in `tools/overlay-phase0/tests/test_setup.py` (from `class LinuxSetupTest`
through the end of `test_under_sudo_gives_the_user_back_what_it_made_in_their_home`) with:

```python
class LinuxSetupTest(unittest.TestCase):
    """Linux is Firefox only: setup writes the Firefox host manifest and nothing else."""

    def setUp(self):
        self.home = pathlib.Path(tempfile.mkdtemp())
        self.ids = write_ids(tempfile.mkdtemp())
        self.hosts = self.home / ".mozilla/native-messaging-hosts"
        self.manifest = self.hosts / "app.cptk.overlay.json"

    def run_setup(self, *extra, ids=None):
        return linux_setup.main(["--ids", str(ids or self.ids), "--host", "/usr/libexec/cptk/probe_host",
                                 "--home", str(self.home), *extra])

    def test_writes_only_the_firefox_host_manifest(self):
        self.assertEqual(self.run_setup(), 0)
        ff = json.loads(self.manifest.read_text())
        self.assertEqual(ff["allowed_extensions"], ["overlay-probe@cptk.app"])
        self.assertEqual(ff["path"], "/usr/libexec/cptk/probe_host")
        self.assertEqual(ff["type"], "stdio")
        written = sorted(p.relative_to(self.home).as_posix() for p in self.home.rglob("*") if p.is_file())
        self.assertEqual(written, [".mozilla/native-messaging-hosts/app.cptk.overlay.json"])
        self.assertEqual(list(linux_setup.plan(IDS, "/usr/libexec/cptk/probe_host", str(self.home))), [self.manifest])

    def test_refuses_the_retired_root_option(self):
        # An old command line that still names /etc gets an error, never a silent policy write.
        with mock.patch("sys.stderr", new_callable=io.StringIO), self.assertRaises(SystemExit):
            self.run_setup("--root", "/")
        self.assertEqual(os.listdir(self.home), [])

    def test_setup_is_idempotent_and_keeps_foreign_manifests(self):
        other = self.hosts / "org.example.other.json"
        self.hosts.mkdir(parents=True)
        other.write_text('{"name": "org.example.other"}')
        self.assertEqual(self.run_setup(), 0)
        first = self.manifest.read_text()
        self.assertEqual(self.run_setup(), 0)
        self.assertEqual(self.manifest.read_text(), first)
        self.assertEqual(other.read_text(), '{"name": "org.example.other"}')

    def test_remove_deletes_only_its_file(self):
        other = self.hosts / "org.example.other.json"
        self.hosts.mkdir(parents=True)
        other.write_text('{"name": "org.example.other"}')
        self.run_setup()
        self.run_setup("--remove")
        self.assertFalse(self.manifest.exists())
        self.assertTrue(other.exists())

    def test_lists_the_folders_it_will_create(self):
        self.assertEqual(linux_setup.missing_dirs(self.hosts), [self.home / ".mozilla", self.hosts])
        self.hosts.mkdir(parents=True)
        self.assertEqual(linux_setup.missing_dirs(self.hosts), [])

    def test_refuses_a_relative_host_path(self):
        with self.assertRaises(SystemExit):
            linux_setup.main(["--ids", str(self.ids), "--host", "probe_host", "--home", str(self.home)])

    def test_refuses_a_bad_gecko_id(self):
        for bad in ({k: v for k, v in IDS.items() if k != "gecko"}, {**IDS, "gecko": "overlay@cptk.app"}):
            with mock.patch("sys.stderr", new_callable=io.StringIO) as err:
                self.assertEqual(self.run_setup(ids=write_ids(tempfile.mkdtemp(), bad)), 1, bad)
            self.assertIn("ids file", err.getvalue())
        self.assertEqual(os.listdir(self.home), [])

    def test_ignores_the_windows_only_store_ids(self):
        # ids.json is shared with Windows; its Chrome and Edge ids play no part on Linux, malformed or missing.
        for ids in ({"gecko": "overlay-probe@cptk.app"}, {**IDS, "chrome_web_store": "A" * 32}):
            with self.subTest(ids=ids):
                self.assertEqual(self.run_setup(ids=write_ids(tempfile.mkdtemp(), ids)), 0)

    def test_under_sudo_gives_the_user_back_what_it_made_in_their_home(self):
        def getpwnam(name):
            self.assertEqual(name, "alice")
            return types.SimpleNamespace(pw_dir=str(self.home), pw_uid=1001, pw_gid=1002)

        fake_pwd = types.ModuleType("pwd")
        fake_pwd.getpwnam = getpwnam
        calls = []
        argv = ["--ids", str(self.ids), "--host", "/usr/libexec/cptk/probe_host"]
        with mock.patch.dict(sys.modules, {"pwd": fake_pwd}), mock.patch.dict(os.environ, {"SUDO_USER": "alice"}), \
                mock.patch.object(os, "chown", lambda p, u, g: calls.append((pathlib.Path(p), u, g)), create=True):
            self.assertEqual(linux_setup.main(argv), 0)
            self.assertEqual(calls, [(self.home / ".mozilla", 1001, 1002), (self.hosts, 1001, 1002),
                                     (self.manifest, 1001, 1002)])
            calls.clear()
            self.assertEqual(linux_setup.main(argv), 0)
            self.assertEqual(calls, [(self.manifest, 1001, 1002)])
        self.assertTrue(self.manifest.exists())
```

- [ ] **Step 2: Run the one safe test to see it fail**

Run only `test_ignores_the_windows_only_store_ids` against the old script: it is refused before any write. Do NOT
run the others yet: the old script's default `--root /` would write Chrome and Edge policy files into the real
filesystem root (`C:\etc\opt\...` on Windows).

Run: `python -m unittest discover -s C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec/tools/overlay-phase0/tests -p test_setup.py -k test_ignores_the_windows_only_store_ids -v`
Expected: FAIL: `1 != 0` (the old `bad_ids` refuses an ids file without valid Chrome and Edge ids).

- [ ] **Step 3: Rewrite `linux_setup.py`**

`tools/overlay-phase0/setup/linux_setup.py`:

```python
#!/usr/bin/env python3
"""Phase 0 setup on Linux: the Firefox native-messaging host manifest. Linux is Firefox only, so no browser policy is
written. Run it as the desktop user; under sudo it writes into the invoking user's home and gives them the files."""
import argparse
import json
import os
import pathlib
import posixpath
import sys

HOST = "app.cptk.overlay"
GECKO = "overlay-probe@cptk.app"


def bad_ids(ids):
    """What is wrong with the ids file for Linux, or None. Only the Gecko id is used here: the Chrome Web Store and
    Edge Add-ons ids in the same file are Windows-only."""
    # The profile scripts name the sideloaded XPI after this id, and Firefox loads it only under that name.
    if ids.get("gecko") != GECKO:
        return f"gecko must be {GECKO}, got {ids.get('gecko')!r}"
    return None


def plan(ids, host_path, home):
    manifest = {"name": HOST, "description": "CPTK Overlay Probe host", "path": host_path, "type": "stdio",
                "allowed_extensions": [ids["gecko"]]}
    return {pathlib.Path(home) / f".mozilla/native-messaging-hosts/{HOST}.json": manifest}


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
    ap.add_argument("--home", default=None)
    ap.add_argument("--remove", action="store_true")
    args = ap.parse_args(argv)
    if not posixpath.isabs(args.host):  # a Linux path, even when the tests run on Windows
        ap.error("--host must be an absolute path")
    ids = json.loads(pathlib.Path(args.ids).read_text())
    problem = bad_ids(ids)
    if problem:
        print(f"setup: ids file {args.ids}: {problem}", file=sys.stderr)
        return 1
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
    for path, content in plan(ids, args.host, home).items():
        if args.remove:
            if path.exists():
                path.unlink()
            print(f"setup: removed {path}")
            continue
        made = missing_dirs(path.parent)
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(json.dumps(content, indent=2) + "\n")
        os.chmod(path, 0o644)
        if owner:
            # Under sudo every folder mkdir made is root's; give the user back each one it made in their home.
            for folder in made:
                os.chown(folder, *owner)
            os.chown(path, *owner)
        print(f"setup: wrote {path}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
```

- [ ] **Step 4: Run the tests to see them pass**

Run: `python -m unittest discover -s C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec/tools/overlay-phase0/tests -p test_setup.py -v`
Expected: OK; every `LinuxSetupTest` test passes, and `FirefoxProfileTest`, `FirefoxProfilePsTest` and
`WindowsSetupTest` are unchanged and pass.

- [ ] **Step 5: Update the docs that named Linux Chrome and Edge**

In `tools/overlay-phase0/README.md`, replace the `setup/` row:

```
| `setup/` | `windows.ps1`, `linux_setup.py`: policy and host registration. `firefox-profile.ps1`, `firefox_profile.py`: a Firefox profile with the sideloaded XPI. |
```

with:

```
| `setup/` | `windows.ps1`: policy and host registration. `linux_setup.py`: the Firefox host registration (Linux is Firefox only; no policy). `firefox-profile.ps1`, `firefox_profile.py`: a Firefox profile with the sideloaded XPI. |
```

In `design/research/overlay/phase0/guest-protocol.md`, replace:

```
  `C:\Program Files\Google\Chrome\Application\chrome.exe`, Firefox `C:\Program Files\Mozilla Firefox\firefox.exe`;
  Linux `/opt/microsoft/msedge/msedge`, `/opt/google/chrome/chrome`, and the full Firefox's own path. Record each
  browser's version with every result.
```

with:

```
  `C:\Program Files\Google\Chrome\Application\chrome.exe`, Firefox `C:\Program Files\Mozilla Firefox\firefox.exe`;
  Linux, the full Firefox's own path (Linux runs Firefox only). Record each browser's version with every result.
```

and replace:

```
in G9: on Windows `windows.ps1` as in G1 step 1, on Linux `sudo python3 setup/linux_setup.py --ids ids.json --host
<abs probe_host>`. Without it nothing reaches `probe.log`.
```

with:

```
in G9: on Windows `windows.ps1` as in G1 step 1, on Linux `python3 setup/linux_setup.py --ids ids.json --host
<abs probe_host>` as the desktop user (it writes only the Firefox host manifest, in that user's home). Without it
nothing reaches `probe.log`.
```

In `design/plans/2026-10-27-overlay-phase0.md`, replace:

```
Linux run uses Firefox only. The kit built below still writes the Linux Chrome and Edge policy files in
`linux_setup.py` (Task 6); a guest run on Linux needs only its host registration, and the kit is not changed for it.
```

with:

```
Linux run uses Firefox only. Task 6 below built `linux_setup.py` with Linux Chrome and Edge policy files;
`design/plans/2026-11-03-overlay-phase0-kit-g17-g18.md` (Task 1) cut it to the Firefox host manifest alone.
```

Check nothing else in the kit names Linux Chrome or Edge:

Run: `rg -n "etc/opt|/opt/google|msedge|policies/managed" C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec/tools/overlay-phase0 --glob "!**/dist/**"`
Expected: no line from `setup/linux_setup.py`, `tests/test_setup.py`, `ci/`, `README.md` or `make-kit.ps1`
(Windows paths in `windows.ps1` and `extension/` are expected and stay).

- [ ] **Step 6: Run the Linux container check**

In its own call:
`python C:/Users/jscha/.claude/hooks/box_queue.py request --session <session id> --work "overlay kit: Linux check image build (Task 1)" --overlap ok --touches workdir:c:/users/jscha/claudescratch/wt-streamflex-overlay-spec --big`

Then, in the next call:
`docker build -t cptk-overlay-p0-linux:local -f C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec/tools/overlay-phase0/ci/Dockerfile.linux C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec/tools/overlay-phase0/ci`

In its own call:
`python C:/Users/jscha/.claude/hooks/box_queue.py request --session <session id> --work "overlay kit: Linux check run (Task 1)" --overlap ok --touches workdir:c:/users/jscha/claudescratch/wt-streamflex-overlay-spec --big`

Then, in the next call:
`docker run --rm -v "C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec:/src:ro" cptk-overlay-p0-linux:local bash /src/tools/overlay-phase0/ci/linux-check.sh`

Expected: the last line reads `RESULT: build exit 0 (0 error(s), ...), tests exit 0`, and the summary is `OK`.

- [ ] **Step 7: Commit**

```powershell
git -C "C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec" add tools/overlay-phase0/setup/linux_setup.py tools/overlay-phase0/tests/test_setup.py tools/overlay-phase0/README.md design/research/overlay/phase0/guest-protocol.md design/plans/2026-10-27-overlay-phase0.md
git -C "C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec" commit -m "tools: the Phase 0 kit's Linux setup writes only the Firefox host manifest"
```

---

### Task 2: `probe_spawn pip` on Windows (and the shared watch)

**Files:**
- Modify: `tools/overlay-phase0/src/spawn.h` (the watch, `probe_supervise`, the membership queries, `probe_pip`)
- Modify: `tools/overlay-phase0/src/common.h`, `tools/overlay-phase0/src/common.c` (`probe_mono_ms`)
- Modify: `tools/overlay-phase0/src/proc_win32.c` (`probe_run` becomes `probe_supervise`; `probe_child_pids`)
- Modify: `tools/overlay-phase0/src/proc_linux.c` (the same; `probe_child_has_pid`; `mono_ms` replaced)
- Create: `tools/overlay-phase0/src/pip.h`, `tools/overlay-phase0/src/pip.c`, `tools/overlay-phase0/src/pip_win32.c`
- Create: `tools/overlay-phase0/src/pip_x11.c` (the session report only; Task 3 adds X11)
- Modify: `tools/overlay-phase0/src/spawn_main.c`, `tools/overlay-phase0/CMakeLists.txt:23-30`
- Modify: `tools/overlay-phase0/README.md:56` (the `probe_spawn` row)
- Create: `tools/overlay-phase0/tests/test_pip.py`

**Interfaces:**
- Consumes: `probe_emit`, `probe_json_string` (common.h); `probe_lines_feed` and `run`'s loop (unchanged).
- Produces (spawn.h):
  - `typedef struct { void (*tick)(void *ctx); void (*ended)(void *ctx); void *ctx; } probe_watch;`
  - `int probe_supervise(int argc, char **argv, const char *usage, const probe_watch *watch);` `run` is
    `probe_supervise(argc, argv, "usage: probe_spawn run <program> [args...]\n", NULL)`.
  - Windows: `int probe_child_pids(unsigned long *out, int cap);` Linux: `int probe_child_has_pid(long pid);`
  - `int probe_pip(int argc, char **argv);` (`pip [--no-close] [--no-raise] <program> [args...]`, the two flags in
    either order, or `pip --classify <title> [<role>]`)
- Produces (common.h): `int64_t probe_mono_ms(void);`
- Produces (pip.h): `probe_win` (fields `id`, `pid`, `title[512]`, `cls[128]`, `instance[128]`, `role[64]`,
  `topmost`, `area`); `const char *probe_pip_rule(const char *title, const char *role);` (returns `"title"`,
  `"role"` or NULL); `const char *probe_session_kind(const char *wayland_display, const char *session_type,
  const char *display);` (returns `"wayland"`, `"x11"` or `"none"`); `probe_pip_platform` with `session`,
  `snapshot`, `close`, `raise`, `foreground`, in that order; `extern const probe_pip_platform
  probe_pip_platform_impl;`
- Output lines: `pip_session` (`session`, plus on Linux `display`, `x11`, `note`; `can_list`, `can_raise`),
  `window` / `window_changed` / `window_gone` (`id`, `pid`, `title`, `class`, `instance`, `role`, `topmost`, `area`,
  `pip`, `rule`), `pip_seen` (`id`, `main`, `rule`, `ms_since_seen`), then for that window `pip_closed` (`id`,
  `call_ok`, `method`, `ms`) or `close_skipped` (`id`, `why`: `no_close`), then `raised` (`id`, `pip_id`, `call_ok`,
  `method`, `ms`) or `raise_skipped` (`id`, `why`: `no_raise` or `no_main_window`); later `pip_gone` (`id`,
  `closed`, `ms`) once a picture-in-picture window is gone, and `foreground` (`id`, `pip_id`, `ms`) once the main
  window has the focus; `classify` (`pip`, `rule`), `pip_window_limit`, `pip_summary` (`listed`, `windows`, `pip`,
  `closed`, `raised`, `foreground`). Every `ms` counts from the picture-in-picture window's first appearance. `id`s
  are strings `"0x<hex>"`. `run`'s own lines come between `pip_session` and `pip_summary` unchanged.

- [ ] **Step 1: Write the failing tests**

`tools/overlay-phase0/tests/test_pip.py`:

```python
"""Tests for probe_spawn pip. PROBE_SPAWN names the built executable."""
import os
import subprocess
import sys
import tempfile
import threading
import time
import unittest

from test_spawn import EXIT_EVENTS, SPAWN, close_pipes, events

LINUX_SESSION_VARS = ("DISPLAY", "WAYLAND_DISPLAY", "XDG_SESSION_TYPE")


def run_spawn(test, args, env=None):
    """Runs probe_spawn with args and stdin closed; one that does not end inside the hang guard fails as HUNG."""
    try:
        return subprocess.run([SPAWN, *args], input="", capture_output=True, encoding="utf-8", timeout=120, env=env)
    except subprocess.TimeoutExpired:
        test.fail("HUNG: probe_spawn %s did not end" % " ".join(args[:2]))


def bare_env(**extra):
    """This environment without a display or a session type, plus extra."""
    env = {k: v for k, v in os.environ.items() if k not in LINUX_SESSION_VARS}
    env.update(extra)
    return env


class Lines:
    """Collects a running process's stdout lines on a thread, so a test can wait for one event at a time."""

    def __init__(self, proc):
        self.proc, self.lines = proc, []
        self.thread = threading.Thread(target=self.read, daemon=True)
        self.thread.start()

    def read(self):
        for line in self.proc.stdout:
            self.lines.append(line)

    def events(self):
        return events("".join(list(self.lines)))

    def wait_for(self, test, what, **fields):
        """Returns the first event whose fields equal fields; fails as HUNG, naming what, after the hang guard."""
        end = time.monotonic() + 120
        while True:
            for e in self.events():
                if all(e.get(k) == v for k, v in fields.items()):
                    return e
            if not self.thread.is_alive():
                test.fail("probe_spawn ended before %s: %s" % (what, "".join(self.lines)))
            if time.monotonic() > end:
                test.fail("HUNG: no %s" % what)
            time.sleep(0.05)


# A tkinter stand-in for a browser: a main window, then on cue a second window, its new title, and its close.
TK_CHILD = r'''
import os, sys, tkinter
tmp = sys.argv[1]
root = tkinter.Tk()
root.title("cptk main")
root.geometry("400x300+40+40")
phase = {"n": 0, "top": None}
def poll():
    n = phase["n"]
    if n == 0 and os.path.exists(os.path.join(tmp, "open")):
        phase["top"] = tkinter.Toplevel(root)
        phase["top"].title("cptk video")
        phase["top"].geometry("160x90+80+80")
        phase["n"] = 1
    elif n == 1 and os.path.exists(os.path.join(tmp, "retitle")):
        phase["top"].title("Picture-in-Picture")
        phase["n"] = 2
    elif n == 2 and os.path.exists(os.path.join(tmp, "shut")):
        phase["top"].destroy()
        phase["n"] = 3
    root.after(50, poll)
root.after(50, poll)
root.mainloop()
'''


@unittest.skipUnless(SPAWN, "set PROBE_SPAWN to the built probe_spawn")
class PipTest(unittest.TestCase):
    def test_classify_names_the_rule(self):
        cases = [
            (["Picture in picture"], True, "title"),  # Chrome and Edge, English
            (["Picture-in-Picture"], True, "title"),  # Firefox, English
            (["  PICTURE-IN-PICTURE  "], True, "title"),
            (["", "PictureInPicture"], True, "role"),  # Firefox on X11
            (["Shaka Player Demo - Google Chrome"], False, None),
            (["Picture in picture demo - Mozilla Firefox"], False, None),
            ([""], False, None),
        ]
        for args, pip, rule in cases:
            with self.subTest(args=args):
                proc = run_spawn(self, ["pip", "--classify", *args])
                self.assertEqual(proc.returncode, 0, proc.stderr)
                (ev,) = events(proc.stdout)
                self.assertEqual((ev["event"], ev["pip"], ev["rule"]), ("classify", pip, rule))

    def test_pip_needs_a_program(self):
        for args in (["pip"], ["pip", "--no-raise"], ["pip", "--no-close"], ["pip", "--no-close", "--no-raise"],
                     ["pip", "--classify"], ["pip", "--classify", "a", "b", "c"]):
            with self.subTest(args=args):
                proc = run_spawn(self, args)
                self.assertEqual(proc.returncode, 2)
                self.assertEqual(events(proc.stdout), [])
                self.assertIn("usage:", proc.stderr)

    def test_pip_supervises_like_run(self):
        proc = run_spawn(self, ["pip", sys.executable, "-c", "import sys; sys.exit(3)"], env=bare_env())
        self.assertEqual(proc.returncode, 0, proc.stderr)
        evs = [e for e in events(proc.stdout) if not e["event"].startswith("window")]
        self.assertEqual([e["event"] for e in evs], ["pip_session", "started"] + EXIT_EVENTS + ["pip_summary"])
        self.assertEqual(evs[-2]["code"], 3)
        self.assertEqual(evs[0]["session"], "windows" if os.name == "nt" else "none")
        self.assertEqual(evs[0]["can_list"], os.name == "nt")
        self.assertEqual(evs[-1]["listed"], os.name == "nt")
        self.assertEqual((evs[-1]["pip"], evs[-1]["closed"], evs[-1]["raised"], evs[-1]["foreground"]), (0, 0, 0, 0))

    def watch_tk(self, tmp, flags, steps):
        """Runs pip <flags> on TK_CHILD: waits for the main window, opens the second one, retitles it
        Picture-in-Picture and checks the watch re-classifies it, then calls steps(lines, main, video). Closes the
        child through run's close and returns every event."""
        pythonw = os.path.join(os.path.dirname(sys.executable), "pythonw.exe")
        proc = subprocess.Popen([SPAWN, "pip", *flags, pythonw, "-c", TK_CHILD, tmp],
                                stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                encoding="utf-8")
        try:
            lines = Lines(proc)
            main = lines.wait_for(self, "the main window's line", event="window", title="cptk main")
            self.assertFalse(main["pip"])
            open(os.path.join(tmp, "open"), "w").close()
            video = lines.wait_for(self, "the second window's line", event="window", title="cptk video")
            self.assertFalse(video["pip"])
            open(os.path.join(tmp, "retitle"), "w").close()
            changed = lines.wait_for(self, "the retitled window's line", event="window_changed", id=video["id"])
            self.assertEqual((changed["title"], changed["pip"], changed["rule"]), ("Picture-in-Picture", True, "title"))
            seen = lines.wait_for(self, "the pip_seen line", event="pip_seen", id=video["id"])
            self.assertEqual(seen["main"], main["id"])
            self.assertGreaterEqual(seen["ms_since_seen"], 0)
            steps(lines, main, video)
            proc.stdin.write("close\n")
            proc.stdin.flush()
            lines.thread.join(120)
            if lines.thread.is_alive():
                self.fail("HUNG: probe_spawn pip did not end after close")
            proc.wait(timeout=120)
        finally:
            if proc.poll() is None:
                proc.kill()  # the job closes with it and ends the child
                proc.wait(timeout=120)
            close_pipes(proc)
        evs = lines.events()
        self.assertEqual([e["event"] for e in evs][-2:], ["exited", "pip_summary"])
        self.assertEqual([e for e in evs if e["event"] == "raised"], [])
        return evs

    @unittest.skipUnless(os.name == "nt", "the Windows watch lists the job's windows")
    def test_pip_closes_a_retitled_window_and_names_the_main_one(self):
        def steps(lines, main, video):
            closed = lines.wait_for(self, "the pip_closed line", event="pip_closed", id=video["id"])
            self.assertEqual((closed["call_ok"], closed["method"]), (True, "WM_CLOSE"))
            self.assertGreaterEqual(closed["ms"], 0)
            skipped = lines.wait_for(self, "the raise_skipped line", event="raise_skipped", id=video["id"])
            self.assertEqual(skipped["why"], "no_raise")
            # tkinter destroys a toplevel on WM_CLOSE, as a browser closes its picture-in-picture window.
            lines.wait_for(self, "the closed window's window_gone line", event="window_gone", id=video["id"])
            gone = lines.wait_for(self, "the pip_gone line", event="pip_gone", id=video["id"])
            self.assertTrue(gone["closed"])
            self.assertGreaterEqual(gone["ms"], closed["ms"])

        with tempfile.TemporaryDirectory() as tmp:
            # --no-raise: the test never takes the focus; the close reaches only the test's own tkinter window.
            evs = self.watch_tk(tmp, ["--no-raise"], steps)
        summary = evs[-1]
        self.assertEqual((summary["listed"], summary["pip"], summary["closed"], summary["raised"]), (True, 1, 1, 0))
        self.assertGreaterEqual(summary["windows"], 2)

    @unittest.skipUnless(os.name == "nt", "the Windows watch lists the job's windows")
    def test_no_close_leaves_the_window_open(self):
        def steps(lines, main, video):
            skipped = lines.wait_for(self, "the close_skipped line", event="close_skipped", id=video["id"])
            self.assertEqual(skipped["why"], "no_close")
            lines.wait_for(self, "the raise_skipped line", event="raise_skipped", id=video["id"])
            open(os.path.join(tmp, "shut"), "w").close()
            gone = lines.wait_for(self, "the pip_gone line", event="pip_gone", id=video["id"])
            self.assertFalse(gone["closed"])

        with tempfile.TemporaryDirectory() as tmp:
            evs = self.watch_tk(tmp, ["--no-close", "--no-raise"], steps)
        self.assertEqual([e for e in evs if e["event"] == "pip_closed"], [])
        self.assertEqual((evs[-1]["pip"], evs[-1]["closed"]), (1, 0))


if __name__ == "__main__":
    unittest.main()
```

- [ ] **Step 2: Build and run the tests to see them fail**

Run: `cmake --build C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec/build-phase0 --config Release`
Then: `$env:PROBE_SPAWN = "C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec/build-phase0/Release/probe_spawn.exe"; python -m unittest discover -s C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec/tools/overlay-phase0/tests -p test_pip.py -v`
Expected: FAIL: every test sees `usage: probe_spawn run|keys|keylog|pad|focus ...` and exit 2.

- [ ] **Step 3: Add the watch and the monotonic clock**

In `tools/overlay-phase0/src/spawn.h`, replace:

```c
int probe_run(int argc, char **argv);    /* run <program> [args...]; stdin lines: close | kill */
```

with:

```c
/* What a supervised run calls while its child runs: tick on every pass of run's loop (about every 50 ms, sooner when
   the child's processes change), ended once, after the exited line. Either may be NULL. */
typedef struct {
    void (*tick)(void *ctx);
    void (*ended)(void *ctx);
    void *ctx;
} probe_watch;

/* run's whole job with an optional watch: usage (printed when argc is 0), spawn, supervision with close and kill on
   stdin, and the exit lines. Returns run's exit code. */
int probe_supervise(int argc, char **argv, const char *usage, const probe_watch *watch);

/* The supervised child's processes, once probe_supervise has started it. Windows: up to cap pids of the job's
   processes; returns the count. Linux: whether pid is the child or one of its descendants (orphans reparent to this
   subreaper, so a descendant's PPid chain always reaches this process). */
#ifdef _WIN32
int probe_child_pids(unsigned long *out, int cap);
#else
int probe_child_has_pid(long pid);
#endif

int probe_run(int argc, char **argv);    /* run <program> [args...]; stdin lines: close | kill */
int probe_pip(int argc, char **argv);    /* pip [--no-raise] <program> [args...] | pip --classify <title> [<role>] */
```

In `tools/overlay-phase0/src/common.h`, after the `probe_now_ms` declaration add:

```c
/* Milliseconds on a monotonic clock, for intervals; the event lines' t stays wall time. */
int64_t probe_mono_ms(void);
```

In `tools/overlay-phase0/src/common.c`, after the Windows `probe_now_ms` (before `#else`) add:

```c
int64_t probe_mono_ms(void)
{
    return (int64_t)GetTickCount64();
}
```

and after the Linux `probe_now_ms` (before `#endif`) add:

```c
int64_t probe_mono_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}
```

- [ ] **Step 4: Turn Windows `run` into `probe_supervise`**

In `tools/overlay-phase0/src/proc_win32.c`, after `job_is_empty` add:

```c
int probe_child_pids(unsigned long *out, int cap)
{
    /* The list struct holds one pid; the array after it gives room for 255 more. */
    struct {
        JOBOBJECT_BASIC_PROCESS_ID_LIST list;
        ULONG_PTR more[255];
    } buf;
    if (!g_job)
        return 0;
    memset(&buf, 0, sizeof buf);
    if (!QueryInformationJobObject(g_job, JobObjectBasicProcessIdList, &buf, sizeof buf, NULL)
        && GetLastError() != ERROR_MORE_DATA)
        return 0;
    int n = (int)buf.list.NumberOfProcessIdsInList;
    if (n > cap)
        n = cap;
    for (int i = 0; i < n; i++)
        out[i] = (unsigned long)buf.list.ProcessIdList[i];
    return n;
}
```

Replace:

```c
int probe_run(int argc, char **argv)
{
    if (argc < 1) {
        fputs("usage: probe_spawn run <program> [args...]\n", stderr);
        return 2;
    }
```

with:

```c
int probe_run(int argc, char **argv)
{
    return probe_supervise(argc, argv, "usage: probe_spawn run <program> [args...]\n", NULL);
}

int probe_supervise(int argc, char **argv, const char *usage, const probe_watch *watch)
{
    if (argc < 1) {
        fputs(usage, stderr);
        return 2;
    }
```

Replace:

```c
        if (job_is_empty())
            break;
```

with:

```c
        if (job_is_empty())
            break;
        if (watch && watch->tick)
            watch->tick(watch->ctx);
```

Replace:

```c
    snprintf(fields, sizeof fields, "\"event\":\"exited\",\"code\":%lu,\"ms_since_close\":%lld", exit_code, since);
    probe_emit(fields);
    return 0;
}
```

with:

```c
    snprintf(fields, sizeof fields, "\"event\":\"exited\",\"code\":%lu,\"ms_since_close\":%lld", exit_code, since);
    probe_emit(fields);
    if (watch && watch->ended)
        watch->ended(watch->ctx);
    return 0;
}
```

- [ ] **Step 5: Turn Linux `run` into `probe_supervise`**

In `tools/overlay-phase0/src/proc_linux.c`, delete the static `mono_ms` function and its comment:

```c
/* Milliseconds on the monotonic clock, for intervals; the event lines' t stays wall time. */
static int64_t mono_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}
```

and replace its two uses: `return s->closed ? (long long)(mono_ms() - s->t_close) : -1;` becomes
`return s->closed ? (long long)(probe_mono_ms() - s->t_close) : -1;`, and `s->t_close = mono_ms();` becomes
`s->t_close = probe_mono_ms();`. The `struct run_state` comment `/* mono_ms of the last close */` becomes
`/* probe_mono_ms of the last close */`.

Replace the whole `kill_children` function with:

```c
static int g_supervising; /* probe_supervise has started a child */

/* The PPid in /proc/<pid>/status, or -1. */
static long parent_of(long pid)
{
    char path[64], line[128];
    snprintf(path, sizeof path, "/proc/%ld/status", pid);
    FILE *f = fopen(path, "r");
    if (!f)
        return -1;
    long ppid = -1;
    while (fgets(line, sizeof line, f))
        if (sscanf(line, "PPid: %ld", &ppid) == 1)
            break;
    fclose(f);
    return ppid;
}

int probe_child_has_pid(long pid)
{
    long self = (long)getpid();
    if (!g_supervising || pid == self)
        return 0;
    for (int depth = 0; pid > 1 && depth < 64; depth++) {
        pid = parent_of(pid);
        if (pid == self)
            return 1;
    }
    return 0;
}

/* SIGKILLs every direct child: the leader, and the descendants this subreaper inherited, which may have left the
   group. Run on every pass after a kill, so a descendant orphaned by the last pass is caught by the next. */
static void kill_children(void)
{
    DIR *d = opendir("/proc");
    if (!d)
        return;
    long self = (long)getpid();
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        char *end;
        long p = strtol(e->d_name, &end, 10);
        if (*end || p <= 0)
            continue;
        if (parent_of(p) == self)
            kill((pid_t)p, SIGKILL);
    }
    closedir(d);
}
```

Replace:

```c
int probe_run(int argc, char **argv)
{
    if (argc < 1) {
        fputs("usage: probe_spawn run <program> [args...]\n", stderr);
        return 2;
    }
```

with:

```c
int probe_run(int argc, char **argv)
{
    return probe_supervise(argc, argv, "usage: probe_spawn run <program> [args...]\n", NULL);
}

int probe_supervise(int argc, char **argv, const char *usage, const probe_watch *watch)
{
    if (argc < 1) {
        fputs(usage, stderr);
        return 2;
    }
```

Replace:

```c
    snprintf(fields, sizeof fields, "\"event\":\"started\",\"pid\":%d", (int)pid);
    probe_emit(fields);
```

with:

```c
    snprintf(fields, sizeof fields, "\"event\":\"started\",\"pid\":%d", (int)pid);
    probe_emit(fields);
    g_supervising = 1;
```

Replace:

```c
        if (no_child)
            break;
```

with:

```c
        if (no_child)
            break;
        if (watch && watch->tick)
            watch->tick(watch->ctx);
```

Replace:

```c
    snprintf(fields, sizeof fields, "\"event\":\"exited\",\"code\":%d,\"ms_since_close\":%lld",
             exit_code(leader_reaped, status), since_close(&s));
    probe_emit(fields);
    return 0;
}
```

with:

```c
    snprintf(fields, sizeof fields, "\"event\":\"exited\",\"code\":%d,\"ms_since_close\":%lld",
             exit_code(leader_reaped, status), since_close(&s));
    probe_emit(fields);
    if (watch && watch->ended)
        watch->ended(watch->ctx);
    return 0;
}
```

- [ ] **Step 6: Write the shared watch**

`tools/overlay-phase0/src/pip.h`:

```c
/* probe_spawn pip: run's supervision plus a watch on the browser's top-level windows. */
#ifndef PROBE_PIP_H
#define PROBE_PIP_H

#include <stddef.h>
#include <stdint.h>

#define PROBE_PIP_MAX_WINDOWS 256

/* One visible top-level window of the browser, as a platform lists it. Strings are UTF-8. */
typedef struct {
    uint64_t id;        /* the HWND, or the X11 window id */
    long pid;           /* the process that owns it */
    char title[512];
    char cls[128];      /* Windows: the window class; X11: WM_CLASS's class part */
    char instance[128]; /* X11: WM_CLASS's instance part; Windows: "" */
    char role[64];      /* X11: WM_WINDOW_ROLE; Windows: "" */
    int topmost;        /* WS_EX_TOPMOST, or _NET_WM_STATE_ABOVE */
    long long area;     /* width times height, in pixels */
} probe_win;

/* "title" when title, lowercased with hyphens read as spaces and outer spaces trimmed, is exactly
   "picture in picture"; "role" when role is "PictureInPicture"; else NULL. */
const char *probe_pip_rule(const char *title, const char *role);

/* "wayland" when wayland_display is set or session_type is "wayland"; else "x11" when display is set; else "none". */
const char *probe_session_kind(const char *wayland_display, const char *session_type, const char *display);

typedef struct {
    /* Prints the pip_session line; returns 1 when it can list windows, else 0 (the watch then only supervises). */
    int (*session)(void);
    /* Fills out with the browser's visible top-level windows; returns how many. */
    int (*snapshot)(probe_win *out, int cap);
    /* Asks the browser to close window id, as its own close button would; writes the method into method; returns 1
       when the request was sent. The window going away shows in a later snapshot. */
    int (*close)(uint64_t id, char *method, size_t cap);
    /* Brings window id to the front; writes the method used into method; returns 1 when the call reported success. */
    int (*raise)(uint64_t id, char *method, size_t cap);
    /* The window that has the keyboard focus now, or 0. */
    uint64_t (*foreground)(void);
} probe_pip_platform;

extern const probe_pip_platform probe_pip_platform_impl;

int probe_pip_main(int argc, char **argv, const probe_pip_platform *p);

#endif
```

`tools/overlay-phase0/src/pip.c`:

```c
/* probe_spawn pip: run's supervision with a watch on the browser's top-level windows. Each new window is reported
   with its pid, title and class. One that looks like picture-in-picture is closed (it is always on top, so a raise
   alone would leave it covering the main window, and closing it returns the video to the page), then the browser's
   main window is raised; every step is timed from the window's first appearance. Both act on the browser's own
   windows, never on a page. The platform files list, close, raise and read the focus; this file decides. */
#include "pip.h"

#include <stdio.h>
#include <string.h>

#include "common.h"
#include "spawn.h"

#define PIP_USAGE \
    "usage: probe_spawn pip [--no-close] [--no-raise] <program> [args...] | pip --classify <title> [<role>]\n"

const char *probe_pip_rule(const char *title, const char *role)
{
    if (role && strcmp(role, "PictureInPicture") == 0)
        return "role";
    char norm[64];
    size_t n = 0;
    const char *p = title ? title : "";
    while (*p == ' ')
        p++;
    for (; *p && n + 1 < sizeof norm; p++) {
        char c = *p;
        if (c >= 'A' && c <= 'Z')
            c = (char)(c - 'A' + 'a');
        else if (c == '-')
            c = ' ';
        norm[n++] = c;
    }
    if (*p)
        return NULL; /* longer than any title the rule names */
    while (n && norm[n - 1] == ' ')
        n--;
    norm[n] = '\0';
    return strcmp(norm, "picture in picture") == 0 ? "title" : NULL;
}

const char *probe_session_kind(const char *wayland_display, const char *session_type, const char *display)
{
    if ((wayland_display && *wayland_display) || (session_type && strcmp(session_type, "wayland") == 0))
        return "wayland";
    if (display && *display)
        return "x11";
    return "none";
}

typedef struct {
    probe_win w;
    int64_t seen_ms;  /* probe_mono_ms when first listed */
    const char *rule; /* NULL: not picture-in-picture */
    int live;         /* listed in the latest pass */
    int closed;       /* the watch asked the browser to close it */
} tracked;

static struct {
    const probe_pip_platform *p;
    int can_list, no_close, no_raise, limit_reported;
    tracked win[PROBE_PIP_MAX_WINDOWS];
    int n;
    uint64_t want_fg, want_pip; /* the last raise: the main window that should get the focus, and its cause */
    int64_t want_since;         /* the cause's seen_ms */
    int windows, pips, closes, raises, focused;
} g;

static probe_win g_snap[PROBE_PIP_MAX_WINDOWS];

static void quoted_id(char *out, size_t cap, uint64_t id)
{
    snprintf(out, cap, "\"0x%llx\"", (unsigned long long)id);
}

static void emit_window(const char *event, const tracked *t)
{
    char title[1100], cls[300], inst[300], role[160], id[32], rule[16], fields[2200];
    probe_json_string(title, sizeof title, t->w.title);
    probe_json_string(cls, sizeof cls, t->w.cls);
    probe_json_string(inst, sizeof inst, t->w.instance);
    probe_json_string(role, sizeof role, t->w.role);
    quoted_id(id, sizeof id, t->w.id);
    if (t->rule)
        snprintf(rule, sizeof rule, "\"%s\"", t->rule);
    else
        snprintf(rule, sizeof rule, "null");
    snprintf(fields, sizeof fields,
             "\"event\":\"%s\",\"id\":%s,\"pid\":%ld,\"title\":%s,\"class\":%s,\"instance\":%s,\"role\":%s,"
             "\"topmost\":%s,\"area\":%lld,\"pip\":%s,\"rule\":%s",
             event, id, t->w.pid, title, cls, inst, role, t->w.topmost ? "true" : "false", t->w.area,
             t->rule ? "true" : "false", rule);
    probe_emit(fields);
}

static tracked *find(uint64_t id)
{
    for (int i = 0; i < g.n; i++)
        if (g.win[i].w.id == id)
            return &g.win[i];
    return NULL;
}

/* The browser's main window: the largest live window that is not picture-in-picture. */
static tracked *main_window(void)
{
    tracked *best = NULL;
    for (int i = 0; i < g.n; i++) {
        tracked *t = &g.win[i];
        if (t->live && !t->rule && (!best || t->w.area > best->w.area))
            best = t;
    }
    return best;
}

/* A picture-in-picture window: close it first, then raise the main window. */
static void on_pip(tracked *t, int64_t now)
{
    char id[32], main_id[32], method[64], fields[320];
    tracked *m = main_window();
    g.pips++;
    quoted_id(id, sizeof id, t->w.id);
    if (m)
        quoted_id(main_id, sizeof main_id, m->w.id);
    else
        snprintf(main_id, sizeof main_id, "null");
    snprintf(fields, sizeof fields, "\"event\":\"pip_seen\",\"id\":%s,\"main\":%s,\"rule\":\"%s\",\"ms_since_seen\":%lld",
             id, main_id, t->rule, (long long)(now - t->seen_ms));
    probe_emit(fields);
    if (g.no_close) {
        snprintf(fields, sizeof fields, "\"event\":\"close_skipped\",\"id\":%s,\"why\":\"no_close\"", id);
        probe_emit(fields);
    } else {
        method[0] = '\0';
        int sent = g.p->close(t->w.id, method, sizeof method);
        t->closed = 1;
        g.closes++;
        snprintf(fields, sizeof fields, "\"event\":\"pip_closed\",\"id\":%s,\"call_ok\":%s,\"method\":\"%s\",\"ms\":%lld",
                 id, sent ? "true" : "false", method, (long long)(probe_mono_ms() - t->seen_ms));
        probe_emit(fields);
    }
    if (!m || g.no_raise) {
        snprintf(fields, sizeof fields, "\"event\":\"raise_skipped\",\"id\":%s,\"why\":\"%s\"", id,
                 m ? "no_raise" : "no_main_window");
        probe_emit(fields);
        return;
    }
    method[0] = '\0';
    int ok = g.p->raise(m->w.id, method, sizeof method);
    int64_t after = probe_mono_ms();
    g.raises++;
    snprintf(fields, sizeof fields,
             "\"event\":\"raised\",\"id\":%s,\"pip_id\":%s,\"call_ok\":%s,\"method\":\"%s\",\"ms\":%lld", main_id, id,
             ok ? "true" : "false", method, (long long)(after - t->seen_ms));
    probe_emit(fields);
    g.want_fg = m->w.id;
    g.want_pip = t->w.id;
    g.want_since = t->seen_ms;
}

static void pip_tick(void *unused)
{
    uint64_t pending[PROBE_PIP_MAX_WINDOWS];
    int npending = 0;
    (void)unused;
    if (!g.can_list)
        return;
    int n = g.p->snapshot(g_snap, PROBE_PIP_MAX_WINDOWS);
    int64_t now = probe_mono_ms();
    for (int i = 0; i < g.n; i++)
        g.win[i].live = 0;
    for (int i = 0; i < n; i++) {
        const probe_win *s = &g_snap[i];
        tracked *t = find(s->id);
        if (!t) {
            if (g.n == PROBE_PIP_MAX_WINDOWS) {
                if (!g.limit_reported)
                    probe_emit("\"event\":\"pip_window_limit\",\"max\":256");
                g.limit_reported = 1;
                continue;
            }
            t = &g.win[g.n++];
            t->w = *s;
            t->seen_ms = now;
            t->live = 1;
            t->rule = probe_pip_rule(s->title, s->role);
            g.windows++;
            emit_window("window", t);
            if (t->rule)
                pending[npending++] = t->w.id;
            continue;
        }
        t->live = 1;
        if (strcmp(t->w.title, s->title) != 0 || strcmp(t->w.role, s->role) != 0 || t->w.topmost != s->topmost) {
            const char *was = t->rule;
            t->w = *s; /* seen_ms stays: the time runs from the window's first appearance */
            t->rule = probe_pip_rule(s->title, s->role);
            emit_window("window_changed", t);
            if (t->rule && !was)
                pending[npending++] = t->w.id;
        } else {
            t->w.area = s->area; /* a size change alone is not reported */
        }
    }
    /* After the whole pass, so the main window is live whatever order the platform listed the windows in. */
    for (int k = 0; k < npending; k++) {
        tracked *t = find(pending[k]);
        if (t)
            on_pip(t, now);
    }
    for (int i = 0; i < g.n;) {
        tracked *t = &g.win[i];
        if (t->live) {
            i++;
            continue;
        }
        emit_window("window_gone", t);
        if (t->rule) {
            char id[32], fields[160];
            quoted_id(id, sizeof id, t->w.id);
            snprintf(fields, sizeof fields, "\"event\":\"pip_gone\",\"id\":%s,\"closed\":%s,\"ms\":%lld", id,
                     t->closed ? "true" : "false", (long long)(now - t->seen_ms));
            probe_emit(fields);
        }
        *t = g.win[--g.n];
    }
    if (g.want_fg && g.p->foreground() == g.want_fg) {
        char id[32], pip_id[32], fields[200];
        quoted_id(id, sizeof id, g.want_fg);
        quoted_id(pip_id, sizeof pip_id, g.want_pip);
        snprintf(fields, sizeof fields, "\"event\":\"foreground\",\"id\":%s,\"pip_id\":%s,\"ms\":%lld", id, pip_id,
                 (long long)(probe_mono_ms() - g.want_since));
        probe_emit(fields);
        g.focused++;
        g.want_fg = 0;
    }
}

static void pip_ended(void *unused)
{
    char fields[200];
    (void)unused;
    snprintf(fields, sizeof fields,
             "\"event\":\"pip_summary\",\"listed\":%s,\"windows\":%d,\"pip\":%d,\"closed\":%d,\"raised\":%d,"
             "\"foreground\":%d",
             g.can_list ? "true" : "false", g.windows, g.pips, g.closes, g.raises, g.focused);
    probe_emit(fields);
}

int probe_pip_main(int argc, char **argv, const probe_pip_platform *p)
{
    if (argc >= 1 && strcmp(argv[0], "--classify") == 0) {
        if (argc < 2 || argc > 3) {
            fputs(PIP_USAGE, stderr);
            return 2;
        }
        const char *rule = probe_pip_rule(argv[1], argc == 3 ? argv[2] : "");
        char r[16], fields[96];
        if (rule)
            snprintf(r, sizeof r, "\"%s\"", rule);
        else
            snprintf(r, sizeof r, "null");
        snprintf(fields, sizeof fields, "\"event\":\"classify\",\"pip\":%s,\"rule\":%s", rule ? "true" : "false", r);
        probe_emit(fields);
        return 0;
    }
    memset(&g, 0, sizeof g);
    int i = 0;
    for (; i < argc; i++) {
        if (strcmp(argv[i], "--no-raise") == 0)
            g.no_raise = 1;
        else if (strcmp(argv[i], "--no-close") == 0)
            g.no_close = 1;
        else
            break;
    }
    if (argc - i < 1) {
        fputs(PIP_USAGE, stderr);
        return 2;
    }
    g.p = p;
    g.can_list = p->session();
    probe_watch w = { pip_tick, pip_ended, NULL };
    return probe_supervise(argc - i, argv + i, PIP_USAGE, &w);
}

int probe_pip(int argc, char **argv)
{
    return probe_pip_main(argc, argv, &probe_pip_platform_impl);
}
```

- [ ] **Step 7: Write the Windows platform**

`tools/overlay-phase0/src/pip_win32.c`:

```c
/* probe_spawn pip, Windows: the job's visible top-level windows, a close, a raise from a background process, and the
   foreground window. */
#include <windows.h>

#include <stdio.h>
#include <string.h>

#include "common.h"
#include "pip.h"
#include "spawn.h"

typedef struct {
    probe_win *out;
    int cap, n;
    unsigned long pids[256];
    int npids;
} snap_ctx;

static void utf8_of(const wchar_t *w, char *out, int cap)
{
    if (!WideCharToMultiByte(CP_UTF8, 0, w, -1, out, cap, NULL, NULL))
        out[0] = '\0';
}

/* EnumWindows callback: adds a visible top-level window owned by one of the job's processes. */
static BOOL CALLBACK add_window(HWND hwnd, LPARAM lp)
{
    snap_ctx *c = (snap_ctx *)lp;
    DWORD pid = 0;
    int ours = 0;
    if (c->n == c->cap || !IsWindowVisible(hwnd))
        return TRUE;
    GetWindowThreadProcessId(hwnd, &pid);
    for (int i = 0; i < c->npids && !ours; i++)
        ours = c->pids[i] == pid;
    if (!ours)
        return TRUE;
    probe_win *w = &c->out[c->n++];
    memset(w, 0, sizeof *w);
    w->id = (uint64_t)(uintptr_t)hwnd;
    w->pid = (long)pid;
    /* Another process's window: GetWindowTextW reads the caption the system keeps and sends no message, so a hung
       browser cannot hang the watch. 160 UTF-16 units fit title's 512 bytes as UTF-8. */
    wchar_t text[160];
    text[0] = L'\0';
    GetWindowTextW(hwnd, text, 160);
    utf8_of(text, w->title, (int)sizeof w->title);
    text[0] = L'\0';
    GetClassNameW(hwnd, text, 40);
    utf8_of(text, w->cls, (int)sizeof w->cls);
    w->topmost = (GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & WS_EX_TOPMOST) != 0;
    RECT r;
    if (GetWindowRect(hwnd, &r))
        w->area = (long long)(r.right - r.left) * (r.bottom - r.top);
    return TRUE;
}

static int session(void)
{
    probe_emit("\"event\":\"pip_session\",\"session\":\"windows\",\"can_list\":true,\"can_raise\":true");
    return 1;
}

static int snapshot(probe_win *out, int cap)
{
    static snap_ctx c;
    c.out = out;
    c.cap = cap;
    c.n = 0;
    c.npids = probe_child_pids(c.pids, 256);
    if (c.npids == 0)
        return 0;
    EnumWindows(add_window, (LPARAM)&c);
    return c.n;
}

/* WM_CLOSE is what the window's own close button sends; a browser then puts the video back in its page. Posted, so a
   busy browser cannot hold up the watch. */
static int close_window(uint64_t id, char *method, size_t cap)
{
    snprintf(method, cap, "WM_CLOSE");
    return PostMessageW((HWND)(uintptr_t)id, WM_CLOSE, 0, 0) != 0;
}

/* SetForegroundWindow works for a caller that meets one of its documented conditions; for a background process the
   one within reach is "the calling process received the last input event". One empty mouse INPUT (no flags: it moves
   and presses nothing) makes this process that one. If the call still fails, the fallback joins the foreground
   thread's input state for the call, then leaves it. The method used is reported, so G17 records which one each
   browser needed. */
static int raise_window(uint64_t id, char *method, size_t cap)
{
    HWND main = (HWND)(uintptr_t)id;
    if (IsIconic(main))
        ShowWindow(main, SW_RESTORE);
    INPUT in;
    memset(&in, 0, sizeof in);
    in.type = INPUT_MOUSE;
    UINT sent = SendInput(1, &in, sizeof in);
    if (SetForegroundWindow(main)) {
        snprintf(method, cap, sent == 1 ? "input" : "input_unsent");
        return 1;
    }
    HWND fg = GetForegroundWindow();
    DWORD fg_thread = fg ? GetWindowThreadProcessId(fg, NULL) : 0, me = GetCurrentThreadId();
    BOOL joined = fg_thread && fg_thread != me && AttachThreadInput(me, fg_thread, TRUE);
    BringWindowToTop(main);
    BOOL ok = SetForegroundWindow(main);
    if (joined)
        AttachThreadInput(me, fg_thread, FALSE);
    snprintf(method, cap, joined ? "attach" : "attach_unjoined");
    return ok != 0;
}

static uint64_t foreground(void)
{
    return (uint64_t)(uintptr_t)GetForegroundWindow();
}

const probe_pip_platform probe_pip_platform_impl = { session, snapshot, close_window, raise_window, foreground };
```

- [ ] **Step 8: Write the Linux platform's session report**

This version has no X11 code; it keeps the Linux build working and reports what it can see, which is nothing.
Task 3 replaces the whole file.

`tools/overlay-phase0/src/pip_x11.c`:

```c
/* probe_spawn pip, Linux: the session kind from the environment. This version has no X11 support, so the watch lists
   no windows and only supervises, and its pip_session line says so. */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "pip.h"

static int session(void)
{
    const char *kind = probe_session_kind(getenv("WAYLAND_DISPLAY"), getenv("XDG_SESSION_TYPE"), getenv("DISPLAY"));
    const char *display = getenv("DISPLAY");
    char disp[160], fields[512];
    probe_json_string(disp, sizeof disp, display ? display : "");
    snprintf(fields, sizeof fields,
             "\"event\":\"pip_session\",\"session\":\"%s\",\"display\":%s,\"x11\":\"not_built\",\"can_list\":false,"
             "\"can_raise\":false,\"note\":\"built without libX11: no windows to list\"",
             kind, disp);
    probe_emit(fields);
    return 0;
}

static int snapshot(probe_win *out, int cap)
{
    (void)out;
    (void)cap;
    return 0;
}

static int close_window(uint64_t id, char *method, size_t cap)
{
    (void)id;
    snprintf(method, cap, "none");
    return 0;
}

static int raise_window(uint64_t id, char *method, size_t cap)
{
    (void)id;
    snprintf(method, cap, "none");
    return 0;
}

static uint64_t foreground(void)
{
    return 0;
}

const probe_pip_platform probe_pip_platform_impl = { session, snapshot, close_window, raise_window, foreground };
```

- [ ] **Step 9: Dispatch and build**

In `tools/overlay-phase0/src/spawn_main.c`, replace `fputs("usage: probe_spawn run|keys|keylog|pad|focus ...\n", stderr);`
with `fputs("usage: probe_spawn run|keys|keylog|pad|focus|pip ...\n", stderr);`, and before `return usage();` at the
end of `main` add:

```c
    if (strcmp(cmd, "pip") == 0)
        return probe_pip(argc - 2, argv + 2);
```

In `tools/overlay-phase0/CMakeLists.txt`, replace:

```cmake
add_executable(probe_spawn src/spawn_main.c src/sdlprobe.c)
if (WIN32)
  target_sources(probe_spawn PRIVATE src/proc_win32.c src/inject_win32.c)
  target_link_libraries(probe_spawn PRIVATE user32)
else ()
  target_sources(probe_spawn PRIVATE src/proc_linux.c src/inject_linux.c)
endif ()
```

with:

```cmake
add_executable(probe_spawn src/spawn_main.c src/sdlprobe.c src/pip.c)
if (WIN32)
  target_sources(probe_spawn PRIVATE src/proc_win32.c src/inject_win32.c src/pip_win32.c)
  target_link_libraries(probe_spawn PRIVATE user32)
else ()
  target_sources(probe_spawn PRIVATE src/proc_linux.c src/inject_linux.c src/pip_x11.c)
endif ()
```

In `tools/overlay-phase0/README.md`, replace the `probe_spawn` row's text
`` `run` (supervise a browser), `keys` (inject keys), `keylog` (log raw keys), `pad` (gamepad in the background), `focus` (does the launcher get focus back). ``
with
`` `run` (supervise a browser), `keys` (inject keys), `keylog` (log raw keys), `pad` (gamepad in the background), `focus` (does the launcher get focus back), `pip` (supervise a browser, report each new window, and when picture-in-picture opens close it and raise the main one). ``

- [ ] **Step 10: Build and run the new tests and `run`'s tests**

Run: `cmake --build C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec/build-phase0 --config Release`
Expected: builds with no warning from `pip.c`, `pip_win32.c` or `proc_win32.c`.

Run: `$env:PROBE_SPAWN = "C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec/build-phase0/Release/probe_spawn.exe"; python -m unittest discover -s C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec/tools/overlay-phase0/tests -p test_pip.py -v`
Expected: OK, 5 tests.

Run: `$env:PROBE_SPAWN = "C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec/build-phase0/Release/probe_spawn.exe"; python -m unittest discover -s C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec/tools/overlay-phase0/tests -p test_spawn.py -v`
Expected: OK; `RunTest` is unchanged and passes, so `run` behaves as before.

- [ ] **Step 11: Run the Linux container check**

In its own call:
`python C:/Users/jscha/.claude/hooks/box_queue.py request --session <session id> --work "overlay kit: Linux check image build (Task 2)" --overlap ok --touches workdir:c:/users/jscha/claudescratch/wt-streamflex-overlay-spec --big`

Then, in the next call:
`docker build -t cptk-overlay-p0-linux:local -f C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec/tools/overlay-phase0/ci/Dockerfile.linux C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec/tools/overlay-phase0/ci`

In its own call:
`python C:/Users/jscha/.claude/hooks/box_queue.py request --session <session id> --work "overlay kit: Linux check run (Task 2)" --overlap ok --touches workdir:c:/users/jscha/claudescratch/wt-streamflex-overlay-spec --big`

Then, in the next call:
`docker run --rm -v "C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec:/src:ro" cptk-overlay-p0-linux:local bash /src/tools/overlay-phase0/ci/linux-check.sh`

Expected: `RESULT: build exit 0 (0 error(s), ...), tests exit 0`; no warning from `pip.c`, `pip_x11.c` or
`proc_linux.c`; `test_pip_supervises_like_run` passes with `session` `none`, and the Windows-only pip test is skipped.

- [ ] **Step 12: Commit**

```powershell
git -C "C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec" add tools/overlay-phase0/src/spawn.h tools/overlay-phase0/src/common.h tools/overlay-phase0/src/common.c tools/overlay-phase0/src/proc_win32.c tools/overlay-phase0/src/proc_linux.c tools/overlay-phase0/src/pip.h tools/overlay-phase0/src/pip.c tools/overlay-phase0/src/pip_win32.c tools/overlay-phase0/src/pip_x11.c tools/overlay-phase0/src/spawn_main.c tools/overlay-phase0/CMakeLists.txt tools/overlay-phase0/README.md tools/overlay-phase0/tests/test_pip.py
git -C "C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec" commit -m "tools: probe_spawn pip watches the browser's windows and raises the main one on picture-in-picture (Windows)"
```

---

### Task 3: `pip` on Linux: X11, XWayland and what Wayland cannot see

**Files:**
- Modify: `tools/overlay-phase0/src/pip_x11.c` (whole file)
- Modify: `tools/overlay-phase0/CMakeLists.txt` (the Linux branch: optional libX11)
- Modify: `tools/overlay-phase0/ci/Dockerfile.linux` (add `libx11-dev`), `tools/overlay-phase0/ci/linux-check.sh` (print its version)
- Modify: `tools/overlay-phase0/README.md:16` (the Linux build line)
- Modify: `tools/overlay-phase0/tests/test_pip.py` (a Linux session class)

**Interfaces:**
- Consumes: `probe_pip_platform`, `probe_win`, `probe_session_kind` (pip.h); `probe_child_has_pid(long)` (spawn.h);
  `probe_json_string`, `probe_emit`.
- Produces: the X11 `close` (`_NET_CLOSE_WINDOW`, method `"_NET_CLOSE_WINDOW"`), `raise` (`_NET_ACTIVE_WINDOW`) and
  `foreground` (the root's `_NET_ACTIVE_WINDOW`). The Linux `pip_session` line, final: `session` (`wayland`, `x11`, `none`), `display`, `x11` (`open`,
  `not_built`, `no_display`, `open_failed`, `no_client_list`), `can_list`, `can_raise`, `note`. The compile
  definition `PROBE_HAVE_X11` and the link to `X11::X11` when CMake finds libX11 (Task 5's `text_linux.c` uses both).

- [ ] **Step 1: Write the failing tests**

Add to `tools/overlay-phase0/tests/test_pip.py`, before `if __name__ == "__main__":`:

```python
@unittest.skipUnless(SPAWN and os.name == "posix", "the Linux watch's session report")
class PipLinuxSessionTest(unittest.TestCase):
    """The Linux check image has libX11 and no display, so each session kind reports what it cannot see."""

    def session(self, **env):
        proc = run_spawn(self, ["pip", sys.executable, "-c", "pass"], env=bare_env(**env))
        self.assertEqual(proc.returncode, 0, proc.stderr)
        evs = events(proc.stdout)
        self.assertEqual(evs[0]["event"], "pip_session")
        self.assertEqual(evs[-1]["event"], "pip_summary")
        self.assertFalse(evs[-1]["listed"])
        return evs[0]

    def test_no_display(self):
        s = self.session()
        self.assertEqual((s["session"], s["x11"], s["can_list"], s["can_raise"]), ("none", "no_display", False, False))
        self.assertEqual(s["note"], "no display: no windows to list")

    def test_wayland_says_it_cannot_list(self):
        s = self.session(WAYLAND_DISPLAY="wayland-cptk-test")
        self.assertEqual((s["session"], s["x11"], s["can_list"], s["can_raise"]), ("wayland", "no_display", False, False))
        self.assertIn("Wayland has no portable window list", s["note"])

    def test_the_session_type_alone_means_wayland(self):
        self.assertEqual(self.session(XDG_SESSION_TYPE="wayland")["session"], "wayland")

    def test_an_x11_display_that_cannot_open(self):
        s = self.session(DISPLAY=":99")
        self.assertEqual((s["session"], s["display"], s["x11"], s["can_list"]), ("x11", ":99", "open_failed", False))
        self.assertEqual(s["note"], "no usable X11 connection")

    def test_an_xwayland_display_that_cannot_open(self):
        s = self.session(WAYLAND_DISPLAY="wayland-cptk-test", DISPLAY=":99")
        self.assertEqual((s["session"], s["x11"], s["can_list"]), ("wayland", "open_failed", False))
        self.assertIn("Wayland has no portable window list", s["note"])
```

- [ ] **Step 2: Run them in the container to see them fail**

In its own call:
`python C:/Users/jscha/.claude/hooks/box_queue.py request --session <session id> --work "overlay kit: Linux check run (Task 3, failing first)" --overlap ok --touches workdir:c:/users/jscha/claudescratch/wt-streamflex-overlay-spec --big`

Then, in the next call:
`docker run --rm -v "C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec:/src:ro" cptk-overlay-p0-linux:local bash /src/tools/overlay-phase0/ci/linux-check.sh`

Expected: FAIL in every `PipLinuxSessionTest` test that reads `x11` or `note`: they read `not_built`.

- [ ] **Step 3: Write the X11 platform**

`tools/overlay-phase0/src/pip_x11.c` (the whole file):

```c
/* probe_spawn pip, Linux. The session kind comes from the environment. On X11, and on Wayland through XWayland when
   DISPLAY is set, the window manager's _NET_CLIENT_LIST is the window list, each window's _NET_WM_PID ties it to the
   browser, _NET_CLOSE_WINDOW closes a picture-in-picture window, and _NET_ACTIVE_WINDOW both raises the main window
   and says which window has the focus. A Wayland session has no portable window list: a browser drawing on Wayland itself is out of reach, and the pip_session line
   says so instead of failing quietly. */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "pip.h"
#include "spawn.h"

#ifdef PROBE_HAVE_X11
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

static Display *g_dpy;
static Atom a_client_list, a_pid, a_name, a_utf8, a_role, a_state, a_above, a_active, a_close;

/* A window can close between the list and a read of its properties; Xlib's default handler would end the process. */
static int ignore_x_error(Display *d, XErrorEvent *e)
{
    (void)d;
    (void)e;
    return 0;
}

/* A property's items (n of them), or NULL when the window has no such property of that type. XFree the result. */
static unsigned char *get_prop(Window w, Atom prop, Atom type, unsigned long *n)
{
    Atom actual = None;
    int format = 0;
    unsigned long after = 0;
    unsigned char *data = NULL;
    *n = 0;
    if (XGetWindowProperty(g_dpy, w, prop, 0, 4096, False, type, &actual, &format, n, &after, &data) != Success
        || actual == None || !data) {
        if (data)
            XFree(data);
        *n = 0;
        return NULL;
    }
    return data;
}

/* Drops a UTF-8 sequence that a cut left unfinished at the end of s. */
static void trim_utf8(char *s)
{
    size_t n = strlen(s), k = n;
    while (k > 0 && ((unsigned char)s[k - 1] & 0xC0) == 0x80)
        k--;
    if (k > 0 && (unsigned char)s[k - 1] >= 0xC0) {
        unsigned char lead = (unsigned char)s[k - 1];
        size_t want = lead >= 0xF0 ? 4 : lead >= 0xE0 ? 3 : 2;
        if (n - (k - 1) < want)
            s[k - 1] = '\0';
    }
}

static void copy_prop(Window w, Atom prop, Atom type, char *out, size_t cap)
{
    unsigned long n = 0;
    unsigned char *d = get_prop(w, prop, type, &n);
    out[0] = '\0';
    if (!d)
        return;
    size_t len = n < cap - 1 ? (size_t)n : cap - 1;
    memcpy(out, d, len);
    out[len] = '\0';
    XFree(d);
    trim_utf8(out);
}

static int has_state(Window w, Atom state)
{
    unsigned long n = 0;
    Atom *atoms = (Atom *)get_prop(w, a_state, XA_ATOM, &n);
    int found = 0;
    for (unsigned long i = 0; atoms && i < n; i++)
        if (atoms[i] == state)
            found = 1;
    if (atoms)
        XFree(atoms);
    return found;
}

static const char *open_x11(const char *display)
{
    if (!display || !*display)
        return "no_display";
    XSetErrorHandler(ignore_x_error);
    g_dpy = XOpenDisplay(display);
    if (!g_dpy)
        return "open_failed";
    a_client_list = XInternAtom(g_dpy, "_NET_CLIENT_LIST", False);
    a_pid = XInternAtom(g_dpy, "_NET_WM_PID", False);
    a_name = XInternAtom(g_dpy, "_NET_WM_NAME", False);
    a_utf8 = XInternAtom(g_dpy, "UTF8_STRING", False);
    a_role = XInternAtom(g_dpy, "WM_WINDOW_ROLE", False);
    a_state = XInternAtom(g_dpy, "_NET_WM_STATE", False);
    a_above = XInternAtom(g_dpy, "_NET_WM_STATE_ABOVE", False);
    a_active = XInternAtom(g_dpy, "_NET_ACTIVE_WINDOW", False);
    a_close = XInternAtom(g_dpy, "_NET_CLOSE_WINDOW", False);
    /* A window manager without EWMH keeps no client list, and then there is nothing to watch. */
    unsigned long n = 0;
    unsigned char *list = get_prop(DefaultRootWindow(g_dpy), a_client_list, XA_WINDOW, &n);
    if (!list) {
        XCloseDisplay(g_dpy);
        g_dpy = NULL;
        return "no_client_list";
    }
    XFree(list);
    return "open";
}

static int snapshot(probe_win *out, int cap)
{
    unsigned long n = 0;
    Window *list = (Window *)get_prop(DefaultRootWindow(g_dpy), a_client_list, XA_WINDOW, &n);
    int count = 0;
    for (unsigned long i = 0; list && i < n && count < cap; i++) {
        unsigned long k = 0;
        unsigned long *pid = (unsigned long *)get_prop(list[i], a_pid, XA_CARDINAL, &k);
        long p = pid && k ? (long)pid[0] : -1;
        if (pid)
            XFree(pid);
        if (p <= 0 || !probe_child_has_pid(p))
            continue;
        XWindowAttributes wa;
        if (!XGetWindowAttributes(g_dpy, list[i], &wa) || wa.map_state != IsViewable)
            continue;
        probe_win *w = &out[count++];
        memset(w, 0, sizeof *w);
        w->id = (uint64_t)list[i];
        w->pid = p;
        w->area = (long long)wa.width * wa.height;
        copy_prop(list[i], a_name, a_utf8, w->title, sizeof w->title);
        if (!w->title[0])
            copy_prop(list[i], XA_WM_NAME, AnyPropertyType, w->title, sizeof w->title);
        XClassHint hint;
        if (XGetClassHint(g_dpy, list[i], &hint)) {
            snprintf(w->instance, sizeof w->instance, "%s", hint.res_name ? hint.res_name : "");
            snprintf(w->cls, sizeof w->cls, "%s", hint.res_class ? hint.res_class : "");
            if (hint.res_name)
                XFree(hint.res_name);
            if (hint.res_class)
                XFree(hint.res_class);
        }
        copy_prop(list[i], a_role, XA_STRING, w->role, sizeof w->role);
        w->topmost = has_state(list[i], a_above);
    }
    if (list)
        XFree(list);
    return count;
}

/* EWMH's close request: the window manager closes the window as its own close button would (WM_DELETE_WINDOW to
   the browser), and the browser puts the video back in its page. Source 2, as for the raise below. */
static int close_window(uint64_t id, char *method, size_t cap)
{
    XEvent e;
    memset(&e, 0, sizeof e);
    e.xclient.type = ClientMessage;
    e.xclient.window = (Window)id;
    e.xclient.message_type = a_close;
    e.xclient.format = 32;
    e.xclient.data.l[0] = CurrentTime;
    e.xclient.data.l[1] = 2;
    Status s = XSendEvent(g_dpy, DefaultRootWindow(g_dpy), False, SubstructureRedirectMask | SubstructureNotifyMask,
                          &e);
    XFlush(g_dpy);
    snprintf(method, cap, "_NET_CLOSE_WINDOW");
    return s != 0;
}

/* EWMH's activation request. Source 2 ("a pager or other direct user action") is the one window managers do not hold
   back as focus stealing; the launcher acts for the user's remote, so it is the honest one. */
static int raise_window(uint64_t id, char *method, size_t cap)
{
    XEvent e;
    memset(&e, 0, sizeof e);
    e.xclient.type = ClientMessage;
    e.xclient.window = (Window)id;
    e.xclient.message_type = a_active;
    e.xclient.format = 32;
    e.xclient.data.l[0] = 2;
    e.xclient.data.l[1] = CurrentTime;
    e.xclient.data.l[2] = 0;
    Status s = XSendEvent(g_dpy, DefaultRootWindow(g_dpy), False, SubstructureRedirectMask | SubstructureNotifyMask,
                          &e);
    XFlush(g_dpy);
    snprintf(method, cap, "_NET_ACTIVE_WINDOW");
    return s != 0;
}

static uint64_t foreground(void)
{
    unsigned long n = 0;
    Window *w = (Window *)get_prop(DefaultRootWindow(g_dpy), a_active, XA_WINDOW, &n);
    uint64_t r = w && n ? (uint64_t)w[0] : 0;
    if (w)
        XFree(w);
    return r;
}
#else
static const char *open_x11(const char *display)
{
    (void)display;
    return "not_built";
}

static int snapshot(probe_win *out, int cap)
{
    (void)out;
    (void)cap;
    return 0;
}

static int close_window(uint64_t id, char *method, size_t cap)
{
    (void)id;
    snprintf(method, cap, "none");
    return 0;
}

static int raise_window(uint64_t id, char *method, size_t cap)
{
    (void)id;
    snprintf(method, cap, "none");
    return 0;
}

static uint64_t foreground(void)
{
    return 0;
}
#endif

static int session(void)
{
    const char *kind = probe_session_kind(getenv("WAYLAND_DISPLAY"), getenv("XDG_SESSION_TYPE"), getenv("DISPLAY"));
    const char *display = getenv("DISPLAY");
    const char *x11 = open_x11(display);
    int can = strcmp(x11, "open") == 0;
    const char *note;
    if (strcmp(kind, "wayland") == 0)
        note = can ? "XWayland lists X11 clients only: a Firefox drawing on Wayland itself is not in the list"
                   : "Wayland has no portable window list: the watch cannot see or raise the browser's windows";
    else if (strcmp(x11, "not_built") == 0)
        note = "built without libX11: no windows to list";
    else if (strcmp(kind, "x11") == 0)
        note = can ? "the window manager's client list" : "no usable X11 connection";
    else
        note = "no display: no windows to list";
    char disp[160], fields[640];
    probe_json_string(disp, sizeof disp, display ? display : "");
    snprintf(fields, sizeof fields,
             "\"event\":\"pip_session\",\"session\":\"%s\",\"display\":%s,\"x11\":\"%s\",\"can_list\":%s,"
             "\"can_raise\":%s,\"note\":\"%s\"",
             kind, disp, x11, can ? "true" : "false", can ? "true" : "false", note);
    probe_emit(fields);
    return can;
}

const probe_pip_platform probe_pip_platform_impl = { session, snapshot, close_window, raise_window, foreground };
```

- [ ] **Step 4: Find libX11 in the build, and install it in the check image**

In `tools/overlay-phase0/CMakeLists.txt`, replace:

```cmake
else ()
  target_sources(probe_spawn PRIVATE src/proc_linux.c src/inject_linux.c src/pip_x11.c)
endif ()
```

with:

```cmake
else ()
  target_sources(probe_spawn PRIVATE src/proc_linux.c src/inject_linux.c src/pip_x11.c)
  # Optional: without libX11, pip's pip_session line reads "x11":"not_built" and the watch lists no windows.
  find_package(X11)
  if (X11_FOUND)
    target_link_libraries(probe_spawn PRIVATE X11::X11)
    target_compile_definitions(probe_spawn PRIVATE PROBE_HAVE_X11=1)
    message(STATUS "probe_spawn pip: X11 window watch built")
  else ()
    message(STATUS "probe_spawn pip: no libX11, so pip lists no windows")
  endif ()
endif ()
```

In `tools/overlay-phase0/ci/Dockerfile.linux`, replace
`      build-essential cmake pkg-config libsdl3-dev python3 nodejs \`
with
`      build-essential cmake pkg-config libsdl3-dev libx11-dev python3 nodejs \`,
and in its first comment replace `with the C build tools, cmake, the distro's SDL3 (at least 3.2.10; the build stops below it), python3 for`
with `with the C build tools, cmake, the distro's SDL3 (at least 3.2.10; the build stops below it) and libX11, python3 for`.

In `tools/overlay-phase0/ci/linux-check.sh`, after `echo "SDL3 $(pkg-config --modversion sdl3)"` add:

```sh
echo "x11 $(pkg-config --modversion x11)"
```

In `tools/overlay-phase0/README.md`, replace `Linux (the distro's SDL3 development package):` with:

```
Linux (the distro's SDL3 and libX11 development packages: Debian and Ubuntu `libsdl3-dev libx11-dev`, Fedora
`SDL3-devel libX11-devel`; without libX11, `pip` lists no windows and says so):
```

- [ ] **Step 5: Run the Linux container check**

In its own call:
`python C:/Users/jscha/.claude/hooks/box_queue.py request --session <session id> --work "overlay kit: Linux check image build (Task 3)" --overlap ok --touches workdir:c:/users/jscha/claudescratch/wt-streamflex-overlay-spec --big`

Then, in the next call:
`docker build -t cptk-overlay-p0-linux:local -f C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec/tools/overlay-phase0/ci/Dockerfile.linux C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec/tools/overlay-phase0/ci`

In its own call:
`python C:/Users/jscha/.claude/hooks/box_queue.py request --session <session id> --work "overlay kit: Linux check run (Task 3)" --overlap ok --touches workdir:c:/users/jscha/claudescratch/wt-streamflex-overlay-spec --big`

Then, in the next call:
`docker run --rm -v "C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec:/src:ro" cptk-overlay-p0-linux:local bash /src/tools/overlay-phase0/ci/linux-check.sh`

Expected: the toolchain section prints an `x11` version; the configure log shows `probe_spawn pip: X11 window watch
built`; `RESULT: build exit 0 (0 error(s), ...), tests exit 0`, with no warning from `pip_x11.c`; all five
`PipLinuxSessionTest` tests pass.

- [ ] **Step 6: Confirm Windows is unchanged**

Run: `cmake --build C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec/build-phase0 --config Release`
Then: `$env:PROBE_SPAWN = "C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec/build-phase0/Release/probe_spawn.exe"; python -m unittest discover -s C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec/tools/overlay-phase0/tests -p test_pip.py -v`
Expected: OK; the five Linux tests are skipped on Windows.

- [ ] **Step 7: Commit**

```powershell
git -C "C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec" add tools/overlay-phase0/src/pip_x11.c tools/overlay-phase0/CMakeLists.txt tools/overlay-phase0/ci/Dockerfile.linux tools/overlay-phase0/ci/linux-check.sh tools/overlay-phase0/README.md tools/overlay-phase0/tests/test_pip.py
git -C "C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec" commit -m "tools: probe_spawn pip on Linux lists X11 and XWayland windows and says what Wayland hides"
```

---

### Task 4: `probe_spawn text` (the shared part, and Windows)

**Files:**
- Create: `tools/overlay-phase0/src/text.h`, `tools/overlay-phase0/src/text.c`, `tools/overlay-phase0/src/text_win32.c`
- Create: `tools/overlay-phase0/src/text_linux.c` (refuses to type; Task 5 replaces it)
- Modify: `tools/overlay-phase0/src/common.h`, `tools/overlay-phase0/src/common.c` (`probe_fopen_utf8`)
- Modify: `tools/overlay-phase0/src/spawn.h`, `tools/overlay-phase0/src/spawn_main.c`, `tools/overlay-phase0/CMakeLists.txt`
- Modify: `tools/overlay-phase0/README.md:56` (the `probe_spawn` row)
- Create: `tools/overlay-phase0/tests/test_text.py`

**Interfaces:**
- Consumes: `probe_parse_count`, `PROBE_KEYS_MAX_DELAY_MS` (keys.h); `probe_emit`, `probe_json_string`.
- Produces (text.h): `PROBE_TEXT_MAX` (256); `int probe_utf8_decode(const char *s, size_t n, uint32_t *out, int cap)`
  (count, -1 malformed, -2 more than cap); `int probe_utf8_encode(uint32_t cp, char *out)` (out holds 5 bytes, NUL
  added; returns the length); `int probe_text_is_control(uint32_t cp)`; `probe_text_platform` with `arg`,
  `prepare`, `plan`, `open`, `type`, `sleep_ms`, `close`; `int probe_text_main(int argc, char **argv, const
  probe_text_platform *p)`; `extern const probe_text_platform probe_text_platform_impl;`
- Produces (common.h): `FILE *probe_fopen_utf8(const char *path);` Produces (spawn.h): `int probe_text(int argc,
  char **argv);`
- Output lines: `text_refused` (`reason`: `empty`, `not_utf8`, `too_long` with `max`, `control` with `index` and
  `cp`, `file_unreadable`), `layout` (Windows: `source` `not_needed`, `note`, and `ignored` when `--layout` was
  given), `untyped` (`index`, `char`, `cp`, `why`), `char` (`index`, `char`, `cp`, the platform's fields, `dry`,
  `sent`), `send_failed`, `text_end` (`chars`, `typable`, `typed`, `untyped`, `dry`). `cp` is `"U+XXXX"`.

- [ ] **Step 1: Write the failing tests**

`tools/overlay-phase0/tests/test_text.py`:

```python
"""Tests for probe_spawn text. PROBE_SPAWN names the built executable. Nothing here types into the desktop: every
real send is either --dry-run or refused before it starts."""
import os
import subprocess
import tempfile
import unittest

from test_spawn import SPAWN, events


def text(test, *args, env=None):
    try:
        return subprocess.run([SPAWN, "text", *args], capture_output=True, encoding="utf-8", timeout=120, env=env)
    except subprocess.TimeoutExpired:
        test.fail("HUNG: probe_spawn text %s did not end" % " ".join(args[:3]))


def write_bytes(folder, data, name="text.txt"):
    path = os.path.join(folder, name)
    with open(path, "wb") as f:
        f.write(data)
    return path


@unittest.skipUnless(SPAWN, "set PROBE_SPAWN to the built probe_spawn")
class TextRefusalTest(unittest.TestCase):
    """Every refusal comes before the layout is learned and before anything is typed, on every platform."""

    def refused(self, proc, reason):
        self.assertEqual(proc.returncode, 2, proc.stderr)
        evs = events(proc.stdout)
        self.assertEqual([e["event"] for e in evs], ["text_refused"])
        self.assertEqual(evs[0]["reason"], reason)
        self.assertIn("nothing was typed", proc.stderr)
        return evs[0]

    def test_a_control_character_is_refused(self):
        for s, index, cp in (("a\tb", 1, "U+0009"), ("ab\x7f", 2, "U+007F"), ("\x85", 0, "U+0085")):
            with self.subTest(cp=cp):
                e = self.refused(text(self, "--dry-run", "0", s), "control")
                self.assertEqual((e["index"], e["cp"]), (index, cp))

    def test_an_empty_string_is_refused(self):
        self.refused(text(self, "--dry-run", "0", ""), "empty")

    def test_more_than_256_characters_are_refused(self):
        self.assertEqual(self.refused(text(self, "--dry-run", "0", "a" * 257), "too_long")["max"], 256)
        with tempfile.TemporaryDirectory() as tmp:
            self.refused(text(self, "--dry-run", "0", "--file", write_bytes(tmp, "é".encode() * 257)), "too_long")
            self.refused(text(self, "--dry-run", "0", "--file", write_bytes(tmp, b"a" * 5000)), "too_long")

    def test_malformed_utf8_in_a_file_is_refused(self):
        with tempfile.TemporaryDirectory() as tmp:
            # an encoded surrogate, an overlong slash, a stray byte, a cut sequence
            for data in (b"a\xed\xa0\x80b", b"\xc0\xaf", b"a\xff", b"\xe2\x82"):
                with self.subTest(data=data):
                    self.refused(text(self, "--dry-run", "0", "--file", write_bytes(tmp, data)), "not_utf8")

    def test_an_inner_line_break_in_a_file_is_refused(self):
        with tempfile.TemporaryDirectory() as tmp:
            e = self.refused(text(self, "--dry-run", "0", "--file", write_bytes(tmp, b"ab\ncd\n")), "control")
        self.assertEqual((e["index"], e["cp"]), (2, "U+000A"))

    def test_a_missing_file_is_refused(self):
        with tempfile.TemporaryDirectory() as tmp:
            self.refused(text(self, "--dry-run", "0", "--file", os.path.join(tmp, "none.txt")), "file_unreadable")

    def test_bad_arguments_print_usage(self):
        for args in ([], ["0"], ["-1", "a"], ["x", "a"], ["0", "a", "b"], ["0", "--file"], ["0", "--file", "a", "b"],
                     ["--layout"]):
            with self.subTest(args=args):
                proc = text(self, *args)
                self.assertEqual(proc.returncode, 2)
                self.assertEqual(events(proc.stdout), [])
                self.assertIn("usage:", proc.stderr)


@unittest.skipUnless(SPAWN, "set PROBE_SPAWN to the built probe_spawn")
@unittest.skipUnless(os.name == "nt", "Linux types only once its layouts are built")  # Task 5 removes this line
class TextDryRunTest(unittest.TestCase):
    def chars(self, proc):
        self.assertEqual(proc.returncode, 0, proc.stderr)
        evs = events(proc.stdout)
        self.assertEqual(evs[-1]["event"], "text_end")
        self.assertIn("layout", [e["event"] for e in evs])
        return [e for e in evs if e["event"] == "char"], evs[-1]

    def test_dry_run_reports_each_character_and_types_nothing(self):
        chars, end = self.chars(text(self, "--dry-run", "--layout", "us", "0", "aZ5 "))
        self.assertEqual([(c["index"], c["char"], c["cp"]) for c in chars],
                         [(0, "a", "U+0061"), (1, "Z", "U+005A"), (2, "5", "U+0035"), (3, " ", "U+0020")])
        self.assertTrue(all(c["dry"] and not c["sent"] for c in chars))
        self.assertEqual((end["chars"], end["typable"], end["typed"], end["untyped"], end["dry"]), (4, 4, 0, 0, True))

    def test_a_file_drops_its_bom_and_one_line_ending(self):
        with tempfile.TemporaryDirectory() as tmp:
            for data in (b"\xef\xbb\xbfab\r\n", b"ab\n", b"ab"):
                with self.subTest(data=data):
                    path = write_bytes(tmp, data)
                    chars, _ = self.chars(text(self, "--dry-run", "--layout", "us", "0", "--file", path))
                    self.assertEqual([c["char"] for c in chars], ["a", "b"])

    def test_256_characters_are_accepted(self):
        chars, end = self.chars(text(self, "--dry-run", "--layout", "us", "0", "a" * 256))
        self.assertEqual((len(chars), end["chars"]), (256, 256))


@unittest.skipUnless(SPAWN and os.name == "nt", "KEYEVENTF_UNICODE is Windows' path")
class TextWindowsTest(unittest.TestCase):
    def test_each_character_is_its_utf16_code_units(self):
        proc = text(self, "--dry-run", "0", "aé€\U0001F600")
        self.assertEqual(proc.returncode, 0, proc.stderr)
        chars = [e for e in events(proc.stdout) if e["event"] == "char"]
        self.assertEqual([c["char"] for c in chars], ["a", "é", "€", "\U0001F600"])
        self.assertEqual([c["cp"] for c in chars], ["U+0061", "U+00E9", "U+20AC", "U+1F600"])
        self.assertEqual([c["units"] for c in chars], [[0x61], [0xE9], [0x20AC], [0xD83D, 0xDE00]])

    def test_no_layout_is_needed(self):
        for args, ignored in ((["--dry-run", "0", "a"], None), (["--dry-run", "--layout", "de", "0", "a"], "--layout")):
            with self.subTest(args=args):
                proc = text(self, *args)
                self.assertEqual(proc.returncode, 0, proc.stderr)
                layout = events(proc.stdout)[0]
                self.assertEqual((layout["event"], layout["source"]), ("layout", "not_needed"))
                self.assertEqual(layout.get("ignored"), ignored)
```

- [ ] **Step 2: Run the tests to see them fail**

Run: `$env:PROBE_SPAWN = "C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec/build-phase0/Release/probe_spawn.exe"; python -m unittest discover -s C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec/tools/overlay-phase0/tests -p test_text.py -v`
Expected: FAIL: `text` is unknown, so every run prints the top-level usage and exits 2 with no event.

- [ ] **Step 3: Add the UTF-8 file opener**

In `tools/overlay-phase0/src/common.h`, add `#include <stdio.h>` after `#include <stdint.h>`, and after the
`probe_mono_ms` declaration add:

```c
/* fopen(path, "rb") for a UTF-8 path, on Windows too. */
FILE *probe_fopen_utf8(const char *path);
```

In `tools/overlay-phase0/src/common.c`, after the Windows `probe_mono_ms` (before `#else`) add:

```c
FILE *probe_fopen_utf8(const char *path)
{
    wchar_t w[4096];
    if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, w, 4096))
        return NULL;
    return _wfopen(w, L"rb");
}
```

and after the Linux `probe_mono_ms` (before `#endif`) add:

```c
FILE *probe_fopen_utf8(const char *path)
{
    return fopen(path, "rb");
}
```

- [ ] **Step 4: Write the shared part**

`tools/overlay-phase0/src/text.h`:

```c
/* probe_spawn text: type a string as trusted keystrokes. */
#ifndef PROBE_TEXT_H
#define PROBE_TEXT_H

#include <stddef.h>
#include <stdint.h>

#define PROBE_TEXT_MAX 256 /* characters, as the spec's send_text allows a message */

/* Decodes n bytes of UTF-8; returns the count of code points, -1 for a malformed sequence (an overlong form, a
   surrogate, a value above U+10FFFF, a stray or cut byte), or -2 for more than cap code points. */
int probe_utf8_decode(const char *s, size_t n, uint32_t *out, int cap);

/* Writes cp as UTF-8 and a NUL into out (5 bytes); returns the length. */
int probe_utf8_encode(uint32_t cp, char *out);

/* A control character the command refuses: U+0000-U+001F and U+007F-U+009F. */
int probe_text_is_control(uint32_t cp);

/* What a platform gives probe_text_main.
   arg: argument i of the command (after "text") as UTF-8, or NULL when it is not valid text.
   prepare: learns the layout from layout (--layout's value, or NULL), printing a layout line; returns 0, or prints why
     not and returns the exit code.
   plan: for every character before anything is typed: returns 1 and writes the char line's platform fields (JSON,
     no leading comma) into detail, or returns 0 and writes the reason in words.
   open (may be NULL), type, close (may be NULL): only for a real send. open returns 0 or prints its own failure
     line; type returns 1 when every event went out, else prints send_failed and returns 0. */
typedef struct {
    const char *(*arg)(int argc, char **argv, int i);
    int (*prepare)(const char *layout);
    int (*plan)(uint32_t cp, char *detail, size_t cap);
    int (*open)(void);
    int (*type)(uint32_t cp);
    void (*sleep_ms)(long ms);
    void (*close)(void);
} probe_text_platform;

extern const probe_text_platform probe_text_platform_impl;

/* text [--dry-run] [--layout <layout>[(<variant>)]] <delay_ms> (<string> | --file <path>). Exit: 0 every character
   typed; 3 some named untyped and the rest typed; 2 refused before anything was typed; 1 a setup or send failure. */
int probe_text_main(int argc, char **argv, const probe_text_platform *p);

#endif
```

`tools/overlay-phase0/src/text.c`:

```c
/* probe_spawn text, the part every platform shares: arguments, decoding, the refusals, and plan-then-type. Every
   character is planned before the first one is typed, so the untyped ones are named before typing starts. */
#include "text.h"

#include <stdio.h>
#include <string.h>

#include "common.h"
#include "keys.h"
#include "spawn.h"

#define TEXT_USAGE \
    "usage: probe_spawn text [--dry-run] [--layout <layout>[(<variant>)]] <delay_ms> (<string> | --file <path>)\n"

int probe_utf8_decode(const char *s, size_t n, uint32_t *out, int cap)
{
    const unsigned char *p = (const unsigned char *)s, *end = p + n;
    int count = 0;
    while (p < end) {
        unsigned char c = *p;
        uint32_t cp;
        int len;
        if (c < 0x80) {
            cp = c;
            len = 1;
        } else if (c >= 0xC2 && c <= 0xDF) {
            cp = c & 0x1Fu;
            len = 2;
        } else if (c >= 0xE0 && c <= 0xEF) {
            cp = c & 0x0Fu;
            len = 3;
        } else if (c >= 0xF0 && c <= 0xF4) {
            cp = c & 0x07u;
            len = 4;
        } else {
            return -1;
        }
        if (end - p < len)
            return -1;
        for (int k = 1; k < len; k++) {
            if ((p[k] & 0xC0) != 0x80)
                return -1;
            cp = (cp << 6) | (p[k] & 0x3Fu);
        }
        if ((len == 3 && cp < 0x800) || (len == 4 && (cp < 0x10000 || cp > 0x10FFFF)) || (cp >= 0xD800 && cp <= 0xDFFF))
            return -1;
        if (count == cap)
            return -2;
        out[count++] = cp;
        p += len;
    }
    return count;
}

int probe_utf8_encode(uint32_t cp, char *out)
{
    int n;
    if (cp < 0x80) {
        out[0] = (char)cp;
        n = 1;
    } else if (cp < 0x800) {
        out[0] = (char)(0xC0 | (cp >> 6));
        out[1] = (char)(0x80 | (cp & 0x3F));
        n = 2;
    } else if (cp < 0x10000) {
        out[0] = (char)(0xE0 | (cp >> 12));
        out[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        out[2] = (char)(0x80 | (cp & 0x3F));
        n = 3;
    } else {
        out[0] = (char)(0xF0 | (cp >> 18));
        out[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
        out[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
        out[3] = (char)(0x80 | (cp & 0x3F));
        n = 4;
    }
    out[n] = '\0';
    return n;
}

int probe_text_is_control(uint32_t cp)
{
    return cp < 0x20 || (cp >= 0x7F && cp <= 0x9F);
}

/* Prints text_refused and a stderr line; returns the refusal's exit code. */
static int refuse(const char *reason, const char *extra)
{
    char fields[200];
    snprintf(fields, sizeof fields, "\"event\":\"text_refused\",\"reason\":\"%s\"%s", reason, extra ? extra : "");
    probe_emit(fields);
    fprintf(stderr, "probe_spawn text: refused (%s); nothing was typed\n", reason);
    return 2;
}

/* Reads a UTF-8 path into buf, dropping a leading byte order mark and one trailing line ending (an editor's), and
   NUL-terminates it. Returns the length, -1 when the file cannot be read, -2 when it holds cap or more bytes. */
static long read_text_file(const char *path, char *buf, size_t cap)
{
    FILE *f = probe_fopen_utf8(path);
    if (!f)
        return -1;
    size_t n = fread(buf, 1, cap - 1, f);
    int more = n == cap - 1 && fgetc(f) != EOF;
    int err = ferror(f);
    fclose(f);
    if (err)
        return -1;
    if (more)
        return -2;
    size_t start = n >= 3 && memcmp(buf, "\xEF\xBB\xBF", 3) == 0 ? 3 : 0;
    if (n > start && buf[n - 1] == '\n') {
        n--;
        if (n > start && buf[n - 1] == '\r')
            n--;
    }
    memmove(buf, buf + start, n - start);
    buf[n - start] = '\0';
    return (long)(n - start);
}

int probe_text_main(int argc, char **argv, const probe_text_platform *p)
{
    int i = 0, dry = 0;
    const char *layout = NULL;
    while (i < argc) {
        if (strcmp(argv[i], "--dry-run") == 0) {
            dry = 1;
            i++;
        } else if (strcmp(argv[i], "--layout") == 0 && i + 1 < argc) {
            layout = argv[i + 1];
            i += 2;
        } else {
            break;
        }
    }
    long delay; /* never negative: that is Sleep(INFINITE) on Windows */
    if (argc - i < 2 || probe_parse_count(argv[i], PROBE_KEYS_MAX_DELAY_MS, &delay) < 0) {
        fputs(TEXT_USAGE, stderr);
        return 2;
    }
    i++;
    static char buf[PROBE_TEXT_MAX * 4 + 8];
    char too_long[32];
    snprintf(too_long, sizeof too_long, ",\"max\":%d", PROBE_TEXT_MAX);
    const char *text;
    size_t len;
    if (strcmp(argv[i], "--file") == 0) {
        if (argc - i != 2) {
            fputs(TEXT_USAGE, stderr);
            return 2;
        }
        const char *path = p->arg(argc, argv, i + 1);
        long got = path ? read_text_file(path, buf, sizeof buf) : -1;
        if (got == -1)
            return refuse("file_unreadable", NULL);
        if (got == -2)
            return refuse("too_long", too_long);
        text = buf;
        len = (size_t)got;
    } else {
        if (argc - i != 1) {
            fputs(TEXT_USAGE, stderr);
            return 2;
        }
        text = p->arg(argc, argv, i);
        if (!text)
            return refuse("not_utf8", NULL);
        len = strlen(text);
    }
    static uint32_t cps[PROBE_TEXT_MAX];
    int n = probe_utf8_decode(text, len, cps, PROBE_TEXT_MAX);
    if (n == -1)
        return refuse("not_utf8", NULL);
    if (n == -2)
        return refuse("too_long", too_long);
    if (n == 0)
        return refuse("empty", NULL);
    for (int k = 0; k < n; k++) {
        if (probe_text_is_control(cps[k])) {
            char extra[64];
            snprintf(extra, sizeof extra, ",\"index\":%d,\"cp\":\"U+%04X\"", k, (unsigned)cps[k]);
            return refuse("control", extra);
        }
    }
    int rc = p->prepare(layout);
    if (rc)
        return rc;

    static char detail[PROBE_TEXT_MAX][320]; /* room for a four-key dead-key or compose sequence on Linux */
    static int typable[PROBE_TEXT_MAX];
    char ch[8], chj[32], why[700], fields[900];
    int untyped = 0;
    for (int k = 0; k < n; k++) {
        typable[k] = p->plan(cps[k], detail[k], sizeof detail[k]);
        if (typable[k])
            continue;
        untyped++;
        probe_utf8_encode(cps[k], ch);
        probe_json_string(chj, sizeof chj, ch);
        probe_json_string(why, sizeof why, detail[k]);
        snprintf(fields, sizeof fields, "\"event\":\"untyped\",\"index\":%d,\"char\":%s,\"cp\":\"U+%04X\",\"why\":%s", k,
                 chj, (unsigned)cps[k], why);
        probe_emit(fields);
        fprintf(stderr, "probe_spawn text: cannot type U+%04X (%s): %s\n", (unsigned)cps[k], ch, detail[k]);
    }
    if (!dry) {
        if (p->open && p->open() != 0)
            return 1;
        p->sleep_ms(delay);
    }
    int failed = 0, typed = 0;
    for (int k = 0; k < n; k++) {
        if (!typable[k])
            continue;
        int sent = 0;
        if (!dry) {
            sent = p->type(cps[k]);
            if (sent)
                typed++;
            else
                failed = 1;
            p->sleep_ms(20); /* one character at a time, as a person types; not a timing check */
        }
        probe_utf8_encode(cps[k], ch);
        probe_json_string(chj, sizeof chj, ch);
        snprintf(fields, sizeof fields, "\"event\":\"char\",\"index\":%d,\"char\":%s,\"cp\":\"U+%04X\",%s,\"dry\":%s,\"sent\":%s",
                 k, chj, (unsigned)cps[k], detail[k], dry ? "true" : "false", sent ? "true" : "false");
        probe_emit(fields);
    }
    if (!dry && p->close)
        p->close();
    snprintf(fields, sizeof fields,
             "\"event\":\"text_end\",\"chars\":%d,\"typable\":%d,\"typed\":%d,\"untyped\":%d,\"dry\":%s", n,
             n - untyped, typed, untyped, dry ? "true" : "false");
    probe_emit(fields);
    return failed ? 1 : untyped ? 3 : 0;
}
```

- [ ] **Step 5: Write the Windows platform**

`tools/overlay-phase0/src/text_win32.c`:

```c
/* probe_spawn text, Windows: each UTF-16 code unit as a KEYEVENTF_UNICODE key press and release, whatever the
   keyboard layout. A character outside the BMP is its two surrogates, sent in one SendInput call so no other input
   lands between them. */
#include <windows.h>
#include <shellapi.h>

#include <stdio.h>
#include <string.h>

#include "common.h"
#include "spawn.h"
#include "text.h"

/* main's argv is in the ANSI code page, so text it cannot hold arrives as '?'. The command line itself is UTF-16:
   argument i of text is the command line's argument wargc - argc + i, since argv is the tail of main's. */
static const char *arg_utf8(int argc, char **argv, int i)
{
    static char buf[2][4096];
    int wargc = 0;
    (void)argv;
    wchar_t **wargv = CommandLineToArgvW(GetCommandLineW(), &wargc);
    int at = wargc - argc + i;
    char *out = buf[i & 1];
    int ok = wargv && at >= 0 && at < wargc
             && WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wargv[at], -1, out, (int)sizeof buf[0], NULL, NULL);
    if (wargv)
        LocalFree(wargv);
    return ok ? out : NULL;
}

static int units_of(uint32_t cp, WORD units[2])
{
    if (cp < 0x10000) {
        units[0] = (WORD)cp;
        return 1;
    }
    cp -= 0x10000;
    units[0] = (WORD)(0xD800 | (cp >> 10));
    units[1] = (WORD)(0xDC00 | (cp & 0x3FF));
    return 2;
}

static int prepare(const char *layout)
{
    probe_emit(layout ? "\"event\":\"layout\",\"source\":\"not_needed\",\"ignored\":\"--layout\","
                        "\"note\":\"KEYEVENTF_UNICODE sends each UTF-16 code unit, whatever the layout\""
                      : "\"event\":\"layout\",\"source\":\"not_needed\","
                        "\"note\":\"KEYEVENTF_UNICODE sends each UTF-16 code unit, whatever the layout\"");
    return 0;
}

static int plan(uint32_t cp, char *detail, size_t cap)
{
    WORD u[2];
    if (units_of(cp, u) == 1)
        snprintf(detail, cap, "\"units\":[%u]", (unsigned)u[0]);
    else
        snprintf(detail, cap, "\"units\":[%u,%u]", (unsigned)u[0], (unsigned)u[1]);
    return 1;
}

static int type_char(uint32_t cp)
{
    WORD u[2];
    int n = units_of(cp, u);
    INPUT in[4];
    memset(in, 0, sizeof in);
    for (int k = 0; k < n; k++) {
        in[2 * k].type = in[2 * k + 1].type = INPUT_KEYBOARD;
        in[2 * k].ki.wScan = in[2 * k + 1].ki.wScan = u[k];
        in[2 * k].ki.dwFlags = KEYEVENTF_UNICODE;
        in[2 * k + 1].ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
    }
    UINT want = (UINT)(2 * n), sent = SendInput(want, in, sizeof in[0]);
    if (sent == want)
        return 1;
    char fields[160];
    snprintf(fields, sizeof fields, "\"event\":\"send_failed\",\"cp\":\"U+%04X\",\"sent\":%u,\"of\":%u,\"error\":%lu",
             (unsigned)cp, sent, want, GetLastError());
    probe_emit(fields);
    return 0;
}

static void sleep_ms(long ms)
{
    Sleep((DWORD)ms);
}

const probe_text_platform probe_text_platform_impl = { arg_utf8, prepare, plan, NULL, type_char, sleep_ms, NULL };

int probe_text(int argc, char **argv)
{
    return probe_text_main(argc, argv, &probe_text_platform_impl);
}
```

- [ ] **Step 6: Write the Linux platform that refuses to type**

This keeps the Linux build working until Task 5 replaces the whole file; its refusals and arguments already work.

`tools/overlay-phase0/src/text_linux.c`:

```c
/* probe_spawn text, Linux. This version knows no keyboard layouts, so after the shared checks it types nothing and
   says so. */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <time.h>

#include "common.h"
#include "spawn.h"
#include "text.h"

static const char *arg_utf8(int argc, char **argv, int i)
{
    (void)argc;
    return argv[i];
}

static int prepare(const char *layout)
{
    (void)layout;
    probe_emit("\"event\":\"text_failed\",\"step\":\"not_built\",\"need\":\"libxkbcommon\"");
    fputs("probe_spawn text: built without keyboard layouts; nothing was typed\n", stderr);
    return 1;
}

static int plan(uint32_t cp, char *detail, size_t cap)
{
    (void)cp;
    snprintf(detail, cap, "no layout");
    return 0;
}

static int type_char(uint32_t cp)
{
    (void)cp;
    return 0;
}

static void sleep_ms(long ms)
{
    struct timespec ts = { ms / 1000, (ms % 1000) * 1000000L };
    nanosleep(&ts, NULL);
}

const probe_text_platform probe_text_platform_impl = { arg_utf8, prepare, plan, NULL, type_char, sleep_ms, NULL };

int probe_text(int argc, char **argv)
{
    return probe_text_main(argc, argv, &probe_text_platform_impl);
}
```

- [ ] **Step 7: Dispatch and build**

In `tools/overlay-phase0/src/spawn.h`, after the `probe_pip` declaration add:

```c
int probe_text(int argc, char **argv);   /* text [--dry-run] [--layout <l>] <delay_ms> (<string> | --file <path>) */
```

In `tools/overlay-phase0/src/spawn_main.c`, replace `fputs("usage: probe_spawn run|keys|keylog|pad|focus|pip ...\n", stderr);`
with `fputs("usage: probe_spawn run|keys|keylog|pad|focus|pip|text ...\n", stderr);`, and after the `pip` branch add:

```c
    if (strcmp(cmd, "text") == 0)
        return probe_text(argc - 2, argv + 2);
```

In `tools/overlay-phase0/CMakeLists.txt`, replace:

```cmake
add_executable(probe_spawn src/spawn_main.c src/sdlprobe.c src/pip.c)
if (WIN32)
  target_sources(probe_spawn PRIVATE src/proc_win32.c src/inject_win32.c src/pip_win32.c)
  target_link_libraries(probe_spawn PRIVATE user32)
else ()
  target_sources(probe_spawn PRIVATE src/proc_linux.c src/inject_linux.c src/pip_x11.c)
```

with:

```cmake
add_executable(probe_spawn src/spawn_main.c src/sdlprobe.c src/pip.c src/text.c)
if (WIN32)
  target_sources(probe_spawn PRIVATE src/proc_win32.c src/inject_win32.c src/pip_win32.c src/text_win32.c)
  target_link_libraries(probe_spawn PRIVATE user32 shell32) # shell32: CommandLineToArgvW
else ()
  target_sources(probe_spawn PRIVATE src/proc_linux.c src/inject_linux.c src/pip_x11.c src/text_linux.c)
```

In `tools/overlay-phase0/README.md`, in the `probe_spawn` row, replace
`` `pip` (supervise a browser, report each new window, and when picture-in-picture opens close it and raise the main one). ``
with
`` `pip` (supervise a browser, report each new window, and when picture-in-picture opens close it and raise the main one), `text` (type a string, or a UTF-8 file's text, as trusted keystrokes). ``

- [ ] **Step 8: Build and run the tests**

Run: `cmake --build C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec/build-phase0 --config Release`
Expected: builds with no warning from `text.c`, `text_win32.c` or `common.c`.

Run: `$env:PROBE_SPAWN = "C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec/build-phase0/Release/probe_spawn.exe"; python -m unittest discover -s C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec/tools/overlay-phase0/tests -p test_text.py -v`
Expected: OK, 12 tests.

- [ ] **Step 9: Run the Linux container check**

In its own call:
`python C:/Users/jscha/.claude/hooks/box_queue.py request --session <session id> --work "overlay kit: Linux check image build (Task 4)" --overlap ok --touches workdir:c:/users/jscha/claudescratch/wt-streamflex-overlay-spec --big`

Then, in the next call:
`docker build -t cptk-overlay-p0-linux:local -f C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec/tools/overlay-phase0/ci/Dockerfile.linux C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec/tools/overlay-phase0/ci`

In its own call:
`python C:/Users/jscha/.claude/hooks/box_queue.py request --session <session id> --work "overlay kit: Linux check run (Task 4)" --overlap ok --touches workdir:c:/users/jscha/claudescratch/wt-streamflex-overlay-spec --big`

Then, in the next call:
`docker run --rm -v "C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec:/src:ro" cptk-overlay-p0-linux:local bash /src/tools/overlay-phase0/ci/linux-check.sh`

Expected: `RESULT: build exit 0 (0 error(s), ...), tests exit 0`, no warning from `text.c` or `text_linux.c`; all
seven `TextRefusalTest` tests pass on Linux; `TextDryRunTest` and `TextWindowsTest` are skipped.

- [ ] **Step 10: Commit**

```powershell
git -C "C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec" add tools/overlay-phase0/src/text.h tools/overlay-phase0/src/text.c tools/overlay-phase0/src/text_win32.c tools/overlay-phase0/src/text_linux.c tools/overlay-phase0/src/common.h tools/overlay-phase0/src/common.c tools/overlay-phase0/src/spawn.h tools/overlay-phase0/src/spawn_main.c tools/overlay-phase0/CMakeLists.txt tools/overlay-phase0/README.md tools/overlay-phase0/tests/test_text.py
git -C "C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec" commit -m "tools: probe_spawn text types a string as KEYEVENTF_UNICODE keystrokes on Windows"
```

---

### Task 5: `text` on Linux: the session's layout, dead keys and compose, and uinput

**Files:**
- Create: `tools/overlay-phase0/src/uinput_linux.h`
- Modify: `tools/overlay-phase0/src/inject_linux.c:19-98` (the uinput device becomes shared)
- Modify: `tools/overlay-phase0/src/text_linux.c` (whole file)
- Modify: `tools/overlay-phase0/CMakeLists.txt` (the Linux branch: optional libxkbcommon)
- Modify: `tools/overlay-phase0/ci/Dockerfile.linux`, `tools/overlay-phase0/ci/linux-check.sh`, `tools/overlay-phase0/README.md:16`
- Modify: `tools/overlay-phase0/tests/test_text.py` (remove `TextDryRunTest`'s Windows-only line; add `TextLinuxTest`)

**Interfaces:**
- Consumes: `probe_text_platform`, `probe_text_main` (text.h); `probe_session_kind` (pip.h); `probe_mono_ms`;
  `PROBE_HAVE_X11` (Task 3).
- Produces (uinput_linux.h): `int probe_uinput_open(const unsigned short *codes, unsigned n);` (NULL codes: every
  code 1-247), `int probe_uinput_key(unsigned short code, int down);`, `void probe_uinput_close(void);`
- Output lines (Linux): `layout_note` (when run as root), `layout_source` (`source`, `result`), `layout` (`source`,
  `rules`, `model`, `layout`, `variant`, `options`), `compose` (`locale`, `table`: `ok` or `none`),
  `layout_unknown` (`session`, `desktop`), `text_failed` (`step` `not_built` or `keymap`). `char`'s platform fields:
  `"path":"direct","key":<evdev>,"mods":[...]`, or `"path":"dead"|"compose","keys":[{"key":<evdev>,"mods":[...]},
  ...]`; mods are `"Shift"`, `"AltGr"`.
  Source names: `argument`, `x11:_XKB_RULES_NAMES`, `gnome:org.gnome.desktop.input-sources`, `kde:kxkbrc`,
  `localectl`. Results: `ok`, `bad_argument`, `not_built`, `open_failed`, `no_property`, `no_layout`,
  `no_xkb_source`, `not_in_use`, `no_file`, `no_home`, `not_found`, `failed`, `stalled`.

- [ ] **Step 1: Write the failing tests**

In `tools/overlay-phase0/tests/test_text.py`, delete the line
`@unittest.skipUnless(os.name == "nt", "Linux types only once its layouts are built")  # Task 5 removes this line`,
and add before the end of the file:

```python
@unittest.skipUnless(SPAWN and os.name == "posix", "the Linux layouts")
class TextLinuxTest(unittest.TestCase):
    def keys(self, layout, s):
        proc = text(self, "--dry-run", "--layout", layout, "0", s)
        self.assertEqual(proc.returncode, 0, proc.stderr)
        evs = events(proc.stdout)
        self.assertEqual([e["source"] for e in evs if e["event"] == "layout"], ["argument"])
        chars = [e for e in evs if e["event"] == "char"]
        self.assertEqual({e["path"] for e in chars}, {"direct"})
        return [(e["char"], e["key"], e["mods"]) for e in chars]

    def compose_env(self, tmp, locale, **extra):
        """This environment with LC_ALL set to locale and no compose file of the user's own in play."""
        env = {k: v for k, v in os.environ.items()
               if k not in ("XCOMPOSEFILE", "LC_CTYPE", "LANG", "XDG_CONFIG_HOME", "DISPLAY", "XDG_CURRENT_DESKTOP")}
        env.update(LC_ALL=locale, HOME=tmp, **extra)
        return env

    def sequences(self, proc):
        return [(e["char"], e["path"], [(k["key"], k["mods"]) for k in e.get("keys", [])])
                for e in events(proc.stdout) if e["event"] == "char"]

    def test_dead_keys_in_the_german_layout(self):
        with tempfile.TemporaryDirectory() as tmp:
            proc = text(self, "--dry-run", "--layout", "de", "0", "éêà", env=self.compose_env(tmp, "en_US.UTF-8"))
        self.assertEqual(proc.returncode, 0, proc.stderr)
        self.assertEqual([(e["locale"], e["table"]) for e in events(proc.stdout) if e["event"] == "compose"],
                         [("en_US.UTF-8", "ok")])
        # de: dead acute on the key right of ß (13), dead grave with Shift on it, dead circumflex left of 1 (41).
        self.assertEqual(self.sequences(proc), [("é", "dead", [(13, []), (18, [])]),
                                                ("ê", "dead", [(41, []), (18, [])]),
                                                ("à", "dead", [(13, ["Shift"]), (30, [])])])

    def test_compose_through_a_compose_key(self):
        with tempfile.TemporaryDirectory() as tmp:
            os.makedirs(os.path.join(tmp, ".config"))
            with open(os.path.join(tmp, ".config", "kxkbrc"), "w", encoding="utf-8") as f:
                f.write("[Layout]\nLayoutList=us\nOptions=compose:ralt\nUse=true\n")
            env = self.compose_env(tmp, "en_US.UTF-8", WAYLAND_DISPLAY="wayland-0", XDG_CURRENT_DESKTOP="KDE",
                                   PATH=os.path.join(tmp, "bin"))
            proc = text(self, "--dry-run", "0", "é", env=env)
        self.assertEqual(proc.returncode, 0, proc.stderr)
        (char,) = [e for e in events(proc.stdout) if e["event"] == "char"]
        self.assertEqual(char["path"], "compose")
        self.assertEqual(len(char["keys"]), 3)
        self.assertEqual(char["keys"][0], {"key": 100, "mods": []})  # Multi_key on the right Alt

    def test_a_character_with_no_path_is_named(self):
        with tempfile.TemporaryDirectory() as tmp:
            proc = text(self, "--dry-run", "--layout", "de", "0", "a中", env=self.compose_env(tmp, "en_US.UTF-8"))
        self.assertEqual(proc.returncode, 3)
        evs = events(proc.stdout)
        self.assertEqual([(e["char"], e["cp"]) for e in evs if e["event"] == "untyped"], [("中", "U+4E2D")])
        self.assertEqual([e["char"] for e in evs if e["event"] == "char"], ["a"])
        self.assertIn("cannot type U+4E2D", proc.stderr)

    def test_no_compose_table_for_the_locale(self):
        with tempfile.TemporaryDirectory() as tmp:
            proc = text(self, "--dry-run", "--layout", "de", "0", "aé", env=self.compose_env(tmp, "xx_NOSUCH.UTF-8"))
        self.assertEqual(proc.returncode, 3)
        evs = events(proc.stdout)
        self.assertEqual([(e["locale"], e["table"]) for e in evs if e["event"] == "compose"],
                         [("xx_NOSUCH.UTF-8", "none")])
        self.assertEqual([e["char"] for e in evs if e["event"] == "char"], ["a"])
        (untyped,) = [e for e in evs if e["event"] == "untyped"]
        self.assertEqual(untyped["char"], "é")
        self.assertIn("no compose table", untyped["why"])

    def test_us_layout(self):
        self.assertEqual(self.keys("us", "aA1!"),
                         [("a", 30, []), ("A", 30, ["Shift"]), ("1", 2, []), ("!", 2, ["Shift"])])

    def test_german_layout_uses_altgr_and_its_own_keys(self):
        self.assertEqual(self.keys("de", "@€zyßä"),
                         [("@", 16, ["AltGr"]), ("€", 18, ["AltGr"]), ("z", 21, []), ("y", 44, []), ("ß", 12, []),
                          ("ä", 40, [])])

    def test_a_character_the_layout_lacks_is_refused_by_name(self):
        proc = text(self, "--dry-run", "--layout", "us", "0", "aé")
        self.assertEqual(proc.returncode, 3)
        evs = events(proc.stdout)
        self.assertEqual([(e["index"], e["char"], e["cp"]) for e in evs if e["event"] == "untyped"], [(1, "é", "U+00E9")])
        self.assertEqual([e["char"] for e in evs if e["event"] == "char"], ["a"])
        self.assertEqual((evs[-1]["typable"], evs[-1]["untyped"]), (1, 1))
        self.assertIn("cannot type U+00E9 (é)", proc.stderr)

    def test_an_unknown_layout_name(self):
        proc = text(self, "--dry-run", "--layout", "nosuchlayout", "0", "a")
        self.assertEqual(proc.returncode, 1)
        self.assertEqual([e["step"] for e in events(proc.stdout) if e["event"] == "text_failed"], ["keymap"])

    def session_env(self, tmp, **extra):
        """No display, no desktop, and only tmp/bin on PATH, so no real gsettings or localectl answers."""
        os.makedirs(os.path.join(tmp, "bin"), exist_ok=True)
        env = {"PATH": os.path.join(tmp, "bin"), "HOME": tmp, "LANG": "C.UTF-8"}
        env.update(extra)
        return env

    def fake(self, tmp, name, body):
        path = os.path.join(tmp, "bin", name)
        with open(path, "w", encoding="utf-8") as f:
            f.write("#!/bin/sh\n" + body)
        os.chmod(path, 0o755)

    def sources(self, proc):
        return [(e["source"], e["result"]) for e in events(proc.stdout) if e["event"] == "layout_source"]

    def layout(self, proc):
        return [e for e in events(proc.stdout) if e["event"] == "layout"]

    def test_kde_layout_from_kxkbrc(self):
        with tempfile.TemporaryDirectory() as tmp:
            os.makedirs(os.path.join(tmp, ".config"))
            with open(os.path.join(tmp, ".config", "kxkbrc"), "w", encoding="utf-8") as f:
                f.write("[Layout]\nLayoutList=de,us\nVariantList=,\nUse=true\n")
            env = self.session_env(tmp, WAYLAND_DISPLAY="wayland-0", XDG_CURRENT_DESKTOP="KDE")
            proc = text(self, "--dry-run", "0", "z", env=env)
        self.assertEqual(proc.returncode, 0, proc.stderr)
        self.assertEqual(self.sources(proc), [("kde:kxkbrc", "ok")])
        (layout,) = self.layout(proc)
        self.assertEqual((layout["source"], layout["layout"], layout["variant"]), ("kde:kxkbrc", "de", ""))
        self.assertEqual([e["key"] for e in events(proc.stdout) if e["event"] == "char"], [21])

    def test_gnome_layout_from_gsettings(self):
        with tempfile.TemporaryDirectory() as tmp:
            env = self.session_env(tmp, WAYLAND_DISPLAY="wayland-0", XDG_CURRENT_DESKTOP="ubuntu:GNOME")
            self.fake(tmp, "gsettings", 'case "$3" in\n'
                                        "  mru-sources) echo \"[('xkb', 'de+nodeadkeys'), ('xkb', 'us')]\" ;;\n"
                                        "  xkb-options) echo \"['compose:ralt']\" ;;\n"
                                        "  *) echo \"@a(ss) []\" ;;\n"
                                        "esac\n")
            proc = text(self, "--dry-run", "0", "z", env=env)
        self.assertEqual(proc.returncode, 0, proc.stderr)
        self.assertEqual(self.sources(proc), [("gnome:org.gnome.desktop.input-sources", "ok")])
        (layout,) = self.layout(proc)
        self.assertEqual((layout["layout"], layout["variant"], layout["options"]), ("de", "nodeadkeys", "compose:ralt"))
        self.assertEqual([e["key"] for e in events(proc.stdout) if e["event"] == "char"], [21])

    def test_an_x11_session_tries_its_display_first(self):
        with tempfile.TemporaryDirectory() as tmp:
            proc = text(self, "--dry-run", "0", "a", env=self.session_env(tmp, DISPLAY=":99"))
        self.assertEqual(self.sources(proc), [("x11:_XKB_RULES_NAMES", "open_failed"), ("localectl", "not_found")])
        self.assertEqual(proc.returncode, 1)

    def test_a_stalled_source_is_named_and_skipped(self):
        with tempfile.TemporaryDirectory() as tmp:
            env = self.session_env(tmp, WAYLAND_DISPLAY="wayland-0", XDG_CURRENT_DESKTOP="GNOME")
            self.fake(tmp, "gsettings", "exec /bin/sleep 600\n")  # no output, no CPU: stalled, never slow
            proc = text(self, "--dry-run", "0", "a", env=env)
        self.assertEqual(self.sources(proc), [("gnome:org.gnome.desktop.input-sources", "stalled"),
                                              ("localectl", "not_found")])
        self.assertEqual(self.layout(proc), [])
        self.assertEqual(proc.returncode, 1)

    def test_no_source_refuses_and_types_nothing(self):
        with tempfile.TemporaryDirectory() as tmp:
            proc = text(self, "0", "a", env=self.session_env(tmp))  # a real send: it must stop before uinput
        self.assertEqual(proc.returncode, 1)
        self.assertEqual(self.sources(proc), [("localectl", "not_found")])
        evs = events(proc.stdout)
        self.assertEqual(evs[-1]["event"], "layout_unknown")
        self.assertEqual([e for e in evs if e["event"] in ("char", "uinput_failed")], [])
        self.assertIn("pass --layout", proc.stderr)

    @unittest.skipIf(os.access("/dev/uinput", os.W_OK), "this box could really type")
    def test_a_send_without_uinput_types_nothing(self):
        proc = text(self, "--layout", "us", "0", "a")
        self.assertEqual(proc.returncode, 1)
        self.assertEqual([e["event"] for e in events(proc.stdout) if e["event"] != "layout_note"],
                         ["layout_source", "layout", "compose", "uinput_failed"])
```

- [ ] **Step 2: Run them in the container to see them fail**

In its own call:
`python C:/Users/jscha/.claude/hooks/box_queue.py request --session <session id> --work "overlay kit: Linux check run (Task 5, failing first)" --overlap ok --touches workdir:c:/users/jscha/claudescratch/wt-streamflex-overlay-spec --big`

Then, in the next call:
`docker run --rm -v "C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec:/src:ro" cptk-overlay-p0-linux:local bash /src/tools/overlay-phase0/ci/linux-check.sh`

Expected: FAIL in `TextDryRunTest` and `TextLinuxTest` (exit 1, `text_failed` `not_built`).

- [ ] **Step 3: Share the uinput keyboard**

`tools/overlay-phase0/src/uinput_linux.h`:

```c
/* The one uinput keyboard probe_spawn's Linux commands send through. */
#ifndef PROBE_UINPUT_LINUX_H
#define PROBE_UINPUT_LINUX_H

/* Creates the device with the key codes codes[0..n-1], or every code from 1 to 247 when codes is NULL, and waits 1 s
   for the desktop to pick it up. Returns 0, or prints uinput_failed with the step and errno and returns 1. */
int probe_uinput_open(const unsigned short *codes, unsigned n);

/* One key event and its report: 0, or -1 with errno set. */
int probe_uinput_key(unsigned short code, int down);

/* Waits 0.5 s for the desktop to read the last event, then destroys the device. */
void probe_uinput_close(void);

#endif
```

In `tools/overlay-phase0/src/inject_linux.c`, add `#include "uinput_linux.h"` after `#include "spawn.h"`, and
replace everything from `static int uinput_open(void)` through
`static const probe_key_sender sender = { uinput_open, send_key, sleep_ms, uinput_close };` with:

```c
int probe_uinput_open(const unsigned short *codes, unsigned n)
{
    g_fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (g_fd < 0)
        return uinput_fail("open");
    if (ioctl(g_fd, UI_SET_EVBIT, EV_KEY) < 0)
        return uinput_fail("UI_SET_EVBIT");
    unsigned count = codes ? n : 247;
    for (unsigned i = 0; i < count; i++) {
        if (ioctl(g_fd, UI_SET_KEYBIT, (int)(codes ? codes[i] : i + 1)) < 0)
            return uinput_fail("UI_SET_KEYBIT");
    }
    struct uinput_setup us;
    memset(&us, 0, sizeof us);
    us.id.bustype = BUS_USB;
    us.id.vendor = 0x1209;
    us.id.product = 0x0001;
    snprintf(us.name, sizeof us.name, "CPTK probe keyboard");
    if (ioctl(g_fd, UI_DEV_SETUP, &us) < 0)
        return uinput_fail("UI_DEV_SETUP");
    if (ioctl(g_fd, UI_DEV_CREATE) < 0)
        return uinput_fail("UI_DEV_CREATE");
    sleep_ms(1000); /* let the desktop pick up the new device */
    return 0;
}

int probe_uinput_key(unsigned short code, int down)
{
    return emit_ev(g_fd, EV_KEY, code, down ? 1 : 0) == 0 && emit_ev(g_fd, EV_SYN, SYN_REPORT, 0) == 0 ? 0 : -1;
}

void probe_uinput_close(void)
{
    sleep_ms(500); /* let the desktop read the last key-up before the device goes away */
    ioctl(g_fd, UI_DEV_DESTROY);
    close(g_fd);
    g_fd = -1;
}

static int keys_open(void)
{
    unsigned short codes[64];
    for (unsigned i = 0; i < probe_keys_count; i++)
        codes[i] = probe_keys_table[i].evdev;
    return probe_uinput_open(codes, probe_keys_count);
}

static void send_key(const probe_key *k, int up)
{
    if (probe_uinput_key(k->evdev, !up) == 0)
        return;
    int err = errno;
    char fields[160];
    snprintf(fields, sizeof fields, "\"event\":\"send_failed\",\"name\":\"%s\",\"up\":%s,\"error\":%d", k->name,
             up ? "true" : "false", err);
    probe_emit(fields);
    g_send_failed = 1;
}

static const probe_key_sender sender = { keys_open, send_key, sleep_ms, probe_uinput_close };
```

(`g_fd`, `sleep_ms`, `g_send_failed`, `emit_ev` and `uinput_fail` above that range stay as they are; `keys`'s
behavior does not change.)

- [ ] **Step 4: Write the Linux platform**

`tools/overlay-phase0/src/text_linux.c` (the whole file):

```c
/* probe_spawn text, Linux: each character as the key, and Shift or AltGr, that give it in the session's keyboard
   layout, sent through uinput. The layout comes from --layout, else from the session: X11's _XKB_RULES_NAMES,
   GNOME's org.gnome.desktop.input-sources, KDE's kxkbrc, then localectl's system default, each attempt reported. The
   chosen layout is compiled with xkbcommon, and each character is found by pressing nothing, Shift, AltGr, then
   Shift and AltGr on an xkb_state, keys in code order: the same reading the compositor makes of those presses. A
   character no key gives directly is looked up in xkbcommon's compose table for the session's locale: the shortest
   entry producing it whose every keysym the layout can type, a dead-key sequence before a Multi_key one, is typed
   key by key, and the compositor composes it. Only a character with no path is named untyped. */
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <linux/input-event-codes.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "common.h"
#include "pip.h"
#include "spawn.h"
#include "text.h"
#include "uinput_linux.h"

#ifdef PROBE_HAVE_XKB
#include <xkbcommon/xkbcommon-compose.h>
#include <xkbcommon/xkbcommon.h>
#endif
#ifdef PROBE_HAVE_X11
#include <X11/XKBlib.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#endif

#define MOD_SHIFT 1
#define MOD_ALTGR 2
#define STALL_MS 10000 /* a source with no output, no CPU use and no exit for this long is stalled */

static void sleep_ms(long ms)
{
    struct timespec ts = { ms / 1000, (ms % 1000) * 1000000L };
    nanosleep(&ts, NULL);
}

/* Everything from here to press exists only to feed xkbcommon, so a build without it carries none of it. */
#ifdef PROBE_HAVE_XKB
typedef struct {
    char rules[64], model[64], layout[64], variant[64], options[256];
    const char *source;
} rmlvo;

static void set_field(char *field, size_t cap, const char *value)
{
    snprintf(field, cap, "%s", value ? value : "");
}
#define SET(field, value) set_field(field, sizeof field, value)

static char *trim(char *s)
{
    while (*s == ' ' || *s == '\t')
        s++;
    size_t n = strlen(s);
    while (n && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\r' || s[n - 1] == '\n'))
        s[--n] = '\0';
    return s;
}

/* The n-th (from 0) comma-separated item of list, or "" when there is none. */
static void nth_item(const char *list, int n, char *out, size_t cap)
{
    out[0] = '\0';
    for (int i = 0; list && i < n; i++) {
        list = strchr(list, ',');
        if (list)
            list++;
    }
    if (!list)
        return;
    size_t len = strcspn(list, ",");
    if (len >= cap)
        len = cap - 1;
    memcpy(out, list, len);
    out[len] = '\0';
}

/* utime + stime of pid, in clock ticks, from /proc/<pid>/stat; -1 when unreadable. */
static long long cpu_ticks(pid_t pid)
{
    char path[64], buf[1024];
    snprintf(path, sizeof path, "/proc/%d/stat", (int)pid);
    FILE *f = fopen(path, "r");
    if (!f)
        return -1;
    size_t n = fread(buf, 1, sizeof buf - 1, f);
    fclose(f);
    buf[n] = '\0';
    char *p = strrchr(buf, ')'); /* the command name may hold spaces; the fields after it do not */
    unsigned long long ut = 0, st = 0;
    if (!p || sscanf(p + 2, "%*c %*d %*d %*d %*d %*d %*u %*lu %*lu %*lu %*lu %llu %llu", &ut, &st) != 2)
        return -1;
    return (long long)(ut + st);
}

/* Runs argv from PATH with stdin and stderr on /dev/null and its stdout into out (the first cap - 1 bytes). Returns
   "ok", or why not: "not_found" (exit 127), "failed" (another failure), "stalled" (STALL_MS with no output, no CPU
   use and no exit; it is killed). A source that keeps working is never cut off. */
static const char *capture(char *const argv[], char *out, size_t cap)
{
    int fds[2];
    out[0] = '\0';
    if (pipe(fds) == -1)
        return "failed";
    pid_t pid = fork();
    if (pid < 0) {
        close(fds[0]);
        close(fds[1]);
        return "failed";
    }
    if (pid == 0) {
        int null_fd = open("/dev/null", O_RDWR);
        if (null_fd >= 0) {
            dup2(null_fd, STDIN_FILENO);
            dup2(null_fd, STDERR_FILENO);
        }
        dup2(fds[1], STDOUT_FILENO);
        close(fds[0]);
        close(fds[1]);
        execvp(argv[0], argv);
        _exit(127);
    }
    close(fds[1]);
    size_t len = 0;
    int pipe_open = 1, status = 0;
    long long cpu = cpu_ticks(pid);
    int64_t last = probe_mono_ms();
    for (;;) {
        if (pipe_open) {
            struct pollfd pfd = { fds[0], POLLIN, 0 };
            if (poll(&pfd, 1, 100) > 0) {
                char buf[512];
                ssize_t n = read(fds[0], buf, sizeof buf);
                if (n > 0) {
                    size_t keep = len + (size_t)n < cap - 1 ? (size_t)n : cap - 1 - len;
                    memcpy(out + len, buf, keep);
                    len += keep;
                    out[len] = '\0';
                    last = probe_mono_ms();
                } else if (n == 0 || errno != EINTR) {
                    pipe_open = 0;
                }
            }
        } else {
            poll(NULL, 0, 100);
        }
        if (waitpid(pid, &status, WNOHANG) == pid)
            break;
        long long now_cpu = cpu_ticks(pid);
        if (now_cpu != cpu) {
            cpu = now_cpu;
            last = probe_mono_ms();
        }
        if (probe_mono_ms() - last > STALL_MS) {
            kill(pid, SIGKILL);
            waitpid(pid, NULL, 0);
            close(fds[0]);
            return "stalled";
        }
    }
    /* What it wrote just before it exited. */
    struct pollfd pfd = { fds[0], POLLIN, 0 };
    ssize_t n;
    char buf[512];
    while (pipe_open && poll(&pfd, 1, 0) > 0 && (n = read(fds[0], buf, sizeof buf)) > 0) {
        size_t keep = len + (size_t)n < cap - 1 ? (size_t)n : cap - 1 - len;
        memcpy(out + len, buf, keep);
        len += keep;
        out[len] = '\0';
    }
    close(fds[0]);
    if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
        return "ok";
    return WIFEXITED(status) && WEXITSTATUS(status) == 127 ? "not_found" : "failed";
}

/* "de" or "de(nodeadkeys)". */
static const char *from_argument(const char *arg, rmlvo *r)
{
    const char *open_paren = strchr(arg, '(');
    size_t len = open_paren ? (size_t)(open_paren - arg) : strlen(arg);
    if (!len || len >= sizeof r->layout)
        return "bad_argument";
    memcpy(r->layout, arg, len);
    r->layout[len] = '\0';
    if (open_paren) {
        const char *close_paren = strchr(open_paren, ')');
        size_t vlen = close_paren ? (size_t)(close_paren - open_paren - 1) : 0;
        if (!close_paren || close_paren[1] || !vlen || vlen >= sizeof r->variant)
            return "bad_argument";
        memcpy(r->variant, open_paren + 1, vlen);
        r->variant[vlen] = '\0';
    }
    return "ok";
}

/* The X server's own rules, model, layouts, variants and options, and the group it has active. */
static const char *from_x11(rmlvo *r)
{
#ifndef PROBE_HAVE_X11
    (void)r;
    return "not_built";
#else
    const char *d = getenv("DISPLAY");
    Display *dpy = d && *d ? XOpenDisplay(d) : NULL;
    if (!dpy)
        return "open_failed";
    Atom names = XInternAtom(dpy, "_XKB_RULES_NAMES", True);
    Atom actual = None;
    int format = 0;
    unsigned long n = 0, after = 0;
    unsigned char *data = NULL;
    if (names == None
        || XGetWindowProperty(dpy, DefaultRootWindow(dpy), names, 0, 1024, False, XA_STRING, &actual, &format, &n,
                              &after, &data) != Success
        || actual == None || !data) {
        if (data)
            XFree(data);
        XCloseDisplay(dpy);
        return "no_property";
    }
    /* Five NUL-separated fields: rules, model, layouts, variants, options. */
    const char *field[5] = { "", "", "", "", "" };
    unsigned long at = 0;
    for (int k = 0; k < 5 && at < n; k++) {
        field[k] = (const char *)data + at;
        at += strlen(field[k]) + 1;
    }
    XkbStateRec st;
    int group = XkbGetState(dpy, XkbUseCoreKbd, &st) == Success ? st.group : 0;
    SET(r->rules, field[0]);
    SET(r->model, field[1]);
    nth_item(field[2], group, r->layout, sizeof r->layout);
    nth_item(field[3], group, r->variant, sizeof r->variant);
    SET(r->options, field[4]);
    XFree(data);
    XCloseDisplay(dpy);
    return r->layout[0] ? "ok" : "no_layout";
#endif
}

/* The first xkb source in gsettings' printout of an a(ss) value ("layout" or "layout+variant"); 0 when none. */
static int gnome_first_xkb(const char *text, char *out, size_t cap)
{
    const char *p = text;
    while ((p = strstr(p, "'xkb'")) != NULL) {
        p += 5;
        const char *q = strchr(p, '\'');
        const char *end = q ? strchr(q + 1, '\'') : NULL;
        if (!end)
            return 0;
        size_t len = (size_t)(end - q - 1);
        if (len && len < cap) {
            memcpy(out, q + 1, len);
            out[len] = '\0';
            return 1;
        }
        p = end + 1;
    }
    return 0;
}

/* gsettings' printout of an as value ("['a', 'b']", or "@as []"), joined with commas. */
static void gnome_list(const char *text, char *out, size_t cap)
{
    size_t len = 0;
    out[0] = '\0';
    for (const char *p = strchr(text, '\''); p;) {
        const char *end = strchr(p + 1, '\'');
        if (!end)
            break;
        size_t n = (size_t)(end - p - 1);
        if (len + n + 2 > cap)
            break;
        if (len)
            out[len++] = ',';
        memcpy(out + len, p + 1, n);
        len += n;
        out[len] = '\0';
        p = strchr(end + 1, '\'');
    }
}

/* GNOME keeps the most recently used input source first in mru-sources. */
static const char *from_gnome(rmlvo *r)
{
    char out[2048], id[128];
    char *mru[] = { "gsettings", "get", "org.gnome.desktop.input-sources", "mru-sources", NULL };
    char *all[] = { "gsettings", "get", "org.gnome.desktop.input-sources", "sources", NULL };
    char *opts[] = { "gsettings", "get", "org.gnome.desktop.input-sources", "xkb-options", NULL };
    const char *why = capture(mru, out, sizeof out);
    if (strcmp(why, "ok") != 0)
        return why;
    if (!gnome_first_xkb(out, id, sizeof id)) {
        why = capture(all, out, sizeof out);
        if (strcmp(why, "ok") != 0)
            return why;
        if (!gnome_first_xkb(out, id, sizeof id))
            return "no_xkb_source";
    }
    char *plus = strchr(id, '+');
    if (plus) {
        *plus = '\0';
        SET(r->variant, plus + 1);
    }
    SET(r->layout, id);
    if (strcmp(capture(opts, out, sizeof out), "ok") == 0)
        gnome_list(out, r->options, sizeof r->options);
    return "ok";
}

/* KDE's kxkbrc lists the layouts; the current one is not stored there, so the first is taken. */
static const char *from_kde(rmlvo *r)
{
    char path[1024];
    const char *xdg = getenv("XDG_CONFIG_HOME"), *home = getenv("HOME");
    if (xdg && *xdg)
        snprintf(path, sizeof path, "%s/kxkbrc", xdg);
    else if (home && *home)
        snprintf(path, sizeof path, "%s/.config/kxkbrc", home);
    else
        return "no_home";
    FILE *f = fopen(path, "r");
    if (!f)
        return "no_file";
    char line[512], layouts[256] = "", variants[256] = "", use[16] = "";
    int in_layout = 0;
    while (fgets(line, sizeof line, f)) {
        char *s = trim(line);
        if (*s == '[') {
            in_layout = strcmp(s, "[Layout]") == 0;
            continue;
        }
        char *eq = strchr(s, '=');
        if (!in_layout || !eq)
            continue;
        *eq = '\0';
        char *key = trim(s), *value = trim(eq + 1);
        if (strcmp(key, "LayoutList") == 0)
            SET(layouts, value);
        else if (strcmp(key, "VariantList") == 0)
            SET(variants, value);
        else if (strcmp(key, "Options") == 0)
            SET(r->options, value);
        else if (strcmp(key, "Model") == 0)
            SET(r->model, value);
        else if (strcmp(key, "Use") == 0)
            SET(use, value);
    }
    fclose(f);
    if (strcmp(use, "false") == 0)
        return "not_in_use";
    nth_item(layouts, 0, r->layout, sizeof r->layout);
    nth_item(variants, 0, r->variant, sizeof r->variant);
    return r->layout[0] ? "ok" : "no_layout";
}

/* The system default (systemd-localed): what a session uses when it sets nothing of its own. */
static const char *from_localectl(rmlvo *r)
{
    char out[2048], layouts[256] = "", variants[256] = "";
    char *argv[] = { "localectl", "status", NULL };
    const char *why = capture(argv, out, sizeof out);
    if (strcmp(why, "ok") != 0)
        return why;
    for (char *line = strtok(out, "\n"); line; line = strtok(NULL, "\n")) {
        char *colon = strchr(line, ':');
        if (!colon)
            continue;
        *colon = '\0';
        char *key = trim(line), *value = trim(colon + 1);
        if (strcmp(key, "X11 Layout") == 0)
            SET(layouts, value);
        else if (strcmp(key, "X11 Variant") == 0)
            SET(variants, value);
        else if (strcmp(key, "X11 Model") == 0)
            SET(r->model, value);
        else if (strcmp(key, "X11 Options") == 0)
            SET(r->options, value);
    }
    nth_item(layouts, 0, r->layout, sizeof r->layout);
    nth_item(variants, 0, r->variant, sizeof r->variant);
    return r->layout[0] ? "ok" : "no_layout";
}

/* Prints the attempt; a failed one leaves nothing behind in r. */
static int try_source(const char *name, const char *result, rmlvo *r)
{
    char fields[200];
    snprintf(fields, sizeof fields, "\"event\":\"layout_source\",\"source\":\"%s\",\"result\":\"%s\"", name, result);
    probe_emit(fields);
    if (strcmp(result, "ok") != 0) {
        memset(r, 0, sizeof *r);
        return 0;
    }
    r->source = name;
    return 1;
}

static int learn_layout(const char *arg, rmlvo *r)
{
    memset(r, 0, sizeof *r);
    if (arg)
        return try_source("argument", from_argument(arg, r), r);
    const char *kind = probe_session_kind(getenv("WAYLAND_DISPLAY"), getenv("XDG_SESSION_TYPE"), getenv("DISPLAY"));
    const char *desktop = getenv("XDG_CURRENT_DESKTOP");
    if (strcmp(kind, "x11") == 0 && try_source("x11:_XKB_RULES_NAMES", from_x11(r), r))
        return 1;
    if (desktop && strstr(desktop, "GNOME") && try_source("gnome:org.gnome.desktop.input-sources", from_gnome(r), r))
        return 1;
    if (desktop && strstr(desktop, "KDE") && try_source("kde:kxkbrc", from_kde(r), r))
        return 1;
    return try_source("localectl", from_localectl(r), r);
}

static struct xkb_context *g_ctx;
static struct xkb_keymap *g_map;
static struct xkb_compose_table *g_compose; /* NULL: no compose table for the locale */
static char g_name[140];                    /* "de(nodeadkeys)", for the untyped reasons */
static char g_locale[80];

#define MAX_SEQ 4 /* the longest dead-key or compose sequence the search takes */

typedef struct {
    int code, mods; /* an evdev key code, and MOD_SHIFT | MOD_ALTGR */
} step;

typedef struct {
    const char *path; /* "direct", "dead" or "compose" */
    int n;
    step s[MAX_SEQ];
} route;

/* The first key, in code order, that gives the character cp (when sym is 0) or the keysym sym, with no modifier,
   then Shift, then AltGr, then both. A modifier's own key is skipped only while it is held, so a Multi_key on the
   right Alt (compose:ralt) is still found with nothing held. */
static int find_step(uint32_t cp, xkb_keysym_t sym, step *out)
{
    static const int combos[4] = { 0, MOD_SHIFT, MOD_ALTGR, MOD_SHIFT | MOD_ALTGR };
    xkb_keycode_t lo = xkb_keymap_min_keycode(g_map), hi = xkb_keymap_max_keycode(g_map);
    if (lo < 9)
        lo = 9; /* evdev code 1 */
    if (hi > 255)
        hi = 255; /* evdev code 247, the last the device declares */
    for (int c = 0; c < 4; c++) {
        struct xkb_state *st = xkb_state_new(g_map);
        if (!st)
            return 0;
        if (combos[c] & MOD_SHIFT)
            xkb_state_update_key(st, KEY_LEFTSHIFT + 8, XKB_KEY_DOWN);
        if (combos[c] & MOD_ALTGR)
            xkb_state_update_key(st, KEY_RIGHTALT + 8, XKB_KEY_DOWN);
        for (xkb_keycode_t kc = lo; kc <= hi; kc++) {
            if (((combos[c] & MOD_SHIFT) && kc == KEY_LEFTSHIFT + 8) || ((combos[c] & MOD_ALTGR) && kc == KEY_RIGHTALT + 8))
                continue;
            int match = sym ? xkb_state_key_get_one_sym(st, kc) == sym : xkb_state_key_get_utf32(st, kc) == cp;
            if (match) {
                out->code = (int)kc - 8;
                out->mods = combos[c];
                xkb_state_unref(st);
                return 1;
            }
        }
        xkb_state_unref(st);
    }
    return 0;
}

/* Whether a compose table entry produces cp (want is cp as UTF-8). */
static int entry_makes(struct xkb_compose_table_entry *e, uint32_t cp, const char *want)
{
    const char *utf8 = xkb_compose_table_entry_utf8(e);
    if (utf8 && *utf8)
        return strcmp(utf8, want) == 0;
    return xkb_keysym_to_utf32(xkb_compose_table_entry_keysym(e)) == cp;
}

/* A direct key; else the shortest compose table entry producing cp whose every keysym the layout can type, a
   dead-key sequence before a Multi_key one of the same length, else the first one found. */
static int find_route(uint32_t cp, route *r)
{
    if (find_step(cp, 0, &r->s[0])) {
        r->path = "direct";
        r->n = 1;
        return 1;
    }
    if (!g_compose)
        return 0;
    char want[8];
    probe_utf8_encode(cp, want);
    int found = 0;
    struct xkb_compose_table_iterator *it = xkb_compose_table_iterator_new(g_compose);
    struct xkb_compose_table_entry *e;
    while (it && (e = xkb_compose_table_iterator_next(it)) != NULL) {
        if (!entry_makes(e, cp, want))
            continue;
        size_t len = 0;
        const xkb_keysym_t *seq = xkb_compose_table_entry_sequence(e, &len);
        if (len == 0 || len > MAX_SEQ)
            continue;
        const char *path = seq[0] == XKB_KEY_Multi_key ? "compose" : "dead";
        if (found && ((int)len > r->n
                      || ((int)len == r->n && (strcmp(path, "compose") == 0 || strcmp(r->path, "dead") == 0))))
            continue;
        route c;
        c.path = path;
        c.n = (int)len;
        int ok = 1;
        for (size_t k = 0; k < len && ok; k++)
            ok = find_step(0, seq[k], &c.s[k]);
        if (ok) {
            *r = c;
            found = 1;
        }
    }
    if (it)
        xkb_compose_table_iterator_free(it);
    return found;
}

static const char *mods_json(int mods)
{
    return mods == (MOD_SHIFT | MOD_ALTGR) ? "\"Shift\",\"AltGr\""
           : mods == MOD_SHIFT             ? "\"Shift\""
           : mods == MOD_ALTGR             ? "\"AltGr\""
                                           : "";
}

/* One step: its modifiers down, the key down and up, the modifiers up. The compositor composes a dead-key or
   Multi_key sequence from the steps as it would from a real keyboard. */
static int press(const step *s)
{
    int ok = 1;
    if (s->mods & MOD_SHIFT)
        ok &= probe_uinput_key(KEY_LEFTSHIFT, 1) == 0;
    if (s->mods & MOD_ALTGR)
        ok &= probe_uinput_key(KEY_RIGHTALT, 1) == 0;
    ok &= probe_uinput_key((unsigned short)s->code, 1) == 0;
    ok &= probe_uinput_key((unsigned short)s->code, 0) == 0;
    if (s->mods & MOD_ALTGR)
        ok &= probe_uinput_key(KEY_RIGHTALT, 0) == 0;
    if (s->mods & MOD_SHIFT)
        ok &= probe_uinput_key(KEY_LEFTSHIFT, 0) == 0;
    return ok;
}
#endif

static int prepare(const char *arg)
{
#ifndef PROBE_HAVE_XKB
    (void)arg;
    probe_emit("\"event\":\"text_failed\",\"step\":\"not_built\",\"need\":\"libxkbcommon\"");
    fputs("probe_spawn text: built without libxkbcommon; nothing was typed\n", stderr);
    return 1;
#else
    if (geteuid() == 0)
        probe_emit("\"event\":\"layout_note\",\"note\":\"running as root: the desktop user's settings and display may "
                   "be out of reach; run as that user with /dev/uinput opened to it\"");
    rmlvo r;
    char a[160], b[160], c[160], d[160], e[400], fields[1200];
    if (!learn_layout(arg, &r)) {
        const char *desktop = getenv("XDG_CURRENT_DESKTOP");
        probe_json_string(a, sizeof a, desktop ? desktop : "");
        snprintf(fields, sizeof fields, "\"event\":\"layout_unknown\",\"session\":\"%s\",\"desktop\":%s",
                 probe_session_kind(getenv("WAYLAND_DISPLAY"), getenv("XDG_SESSION_TYPE"), getenv("DISPLAY")), a);
        probe_emit(fields);
        fputs("probe_spawn text: could not learn the session's keyboard layout (pass --layout); nothing was typed\n",
              stderr);
        return 1;
    }
    const char *rules = r.rules[0] ? r.rules : "evdev";
    probe_json_string(a, sizeof a, rules);
    probe_json_string(b, sizeof b, r.model);
    probe_json_string(c, sizeof c, r.layout);
    probe_json_string(d, sizeof d, r.variant);
    probe_json_string(e, sizeof e, r.options);
    snprintf(fields, sizeof fields,
             "\"event\":\"layout\",\"source\":\"%s\",\"rules\":%s,\"model\":%s,\"layout\":%s,\"variant\":%s,"
             "\"options\":%s",
             r.source, a, b, c, d, e);
    probe_emit(fields);
    if (r.variant[0])
        snprintf(g_name, sizeof g_name, "%s(%s)", r.layout, r.variant);
    else
        snprintf(g_name, sizeof g_name, "%s", r.layout);
    struct xkb_rule_names names = { rules, r.model[0] ? r.model : NULL, r.layout, r.variant[0] ? r.variant : NULL,
                                    r.options[0] ? r.options : NULL };
    g_ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    g_map = g_ctx ? xkb_keymap_new_from_names(g_ctx, &names, XKB_KEYMAP_COMPILE_NO_FLAGS) : NULL;
    if (!g_map) {
        snprintf(fields, sizeof fields, "\"event\":\"text_failed\",\"step\":\"keymap\",\"layout\":%s,\"variant\":%s", c, d);
        probe_emit(fields);
        fprintf(stderr, "probe_spawn text: xkbcommon could not compile layout %s; nothing was typed\n", g_name);
        return 1;
    }
    /* The dead-key and compose sequences: the locale's compose table, as the session's own input stack loads it.
       xkbcommon itself honors XCOMPOSEFILE and ~/.XCompose. */
    static const char *const vars[] = { "LC_ALL", "LC_CTYPE", "LANG" };
    const char *loc = NULL;
    for (int k = 0; k < 3 && !loc; k++) {
        const char *v = getenv(vars[k]);
        if (v && *v)
            loc = v;
    }
    snprintf(g_locale, sizeof g_locale, "%s", loc ? loc : "C");
    g_compose = xkb_compose_table_new_from_locale(g_ctx, g_locale, XKB_COMPOSE_COMPILE_NO_FLAGS);
    probe_json_string(a, sizeof a, g_locale);
    snprintf(fields, sizeof fields, "\"event\":\"compose\",\"locale\":%s,\"table\":\"%s\"", a, g_compose ? "ok" : "none");
    probe_emit(fields);
    return 0;
#endif
}

static int plan(uint32_t cp, char *detail, size_t cap)
{
#ifdef PROBE_HAVE_XKB
    route r;
    if (find_route(cp, &r)) {
        if (strcmp(r.path, "direct") == 0) {
            snprintf(detail, cap, "\"path\":\"direct\",\"key\":%d,\"mods\":[%s]", r.s[0].code, mods_json(r.s[0].mods));
            return 1;
        }
        size_t len = (size_t)snprintf(detail, cap, "\"path\":\"%s\",\"keys\":[", r.path);
        for (int k = 0; k < r.n && len < cap; k++)
            len += (size_t)snprintf(detail + len, cap - len, "%s{\"key\":%d,\"mods\":[%s]}", k ? "," : "",
                                    r.s[k].code, mods_json(r.s[k].mods));
        if (len < cap)
            snprintf(detail + len, cap - len, "]");
        return 1;
    }
    if (!g_compose)
        snprintf(detail, cap,
                 "no key in layout %s gives it, and there is no compose table for locale %s, so no dead-key or compose "
                 "sequence could be tried",
                 g_name, g_locale);
    else
        snprintf(detail, cap, "no key, dead-key sequence or compose sequence in layout %s gives it", g_name);
    return 0;
#else
    (void)cp;
    snprintf(detail, cap, "no layout");
    return 0;
#endif
}

static int open_device(void)
{
    return probe_uinput_open(NULL, 0);
}

static int type_char(uint32_t cp)
{
#ifdef PROBE_HAVE_XKB
    route r;
    if (!find_route(cp, &r))
        return 0;
    int ok = 1;
    for (int k = 0; k < r.n; k++)
        ok &= press(&r.s[k]);
    if (!ok) {
        char fields[160];
        snprintf(fields, sizeof fields, "\"event\":\"send_failed\",\"cp\":\"U+%04X\",\"error\":%d", (unsigned)cp, errno);
        probe_emit(fields);
    }
    return ok;
#else
    (void)cp;
    return 0;
#endif
}

static const char *arg_utf8(int argc, char **argv, int i)
{
    (void)argc;
    return argv[i];
}

const probe_text_platform probe_text_platform_impl = { arg_utf8, prepare, plan, open_device, type_char, sleep_ms,
                                                       probe_uinput_close };

int probe_text(int argc, char **argv)
{
    return probe_text_main(argc, argv, &probe_text_platform_impl);
}
```

- [ ] **Step 5: Find libxkbcommon in the build, and install it and the XKB data in the check image**

In `tools/overlay-phase0/CMakeLists.txt`, replace:

```cmake
    message(STATUS "probe_spawn pip: no libX11, so pip lists no windows")
  endif ()
endif ()
```

with:

```cmake
    message(STATUS "probe_spawn pip: no libX11, so pip lists no windows")
  endif ()
  # Optional: without libxkbcommon (1.6 or later, for the compose table iterator), text learns no layout and types
  # nothing; it says so.
  find_package(PkgConfig)
  if (PKG_CONFIG_FOUND)
    pkg_check_modules(XKBCOMMON QUIET IMPORTED_TARGET xkbcommon>=1.6)
  endif ()
  if (XKBCOMMON_FOUND)
    target_link_libraries(probe_spawn PRIVATE PkgConfig::XKBCOMMON)
    target_compile_definitions(probe_spawn PRIVATE PROBE_HAVE_XKB=1)
    message(STATUS "probe_spawn text: keyboard layouts built (xkbcommon ${XKBCOMMON_VERSION})")
  else ()
    message(STATUS "probe_spawn text: no libxkbcommon, so text types nothing on Linux")
  endif ()
endif ()
```

In `tools/overlay-phase0/ci/Dockerfile.linux`, replace
`      build-essential cmake pkg-config libsdl3-dev libx11-dev python3 nodejs \`
with
`      build-essential cmake pkg-config libsdl3-dev libx11-dev libxkbcommon-dev xkb-data python3 nodejs \`,
and in its first comment replace `stops below it) and libX11, python3 for` with
`stops below it), libX11, libxkbcommon and the XKB data, python3 for`.

In `tools/overlay-phase0/ci/linux-check.sh`, after `echo "x11 $(pkg-config --modversion x11)"` add:

```sh
echo "xkbcommon $(pkg-config --modversion xkbcommon)"
```

In `tools/overlay-phase0/README.md`, replace the Linux build line Task 3 wrote:

```
Linux (the distro's SDL3 and libX11 development packages: Debian and Ubuntu `libsdl3-dev libx11-dev`, Fedora
`SDL3-devel libX11-devel`; without libX11, `pip` lists no windows and says so):
```

with:

```
Linux (the distro's SDL3, libX11 and libxkbcommon 1.6 or later development packages and the XKB data: Debian and
Ubuntu `libsdl3-dev libx11-dev libxkbcommon-dev xkb-data`, Fedora `SDL3-devel libX11-devel libxkbcommon-devel
xkeyboard-config`; the compose tables come with libX11's data. Without libX11 `pip` lists no windows, and without
libxkbcommon `text` types nothing, and each says so):
```

- [ ] **Step 6: Run the Linux container check**

In its own call:
`python C:/Users/jscha/.claude/hooks/box_queue.py request --session <session id> --work "overlay kit: Linux check image build (Task 5)" --overlap ok --touches workdir:c:/users/jscha/claudescratch/wt-streamflex-overlay-spec --big`

Then, in the next call:
`docker build -t cptk-overlay-p0-linux:local -f C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec/tools/overlay-phase0/ci/Dockerfile.linux C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec/tools/overlay-phase0/ci`

In its own call:
`python C:/Users/jscha/.claude/hooks/box_queue.py request --session <session id> --work "overlay kit: Linux check run (Task 5)" --overlap ok --touches workdir:c:/users/jscha/claudescratch/wt-streamflex-overlay-spec --big`

Then, in the next call:
`docker run --rm -v "C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec:/src:ro" cptk-overlay-p0-linux:local bash /src/tools/overlay-phase0/ci/linux-check.sh`

Expected: an `xkbcommon` version in the toolchain section; `probe_spawn text: keyboard layouts built` in the
configure log; `RESULT: build exit 0 (0 error(s), ...), tests exit 0` with no warning from `text_linux.c` or
`inject_linux.c`; `TextDryRunTest` (3) and `TextLinuxTest` (14) pass, `test_a_stalled_source_is_named_and_skipped`
among them after its 10 s stall window; the `KeysTest` tests from `test_spawn.py` still pass. If
`test_no_compose_table_for_the_locale` reads `table` `ok`, this libxkbcommon falls back to a default table for an
unknown locale: stop and report rather than loosen the test, since then the line's `none` can never appear.

- [ ] **Step 7: Confirm Windows is unchanged**

Run: `cmake --build C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec/build-phase0 --config Release`
Then: `$env:PROBE_SPAWN = "C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec/build-phase0/Release/probe_spawn.exe"; python -m unittest discover -s C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec/tools/overlay-phase0/tests -p test_text.py -v`
Expected: OK; the fourteen Linux tests are skipped.

- [ ] **Step 8: Commit**

```powershell
git -C "C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec" add tools/overlay-phase0/src/uinput_linux.h tools/overlay-phase0/src/inject_linux.c tools/overlay-phase0/src/text_linux.c tools/overlay-phase0/CMakeLists.txt tools/overlay-phase0/ci/Dockerfile.linux tools/overlay-phase0/ci/linux-check.sh tools/overlay-phase0/README.md tools/overlay-phase0/tests/test_text.py
git -C "C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec" commit -m "tools: probe_spawn text on Linux types through uinput with the session's layout, dead keys and compose, and names what it cannot type"
```

---

### Task 6: The React text probe page

**Files:**
- Create: `tools/overlay-phase0/pages/text.html`, `tools/overlay-phase0/pages/text-g18.txt`
- Create: `tools/overlay-phase0/pages/vendor/react-18.3.1/react.production.min.js`,
  `tools/overlay-phase0/pages/vendor/react-18.3.1/react-dom.production.min.js`,
  `tools/overlay-phase0/pages/vendor/react-18.3.1/LICENSE`, `tools/overlay-phase0/pages/vendor/react-18.3.1/README.md`
- Modify: `tools/overlay-phase0/pages/serve.ps1` (serve the vendor folder's scripts)
- Modify: `.gitattributes` (the vendored files and the G18 string keep their bytes)
- Modify: `tools/overlay-phase0/make-kit.ps1:1-7` (comment), `tools/overlay-phase0/tests/test_make_kit.py`
- Modify: `tools/overlay-phase0/README.md:58` (the `pages/` row)
- Create: `tools/overlay-phase0/tests/test_pages.py`

**Interfaces:**
- Consumes: nothing from earlier tasks (the page is driven by Task 4's and Task 5's `text`).
- Produces: `http://127.0.0.1:8765/text.html`, whose report rows are `value: <JSON>`, `state: <JSON>`,
  `state equals value: yes|no`, `code points: U+...`, `events: N (keydown n, beforeinput n, input n, keyup n);
  untrusted: N`, `G18 characters missing: ...`, `characters not in G18: ...`, a `<pre id="report">` JSON object
  `{value, state, equal, events, untrusted, missing, extra}`, and an event list (`type`, `trusted`, `key`, `data`,
  `inputType`). `pages/text-g18.txt` is the 156-character G18 string, UTF-8, no byte order mark, no line ending.

- [ ] **Step 1: Write the failing tests**

`tools/overlay-phase0/tests/test_pages.py`:

```python
"""Tests for the kit's pages: the React text probe, its vendored React, and serve.ps1."""
import hashlib
import http.client
import json
import os
import pathlib
import re
import shutil
import socket
import subprocess
import tempfile
import time
import unittest

KIT = pathlib.Path(__file__).resolve().parent.parent
PAGES = KIT / "pages"
VENDOR = PAGES / "vendor" / "react-18.3.1"
SCRIPTS = ["vendor/react-18.3.1/react.production.min.js", "vendor/react-18.3.1/react-dom.production.min.js"]


def page():
    return (PAGES / "text.html").read_text(encoding="utf-8")


class TextPageTest(unittest.TestCase):
    def test_loads_only_its_vendored_scripts(self):
        html = page()
        self.assertEqual(re.findall(r'<script[^>]*\ssrc="([^"]+)"', html), SCRIPTS)
        for src in SCRIPTS:
            self.assertTrue((PAGES / src).is_file(), src)
        self.assertNotRegex(html, r"https?://")
        for api in ("fetch(", "XMLHttpRequest", "WebSocket", "EventSource", "sendBeacon", "import("):
            self.assertNotIn(api, html, api)

    def test_its_policy_forbids_every_network_request(self):
        m = re.search(r'<meta http-equiv="Content-Security-Policy" content="([^"]+)">', page())
        self.assertIsNotNone(m)
        policy = [d.strip() for d in m.group(1).split(";")]
        self.assertIn("default-src 'none'", policy)
        self.assertIn("script-src 'self' 'unsafe-inline'", policy)
        self.assertNotIn("connect-src", m.group(1))  # default-src 'none' already covers every request

    def test_the_g18_string_is_the_kit_file(self):
        m = re.search(r'<script type="application/json" id="g18">(.*?)</script>', page(), re.S)
        expected = json.loads(m.group(1))
        raw = (PAGES / "text-g18.txt").read_bytes()
        self.assertFalse(raw.startswith(b"\xef\xbb\xbf"))
        self.assertFalse(raw.endswith(b"\n"))
        self.assertEqual(raw.decode("utf-8"), expected)
        self.assertEqual(len(expected), 156)
        self.assertEqual([c for c in expected if ord(c) < 0x20 or 0x7F <= ord(c) <= 0x9F], [])
        self.assertEqual(hashlib.sha256(raw).hexdigest(),
                         "f48989965246e2739c6e34e7e0b68e4106793bcfec1b1446671cea401edd92f6")

    @unittest.skipUnless(shutil.which("node"), "needs node")
    def test_its_inline_script_parses(self):
        scripts = re.findall(r"<script>(.*?)</script>", page(), re.S)
        self.assertEqual(len(scripts), 1)
        with tempfile.TemporaryDirectory() as tmp:
            path = os.path.join(tmp, "inline.js")
            pathlib.Path(path).write_text(scripts[0], encoding="utf-8")
            proc = subprocess.run(["node", "--check", path], capture_output=True, text=True, timeout=120)
        self.assertEqual(proc.returncode, 0, proc.stderr)


class VendoredReactTest(unittest.TestCase):
    def recorded(self):
        table = (VENDOR / "README.md").read_text(encoding="utf-8")
        return dict(re.findall(r"^\| `([^`]+)` \|.*\| `([0-9a-f]{64})` \|$", table, re.M))

    def test_every_file_matches_its_recorded_sha256(self):
        recorded = self.recorded()
        present = {p.name for p in VENDOR.iterdir() if p.is_file() and p.name != "README.md"}
        self.assertEqual(set(recorded), present)
        for name, digest in recorded.items():
            self.assertEqual(hashlib.sha256((VENDOR / name).read_bytes()).hexdigest(), digest, name)

    def test_the_license_is_mit(self):
        self.assertTrue((VENDOR / "LICENSE").read_text(encoding="utf-8").startswith("MIT License"))


def free_port():
    with socket.socket() as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]


@unittest.skipUnless(os.name == "nt" and shutil.which("powershell"), "Windows PowerShell only")
class ServeTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.port = free_port()
        cls.proc = subprocess.Popen(["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File",
                                     str(PAGES / "serve.ps1"), "-Port", str(cls.port)],
                                    stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True)
        end = time.monotonic() + 120
        while True:
            try:
                with socket.create_connection(("127.0.0.1", cls.port), timeout=5):
                    return
            except OSError:
                if cls.proc.poll() is not None:
                    raise AssertionError("serve.ps1 exited before it listened: " + cls.proc.stderr.read())
                if time.monotonic() > end:
                    cls.proc.kill()
                    raise AssertionError("HUNG: serve.ps1 never listened on port %d" % cls.port)
                time.sleep(0.1)

    @classmethod
    def tearDownClass(cls):
        cls.proc.kill()
        cls.proc.wait(timeout=120)
        cls.proc.stderr.close()

    def get(self, path):
        conn = http.client.HTTPConnection("127.0.0.1", self.port, timeout=120)
        try:
            conn.request("GET", path)
            r = conn.getresponse()
            return r.status, r.getheader("Content-Type"), r.read()
        finally:
            conn.close()

    def test_serves_the_page_and_its_scripts(self):
        status, kind, body = self.get("/text.html")
        self.assertEqual((status, kind), (200, "text/html; charset=utf-8"))
        self.assertEqual(body, (PAGES / "text.html").read_bytes())
        for src in SCRIPTS:
            status, kind, body = self.get("/" + src)
            self.assertEqual((status, kind), (200, "text/javascript; charset=utf-8"), src)
            self.assertEqual(body, (PAGES / src).read_bytes(), src)

    def test_the_root_is_still_the_probe_page(self):
        status, _, body = self.get("/")
        self.assertEqual(status, 200)
        self.assertEqual(body, (PAGES / "probe.html").read_bytes())

    def test_refuses_everything_else(self):
        for path in ("/serve.ps1", "/vendor/react-18.3.1/LICENSE", "/vendor/react-18.3.1/README.md",
                     "/text-g18.txt", "/../setup/windows.ps1", "/vendor/../serve.ps1", "/a/b/text.html",
                     "/nosuch.html"):
            with self.subTest(path=path):
                self.assertEqual(self.get(path)[0], 404)
```

In `tools/overlay-phase0/tests/test_make_kit.py`, add to `SOURCE_FILES`:

```python
    "pages/text.html": "text",
    "pages/text-g18.txt": "g18",
    "pages/vendor/react-18.3.1/react.production.min.js": "react",
```

add to `KIT_FILES`: `"pages/text.html", "pages/text-g18.txt", "pages/vendor/react-18.3.1/react.production.min.js",`,
and in `test_copies_the_kit_layout`, after `self.assertIn("  extension\\src\\content.js", lines)` add:

```python
        self.assertIn("  pages\\vendor\\react-18.3.1\\react.production.min.js", lines)
```

- [ ] **Step 2: Run the tests to see them fail**

Run: `python -m unittest discover -s C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec/tools/overlay-phase0/tests -p test_pages.py -v`
Expected: ERROR in every `TextPageTest`, `VendoredReactTest` and `ServeTest` test (`text.html`, the vendor folder
and the scripts do not exist). `test_make_kit.py` passes already (make-kit copies `pages\` whole), which is what
pins that it ships the page and React.

- [ ] **Step 3: Vendor React 18.3.1**

Download into a new, empty folder outside the repo, check the registry's integrity, then copy the three files in:

```powershell
$dl = 'C:/Users/jscha/ClaudeScratch/react-18.3.1-download'
New-Item -ItemType Directory -Path $dl | Out-Null
foreach ($p in 'react', 'react-dom') {
    Invoke-WebRequest -UseBasicParsing -Uri "https://registry.npmjs.org/$p/-/$p-18.3.1.tgz" -OutFile "$dl/$p.tgz"
    New-Item -ItemType Directory -Path "$dl/$p" | Out-Null
    tar -xzf "$dl/$p.tgz" -C "$dl/$p"
    $sha = [Security.Cryptography.SHA512]::Create().ComputeHash([IO.File]::ReadAllBytes("$dl/$p.tgz"))
    "$p sha512-" + [Convert]::ToBase64String($sha)
}
```

Expected output:

```
react sha512-wS+hAgJShR0KhEvPJArfuPVN1+Hz1t0Y6n5jLrGQbkb4urgPE/0Rve+1kMB1v/oWgHgm4WIcV+i7F2pTVj+2iQ==
react-dom sha512-5m4nQKp+rZRb09LNH59GM4BxTh9251/ylbKIbpe7TpGxfJ+9kv6BLkLBXIjjspbgbnIBNqlI23tRnTWT0snUIw==
```

These equal `npm view react@18.3.1 dist.integrity` and `npm view react-dom@18.3.1 dist.integrity`. If either
differs, stop and report: do not vendor.

```powershell
$v = 'C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec/tools/overlay-phase0/pages/vendor/react-18.3.1'
New-Item -ItemType Directory -Path $v | Out-Null
Copy-Item -LiteralPath 'C:/Users/jscha/ClaudeScratch/react-18.3.1-download/react/package/umd/react.production.min.js' -Destination $v
Copy-Item -LiteralPath 'C:/Users/jscha/ClaudeScratch/react-18.3.1-download/react-dom/package/umd/react-dom.production.min.js' -Destination $v
Copy-Item -LiteralPath 'C:/Users/jscha/ClaudeScratch/react-18.3.1-download/react/package/LICENSE' -Destination $v
Get-ChildItem -LiteralPath $v | ForEach-Object { (Get-FileHash -Algorithm SHA256 -LiteralPath $_.FullName).Hash.ToLower() + '  ' + $_.Name }
```

Expected:

```
52412d7bc7ce4157ea628bbaacb8829e0a9cb3c58f57f99176126bc8cf2bfc85  LICENSE
35f4f974f4b2bcd44da73963347f8952e341f83909e4498227d4e26b98f66f0d  react-dom.production.min.js
d949f1c3687aedadcedac85261865f29b17cd273997e7f6b2bfc53b2f9d4c4dd  react.production.min.js
```

(Measured while writing this plan, 2026-11-03.) Then delete the download folder by name:
`Remove-Item -LiteralPath 'C:/Users/jscha/ClaudeScratch/react-18.3.1-download' -Recurse -Force`.

`tools/overlay-phase0/pages/vendor/react-18.3.1/README.md`:

```markdown
# Vendored React 18.3.1

The text probe page (`../../text.html`) loads these two files from the kit's own page server; nothing is fetched
from a network. They are the npm packages' UMD production builds, unchanged. React 18.3.1 is the last release with
UMD builds (React 19 ships none), and a UMD build runs from a plain `<script>` tag with no bundler; its
controlled-input path (`onChange` from the native `input` event) is the one React 19 keeps.

- **Upstream:** https://github.com/facebook/react, npm packages `react` and `react-dom`
- **Version:** 18.3.1
- **License:** MIT, in `LICENSE` (the same file in both packages)
- **Package integrity (npm):** `react-18.3.1.tgz`
  `sha512-wS+hAgJShR0KhEvPJArfuPVN1+Hz1t0Y6n5jLrGQbkb4urgPE/0Rve+1kMB1v/oWgHgm4WIcV+i7F2pTVj+2iQ==`;
  `react-dom-18.3.1.tgz`
  `sha512-5m4nQKp+rZRb09LNH59GM4BxTh9251/ylbKIbpe7TpGxfJ+9kv6BLkLBXIjjspbgbnIBNqlI23tRnTWT0snUIw==`

| File | From | sha256 |
|---|---|---|
| `react.production.min.js` | `react-18.3.1.tgz`, `package/umd/react.production.min.js` | `d949f1c3687aedadcedac85261865f29b17cd273997e7f6b2bfc53b2f9d4c4dd` |
| `react-dom.production.min.js` | `react-dom-18.3.1.tgz`, `package/umd/react-dom.production.min.js` | `35f4f974f4b2bcd44da73963347f8952e341f83909e4498227d4e26b98f66f0d` |
| `LICENSE` | `react-18.3.1.tgz`, `package/LICENSE` | `52412d7bc7ce4157ea628bbaacb8829e0a9cb3c58f57f99176126bc8cf2bfc85` |

`tests/test_pages.py` checks every file here against this table, and that the table names every file.
`.gitattributes` marks the folder `-text`, so no checkout rewrites a line ending.

## Updating

1. `npm view react@<version> dist.tarball dist.integrity` and the same for `react-dom`. A version without a
   `umd/` folder (any 19.x) cannot be vendored this way.
2. Download both tarballs into a new, empty folder outside the repo, check each against its `dist.integrity`
   (`sha512-` and the base64 SHA-512 of the `.tgz`), and extract them.
3. Replace the three files, rename this folder to the new version, and update the page's two `<script src>`
   lines, `serve.ps1` needs no change.
4. Update this file: the version, both integrities and the three sha256 values.
```

- [ ] **Step 4: Write the G18 string file**

The string is ASCII-escaped here so no editor can change its bytes; write it with Python:

```powershell
python -c "import json, pathlib; s = json.loads(pathlib.Path(r'C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec/tools/overlay-phase0/pages/g18.json').read_text(encoding='ascii')); pathlib.Path(r'C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec/tools/overlay-phase0/pages/text-g18.txt').write_bytes(s.encode('utf-8'))"
```

where `pages/g18.json` is a temporary file written first with the Write tool, holding exactly this one line (the
same literal the page embeds below):

```
"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 !@#$%^&*()-_=+[]{};:'\",./?\\|~<>`\u00e0\u00e1\u00e2\u00e4\u00e3\u00e5\u00e6\u00e7\u00c0\u00c1\u00c2\u00c4\u00c3\u00c5\u00c6\u00c7\u00e8\u00e9\u00ea\u00eb\u00ec\u00ed\u00ee\u00ef\u00c8\u00c9\u00ca\u00cb\u00cc\u00cd\u00ce\u00cf\u00f1\u00f2\u00f3\u00f4\u00f6\u00f5\u00f8\u0153\u00d1\u00d2\u00d3\u00d4\u00d6\u00d5\u00d8\u0152\u00f9\u00fa\u00fb\u00fc\u00fd\u00ff\u00df\u00d9\u00da\u00db\u00dc\u00dd\u0178"
```

Then delete it: `Remove-Item -LiteralPath 'C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec/tools/overlay-phase0/pages/g18.json'`.
The string is the lowercase and uppercase letters, the digits, a space, the on-screen keyboard's 32 punctuation
characters (`src/osk.c`'s symbol rows) and its *àé* page (61 characters, `ACCENTS_1` to `ACCENTS_4`, each row's
lowercase then its uppercase): 156 characters, sha256 `f48989965246e2739c6e34e7e0b68e4106793bcfec1b1446671cea401edd92f6`.

- [ ] **Step 5: Write the page**

`tools/overlay-phase0/pages/text.html` (the `g18` block is the same literal as Step 4's, on one line):

```html
<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta http-equiv="Content-Security-Policy" content="default-src 'none'; script-src 'self' 'unsafe-inline'; style-src 'unsafe-inline'">
<title>CPTK text probe</title>
<style>
  body { background: #202020; color: #eeeeee; font: 18px monospace; margin: 24px; }
  input { font: 28px monospace; width: 95%; padding: 8px; }
  .row { margin: 6px 0; white-space: pre-wrap; word-break: break-all; }
  .bad { color: #ff8080; }
  .good { color: #80ff80; }
  ol { font-size: 14px; }
</style>
</head>
<body>
<h1>CPTK text probe</h1>
<p>Click the field, then type through <code>probe_spawn text</code>. Reload to start again.</p>
<div id="root"></div>
<script type="application/json" id="g18">"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 !@#$%^&*()-_=+[]{};:'\",./?\\|~<>`\u00e0\u00e1\u00e2\u00e4\u00e3\u00e5\u00e6\u00e7\u00c0\u00c1\u00c2\u00c4\u00c3\u00c5\u00c6\u00c7\u00e8\u00e9\u00ea\u00eb\u00ec\u00ed\u00ee\u00ef\u00c8\u00c9\u00ca\u00cb\u00cc\u00cd\u00ce\u00cf\u00f1\u00f2\u00f3\u00f4\u00f6\u00f5\u00f8\u0153\u00d1\u00d2\u00d3\u00d4\u00d6\u00d5\u00d8\u0152\u00f9\u00fa\u00fb\u00fc\u00fd\u00ff\u00df\u00d9\u00da\u00db\u00dc\u00dd\u0178"</script>
<script src="vendor/react-18.3.1/react.production.min.js"></script>
<script src="vendor/react-18.3.1/react-dom.production.min.js"></script>
<script>
(function () {
  "use strict";
  const h = React.createElement;
  const G18 = JSON.parse(document.getElementById("g18").textContent);
  const TYPES = ["keydown", "beforeinput", "input", "keyup"];

  function codePoints(s) {
    return Array.from(s, (c) => "U+" + c.codePointAt(0).toString(16).toUpperCase().padStart(4, "0")).join(" ");
  }

  // The characters of want with no match left in have, each occurrence counted once, in want's order.
  function without(want, have) {
    const pool = Array.from(have);
    const out = [];
    for (const c of Array.from(want)) {
      const at = pool.indexOf(c);
      if (at < 0) out.push(c);
      else pool.splice(at, 1);
    }
    return out;
  }

  function show(list) {
    return list.length ? list.join(" ") + "  (" + codePoints(list.join("")) + ")" : "none";
  }

  function Probe() {
    const [state, setState] = React.useState("");
    const [fieldValue, setFieldValue] = React.useState("");
    const [log, setLog] = React.useState([]);
    const field = React.useRef(null);

    React.useEffect(() => {
      const el = field.current;
      el.focus();
      // Native listeners on the field itself, apart from React's own handling, so each event's isTrusted is the
      // browser's. The field's own value is read again once the browser and React have applied the event.
      function record(e) {
        setLog((old) => old.concat([{
          type: e.type,
          trusted: e.isTrusted,
          key: e.key === undefined ? null : e.key,
          data: e.data === undefined ? null : e.data,
          inputType: e.inputType === undefined ? null : e.inputType,
        }]));
        setTimeout(() => setFieldValue(el.value), 0);
      }
      for (const t of TYPES) el.addEventListener(t, record, true);
      return () => {
        for (const t of TYPES) el.removeEventListener(t, record, true);
      };
    }, []);

    const untrusted = log.filter((e) => !e.trusted).length;
    const counts = TYPES.map((t) => t + " " + log.filter((e) => e.type === t).length).join(", ");
    const equal = fieldValue === state;
    const missing = without(G18, state);
    const extra = without(state, G18);
    const report = { value: fieldValue, state: state, equal: equal, events: log.length, untrusted: untrusted,
                     missing: missing.join(""), extra: extra.join("") };
    return h("div", null,
      h("input", { ref: field, id: "field", value: state, autoComplete: "off", spellCheck: false,
                   onChange: (e) => setState(e.target.value) }),
      h("div", { className: "row" }, "value: " + JSON.stringify(fieldValue)),
      h("div", { className: "row" }, "state: " + JSON.stringify(state)),
      h("div", { className: "row " + (equal ? "good" : "bad") }, "state equals value: " + (equal ? "yes" : "no")),
      h("div", { className: "row" }, "code points: " + codePoints(fieldValue)),
      h("div", { className: "row " + (untrusted ? "bad" : "good") },
        "events: " + log.length + " (" + counts + "); untrusted: " + untrusted),
      h("div", { className: "row " + (missing.length ? "bad" : "good") }, "G18 characters missing: " + show(missing)),
      h("div", { className: "row " + (extra.length ? "bad" : "good") }, "characters not in G18: " + show(extra)),
      h("pre", { id: "report", className: "row" }, JSON.stringify(report)),
      h("ol", { reversed: true }, log.slice().reverse().map((e, i) => h("li", { key: log.length - i },
        e.type + " trusted=" + e.trusted + " key=" + JSON.stringify(e.key) + " data=" + JSON.stringify(e.data) +
        " inputType=" + e.inputType)))
    );
  }

  ReactDOM.createRoot(document.getElementById("root")).render(h(Probe));
})();
</script>
</body>
</html>
```

- [ ] **Step 6: Serve the vendor folder, keep its bytes, and document it**

In `tools/overlay-phase0/pages/serve.ps1`, replace:

```powershell
        $name = [IO.Path]::GetFileName($ctx.Request.Url.AbsolutePath)
        if (-not $name) { $name = 'probe.html' }
        $file = Join-Path $root $name
        if ($name -like '*.html' -and (Test-Path -LiteralPath $file)) {
            $bytes = [IO.File]::ReadAllBytes($file)
            $ctx.Response.ContentType = 'text/html; charset=utf-8'
            $ctx.Response.OutputStream.Write($bytes, 0, $bytes.Length)
        } else {
```

with:

```powershell
        $rel = $ctx.Request.Url.AbsolutePath.TrimStart('/')
        if (-not $rel) { $rel = 'probe.html' }
        # A page or script in pages\, or a script under vendor\<package>\; never a path that climbs out of pages\.
        $ok = ($rel -match '^(vendor/[A-Za-z0-9._-]+/)?[A-Za-z0-9_-][A-Za-z0-9._-]*\.(html|js)$') -and ($rel -notmatch '\.\.')
        $file = Join-Path $root ($rel -replace '/', '\')
        if ($ok -and (Test-Path -LiteralPath $file -PathType Leaf)) {
            $bytes = [IO.File]::ReadAllBytes($file)
            if ($rel -like '*.js') { $ctx.Response.ContentType = 'text/javascript; charset=utf-8' }
            else { $ctx.Response.ContentType = 'text/html; charset=utf-8' }
            $ctx.Response.OutputStream.Write($bytes, 0, $bytes.Length)
        } else {
```

and in its header comment replace `Serves tools/overlay-phase0/pages at http://127.0.0.1:8765/ for a Windows guest with no Python.`
with `Serves tools/overlay-phase0/pages (its pages, and the scripts under vendor\) at http://127.0.0.1:8765/ for a Windows guest with no Python.`

At the end of `.gitattributes` (repo root) add:

```
# The overlay probe kit's vendored React and the G18 string are recorded by hash; a checkout must not touch their bytes
tools/overlay-phase0/pages/vendor/** -text
tools/overlay-phase0/pages/text-g18.txt -text
```

In `tools/overlay-phase0/make-kit.ps1`'s header comment, replace `setup\, pages\, extension\ (src\ and build.py, never dist\), ids.json when it`
with `setup\, pages\ (the vendored React under pages\vendor\ included), extension\ (src\ and build.py, never dist\), ids.json when it`.

In `tools/overlay-phase0/README.md`, replace the `pages/` row with:

```
| `pages/` | `probe.html` (fullscreen and media-start targets), `keylog.html` (what a page sees), `text.html` (a React-controlled text field reporting its value, the React state and each event's `isTrusted`, for G18), `text-g18.txt` (G18's string), `vendor/react-18.3.1/` (React, vendored: its README records the version, license and hashes), `serve.ps1` (a page server for Windows guests). |
```

- [ ] **Step 7: Run the tests to see them pass**

Run: `python -m unittest discover -s C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec/tools/overlay-phase0/tests -p test_pages.py -v`
Expected: OK, 9 tests. If `ServeTest` fails because `serve.ps1` cannot listen (an `HttpListenerException` naming
access denied), stop and report: whether a non-elevated shell can listen on `127.0.0.1` is UNKNOWN (it was only
checked from an elevated one while this plan was written), and the fix would change the protocol's page server.

Run: `python -m unittest discover -s C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec/tools/overlay-phase0/tests -p test_make_kit.py -v`
Expected: OK, 5 tests.

Run: `git -C "C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec" check-attr text -- tools/overlay-phase0/pages/vendor/react-18.3.1/react.production.min.js tools/overlay-phase0/pages/text-g18.txt`
Expected: both read `text: unset`.

- [ ] **Step 8: Run the Linux container check**

In its own call:
`python C:/Users/jscha/.claude/hooks/box_queue.py request --session <session id> --work "overlay kit: Linux check run (Task 6)" --overlap ok --touches workdir:c:/users/jscha/claudescratch/wt-streamflex-overlay-spec --big`

Then, in the next call:
`docker run --rm -v "C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec:/src:ro" cptk-overlay-p0-linux:local bash /src/tools/overlay-phase0/ci/linux-check.sh`

Expected: `RESULT: build exit 0 ..., tests exit 0`; `TextPageTest` (4) and `VendoredReactTest` (2) pass on Linux
(node is in the image), and `ServeTest` is skipped. (The image is unchanged from Task 5, so no rebuild.)

- [ ] **Step 9: Commit**

```powershell
git -C "C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec" add .gitattributes tools/overlay-phase0/pages/text.html tools/overlay-phase0/pages/text-g18.txt tools/overlay-phase0/pages/vendor/react-18.3.1/react.production.min.js tools/overlay-phase0/pages/vendor/react-18.3.1/react-dom.production.min.js tools/overlay-phase0/pages/vendor/react-18.3.1/LICENSE tools/overlay-phase0/pages/vendor/react-18.3.1/README.md tools/overlay-phase0/pages/serve.ps1 tools/overlay-phase0/make-kit.ps1 tools/overlay-phase0/README.md tools/overlay-phase0/tests/test_pages.py tools/overlay-phase0/tests/test_make_kit.py
git -C "C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec" commit -m "tools: a React text probe page for G18, with React 18.3.1 vendored"
```

- [ ] **Step 10: Run the spelling gate over the branch before any push**

The pre-push hook runs the pinned American-spelling gate over every added line, and the vendored minified files are
added lines. Run the same pinned gate now, so a hit is found before a push is refused:

```powershell
$pin = Get-Content -Raw C:/Users/jscha/.claude/scripts/american-spelling-pin.json | ConvertFrom-Json
python C:/Users/jscha/.claude/state/american-spelling/$($pin.sha)/check-american-spelling.py --repo "C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec" --base origin/master --repo-slug bilbospocketses/streamflex
```

Expected: exit 0, no British word. (If the cached folder for the pinned SHA is missing, the first `git push` fetches
it; run the push's own check instead and read its output.) If the gate names a word inside a vendored file, stop
and report: a vendored file is never edited, and its exception goes through the public allow file in
`bilbospocketses/american-spelling`, which is the user's call.

---

### Task 7: The protocol, and the whole kit on both platforms

**Files:**
- Modify: `design/research/overlay/phase0/guest-protocol.md:240-294` (§G17 and §G18)
- Modify: `design/specs/2026-10-27-streaming-overlay-design.md:301-303` (**The launch path** step 5) and `:392-394`
  (the **Picture-in-picture** fallbacks)
- Modify: `design/plans/2026-10-27-overlay-phase0.md:11-14` (the "Added 2026-11-03" note)

**Interfaces:**
- Consumes: every line name and option from Tasks 2-6, exactly as their Interfaces blocks give them.
- Produces: the protocol text qa-harness-sf runs G17 step 4 and G18 from.

- [ ] **Step 1: Update §G17**

In `design/research/overlay/phase0/guest-protocol.md`, replace:

```
**Kit:** steps 1-3 need only the browsers and the Shaka demo. Step 4 needs a `probe_spawn pip` watch (report each
new top-level window of the launched browser's processes, and raise the main one), which the kit does not have yet;
until a kit task adds it, record step 4 as not run and the row as partial.
```

with:

```
**Kit:** steps 1-3 need only the browsers and the Shaka demo. Step 4 uses `probe_spawn pip`, which runs the browser
as `run` does (`close` and `kill` on its stdin work the same) and reports each new top-level window of the browser's
processes. When one looks like picture-in-picture (its title is *Picture in picture* or *Picture-in-Picture*, in
any case, or on X11 its `WM_WINDOW_ROLE` is `PictureInPicture`), it closes that window (`WM_CLOSE` on Windows, a
`_NET_CLOSE_WINDOW` request on X11, as the window's own close button would), which returns the video to the page,
then raises the browser's main window, the largest of its other windows. It acts on the browser's windows only,
never on the page. On Windows run it in the guest's interactive session, as G5.
```

Replace:

```
4. **The launcher's watch.** `probe_spawn pip <browser launch line>`, then open picture-in-picture as in step 2.
   **Read:** the watch's line for the new window and its raise of the main window, with the time from the window's
   appearance.
```

with:

```
4. **The launcher's watch.** `probe_spawn pip <browser launch line>` on the Shaka demo, play a clear asset, then
   open picture-in-picture as in step 2. **Read:** the `pip_session` line (what the watch can see on this desktop);
   each `window` line (`pid`, `title`, `class`, and on Linux `instance` and `role`), the picture-in-picture window's
   among them; its `pip_seen` line; the `pip_closed` line (`call_ok`, `method`, `ms`) and the `pip_gone` line (`ms`:
   when the window was actually gone); the `raised` line (`call_ok`, and `method`: which way the raise got through);
   the `foreground` line, whose `ms` is the time from the window's first appearance until the main window had the
   focus; and `pip_summary`. Every `ms` counts from the window's first appearance. Then record whether the video
   kept playing in the page after the window closed, or paused, or left the asset (screenshot). A
   picture-in-picture window the watch did not call one (`pip: false` in its `window` line) is recorded with its
   title and class: the rule needs that browser's.
```

Replace:

```
carry it); the watch sees the window and raises the main window
within 2000 ms on Windows and X11. On Wayland record what the watch can see, as data.
```

with:

```
carry it); the watch calls the picture-in-picture window one, closes it (`pip_gone` follows `pip_closed`), and its
`foreground` line reads `ms` of 2000 or less, on Windows and X11. Whether the video kept playing in the page is
recorded either way; it is not a pass condition. On Wayland record the `pip_session` line and every `window` line,
as data.
```

- [ ] **Step 2: Update §G18**

Replace:

```
**Kit:** this item needs a `probe_spawn text` command (each character as trusted keystrokes: `SendInput` with
`KEYEVENTF_UNICODE` on Windows, uinput with the key and Shift or AltGr the session's layout gives on Linux) and a
probe page with a React-controlled text field that reports its value, its React state and each event's
`isTrusted`. The kit has neither yet; until a kit task adds both, G18 is not run.
```

with:

```
**Kit:** `probe_spawn text` types each character as trusted keystrokes: `SendInput` with `KEYEVENTF_UNICODE` on
Windows, and on Linux uinput with the key and Shift or AltGr that the session's layout gives it, else the dead-key
or compose sequence that the locale's compose table gives it (each `char` line's `path`: `direct`, `dead` or
`compose`). A character with no path is named in an `untyped` line and a stderr line, and nothing is typed for it.
Its exit status is
0 when every character was typed, 3 when some were named untyped and the rest typed, 2 when the string was refused
before anything was typed, and 1 on a failure to set up or send. The page `http://127.0.0.1:8765/text.html` holds a
React-controlled text field and reports the field's value, the React state, each event's `isTrusted`, and which of
the string's characters are missing from the field. `pages/text-g18.txt` holds the string.
```

Replace:

```
1. Focus the page's text field and run `probe_spawn text 3000 <string>`, bringing the browser to the front within
   the 3 s, as in G5. The string holds lowercase and uppercase letters, digits, the keyboard's punctuation, and every
   character of the keyboard's *àé* page.
2. On Linux, record how `probe_spawn` learned the session's layout, and on Wayland whether it could at all.
```

with:

```
1. Open `http://127.0.0.1:8765/text.html` in a fresh profile (no token: the probe extension plays no part here)
   and click the text field. Caps Lock off. Run `probe_spawn text 3000 --file pages\text-g18.txt` (Linux
   `pages/text-g18.txt`), bringing the browser to the front within the 3 s, as in G5. The file holds the lowercase
   and uppercase letters, the digits, a space, the keyboard's punctuation and every character of the keyboard's *àé*
   page, 156 characters. On Windows run it in the guest's interactive session. On Linux run it as the desktop user,
   never with `sudo` (which hides the session's settings from it), after opening uinput to that user once per boot:
   `sudo setfacl -m "u:$USER:rw" /dev/uinput` (undo with `sudo setfacl -x "u:$USER" /dev/uinput`).
2. On Linux, record the `layout_source` lines (each source tried, and what it answered), the `layout` line (the
   source used, the layout and the variant), the `compose` line (the locale, and whether its table loaded), and
   each character's `path`; and on Wayland whether any of the session's own sources answered.
```

Replace:

```
**Read:** the page's report: the field's value, the React state, and `isTrusted` per event. **Pass:** every
```

with:

```
**Read:** the page (screenshot): `value`, `state`, `state equals value`, the `events` line with its `untrusted`
count, `G18 characters missing` and `characters not in G18`; and `probe_spawn`'s `untyped` lines and exit status.
**Pass:** every
```

Then read §G18's last paragraph through once: it must read as one sentence flow from **Pass:** (unchanged text).

- [ ] **Step 2a: Correct the spec: close the picture-in-picture window, then raise**

Decided together, 2026-11-03: a picture-in-picture window is always on top, so a raise alone leaves it covering the
main window. In `design/specs/2026-10-27-streaming-overlay-design.md`, replace:

```
   - if a picture-in-picture window opens anyway (Chrome's automatic picture-in-picture, the browser's own control, a
     site's button), the launcher brings the browser's main window back to the front, since the remote cannot drive
     the small floating window; on Windows it sees the new top-level window among the job's processes;
```

with:

```
   - if a picture-in-picture window opens anyway (Chrome's automatic picture-in-picture, the browser's own control, a
     site's button), the launcher closes the picture-in-picture window and brings the browser's main window back to
     the front, since the remote cannot drive the small floating window and it stays on top of every other window;
     closing it returns the video to the page. The close acts on the browser's window (`WM_CLOSE`, or
     `_NET_CLOSE_WINDOW` on X11, as the window's own close button would), never on a provider's page (file 30). On
     Windows it sees the new top-level window among the job's processes;
```

and replace:

```
  picture-in-picture (**The engine**, Player), and the launcher brings the main window back to the front when a
  picture-in-picture window opens anyway (**The launch path**, step 5). On Chrome beyond the automatic setting, and
```

with:

```
  picture-in-picture (**The engine**, Player), and the launcher closes a picture-in-picture window that opens anyway
  and brings the main window back to the front (**The launch path**, step 5). On Chrome beyond the automatic setting,
  and
```

Check the paragraph still reads as one sentence through `they are the only protection.`, and that nothing else in
the spec still says the launcher only raises:

Run: `rg -n "brings the (browser's )?main window back" C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec/design/specs/2026-10-27-streaming-overlay-design.md`
Expected: only the two corrected lines (the first holds "closes" itself; the second follows the line that does).

- [ ] **Step 3: Point the earlier plan at this one**

In `design/plans/2026-10-27-overlay-phase0.md`, replace:

```
this plan's tasks were written. Their protocol entries are in `guest-protocol.md`. They need kit additions that no
task below builds yet: a `probe_spawn pip` watch (G17 step 4), and a `probe_spawn text` command with a React
text-field probe page (G18). Until a kit task adds them, G17 runs partial and G18 does not run.
```

with:

```
this plan's tasks were written. Their protocol entries are in `guest-protocol.md`. The kit additions they need, a
`probe_spawn pip` watch (G17 step 4), and a `probe_spawn text` command with a React text-field probe page (G18),
are built by `design/plans/2026-11-03-overlay-phase0-kit-g17-g18.md`.
```

Task 10's results check there counts items (`G1`-`G18`, `H1`-`H4`), not commands, so it needs no change.

Check no stale text is left:

Run: `rg -n "does not have yet|until a kit task|G18 is not run|no task below builds" C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec/design C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec/tools/overlay-phase0`
Expected: no output.

- [ ] **Step 4: Run the whole tests folder on Windows**

In its own call:
`python C:/Users/jscha/.claude/hooks/box_queue.py request --session <session id> --work "overlay kit: whole tests folder on Windows (Task 7)" --overlap ok --touches workdir:c:/users/jscha/claudescratch/wt-streamflex-overlay-spec`

Then, in the next call:
`cmake --build C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec/build-phase0 --config Release; $env:PROBE_HOST = "C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec/build-phase0/Release/probe_host.exe"; $env:PROBE_SPAWN = "C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec/build-phase0/Release/probe_spawn.exe"; python -m unittest discover -s C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec/tools/overlay-phase0/tests -v`

Expected: `OK (skipped=N)`, where every skip is a Linux-only test (`PipLinuxSessionTest`, `TextLinuxTest`, the
existing Linux `run` tests) and no Windows test is skipped for a missing `PROBE_*` variable.

- [ ] **Step 5: Run the Linux container check**

In its own call:
`python C:/Users/jscha/.claude/hooks/box_queue.py request --session <session id> --work "overlay kit: Linux check image build (Task 7)" --overlap ok --touches workdir:c:/users/jscha/claudescratch/wt-streamflex-overlay-spec --big`

Then, in the next call:
`docker build -t cptk-overlay-p0-linux:local -f C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec/tools/overlay-phase0/ci/Dockerfile.linux C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec/tools/overlay-phase0/ci`

In its own call:
`python C:/Users/jscha/.claude/hooks/box_queue.py request --session <session id> --work "overlay kit: Linux check run (Task 7)" --overlap ok --touches workdir:c:/users/jscha/claudescratch/wt-streamflex-overlay-spec --big`

Then, in the next call:
`docker run --rm -v "C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec:/src:ro" cptk-overlay-p0-linux:local bash /src/tools/overlay-phase0/ci/linux-check.sh`

Expected: `RESULT: build exit 0 (0 error(s), ...), tests exit 0`; the warnings section lists nothing from a file this
plan touched; the skipped list holds only Windows-only tests and tests that need `/dev/uinput`, evdev or a display.

- [ ] **Step 6: Commit**

```powershell
git -C "C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec" add design/research/overlay/phase0/guest-protocol.md design/specs/2026-10-27-streaming-overlay-design.md design/plans/2026-10-27-overlay-phase0.md
git -C "C:/Users/jscha/ClaudeScratch/wt-streamflex-overlay-spec" commit -m "docs: G17 and G18 use the kit's pip watch, text command and React probe page; the launcher closes picture-in-picture, then raises"
```

---

## Self-review

**1. Spec coverage.**
- `probe_spawn pip` under `run`'s supervision, a line per new window with time, pid, title and class: Task 2
  (`probe_supervise`, `window` lines; `t` is every line's timestamp). Heuristic per browser, documented: Design
  decisions, `probe_pip_rule`, Task 2 `test_classify_names_the_rule`. Raise and elapsed ms: `raised`, `foreground`.
  Windows `EnumWindows` + `GetWindowThreadProcessId` over the job's pids and a background raise with the
  foreground-lock rules: `pip_win32.c`, Design decisions. Close the picture-in-picture window first (revision 1):
  `on_pip`, `WM_CLOSE` and `_NET_CLOSE_WINDOW`, `pip_closed` and `pip_gone`, `--no-close`, Task 2's two Windows
  tests; whether the video kept playing is G17's (Task 7), and the spec's step 5 and fallbacks are corrected in Task 7
  Step 2a. X11 `_NET_CLIENT_LIST` + `_NET_WM_PID`, raise through `_NET_ACTIVE_WINDOW`, optional libX11 found by CMake:
  Task 3. Wayland detected and reported as data, never silent:
  `probe_session_kind`, the Linux `pip_session` `note`, Task 3 tests. The 2000 ms bar: protocol text only (Task 7).
- `probe_spawn text <delay_ms> <string>`: Task 4. `KEYEVENTF_UNICODE` per UTF-16 unit with surrogate pairs:
  `text_win32.c`, `test_each_character_is_its_utf16_code_units`. uinput with key plus Shift or AltGr from the
  session's layout, how the layout is learned and which source was used: Task 5 (`layout_source`, `layout`). A
  character the layout cannot produce refused by name on stderr, nothing typed for it, exit status documented:
  `untyped` line, stderr line, exit 3, Task 5 `test_a_character_the_layout_lacks_is_refused_by_name`. Dead keys and
  compose (revision 2): `find_route` over xkbcommon's compose table for the locale (`LC_ALL`, `LC_CTYPE`, `LANG`,
  else `C`), `path` per character, the `compose` line, tests `test_dead_keys_in_the_german_layout`,
  `test_compose_through_a_compose_key`, `test_a_character_with_no_path_is_named` and
  `test_no_compose_table_for_the_locale`. OS-level input only: Global Constraints.
- React page with a controlled field, value, React state, each event's type and `isTrusted`, readable and
  screenshot-friendly, React vendored with version, license and sha256, shipped by make-kit, no network fetch:
  Task 6.
- Linux Chrome and Edge dropped: Task 1 (`linux_setup.py`, `test_setup.py`, README, protocol Browsers and G13 lines;
  `ci/linux-check.sh` and `make-kit.ps1` checked by Step 5's `rg`, nothing to change there).
- Docs: §G17 and §G18 Kit lines and steps (Task 7), the kit README's command list (Tasks 2 and 4), the earlier
  plan's notes (Tasks 1 and 7); its results check counts items, not commands (Task 7 Step 3 says so).
- No gaps found.

**2. Placeholder scan.** Searched for TBD, TODO, "implement later", "similar to", "add error handling": none. Two
deliberate interim files are complete code with honest behavior, each replaced whole by a named later task
(`pip_x11.c` in Task 2 by Task 3, `text_linux.c` in Task 4 by Task 5). The one Windows-only line on
`TextDryRunTest` is removed by Task 5 Step 1. Task 1 Step 2 deliberately runs one test, not the class, because the
old script's default `--root /` would write into the real filesystem root.

**3. Type consistency.** `probe_watch` is `{tick, ended, ctx}` everywhere (spawn.h, both proc files, pip.c).
`probe_child_pids(unsigned long *, int)` (Windows) and `probe_child_has_pid(long)` (Linux) match their callers in
`pip_win32.c` and `pip_x11.c`. `probe_pip_platform` field order `{session, snapshot, close, raise, foreground}` matches all three
initializers (`pip_win32.c`, Task 2's `pip_x11.c`, Task 3's `pip_x11.c`). Linux `char` detail is at most a
four-step sequence, inside `text.c`'s 320-byte `detail`. `probe_text_platform` order `{arg, prepare, plan, open, type, sleep_ms, close}` matches all three
initializers. `probe_session_kind(wayland_display, session_type, display)` has one argument order in pip.c,
pip_x11.c and text_linux.c. Output field names in the tests match the `snprintf` formats (`typable`, `typed`,
`untyped`, `ms_since_seen`, `call_ok`, `listed`).

**4. Review Focus.** Each of the six lines (the sixth added with the dead-key revision) has its test in its owning
task, named above. Checked and left out of the
five, because no unit test can reach them: a localized browser title (the `window` lines carry it as data), Caps Lock
on in the guest (the protocol says Caps Lock off), and an X11 window closing mid-read (handled by
`ignore_x_error`, reviewed by reading).
