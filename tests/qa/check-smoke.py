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
SUMMARY_BEGIN = "<!-- summary:begin -->"
SUMMARY_END = "<!-- summary:end -->"
# qa-harness picks its lane's rows mechanically by these exact cells, so a near miss would drop a row silently
PLANNED = "qa-harness item 75, planned"
QA_HINT_RE = re.compile(r"qa[-\s]?harness|\bitem\s*#?\s*75\b|five-desktop", re.I)
ARC_RE = re.compile(r"^five-desktop lane, arc [A-Za-z0-9][A-Za-z0-9-]*$")
HEADLESS_RE = re.compile(r"`(\d\d-[a-z0-9-]+\.sh)` run `([A-Za-z0-9_.-]+)`")
CTEST_RE = re.compile(r"`(test_[a-z0-9_]+\.c)` `([A-Za-z_][A-Za-z0-9_]*)`")
RESULTS_HEADER = "| Row | Win | Ubuntu | Kubuntu | Fedora | Fedora-KDE |"
RESULTS_NAME_RE = re.compile(r"^tests/qa/results/v\d+\.\d+\.\d+\.md$")
CELL_RE = re.compile(r"^(?:x|-|[FS][1-9]\d*|x \(v\d+\.\d+\.\d+\))$")
CARRY_RE = re.compile(r"^x \(v(\d+)\.(\d+)\.(\d+)\)$")
RESULTS_VERSION_RE = re.compile(r"(?:^|/)v(\d+)\.(\d+)\.(\d+)\.md$")
ENTRY_RE = re.compile(r"^- \*\*([FS][1-9]\d*)\*\*")
ENTRY_SECTION = {"F": "Findings", "S": "Not measured"}


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


class Half(NamedTuple):
    row: str
    tag: "str | None"
    owner: str
    where: str
    line: int


def parse_register(text):
    """Return (halves, errors) from the table under '## Every row'."""
    halves, errors = [], []
    section = None
    for n, line in enumerate(text.splitlines(), 1):
        if line.startswith("## "):
            section = line[3:].strip()
            continue
        if section != "Every row" or not line.startswith("|"):
            continue
        c = cells(line)
        if not c or c[0] == "Row" or (c[0] and set(c[0]) <= set("-: ")):
            continue
        if len(c) != 4:
            errors.append(f"{REGISTER}:{n}: a register line has four cells (Row, Platforms, Owner, Where); write a literal pipe as \\|")
            continue
        rid, platforms, owner, where = c
        if not ID_RE.match(rid):
            errors.append(f"{REGISTER}:{n}: {rid!r} is not a valid id")
            continue
        tags = TAG_RE.findall(platforms)
        tag = tags[0] if len(tags) == 1 and tags[0] in TAGS and platforms == f"`[{tags[0]}]`" else None
        if tag is None:
            errors.append(f"{REGISTER}:{n}: {rid}: Platforms must be exactly one tag, written `[Linux]`")
        if owner not in OWNERS:
            errors.append(f"{REGISTER}:{n}: {rid}: owner {owner!r} is not one of {', '.join(OWNERS)}")
        if not where:
            errors.append(f"{REGISTER}:{n}: {rid}: 'Where it runs, or why not' is empty")
        halves.append(Half(rid, tag, owner, where, n))
    return halves, errors


def check_evidence(half, root):
    """A headless or unit-test credit must name evidence that exists in the tree."""
    errors = []
    if half.owner == "streamflex-headless":
        pairs = HEADLESS_RE.findall(half.where)
        if not pairs:
            return [f"{REGISTER}:{half.line}: {half.row}: a headless half names its check as `NN-name.sh` run `run-name`"]
        for check, run in pairs:
            path = root / "tests/headless/checks" / check
            if not path.is_file():
                errors.append(f"{REGISTER}:{half.line}: {half.row}: headless check {check} does not exist")
            # run_keys_at takes a size before the run name; a comment never names a run
            elif not re.search(rf"^[^#\n]*\brun_(?:keys_at\s+\S+|(?!keys_at\b)[a-z_]+)\s+{re.escape(run)}(?=\s|$)",
                               path.read_text(encoding="utf-8"), re.M):
                errors.append(f"{REGISTER}:{half.line}: {half.row}: headless check {check} has no run named {run}")
    elif half.owner == "streamflex-ctest":
        pairs = CTEST_RE.findall(half.where)
        if not pairs:
            return [f"{REGISTER}:{half.line}: {half.row}: a unit test half names its test as `test_file.c` `function`"]
        for test, function in pairs:
            path = root / "tests" / test
            if not path.is_file():
                errors.append(f"{REGISTER}:{half.line}: {half.row}: unit test file {test} does not exist")
            elif not re.search(rf"^\s*(?:static\s+)?void\s+{re.escape(function)}\s*\(", path.read_text(encoding="utf-8"), re.M):
                errors.append(f"{REGISTER}:{half.line}: {half.row}: unit test file {test} defines no function {function}")
    return errors


