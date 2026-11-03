"""Tests for probe_spawn text. PROBE_SPAWN names the built executable. Nothing here types into the desktop: every
real send is either --dry-run or refused before it starts."""
import os
import signal
import subprocess
import tempfile
import time
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
        # an argument longer than any argument buffer is still too long, never "not UTF-8"
        for s in ("a" * 5000, "é" * 2100):
            with self.subTest(length=len(s)):
                self.assertEqual(self.refused(text(self, "--dry-run", "0", s), "too_long")["max"], 256)
        with tempfile.TemporaryDirectory() as tmp:
            self.refused(text(self, "--dry-run", "0", "--file", write_bytes(tmp, "é".encode() * 257)), "too_long")
            self.refused(text(self, "--dry-run", "0", "--file", write_bytes(tmp, b"a" * 5000)), "too_long")

    def test_malformed_utf8_in_a_file_is_refused(self):
        with tempfile.TemporaryDirectory() as tmp:
            # an encoded surrogate, an overlong slash (2, 3 and 4 bytes), a value above U+10FFFF, a stray byte, a cut
            # sequence
            for data in (b"a\xed\xa0\x80b", b"\xc0\xaf", b"\xe0\x80\xaf", b"\xf0\x80\x80\xaf", b"\xf4\x90\x80\x80",
                         b"a\xff", b"\xe2\x82"):
                with self.subTest(data=data):
                    self.refused(text(self, "--dry-run", "0", "--file", write_bytes(tmp, data)), "not_utf8")

    def test_an_argument_that_is_not_text_is_refused(self):
        # a lone surrogate: Windows passes it in the UTF-16 command line; Linux's surrogateescape makes it byte FF
        self.refused(text(self, "--dry-run", "0", "a\udcffb"), "not_utf8")

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
                self.assertIn("usage: probe_spawn text", proc.stderr)


@unittest.skipUnless(SPAWN, "set PROBE_SPAWN to the built probe_spawn")
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
        with self.subTest(source="argument"):
            chars, end = self.chars(text(self, "--dry-run", "--layout", "us", "0", "a" * 256))
            self.assertEqual((len(chars), end["chars"]), (256, 256))
        with self.subTest(source="file"):
            with tempfile.TemporaryDirectory() as tmp:
                # the largest file accepted: a BOM, 256 four-byte characters and a CRLF, 1029 bytes
                data = b"\xef\xbb\xbf" + "\U0001F600".encode() * 256 + b"\r\n"
                self.assertEqual(len(data), 1029)
                path = write_bytes(tmp, data)
                proc = text(self, "--dry-run", "--layout", "us", "0", "--file", path)
            if os.name == "nt":
                chars, end = self.chars(proc)
                self.assertEqual((len(chars), end["chars"]), (256, 256))
                self.assertEqual({c["char"] for c in chars}, {"\U0001F600"})
            else:
                # No key or sequence of the us layout gives U+1F600 on Linux: the file is still accepted, and each
                # character is named untyped rather than the whole file refused.
                self.assertEqual(proc.returncode, 3, proc.stderr)
                evs = events(proc.stdout)
                self.assertNotIn("text_refused", [e["event"] for e in evs])
                self.assertEqual([e["cp"] for e in evs if e["event"] == "untyped"], ["U+1F600"] * 256)
                self.assertEqual((evs[-1]["event"], evs[-1]["chars"]), ("text_end", 256))


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

    def test_a_command_line_the_two_parsers_split_differently_is_refused(self):
        # The wide argument at text's index would be another one's text, so the command refuses rather than type a
        # fragment.
        cases = (
            # The C runtime reads "" inside quotes as a quote and stays quoted (one argument: a " b);
            # CommandLineToArgvW ends the quotes there (two: a " and b). A string, not a list, so Python passes
            # this command line as it is.
            ("a raw command line with \"\" in quotes", '"%s" text --dry-run 0 "a "" b"' % SPAWN),
            # The ANSI command line the C runtime splits holds a fullwidth quote as its best fit, a real quote.
            ("a fullwidth quote", [SPAWN, "text", "--dry-run", "0", "a＂ b"]),
        )
        for name, command in cases:
            with self.subTest(name):
                try:
                    proc = subprocess.run(command, capture_output=True, encoding="utf-8", timeout=120)
                except subprocess.TimeoutExpired:
                    self.fail("HUNG: probe_spawn text with %s did not end" % name)
                self.assertEqual(proc.returncode, 2, proc.stderr)
                evs = events(proc.stdout)
                self.assertEqual([(e["event"], e.get("reason")) for e in evs], [("text_refused", "argv_mismatch")])
                self.assertIn("nothing was typed", proc.stderr)

    def test_text_the_ansi_code_page_cannot_hold_is_no_mismatch(self):
        # The C runtime's ANSI argv holds each such character as '?', so it must not read as the parsers disagreeing.
        for s in ("\U0001F600", "日本語"):
            with self.subTest(s=ascii(s)):
                proc = text(self, "--dry-run", "0", s)
                self.assertEqual(proc.returncode, 0, proc.stderr)
                self.assertEqual([e["char"] for e in events(proc.stdout) if e["event"] == "char"], list(s))


