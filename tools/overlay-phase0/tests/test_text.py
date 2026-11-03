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
        with self.subTest(source="argument"):
            chars, end = self.chars(text(self, "--dry-run", "--layout", "us", "0", "a" * 256))
            self.assertEqual((len(chars), end["chars"]), (256, 256))
        with self.subTest(source="file"):
            with tempfile.TemporaryDirectory() as tmp:
                # the largest file accepted: a BOM, 256 four-byte characters and a CRLF, 1029 bytes
                data = b"\xef\xbb\xbf" + "\U0001F600".encode() * 256 + b"\r\n"
                self.assertEqual(len(data), 1029)
                path = write_bytes(tmp, data)
                chars, end = self.chars(text(self, "--dry-run", "--layout", "us", "0", "--file", path))
            self.assertEqual((len(chars), end["chars"]), (256, 256))
            self.assertEqual({c["char"] for c in chars}, {"\U0001F600"})


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