def lint_register(rows, halves, root):
    errors = []
    by_row = {}
    for h in halves:
        by_row.setdefault(h.row, []).append(h)
    known = {r.id for r in rows}
    for h in halves:
        if h.row not in known:
            errors.append(f"{REGISTER}:{h.line}: {h.row} is not a row of the smoke doc")
    for r in rows:
        if r.tag is None:
            continue
        want = TAGS[r.tag]
        seen = {}
        for h in by_row.get(r.id, []):
            if h.tag is None:
                continue
            for desktop in TAGS[h.tag]:
                seen[desktop] = seen.get(desktop, 0) + 1
        for desktop in DESKTOPS:
            if desktop in want and desktop not in seen:
                errors.append(f"{REGISTER}: {r.id}: no half covers {desktop}")
            elif desktop in want and seen[desktop] > 1:
                errors.append(f"{REGISTER}: {r.id}: {desktop} is covered by more than one half")
            elif desktop not in want and desktop in seen:
                errors.append(f"{REGISTER}: {r.id}: {desktop} is outside the row's tag [{r.tag}]")
    for h in halves:
        waiting = f"{REGISTER}:{h.line}: {h.row}: a half waiting on qa-harness is owner manual and reads exactly '{PLANNED}'"
        if h.owner == "qa-harness":
            if not ARC_RE.match(h.where):
                errors.append(f"{REGISTER}:{h.line}: {h.row}: a qa-harness half reads exactly 'five-desktop lane, arc <name>'")
        elif h.where == PLANNED:
            if h.owner != "manual":
                errors.append(waiting)
        elif QA_HINT_RE.search(h.where):
            errors.append(f"{waiting}; a reason that only mentions qa-harness must be reworded")
        errors += check_evidence(h, root)
    return errors


def summary(rows, halves):
    """The register's summary block, markers included."""
    count = {o: 0 for o in OWNERS}
    verdicts = {o: 0 for o in OWNERS}
    for h in halves:
        if h.owner in count:
            count[h.owner] += 1
            verdicts[h.owner] += len(TAGS[h.tag]) if h.tag else 0
    total_h, total_v = sum(count.values()), sum(verdicts.values())
    auto_h = sum(count[o] for o in AUTOMATED)
    auto_v = sum(verdicts[o] for o in AUTOMATED)
    pct = f"{100 * auto_v // total_v} %" if total_v else "n/a"
    lines = [
        SUMMARY_BEGIN,
        "<!-- Generated by `python tests/qa/check-smoke.py lint --write`. Do not edit by hand. -->",
        "",
        "| Measure | Count |",
        "|---|---|",
        f"| Rows | {len(rows)} |",
        f"| Row halves | {total_h} |",
        f"| Verdicts per release (row x desktop) | {total_v} |",
    ]
    lines += [f"| `{o}` halves | {count[o]} ({verdicts[o]} verdicts) |" for o in OWNERS]
    lines += [f"| **Automated today** | {auto_h} of {total_h} halves; {auto_v} of {total_v} verdicts ({pct}) |", "", SUMMARY_END]
    return "\n".join(lines)


def replace_summary(text, block):
    """Return (text with the block replaced, errors). None when the markers are missing."""
    start, end = text.find(SUMMARY_BEGIN), text.find(SUMMARY_END)
    if start < 0 or end < start:
        return None, [f"{REGISTER}: the summary markers {SUMMARY_BEGIN} and {SUMMARY_END} are missing"]
    return text[:start] + block + text[end + len(SUMMARY_END):], []


def parse_results(text, path):
    """Return (table, errors). table maps a row id to (its five cells, its line). Only the table
    under RESULTS_HEADER is read; the header's own tables (desktops, images) are not results."""
    table, entries, errors = {}, {}, []
    section, in_table, header_seen = None, False, False
    for n, line in enumerate(text.splitlines(), 1):
        if line.startswith("## "):
            section, in_table = line[3:].strip(), False
            continue
        if section in ENTRY_SECTION.values():
            m = ENTRY_RE.match(line)
            if m:
                eid = m.group(1)
                if section != ENTRY_SECTION[eid[0]]:
                    errors.append(f"{path}:{n}: {eid} belongs under ## {ENTRY_SECTION[eid[0]]}")
                elif eid in entries:
                    errors.append(f"{path}:{n}: {eid} has two entries")
                else:
                    entries[eid] = section
            continue
        if line.startswith("| Row |"):
            header_seen = in_table = True
            if line.strip() != RESULTS_HEADER:
                errors.append(f"{path}:{n}: the results table's header must be exactly {RESULTS_HEADER}")
            continue
        if not in_table:
            continue
        if not line.startswith("|"):
            in_table = False
            continue
        c = cells(line)
        if c and c[0] and set(c[0]) <= set("-: "):
            continue
        if len(c) != 6 or not ID_RE.match(c[0]):
            errors.append(f"{path}:{n}: a results line is | <id> | Win | Ubuntu | Kubuntu | Fedora | Fedora-KDE |")
            continue
        if c[0] in table:
            errors.append(f"{path}:{n}: {c[0]} appears more than once")
        for desktop, cell in zip(DESKTOPS, c[1:]):
            if cell and not CELL_RE.match(cell):
                errors.append(f"{path}:{n}: {c[0]}: {desktop}: {cell!r} is not a results cell (x, F<n>, S<n>, -, x (vA.B.C))")
        table[c[0]] = (c[1:], n)
    if not header_seen:
        errors.append(f"{path}: no results table (its header is {RESULTS_HEADER})")
    for rid, (row_cells, n) in table.items():
        for cell in row_cells:
            if re.match(r"^[FS][1-9]\d*$", cell) and entries.get(cell) != ENTRY_SECTION[cell[0]]:
                errors.append(f"{path}:{n}: {cell} has no entry under ## {ENTRY_SECTION[cell[0]]}")
    return table, errors