def gone(pid):
    """Whether pid has exited (no /proc entry, or only a zombie waiting to be reaped)."""
    try:
        with open("/proc/%d/stat" % pid, encoding="utf-8") as f:
            stat = f.read()
    except OSError:
        return True
    return stat[stat.rindex(")") + 2] == "Z"


def group_members(pgid):
    """The live processes whose process group is pgid."""
    found = []
    for name in os.listdir("/proc"):
        if not name.isdigit():
            continue
        try:
            with open("/proc/%s/stat" % name, encoding="utf-8") as f:
                stat = f.read()
        except OSError:
            continue
        fields = stat[stat.rindex(")") + 2:].split()
        if int(fields[2]) == pgid and fields[0] != "Z":
            found.append(int(name))
    return found


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

    def test_a_stalled_wrapper_source_is_killed_with_its_children(self):
        # A wrapper that does not exec: the shell waits on its own child, so neither uses CPU. The whole process
        # group is watched, judged stalled, and killed, and the next source is tried.
        with tempfile.TemporaryDirectory() as tmp:
            env = self.session_env(tmp, WAYLAND_DISPLAY="wayland-0", XDG_CURRENT_DESKTOP="GNOME")
            pids = os.path.join(tmp, "pids")
            self.fake(tmp, "gsettings", "/bin/sleep 600 &\necho $$ $! > '%s'\nwait\n:\n" % pids)
            proc = text(self, "--dry-run", "0", "a", env=env)
            with open(pids, encoding="utf-8") as f:
                shell_pid, sleep_pid = (int(v) for v in f.read().split())
        try:
            self.assertEqual(self.sources(proc), [("gnome:org.gnome.desktop.input-sources", "stalled"),
                                                  ("localectl", "not_found")])
            self.assertEqual(self.layout(proc), [])
            self.assertEqual(proc.returncode, 1)
            # SIGKILL is delivered at once; this loop only waits for the kernel to finish the exit (hang guard 120 s).
            deadline = time.monotonic() + 120
            while not gone(sleep_pid) and time.monotonic() < deadline:
                time.sleep(0.1)
            self.assertTrue(gone(sleep_pid), "HUNG: the source's child %d outlived the stall kill" % sleep_pid)
            self.assertEqual(group_members(shell_pid), [])
        finally:
            if not gone(sleep_pid):
                os.kill(sleep_pid, signal.SIGKILL)

    def test_a_source_that_writes_forever_is_cut_off(self):
        # Output is kept only up to the capture buffer; then the read end is closed, so a writer that never stops
        # ends on SIGPIPE, and the source is named failed. No clock is involved.
        for name, body in (("a program", "exec /usr/bin/yes\n"), ("a shell loop", "while :; do echo xkb; done\n")):
            with self.subTest(name):
                with tempfile.TemporaryDirectory() as tmp:
                    env = self.session_env(tmp, WAYLAND_DISPLAY="wayland-0", XDG_CURRENT_DESKTOP="GNOME")
                    self.fake(tmp, "gsettings", body)
                    proc = text(self, "--dry-run", "0", "a", env=env)
                self.assertEqual(self.sources(proc), [("gnome:org.gnome.desktop.input-sources", "failed"),
                                                      ("localectl", "not_found")])
                self.assertEqual(proc.returncode, 1)

    def test_a_grandchild_that_writes_on_after_its_source_exits_is_cut_off(self):
        # The source exits 0 while its own child keeps writing: the command still ends, and the writer ends once the
        # read end is closed. Its output holds no xkb source, so that is what is named.
        with tempfile.TemporaryDirectory() as tmp:
            env = self.session_env(tmp, WAYLAND_DISPLAY="wayland-0", XDG_CURRENT_DESKTOP="GNOME")
            pids = os.path.join(tmp, "pids")
            self.fake(tmp, "gsettings", "/usr/bin/yes &\necho $! >> '%s'\n/bin/sleep 0.5\nexit 0\n" % pids)
            proc = text(self, "--dry-run", "0", "a", env=env)
            with open(pids, encoding="utf-8") as f:
                writers = [int(v) for v in f.read().split()]
        try:
            self.assertEqual(self.sources(proc), [("gnome:org.gnome.desktop.input-sources", "no_xkb_source"),
                                                  ("localectl", "not_found")])
            self.assertEqual(proc.returncode, 1)
            deadline = time.monotonic() + 120  # hang guard: SIGPIPE ends each writer once the read end is closed
            while not all(gone(p) for p in writers) and time.monotonic() < deadline:
                time.sleep(0.1)
            self.assertEqual([p for p in writers if not gone(p)], [], "HUNG: a writer outlived the closed read end")
        finally:
            for p in writers:
                if not gone(p):
                    os.kill(p, signal.SIGKILL)

    def test_the_environment_cannot_change_the_compiled_layout(self):
        # xkbcommon would fill an unset model, variant or options from XKB_DEFAULT_*; us(intl) and compose:ralt both
        # give é, so either one honored would type what the printed layout line cannot.
        with tempfile.TemporaryDirectory() as tmp:
            env = self.compose_env(tmp, "en_US.UTF-8", XKB_DEFAULT_VARIANT="intl", XKB_DEFAULT_OPTIONS="compose:ralt",
                                   XKB_DEFAULT_MODEL="pc105")
            proc = text(self, "--dry-run", "--layout", "us", "0", "aé", env=env)
        self.assertEqual(proc.returncode, 3, proc.stderr)
        (layout,) = self.layout(proc)
        self.assertEqual((layout["model"], layout["variant"], layout["options"]), ("", "", ""))
        self.assertEqual([e["char"] for e in events(proc.stdout) if e["event"] == "untyped"], ["é"])

    def test_a_layout_argument_with_a_variant(self):
        proc = text(self, "--dry-run", "--layout", "de(nodeadkeys)", "0", "z")
        self.assertEqual(proc.returncode, 0, proc.stderr)
        self.assertEqual(self.sources(proc), [("argument", "ok")])
        (layout,) = self.layout(proc)
        self.assertEqual((layout["layout"], layout["variant"]), ("de", "nodeadkeys"))
        self.assertEqual([e["key"] for e in events(proc.stdout) if e["event"] == "char"], [21])

    def test_a_bad_layout_argument_is_refused(self):
        for arg in ("", "(intl)", "x" * 64, "de(", "de(nodeadkeys)x", "de()", "de(%s)" % ("v" * 64)):
            with self.subTest(arg=arg):
                proc = text(self, "--dry-run", "--layout", arg, "0", "a")
                self.assertEqual(proc.returncode, 1)
                self.assertEqual(self.sources(proc), [("argument", "bad_argument")])
                self.assertEqual(self.layout(proc), [])
                self.assertEqual(events(proc.stdout)[-1]["event"], "layout_unknown")

    def test_the_system_default_from_localectl(self):
        with tempfile.TemporaryDirectory() as tmp:
            env = self.session_env(tmp)
            self.fake(tmp, "localectl", "/bin/cat <<'EOF'\n"
                                        "   System Locale: LANG=en_US.UTF-8\n"
                                        "       VC Keymap: de-nodeadkeys\n"
                                        "      X11 Layout: de,us\n"
                                        "       X11 Model: pc105\n"
                                        "     X11 Variant: nodeadkeys,\n"
                                        "     X11 Options: compose:ralt\n"
                                        "EOF\n")
            proc = text(self, "--dry-run", "0", "z", env=env)
        self.assertEqual(proc.returncode, 0, proc.stderr)
        self.assertEqual(self.sources(proc), [("localectl", "ok")])
        (layout,) = self.layout(proc)
        self.assertEqual((layout["model"], layout["layout"], layout["variant"], layout["options"]),
                         ("pc105", "de", "nodeadkeys", "compose:ralt"))
        self.assertEqual([e["key"] for e in events(proc.stdout) if e["event"] == "char"], [21])

    def test_gnome_falls_back_to_sources_when_mru_sources_is_empty(self):
        with tempfile.TemporaryDirectory() as tmp:
            env = self.session_env(tmp, WAYLAND_DISPLAY="wayland-0", XDG_CURRENT_DESKTOP="GNOME")
            self.fake(tmp, "gsettings", 'case "$3" in\n'
                                        "  mru-sources) echo \"@a(ss) []\" ;;\n"
                                        "  sources) echo \"[('ibus', 'mozc-jp'), ('xkb', 'de')]\" ;;\n"
                                        "  *) echo \"@as []\" ;;\n"
                                        "esac\n")
            proc = text(self, "--dry-run", "0", "z", env=env)
        self.assertEqual(proc.returncode, 0, proc.stderr)
        self.assertEqual(self.sources(proc), [("gnome:org.gnome.desktop.input-sources", "ok")])
        (layout,) = self.layout(proc)
        self.assertEqual((layout["layout"], layout["variant"], layout["options"]), ("de", "", ""))
        self.assertEqual([e["key"] for e in events(proc.stdout) if e["event"] == "char"], [21])

    def test_kxkbrc_not_in_use_is_skipped(self):
        with tempfile.TemporaryDirectory() as tmp:
            os.makedirs(os.path.join(tmp, ".config"))
            with open(os.path.join(tmp, ".config", "kxkbrc"), "w", encoding="utf-8") as f:
                f.write("[Layout]\nLayoutList=de\nUse=false\n")
            env = self.session_env(tmp, WAYLAND_DISPLAY="wayland-0", XDG_CURRENT_DESKTOP="KDE")
            proc = text(self, "--dry-run", "0", "z", env=env)
        self.assertEqual(self.sources(proc), [("kde:kxkbrc", "not_in_use"), ("localectl", "not_found")])
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
