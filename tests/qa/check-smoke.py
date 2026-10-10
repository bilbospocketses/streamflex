#!/usr/bin/env python3
"""The smoke doc's lint and its PR gate.

  python tests/qa/check-smoke.py lint                      check the doc, the register and the results files
  python tests/qa/check-smoke.py lint --write              also regenerate the register's summary block
  python tests/qa/check-smoke.py lint --complete FILE ...  also require those results files to cover every row
  python tests/qa/check-smoke.py gate                      CI only: see gate() below

Design: design/specs/2026-10-10-smoke-doc-design.md
"""

import argparse
import re
import sys
from pathlib import Path
from typing import NamedTuple

ROOT = Path(__file__).resolve().parents[2]
DOC = "tests/qa/smoke-test.md"
REGISTER = "tests/qa/automation-coverage.md"
RESULTS_DIR = "tests/qa/results"

DESKTOPS = ("Win", "Ubuntu", "Kubuntu", "Fedora", "Fedora-KDE")
TAGS = {
    "Win": frozenset({"Win"}),
    "Ubuntu": frozenset({"Ubuntu"}),
    "Kubuntu": frozenset({"Kubuntu"}),
    "Fedora": frozenset({"Fedora"}),
    "Fedora-KDE": frozenset({"Fedora-KDE"}),
    "GNOME": frozenset({"Ubuntu", "Fedora"}),
    "KDE": frozenset({"Kubuntu", "Fedora-KDE"}),
    "Linux": frozenset({"Ubuntu", "Kubuntu", "Fedora", "Fedora-KDE"}),
    "All": frozenset(DESKTOPS),
}
OWNERS = ("streamflex-ctest", "streamflex-headless", "qa-harness", "manual")
AUTOMATED = ("streamflex-ctest", "streamflex-headless", "qa-harness")

# qa-harness parses ids with the same grammar, so the two must agree byte for byte
ID = r"\d+\.\d+(?:[a-z]|-[a-z][a-z0-9-]*)?"
ID_RE = re.compile(rf"\A{ID}\Z")
TAG_RE = re.compile(r"`\[([^\]`]*)\]`")
# the tags are the run of backticked brackets directly after the bold id; any later one is part of the name
TAG_RUN_RE = re.compile(r"(?:`\[[^\]`]*\]`\s*)+")
ROW_START = "| \u2610"
ROW_RE = re.compile(r'^\| \u2610 <a id="([^"]*)"></a> \*\*([^*]+)\*\* ')
ROW_SHAPE = '| [box] <a id="t-..."></a> **<id>** `[<tag>]` <name> | <steps> | <expected> |'


class Row(NamedTuple):
    id: str
    tag: "str | None"
    line: int


def cells(line):
    """The cells of a markdown table line. An escaped pipe (\\|) stays inside its cell."""
    parts = re.split(r"(?<!\\)\|", line.strip())
    return [p.strip() for p in parts[1:-1]]


def anchor_for(row_id):
    return "t-" + row_id.replace(".", "-")


def parse_doc(text):
    """Return (rows, retired, errors). Rows are lines that start with '| <box>'; retired ids are
    the '- <id>' lines under '## Retired ids'."""
    rows, retired, errors = [], [], []
    section = None
    for n, line in enumerate(text.splitlines(), 1):
        if line.startswith("## "):
            section = line[3:].strip()
            continue
        if section == "Retired ids":
            m = re.match(rf"^- ({ID})(?=\s|$)", line)
            if m:
                retired.append((m.group(1), n))
            elif line.startswith("- "):
                errors.append(f"{DOC}:{n}: a retired-ids entry must start with a valid id")
            continue
        if not line.startswith(ROW_START):
            continue
        m = ROW_RE.match(line)
        if not m or not line.rstrip().endswith("|"):
            errors.append(f"{DOC}:{n}: a row must read {ROW_SHAPE}")
            continue
        anchor, rid = m.groups()
        if not ID_RE.match(rid):
            errors.append(f"{DOC}:{n}: {rid!r} is not a valid id (<module>.<n>, then one lowercase letter or -word)")
            continue
        if anchor != anchor_for(rid):
            errors.append(f'{DOC}:{n}: anchor "{anchor}" should be "{anchor_for(rid)}"')
        row_cells = cells(line)
        if len(row_cells) != 3:
            errors.append(f"{DOC}:{n}: row {rid} has {len(row_cells)} cells, not three cells; write a literal pipe as \\|")
        run = TAG_RUN_RE.match(line, m.end())
        tags = TAG_RE.findall(run.group(0)) if run else []
        tag = tags[0] if len(tags) == 1 and tags[0] in TAGS else None
        if tag is None:
            errors.append(f"{DOC}:{n}: row {rid} needs exactly one tag from {', '.join('[' + t + ']' for t in TAGS)}")
        rows.append(Row(rid, tag, n))
    return rows, retired, errors


def lint_doc(rows, retired):
    errors = []
    seen = {}
    for r in rows:
        if r.id in seen:
            errors.append(f"{DOC}:{r.line}: {r.id} appears more than once (first at line {seen[r.id]})")
        else:
            seen[r.id] = r.line
    retired_ids = {rid for rid, _ in retired}
    for r in rows:
        if r.id in retired_ids:
            errors.append(f"{DOC}:{r.line}: {r.id} is retired and can never be reused; give the row a new id")
    return errors


def run_lint(root, write=False, complete=()):
    """Every lint error in the tree at root, as a list of messages."""
    doc_path = root / DOC
    if not doc_path.is_file():
        return [f"{DOC}: missing"]
    rows, retired, errors = parse_doc(doc_path.read_text(encoding="utf-8"))
    errors += lint_doc(rows, retired)
    return errors


def main(argv=None):
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(errors="replace")
    parser = argparse.ArgumentParser(description="The smoke doc's lint and its PR gate.")
    sub = parser.add_subparsers(dest="mode", required=True)
    lint = sub.add_parser("lint", help="check the doc, the register and the results files")
    lint.add_argument("--write", action="store_true", help="regenerate the register's summary block")
    lint.add_argument("--complete", nargs="+", default=[], metavar="FILE",
                      help="results files that must cover every row of the doc")
    args = parser.parse_args(argv)
    if args.mode == "lint":
        errors = run_lint(ROOT, write=args.write, complete=args.complete)
        for e in errors:
            print(e, file=sys.stderr)
        if errors:
            print(f"smoke lint: {len(errors)} error(s)", file=sys.stderr)
            return 1
        print("smoke lint OK")
        return 0
    return 2


if __name__ == "__main__":
    sys.exit(main())