def lint_results(path, table, rows, retired, complete):
    """Form checks for every results file; completeness only for the files a PR adds or changes."""
    errors = []
    by_id = {r.id: r for r in rows}
    retired_ids = {rid for rid, _ in retired}
    own = RESULTS_VERSION_RE.search(path)
    own = tuple(map(int, own.groups())) if own else None
    for rid, (row_cells, n) in table.items():
        for desktop, cell in zip(DESKTOPS, row_cells):
            m = CARRY_RE.match(cell)
            if m and not (own and own[:2] == tuple(map(int, m.groups()[:2])) and own[2] > int(m.group(3))):
                errors.append(f"{path}:{n}: {rid}: {desktop}: {cell} breaks the carry-forward rule: only a patch release "
                              "carries a pass forward, and only from an earlier patch of its own major.minor")
    for rid, (row_cells, n) in table.items():
        if rid not in by_id and rid not in retired_ids:
            errors.append(f"{path}:{n}: {rid} is not a row of the smoke doc")
            continue
        r = by_id.get(rid)
        if r is None or r.tag is None:
            continue
        for desktop, cell in zip(DESKTOPS, row_cells):
            inside = desktop in TAGS[r.tag]
            if cell == "-" and inside:
                errors.append(f"{path}:{n}: {rid}: {desktop} is inside the row's tag [{r.tag}]; it needs a verdict, not -")
            elif complete and inside and not cell:
                errors.append(f"{path}:{n}: {rid}: {desktop} is empty")
            elif complete and not inside and cell != "-":
                errors.append(f"{path}:{n}: {rid}: {desktop} is outside the row's tag [{r.tag}]; write -")
    if complete:
        for r in rows:
            if r.id not in table:
                errors.append(f"{path}: {r.id} has no line")
    return errors


def run_lint(root, write=False, complete=()):
    """Every lint error in the tree at root, as a list of messages."""
    doc_path, reg_path = root / DOC, root / REGISTER
    if not doc_path.is_file():
        return [f"{DOC}: missing"]
    rows, retired, errors = parse_doc(doc_path.read_text(encoding="utf-8"))
    errors += lint_doc(rows, retired)
    if not reg_path.is_file():
        return errors + [f"{REGISTER}: missing"]
    reg_text = reg_path.read_text(encoding="utf-8")
    halves, reg_errors = parse_register(reg_text)
    errors += reg_errors + lint_register(rows, halves, root)
    new_text, sum_errors = replace_summary(reg_text, summary(rows, halves))
    errors += sum_errors
    if new_text is not None and new_text.replace("\r\n", "\n") != reg_text.replace("\r\n", "\n"):
        if write:
            reg_path.write_text(new_text, encoding="utf-8", newline="\n")
        else:
            errors.append(f"{REGISTER}: the summary is stale; run python tests/qa/check-smoke.py lint --write")
    results_dir = root / RESULTS_DIR
    files = sorted(results_dir.glob("v*.md")) if results_dir.is_dir() else []
    # a relative --complete path is read from the repo root, wherever the command runs
    wanted = {(Path(c) if Path(c).is_absolute() else root / c).resolve() for c in complete}
    for path in files:
        rel = path.relative_to(root).as_posix()
        if not RESULTS_NAME_RE.match(rel):
            errors.append(f"{rel}: a results file is named v<major>.<minor>.<patch>.md")
            continue
        table, res_errors = parse_results(path.read_text(encoding="utf-8"), rel)
        errors += res_errors + lint_results(rel, table, rows, retired, path.resolve() in wanted)
    for c in sorted(wanted - {p.resolve() for p in files}):
        errors.append(f"{c}: not a results file under {RESULTS_DIR}")
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
                      help="results files that must cover every row of the doc (a relative path is from the repo root)")
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
