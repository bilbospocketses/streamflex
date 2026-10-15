#!/usr/bin/env python3
"""Adds up one pass's shards (run.sh <label> [leaks] K/N [timing|rest]) into the pass's one result,
or proves that a pass's timing set and rest set run every check file once between them.

    merge.py [--leg NAME] CONSOLE...
    merge.py --cover [--leg NAME] --timing CONSOLE... --rest CONSOLE...

Each CONSOLE is the saved console of one shard. Prints the pass's result lines (PASS, FAIL and
LEAK, in the order an unsharded run gives them), a line counting them, and "N failed", and exits
non-zero when anything failed: 1 for failed checks or leaks, 2 when the consoles are not one
complete run.

Complete is defined from what the harness declared, never from a list kept here. Before it runs
anything, each shard prints the check files its selection runs (all of them, the timing-sensitive
ones or the rest) and the shard that runs each (its SHARD DECLARED lines), and each file the
selection leaves out (SHARD LEFT OUT); it brackets each check file's results with SHARD BEGIN and
SHARD END (which carries how many results run.sh counted), and ends with SHARD DONE. The consoles
are one complete run when:
  - they are shards 1 to N of one N, each once, of one mode and one selection, built from one
    source tree, and every one declares the same check files with the same plan, and leaves out
    the same ones;
  - every shard reached SHARD DONE, every section it began it ended, each section holds as many
    results as run.sh counted, and no result is outside a section;
  - each declared check file ran in exactly one shard, the one the plan named, or in every shard
    when it is marked to run in all of them, and no shard ran one that is not declared;
  - no case (a result's text) came from two shards.
Each way the consoles fall short is an INCOMPLETE line, and each counts in "N failed", so an
incomplete set can never read "0 failed".

A section every shard runs ((build), 00-harness.sh, 90-titles-fit.sh, 95-every-run.sh) gives one
set of results: its case i passes when it passed in every shard. The leak pass's LEAK lines are
listed once per run they name, since a run that every shard makes can leak in each.

--cover reads one pass's timing set and its rest set, each the consoles of all its shards, and
merges each as above. They cover the pass when each set is complete with its own selection, both
are of one mode and one source tree and list the same check files, and each check file ran in
exactly one of them, or in every shard of both when it is marked to run in all of them; no case
may come from both. Prints the two sets' results as one pass (a section both ran, once, failing
when it failed in either), an INCOMPLETE line for each way they fall short, a line counting them,
and "N failed", and exits as a merge does.
"""
import re
import sys

HEADER = re.compile(r'^SHARD (\d+) of (\d+): label (.+), mode (\S+)(?:, selection (\S+))?, tree (\S+), '
                    r'planned (\d+) s$')
DECLARED = re.compile(r'^SHARD DECLARED (.+) (every|\d+)$')
LEFT_OUT = re.compile(r'^SHARD LEFT OUT (.+)$')
BEGIN = re.compile(r'^SHARD BEGIN (.+)$')
END = re.compile(r'^SHARD END (.+) (\d+)$')
DONE = re.compile(r'^SHARD DONE (\d+) of (\d+): (\d+) failed$')
FAILED = re.compile(r'^(\d+) failed$')
RESULT = re.compile(r'^(PASS|FAIL|LEAK)  (.*)$')
LEAKS = '(leaks)'


