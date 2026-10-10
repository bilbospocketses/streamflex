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


def register(*lines, summary_block=None):
    out = ["# Automation coverage", ""]
    out += [summary_block] if summary_block is not None else [cs.SUMMARY_BEGIN, cs.SUMMARY_END]
    out += ["", "## Every row", "", "| Row | Platforms | Owner | Where it runs, or why not |", "|---|---|---|---|"]
    out += list(lines) + [""]
    return "\n".join(out)


def half(rid, tag, owner="manual", where="A real TV"):
    return f"| {rid} | `[{tag}]` | {owner} | {where} |"


class Tree:
    """A throwaway repo root with the files the evidence check reads."""

    def __init__(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        checks = self.root / "tests/headless/checks"
        checks.mkdir(parents=True)
        (checks / "58-settings-pickers.sh").write_text(
            "CFG=$cfg run_keys f58-hex Menu Down Return\nresult \"pickers: hex\" $ok\n"
            "CFG=$cfg run_keys_at 1280x800 f58-sized Menu Return\n"
            "# comment run_keys waits here\n", encoding="utf-8")
        (self.root / "tests/test_layout.c").write_text(
            "// A function to test it\nstatic void test_resolve(void)\n{\n}\n", encoding="utf-8")

    def close(self):
        self.tmp.cleanup()


class RegisterLint(unittest.TestCase):
    def setUp(self):
        self.tree = Tree()
        self.rows, _, _ = cs.parse_doc(doc(row("1.1", "All"), row("1.2", "Win"), row("1.3", "KDE")))

    def tearDown(self):
        self.tree.close()

    def lint(self, text):
        halves, errors = cs.parse_register(text)
        return errors + cs.lint_register(self.rows, halves, self.tree.root)

    def good(self):
        return [
            half("1.1", "Linux", "streamflex-headless", "`58-settings-pickers.sh` run `f58-hex`"),
            half("1.1", "Win", "manual", "qa-harness item 75, planned"),
            half("1.2", "Win", "streamflex-ctest", "`test_layout.c` `test_resolve`"),
            half("1.3", "KDE"),
        ]

    def test_good_register_passes(self):
        self.assertEqual(self.lint(register(*self.good())), [])

    def test_missing_desktop(self):
        lines = self.good()[:1] + self.good()[2:]
        errors = self.lint(register(*lines))
        self.assertTrue(any("1.1: no half covers Win" in e for e in errors), errors)

    def test_desktop_covered_twice(self):
        errors = self.lint(register(*self.good(), half("1.3", "Kubuntu")))
        self.assertTrue(any("1.3: Kubuntu is covered by more than one half" in e for e in errors), errors)

    def test_half_outside_the_tag(self):
        errors = self.lint(register(*self.good(), half("1.2", "Ubuntu")))
        self.assertTrue(any("1.2: Ubuntu is outside the row's tag [Win]" in e for e in errors), errors)

    def test_orphan_half(self):
        errors = self.lint(register(*self.good(), half("9.9", "All")))
        self.assertTrue(any("9.9 is not a row of the smoke doc" in e for e in errors), errors)

    def test_unknown_owner_and_empty_where(self):
        errors = self.lint(register(half("1.1", "All", "robot"), half("1.2", "Win", "manual", ""), half("1.3", "KDE")))
        self.assertTrue(any("owner 'robot'" in e for e in errors), errors)
        self.assertTrue(any("empty" in e for e in errors), errors)

    def test_bad_platforms_cell(self):
        errors = self.lint(register("| 1.1 | Linux | manual | x |", *self.good()[1:]))
        self.assertTrue(any("one tag" in e for e in errors), errors)

    def test_headless_evidence_must_exist(self):
        cases = {
            "no pair": "the pickers check",
            "no file": "`99-nothing.sh` run `f58-hex`",
            "no run": "`58-settings-pickers.sh` run `f58-nothing`",
            "prefix only": "`58-settings-pickers.sh` run `f58-he`",
        }
        for name, where in cases.items():
            with self.subTest(name=name):
                lines = [half("1.1", "Linux", "streamflex-headless", where)] + self.good()[1:]
                errors = self.lint(register(*lines))
                self.assertTrue(any("1.1" in e and "headless" in e for e in errors), errors)

    def test_headless_run_keys_at_takes_a_size_first(self):
        lines = [half("1.1", "Linux", "streamflex-headless", "`58-settings-pickers.sh` run `f58-sized`")] + self.good()[1:]
        self.assertEqual(self.lint(register(*lines)), [])

    def test_headless_size_and_comment_are_not_run_names(self):
        for run in ("1280x800", "waits"):
            with self.subTest(run=run):
                where = f"`58-settings-pickers.sh` run `{run}`"
                lines = [half("1.1", "Linux", "streamflex-headless", where)] + self.good()[1:]
                errors = self.lint(register(*lines))
                self.assertTrue(any("1.1" in e and f"has no run named {run}" in e for e in errors), errors)

    def test_ctest_evidence_must_exist(self):
        for where in ("`test_layout.c`", "`test_none.c` `test_resolve`", "`test_layout.c` `test_missing`"):
            with self.subTest(where=where):
                lines = self.good()[:2] + [half("1.2", "Win", "streamflex-ctest", where), self.good()[3]]
                errors = self.lint(register(*lines))
                self.assertTrue(any("1.2" in e and "unit test" in e for e in errors), errors)

    def test_summary_counts(self):
        halves, _ = cs.parse_register(register(*self.good()))
        block = cs.summary(self.rows, halves)
        self.assertTrue(block.startswith(cs.SUMMARY_BEGIN) and block.endswith(cs.SUMMARY_END))
        self.assertIn("| Rows | 3 |", block)
        self.assertIn("| Row halves | 4 |", block)
        # verdicts: 1.1 is 5 desktops, 1.2 is 1, 1.3 is 2; automated: Linux (4) + Win ctest (1)
        self.assertIn("| Verdicts per release (row x desktop) | 8 |", block)
        self.assertIn("| **Automated today** | 2 of 4 halves; 5 of 8 verdicts (62 %) |", block)

    def test_summary_stale_then_written(self):
        text = register(*self.good())
        (self.tree.root / "tests/qa").mkdir(parents=True)
        (self.tree.root / cs.DOC).write_text(doc(row("1.1", "All"), row("1.2", "Win"), row("1.3", "KDE")), encoding="utf-8")
        (self.tree.root / cs.REGISTER).write_text(text, encoding="utf-8")
        errors = cs.run_lint(self.tree.root)
        self.assertTrue(any("summary is stale" in e for e in errors), errors)
        self.assertEqual(cs.run_lint(self.tree.root, write=True), [])
        self.assertEqual(cs.run_lint(self.tree.root), [])

    def test_summary_markers_required(self):
        new, errors = cs.replace_summary("# no markers\n", "block")
        self.assertIsNone(new)
        self.assertTrue(any("summary markers" in e for e in errors), errors)

    def test_planned_marker_is_exact(self):
        cases = [("manual", "qa-harness, planned"), ("manual", "item 75 planned"),
                 ("manual", "qa-harness item 75, planned."), ("manual", "Qa-harness item 75, planned"),
                 ("qa-harness", "qa-harness item 75, planned")]
        for owner, where in cases:
            with self.subTest(owner=owner, where=where):
                lines = self.good()[:1] + [half("1.1", "Win", owner, where)] + self.good()[2:]
                errors = self.lint(register(*lines))
                self.assertTrue(any("1.1" in e and "exactly" in e for e in errors), errors)

    def test_honest_manual_reason_mentioning_planned_passes(self):
        lines = self.good()[:1] + [half("1.1", "Win", "manual", "A real TV; no automation planned")] + self.good()[2:]
        self.assertEqual(self.lint(register(*lines)), [])

    def test_manual_half_naming_the_lane_fails(self):
        for where in ("five-desktop lane, arc S3", "qa-harness item #75"):
            with self.subTest(where=where):
                lines = self.good()[:1] + [half("1.1", "Win", "manual", where)] + self.good()[2:]
                errors = self.lint(register(*lines))
                self.assertTrue(any("1.1" in e and "exactly" in e and "reworded" in e for e in errors), errors)

    def test_qa_harness_arc_name_starts_with_a_letter_or_digit(self):
        lines = self.good()[:1] + [half("1.1", "Win", "qa-harness", "five-desktop lane, arc -")] + self.good()[2:]
        errors = self.lint(register(*lines))
        self.assertTrue(any("five-desktop lane, arc <name>" in e for e in errors), errors)

    def test_qa_harness_arc_form(self):
        lines = self.good()[:1] + [half("1.1", "Win", "qa-harness", "five-desktop lane, arc S3")] + self.good()[2:]
        self.assertEqual(self.lint(register(*lines)), [])
        for where in ("five-desktop lane arc S3", "five-desktop lane, arc S3 (Wave 2)", "five-desktop lane, arc"):
            with self.subTest(where=where):
                lines = self.good()[:1] + [half("1.1", "Win", "qa-harness", where)] + self.good()[2:]
                errors = self.lint(register(*lines))
                self.assertTrue(any("five-desktop lane, arc <name>" in e for e in errors), errors)

    def test_missing_files_are_reported(self):
        errors = cs.run_lint(self.tree.root)
        self.assertEqual(errors, [f"{cs.DOC}: missing"])
        (self.tree.root / "tests/qa").mkdir(parents=True)
        (self.tree.root / cs.DOC).write_text(doc(row("1.1")), encoding="utf-8")
        errors = cs.run_lint(self.tree.root)
        self.assertIn(f"{cs.REGISTER}: missing", errors)


def results(*lines, findings=(), skips=()):
    out = ["# Smoke results: v0.5.0", "", "| Desktop | OS | Image | Build |", "|---|---|---|---|", "| Win | Windows 11 24H2 | sha256:abc | sha256:def |", ""]
    out += [cs.RESULTS_HEADER, "|---|---|---|---|---|---|", *lines, ""]
    out += ["## Findings", ""] + [f"- **{f}** It failed." for f in findings] + [""]
    out += ["## Not measured", ""] + [f"- **{s}** NOT MEASURED -- no pad | none attached" for s in skips] + [""]
    return "\n".join(out)


class ResultsLint(unittest.TestCase):
    def setUp(self):
        self.rows, self.retired, _ = cs.parse_doc(doc(row("1.1", "All"), row("1.2", "Win"), row("5.3a", "KDE"),
                                                      row("4.2-system-gui", "GNOME"), retired=["1.9"]))

    def lint(self, text, complete=False):
        table, errors = cs.parse_results(text, "tests/qa/results/v0.5.0.md")
        return errors + cs.lint_results("tests/qa/results/v0.5.0.md", table, self.rows, self.retired, complete)

    def full(self):
        return [
            "| 1.1 | x | x | F1 | S1 | x (v0.4.1) |",
            "| 1.2 | x | - | - | - | - |",
            "| 5.3a | - | - | x | - | x |",
            "| 4.2-system-gui | - | x | - | x | - |",
        ]

    def test_complete_file_passes(self):
        self.assertEqual(self.lint(results(*self.full(), findings=["F1"], skips=["S1"]), complete=True), [])

    def test_bad_cells(self):
        for cell in ("pass", "F", "F0", "S01", "SKIP: NOT MEASURED -- x", "x (0.4.1)"):
            with self.subTest(cell=cell):
                errors = self.lint(results(f"| 1.2 | {cell} | - | - | - | - |"))
                self.assertTrue(any("not a results cell" in e for e in errors), errors)

    def test_finding_and_skip_need_entries(self):
        errors = self.lint(results("| 1.2 | F2 | - | - | - | - |", "| 5.3a | - | - | S3 | - | x |"))
        self.assertTrue(any("F2 has no entry under ## Findings" in e for e in errors), errors)
        self.assertTrue(any("S3 has no entry under ## Not measured" in e for e in errors), errors)

    def test_entry_in_wrong_section(self):
        text = results("| 1.2 | S1 | - | - | - | - |", findings=["S1"])
        errors = self.lint(text)
        self.assertTrue(any("S1 belongs under ## Not measured" in e for e in errors), errors)

    def test_dash_inside_tag(self):
        errors = self.lint(results("| 1.2 | - | - | - | - | - |"))
        self.assertTrue(any("1.2: Win is inside the row's tag [Win]" in e for e in errors), errors)

    def test_unknown_row(self):
        errors = self.lint(results("| 7.7 | x | x | x | x | x |"))
        self.assertTrue(any("7.7 is not a row of the smoke doc" in e for e in errors), errors)

    def test_retired_row_is_allowed_in_old_files(self):
        self.assertEqual(self.lint(results("| 1.9 | x | x | x | x | x |")), [])

    def test_form_only_allows_missing_rows_and_empty_cells(self):
        self.assertEqual(self.lint(results("| 1.2 |  | - | - | - | - |")), [])

    def test_complete_requires_every_row_and_cell(self):
        errors = self.lint(results("| 1.2 |  | - | - | - | - |"), complete=True)
        self.assertTrue(any("1.1 has no line" in e for e in errors), errors)
        self.assertTrue(any("1.2: Win is empty" in e for e in errors), errors)

    def test_complete_refuses_a_verdict_outside_the_tag(self):
        lines = self.full()
        lines[1] = "| 1.2 | x | x | - | - | - |"
        errors = self.lint(results(*lines, findings=["F1"], skips=["S1"]), complete=True)
        self.assertTrue(any("1.2: Ubuntu is outside the row's tag [Win]" in e for e in errors), errors)

    def test_duplicate_line_and_wrong_header(self):
        errors = self.lint(results("| 1.2 | x | - | - | - | - |", "| 1.2 | x | - | - | - | - |"))
        self.assertTrue(any("1.2 appears more than once" in e for e in errors), errors)
        errors = self.lint(results().replace(cs.RESULTS_HEADER, "| Row | Windows | Ubuntu | Kubuntu | Fedora | Fedora-KDE |"))
        self.assertTrue(any("header" in e for e in errors), errors)

    def test_suffixed_ids_and_other_tables(self):
        table, errors = cs.parse_results(results(*self.full(), findings=["F1"], skips=["S1"]), "r.md")
        self.assertEqual(errors, [])
        self.assertEqual(sorted(table), ["1.1", "1.2", "4.2-system-gui", "5.3a"])
        self.assertNotIn("Win", table)

    def test_run_lint_completes_only_named_files(self):
        tree = Tree()
        try:
            root = tree.root
            (root / "tests/qa/results").mkdir(parents=True)
            (root / cs.DOC).write_text(doc(row("1.2", "Win")), encoding="utf-8")
            reg = register(half("1.2", "Win"))
            (root / cs.REGISTER).write_text(reg, encoding="utf-8")
            cs.run_lint(root, write=True)
            old = root / "tests/qa/results/v0.4.1.md"
            old.write_text(results(), encoding="utf-8")
            self.assertEqual(cs.run_lint(root), [])
            errors = cs.run_lint(root, complete=[str(old)])
            self.assertTrue(any("1.2 has no line" in e for e in errors), errors)
            errors = cs.run_lint(root, complete=[str(root / "tests/qa/results/v9.9.9.md")])
            self.assertTrue(any("not a results file" in e for e in errors), errors)
        finally:
            tree.close()


if __name__ == "__main__":
    unittest.main()
