"""Tests for tests/qa/check-smoke.py. Run: python -m unittest discover -s tests/qa -p "test_*.py" -v"""

import importlib.util
import tempfile
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
_spec = importlib.util.spec_from_file_location("check_smoke", HERE / "check-smoke.py")
cs = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(cs)


def row(rid, tag="All", name="A claim", steps="Do it", expected="It happens"):
    return f'| ☐ <a id="{cs.anchor_for(rid)}"></a> **{rid}** `[{tag}]` {name} | {steps} | {expected} |'


def doc(*rows, retired=()):
    lines = ["# Smoke test", "", "## 1. Module", "", "| Row | Steps | Expected |", "|---|---|---|", *rows, ""]
    lines += ["## Retired ids", ""] + [f"- {r} -- retired" for r in retired] + [""]
    return "\n".join(lines)


class DocLint(unittest.TestCase):
    def lint(self, text):
        rows, retired, errors = cs.parse_doc(text)
        return rows, errors + cs.lint_doc(rows, retired)

    def test_good_rows_pass(self):
        rows, errors = self.lint(doc(row("1.1"), row("1.2", "Win"), row("5.3a", "KDE"), row("4.2-system-gui", "GNOME")))
        self.assertEqual(errors, [])
        self.assertEqual([r.id for r in rows], ["1.1", "1.2", "5.3a", "4.2-system-gui"])
        self.assertEqual([r.tag for r in rows], ["All", "Win", "KDE", "GNOME"])

    def test_id_grammar(self):
        for bad in ("1", "1.", "1.1A", "1.1ab", "1.1a-x", "1.1-", "1.1-9x", "a.1", "1.1_x"):
            with self.subTest(bad=bad):
                line = f'| ☐ <a id="t-x"></a> **{bad}** `[All]` n | s | e |'
                _, errors = self.lint(doc(line))
                self.assertTrue(any("not a valid id" in e for e in errors), errors)

    def test_anchor_must_match_id(self):
        line = '| ☐ <a id="t-1-2"></a> **1.1** `[All]` n | s | e |'
        _, errors = self.lint(doc(line))
        self.assertTrue(any('anchor "t-1-2" should be "t-1-1"' in e for e in errors), errors)

    def test_exactly_one_known_tag(self):
        cases = {
            "none": '| ☐ <a id="t-1-1"></a> **1.1** n | s | e |',
            "two": '| ☐ <a id="t-1-1"></a> **1.1** `[Win]` `[KDE]` n | s | e |',
            "unknown": '| ☐ <a id="t-1-1"></a> **1.1** `[Both]` n | s | e |',
        }
        for name, line in cases.items():
            with self.subTest(name=name):
                _, errors = self.lint(doc(line))
                self.assertTrue(any("exactly one tag" in e for e in errors), errors)

    def test_tag_after_the_name_is_an_error(self):
        line = '| ☐ <a id="t-1-1"></a> **1.1** n `[Win]` | s | e |'
        _, errors = self.lint(doc(line))
        self.assertTrue(any("exactly one tag" in e for e in errors), errors)

    def test_backticked_bracket_in_the_name_is_not_a_tag(self):
        rows, errors = self.lint(doc(row("1.1", "Win", name="press `[A]` to pair")))
        self.assertEqual(errors, [])
        self.assertEqual([r.tag for r in rows], ["Win"])

    def test_id_re_is_a_full_match(self):
        self.assertIsNotNone(cs.ID_RE.match("1.1"))
        self.assertIsNone(cs.ID_RE.match("1.1\n"))

    def test_duplicate_id(self):
        _, errors = self.lint(doc(row("1.1"), row("1.1", "Win")))
        self.assertTrue(any("1.1 appears more than once" in e for e in errors), errors)

    def test_retired_id_reused(self):
        _, errors = self.lint(doc(row("1.1"), retired=["1.1"]))
        self.assertTrue(any("1.1 is retired" in e for e in errors), errors)

    def test_malformed_retired_entry(self):
        _, errors = self.lint(doc(row("1.1"), retired=["not-an-id"]))
        self.assertTrue(any("a retired-ids entry must start with a valid id" in e for e in errors), errors)

    def test_malformed_row_start(self):
        _, errors = self.lint(doc("| ☐ **1.1** `[All]` no anchor | s | e |"))
        self.assertTrue(any("a row must read" in e for e in errors), errors)

    def test_row_without_closing_pipe(self):
        _, errors = self.lint(doc('| ☐ <a id="t-1-1"></a> **1.1** `[All]` n | s | e'))
        self.assertTrue(any("a row must read" in e for e in errors), errors)

    def test_unescaped_pipe_is_an_error(self):
        _, errors = self.lint(doc(row("1.1", steps="press A | B")))
        self.assertTrue(any("three cells" in e for e in errors), errors)

    def test_escaped_pipe_is_fine(self):
        _, errors = self.lint(doc(row("1.1", steps=r"press A \| B")))
        self.assertEqual(errors, [])

    def test_crlf_doc_parses_like_lf(self):
        text = doc(row("1.1"), row("1.2", "Linux"), retired=["1.1"])
        crlf = text.replace("\n", "\r\n")
        lf_rows, lf_errors = self.lint(text)
        crlf_rows, crlf_errors = self.lint(crlf)
        self.assertTrue(any("1.1 is retired" in e for e in lf_errors), lf_errors)
        self.assertEqual(lf_errors, crlf_errors)
        self.assertEqual([r.id for r in lf_rows], [r.id for r in crlf_rows])
        self.assertEqual(cs.parse_doc(text)[1], cs.parse_doc(crlf)[1])


if __name__ == "__main__":
    unittest.main()