class Shard:
    """One shard's console, read into its header, its declarations and its sections."""

    def __init__(self, path):
        self.path = path
        self.problems = []
        self.k = self.n = None
        self.label = self.mode = self.selection = self.tree = None
        self.declared = []          # (name, 'every' or a shard number), in the order printed
        self.left_out = []          # the check files the selection left out, in the order printed
        self.sections = {}          # name -> [(verdict, text)]
        self.done = False

    def problem(self, text):
        self.problems.append(f'{self.path}: {text}')

    def read(self):
        try:
            with open(self.path, encoding='utf-8', errors='replace') as f:
                lines = [line.rstrip('\r\n').lstrip('﻿') for line in f]
        except OSError as e:
            self.problem(f'cannot be read ({e.strerror})')
            return
        section = None              # the open section's name
        given = []                  # the results it has given so far
        last_failed = None
        for number, line in enumerate(lines, 1):
            result = RESULT.match(line)
            if self.done:
                if result or line.startswith('SHARD '):
                    self.problem(f'line {number} comes after SHARD DONE: {line}')
                continue
            if result:
                if section is None:
                    self.problem(f'line {number} is a result outside any section: {line}')
                else:
                    given.append((result.group(1), result.group(2)))
                    self.sections[section].append(given[-1])
                continue
            failed = FAILED.match(line)
            if failed:
                last_failed = int(failed.group(1))
                continue
            if not line.startswith('SHARD '):
                continue
            header = HEADER.match(line)
            if header:
                if self.k is not None:
                    self.problem(f'line {number} is a second SHARD header')
                    continue
                self.k, self.n = int(header.group(1)), int(header.group(2))
                self.label, self.mode = header.group(3), header.group(4)
                # A console from before selections names none: it ran every check file
                self.selection, self.tree = header.group(5) or 'all', header.group(6)
                continue
            if self.k is None:
                self.problem(f'line {number} comes before the SHARD header: {line}')
                continue
            declared, begin = DECLARED.match(line), BEGIN.match(line)
            end, done, left_out = END.match(line), DONE.match(line), LEFT_OUT.match(line)
            if declared or left_out:
                name = (declared or left_out).group(1)
                if self.sections:
                    self.problem(f'line {number} declares or leaves out a check file after the run began')
                elif any(n == name for n, _ in self.declared) or name in self.left_out:
                    self.problem(f'line {number} declares or leaves out {name} a second time')
                elif declared:
                    self.declared.append((name, declared.group(2)))
                else:
                    self.left_out.append(name)
            elif begin:
                if section is not None:
                    self.problem(f'line {number} begins {begin.group(1)} inside {section}, which never ended')
                section, given = begin.group(1), []
                if section in self.sections:
                    self.problem(f'line {number} begins {section} a second time')
                else:
                    self.sections[section] = []
            elif end:
                if section != end.group(1):
                    self.problem(f'line {number} ends {end.group(1)}, which is not the open section')
                    continue
                if int(end.group(2)) != len(given):
                    self.problem(f'{section} gave {end.group(2)} results, but {len(given)} are here')
                section = None
            elif done:
                if (int(done.group(1)), int(done.group(2))) != (self.k, self.n):
                    self.problem(f'line {number} ends shard {done.group(1)} of {done.group(2)}, '
                                 f'but the header says {self.k} of {self.n}')
                if section is not None:
                    self.problem(f'{section} began but never ended')
                    section = None
                said = int(done.group(3))
                counted = sum(1 for results in self.sections.values()
                              for verdict, _ in results if verdict != 'PASS')
                if last_failed != said or said != counted:
                    self.problem(f'it says {said} failed (and "{last_failed} failed" before that), '
                                 f'but has {counted} FAIL and LEAK lines')
                self.done = True
            else:
                self.problem(f'line {number} is not a SHARD line merge.py knows: {line}')
        if self.k is None:
            self.problem("is not a shard's console: it has no SHARD header")
            return
        if section is not None:
            self.problem(f'stops inside {section}: the shard crashed or was stopped there')
        if not self.done:
            self.problem(f'shard {self.k} of {self.n} never reached SHARD DONE: it crashed, was '
                         'stopped, or its console was cut short')


def merge(paths):
    """Returns the merged results, the problems that make the set incomplete, and shard 1."""
    sections, problems, first = merge_sections(paths)
    return [result for _, _, results in sections for result in results], problems, first


def merge_sections(paths):
    """Returns the merged results by section, (name, 'every' or the shard planned, results) in the
    order declared, the problems that make the set incomplete, and shard 1."""
    shards, problems = [], []
    for path in paths:
        shard = Shard(path)
        shard.read()
        problems += shard.problems
        if shard.k is not None:
            shards.append(shard)
    if not paths:
        problems.append('no shard consoles were given')
    if not shards:
        return [], problems, None

    first = min(shards, key=lambda s: s.k)
    n = first.n
    for shard in shards:
        if shard.n != n:
            problems.append(f'{shard.path}: is a shard of {shard.n}, but {first.path} is one of {n}')
        if shard.mode != first.mode:
            problems.append(f'{shard.path}: ran mode {shard.mode}, but {first.path} ran {first.mode}')
        if shard.selection != first.selection:
            problems.append(f'{shard.path}: ran selection {shard.selection}, but {first.path} ran '
                            f'{first.selection}')
        if shard.tree != first.tree:
            problems.append(f'{shard.path}: built tree {shard.tree}, but {first.path} built {first.tree}')
        if shard.declared != first.declared or shard.left_out != first.left_out:
            problems.append(f'{shard.path}: declares or leaves out other check files, or has another plan '
                            f'for them, than {first.path}')
    by_k = {}
    for shard in shards:
        if shard.k in by_k:
            problems.append(f'shard {shard.k} was given twice: {by_k[shard.k].path} and {shard.path}')
        elif not 1 <= shard.k <= n:
            problems.append(f'{shard.path}: shard {shard.k} is not one of 1 to {n}')
        else:
            by_k[shard.k] = shard
    for k in range(1, n + 1):
        if k not in by_k:
            problems.append(f'shard {k} of {n} is missing')

    present = [by_k[k] for k in sorted(by_k)]
    declared_names = {name for name, _ in first.declared}
    for shard in present:
        for name in shard.sections:
            if name not in declared_names:
                problems.append(f'shard {shard.k} ran {name}, which is not declared')

    sections = []
    case_shards = {}
    for name, where in first.declared:
        ran = [shard for shard in present if name in shard.sections]
        merged = []
        sections.append((name, where, merged))
        if where == 'every':
            for shard in present:
                if shard not in ran:
                    problems.append(f'{name} runs in every shard, but shard {shard.k} did not run it')
            merged += combine_every(name, [shard.sections[name] for shard in ran],
                                    [f'shard {shard.k}' for shard in ran], problems)
            continue
        if not (where.isdigit() and 1 <= int(where) <= n):
            problems.append(f'{name} is planned for shard {where}, which is not one of 1 to {n}')
        if not ran:
            problems.append(f'{name} ran in no shard')
            continue
        if len(ran) > 1:
            problems.append(f'{name} ran in more than one shard: ' + ', '.join(str(s.k) for s in ran))
        else:
            if where.isdigit() and ran[0].k != int(where):
                problems.append(f'{name} ran in shard {ran[0].k}, but the plan gave it to shard {where}')
            for verdict, text in ran[0].sections[name]:
                case_shards.setdefault(text, set()).add(ran[0].k)
        for shard in ran:
            merged += shard.sections[name]
    for text, ks in case_shards.items():
        if len(ks) > 1:
            problems.append(f'the case "{text}" ran in more than one shard: '
                            + ', '.join(str(k) for k in sorted(ks)))
    return sections, problems, first


def combine_every(name, runs, where, problems, kind='shards'):
    """Returns one set of results for the section NAME, which each of RUNS (one list of results for
    each of the shards or sets, KIND, named in WHERE) ran: LEAK lines once per run they name, and
    otherwise case i, failing when it failed in any of them. Results that do not line up are all
    kept, with a problem."""
    merged = []
    if name == LEAKS:
        seen = set()
        for results in runs:
            for verdict, text in results:
                run = text.split(':', 1)[0]
                if run not in seen:
                    seen.add(run)
                    merged.append((verdict, text))
        return merged
    counts = {len(results) for results in runs}
    if len(counts) > 1:
        problems.append(f'{name} gave a different number of results in different {kind}: '
                        + ', '.join(f'{w} {len(results)}' for w, results in zip(where, runs)))
        for results in runs:
            merged += results
        return merged
    for i in range(counts.pop() if counts else 0):
        cases = [results[i] for results in runs]
        failing = [case for case in cases if case[0] != 'PASS']
        merged.append(failing[0] if failing else cases[0])
    return merged


def cover(timing_paths, rest_paths):
    """Returns the timing and rest sets' results as one pass's, the problems that keep them from
    covering every check file once, and how many check files each set ran."""
    problems, sets = [], {}
    for want, paths in (('timing', timing_paths), ('rest', rest_paths)):
        sections, set_problems, first = merge_sections(paths)
        problems += [f'the {want} set: {text}' for text in set_problems]
        if first is None:
            continue
        if first.selection != want:
            problems.append(f'the {want} set ran selection {first.selection}')
        sets[want] = (sections, first)
    if len(sets) < 2:
        return [], problems, {}
    (t_sections, t), (r_sections, r) = sets['timing'], sets['rest']
    if t.mode != r.mode:
        problems.append(f'the timing set ran mode {t.mode}, but the rest set ran {r.mode}')
    if t.tree != r.tree:
        problems.append(f'the timing set built tree {t.tree}, but the rest set built {r.tree}')

    # Where each check file ran: 'every', 'ran' or 'left out', in each set
    where = {}
    for want, first in (('timing', t), ('rest', r)):
        for name, w in first.declared:
            if not name.startswith('('):
                where.setdefault(name, {})[want] = 'every' if w == 'every' else 'ran'
        for name in first.left_out:
            where.setdefault(name, {})[want] = 'left out'
    counts = {'timing': 0, 'rest': 0, 'every': 0}
    for name in sorted(where):
        tw, rw = where[name].get('timing'), where[name].get('rest')
        if tw is None or rw is None:
            problems.append(f'{name} is not among the check files the {"timing" if tw is None else "rest"} '
                            'set lists')
        elif 'every' in (tw, rw):
            if tw != rw:
                problems.append(f'{name} runs in every shard of one set, but not of the other')
            counts['every'] += 1
        elif tw == rw == 'ran':
            problems.append(f'{name} ran in both sets')
        elif tw == rw:
            problems.append(f'{name} ran in neither set')
        else:
            counts['timing' if tw == 'ran' else 'rest'] += 1

    # The results as one pass: (build), the check files in name order, then (leaks)
    by_name = {}
    for want, sections in (('timing', t_sections), ('rest', r_sections)):
        for name, w, results in sections:
            by_name.setdefault(name, []).append((want, w, results))
    order = sorted(by_name, key=lambda name: (name != '(build)', name == LEAKS, name))
    merged, case_sets = [], {}
    for name in order:
        ran = by_name[name]
        if all(w == 'every' for _, w, _ in ran):
            merged += combine_every(name, [results for _, _, results in ran],
                                    [f'the {want} set' for want, _, _ in ran], problems, 'sets')
            continue
        for want, _, results in ran:
            merged += results
            for _, text in results:
                case_sets.setdefault(text, set()).add(want)
    for text, wants in case_sets.items():
        if len(wants) > 1:
            problems.append(f'the case "{text}" ran in both sets')
    return merged, problems, counts


def main(argv):
    if hasattr(sys.stdout, 'reconfigure'):
        sys.stdout.reconfigure(encoding='utf-8', errors='replace')
    leg, paths = None, list(argv)
    if paths[:1] == ['--cover']:
        return main_cover(paths[1:])
    if paths[:1] == ['--leg']:
        if len(paths) < 2:
            print('usage: merge.py [--leg NAME] CONSOLE...')
            return 2
        leg, paths = paths[1], paths[2:]
    merged, problems, first = merge(paths)
    if leg is None:
        leg = re.sub(r'-s\d+$', '', first.label) if first else 'the pass'
    selection = f', selection {first.selection}' if first and first.selection != 'all' else ''
    return report(leg, merged, problems,
                  f'from {len(paths)} consoles for {first.n if first else "?"} shards{selection}')


def main_cover(argv):
    """merge.py --cover [--leg NAME] --timing CONSOLE... --rest CONSOLE..."""
    leg, groups, group, usable = None, {'--timing': [], '--rest': []}, None, True
    i = 0
    while i < len(argv):
        arg = argv[i]
        if arg == '--leg' and i + 1 < len(argv):
            leg = argv[i + 1]
            i += 2
            continue
        if arg in groups:
            group = groups[arg]
        elif group is not None and not arg.startswith('--'):
            group.append(arg)
        else:
            usable = False
        i += 1
    if not usable or not groups['--timing'] or not groups['--rest']:
        print('usage: merge.py --cover [--leg NAME] --timing CONSOLE... --rest CONSOLE...')
        return 2
    merged, problems, counts = cover(groups['--timing'], groups['--rest'])
    if leg is None:
        leg = 'the pass'
    files = (f'{counts.get("timing", "?")} check files in the timing set, {counts.get("rest", "?")} in the '
             f'rest set, {counts.get("every", "?")} in every shard of both')
    consoles = len(groups['--timing']) + len(groups['--rest'])
    return report(leg, merged, problems, f'from {consoles} consoles: {files}', ok_word='covered')


def report(leg, merged, problems, source, ok_word='complete'):
    """Prints the results, the problems, the line counting them and "N failed"; returns the exit code."""
    for verdict, text in merged:
        print(f'{verdict}  {text}')
    for text in problems:
        print(f'INCOMPLETE  {text}')
    passed = sum(1 for verdict, _ in merged if verdict == 'PASS')
    failed = sum(1 for verdict, _ in merged if verdict == 'FAIL')
    leaked = sum(1 for verdict, _ in merged if verdict == 'LEAK')
    state = ok_word if not problems else f'INCOMPLETE ({len(problems)} problems)'
    print(f'{leg}: {passed} PASS, {failed} FAIL, {leaked} LEAK, {source}, {state}')
    print(f'{failed + leaked + len(problems)} failed')
    if problems:
        return 2
    return 1 if failed + leaked else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
