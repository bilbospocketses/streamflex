# One tracked smoke doc, a coverage register and a CI gate: design

**Date:** 2026-10-10
**Status:** approved in brainstorming, awaiting spec review
**Covers:** todo item 28 (the smoke doc with official test tracking), and item 31 (the american-spelling v1.0.3 pin) riding in its build PR.
**Model:** ws-scrcpy-web's `docs/smoke-tests/smoke-test.md`, `automation-coverage.md`, `.github/workflows/smoke-coverage.yml` and `scripts/check-smoke-coverage.mjs`.
**Consumer:** qa-harness item 75 (P9, the scripted five-desktop StreamFlex lane), which scripts against this doc's row ids. qa-harness reviewed the id scheme, the tags, the register line and the results format during this brainstorm, and its requirements are folded in below.

## Context

StreamFlex has only per-release run sheets. `tests/qa/checklists/` holds four of them: `windows-hands-on.md` and `ubuntu-hands-on.md` for 3a (v0.3.x), and `windows-3b-hands-on.md` and `ubuntu-3b-hands-on.md` for 3b (v0.4.x). About 1,060 lines in all, they hold roughly 60 steps (W0-W10, U0-U9, B0-B14 with suffixes), and each step bundles many assertions under one PASS/FAIL/BLOCKED verdict. They cover Windows 11 and Ubuntu GNOME only. Results went to ad-hoc `results.md` files in ClaudeScratch, so nothing in the repo records what passed where.

The headless harness (`tests/headless/`, 22 check files) proves much of the app in Debian and Fedora containers on every PR, but nothing maps its checks to the hands-on steps, so no one can say which steps a release still has to run by hand.

Nothing outside `tests/qa/` references a W, U or B step id (checked by grep across `design/`, `docs/`, `tests/headless/`, `.github/` and the README), so the old ids can be retired without breaking a reference.

## Decisions

| Question | Decision |
|---|---|
| Where the doc lives | **`tests/qa/`**: `tests/qa/smoke-test.md` and `tests/qa/automation-coverage.md`. `docs/` is the published Pages site and would publish them; `tests/qa/` already holds the guest scripts the rows use. |
| The old checklists | **Folded in, then deleted.** Their step ids are not carried over. |
| Row granularity | **One observable claim per row** (qa-harness's rule, "the row is the contract"). A step that bundles several behaviors becomes several rows. |
| Id scheme | **`<module>.<n>`**, optionally followed by **one** lowercase letter (`5.3a`) **or** `-` plus `[a-z][a-z0-9-]*` (`4.2-system-gui`), never both. Stable and gappy: never renumbered, never reused. |
| Platform tags | **Nine, one per row:** `[Win]`, `[Ubuntu]` (GNOME), `[Kubuntu]`, `[Fedora]` (Workstation), `[Fedora-KDE]`, and the groups `[GNOME]` (Ubuntu + Fedora), `[KDE]` (Kubuntu + Fedora-KDE), `[Linux]` (all four Linux desktops), `[All]` (all five). A behavior whose expectation differs by desktop is split into one row per expectation. |
| Coverage register | **One line per row half:** a set of desktops sharing one owner. Owners: `streamflex-ctest`, `streamflex-headless`, `qa-harness`, `manual`. |
| Results | **A committed file per release**, `tests/qa/results/vX.Y.Z.md`: one line per row, one column per desktop. |
| Who writes qa-harness's cells | **qa-harness emits them.** It keeps a JSON record per run and renders the cells for the halves it owns, plus findings lines, and hands that fragment to the StreamFlex session through ClaudeScratch. StreamFlex commits it. qa-harness never pushes to this repo. |
| The CI gate | **ws-scrcpy-web's rule plus a structure lint**, in a workflow of its own, required on master. |
| Gate language | **Python 3.12, standard library only.** StreamFlex's CI already runs Python and has no Node. |

## The smoke doc: `tests/qa/smoke-test.md`

### Header

- **Smoke target:** the version a pass is run against, bumped at each release.
- **How to mark a row:** the doc's `☐` boxes are for working through a pass by hand (`x` pass, `F` fail, `-` not applicable). The record of a release is its results file, not the doc.
- **The nine tags** and the desktops each one expands to.
- **The rule:** one observable claim per row; a row with no verdict is never green.

### Pre-flight: one block per desktop

Windows 11, Ubuntu GNOME, Kubuntu, Fedora Workstation, Fedora KDE. Each block says:

- how the build gets onto the desktop (the Windows zip; the `.deb` on Ubuntu and Kubuntu; a source build on the Fedoras until StreamFlex ships an `.rpm`, which todo item 25 leaves open);
- where the config lives (`C:\StreamFlex` beside the zip's executable; `~/.config/streamflex` on Linux);
- the input method (SendInput and the ViGEm Xbox 360 pad on Windows; HMP `sendkey` and `tests/qa/virtual-gamepad.py` on Linux), including the facts the old README recorded: the Windows Menu key is `VK_APPS` with `KEYEVENTF_EXTENDEDKEY`, never `VK_MENU`; held keys need repeated key-downs under SendInput; which HMP key name reaches SDL as which Menu code;
- how logs are captured (`-d`, which also records the `Video:` line naming the driver and renderer).

Pre-flight is setup, not tests. The old W0, U0 and B0 setup steps move here, as do the still-relevant points of `tests/qa/README.md`'s "What the qa-harness session must confirm" and "Open points" lists. Settled points are dropped.

### Modules

Numbered by feature area. This list is the starting cut; folding the rows in may merge or split a module, and the plan records the final list.

1. Install and first launch: zip contents (no DLL, no VC++ runtime needed), the `.deb` install, launch from the app menu, the config location.
2. Home grid and navigation: the grid, menus and submenus, titles, scrolling.
3. Launching apps: launch and return, the pad ignored behind a launched app, non-ASCII paths.
4. Input: Start opens settings out of the box, a held Menu key opens it once, the Menu key's codes.
5. Backgrounds: color, image, slideshow, Transparent, with one row per desktop expectation (Windows sees through; GNOME shows none; KDE depends on the compositor).
6. The settings screen: opening, moving around, the save round trip, a read-only config.
7. Every setting (3b's pages), including the clock, VSync switched live and other live changes.
8. Pickers: color, font, command and file, including upper-case extensions counting and hidden files not.
9. Bindings: keys and pad buttons, the safety floor.
10. Restart: the "Restart now?" question and the restart that keeps everything.
11. The exit hotkey `[Win]`, including v0.4.1's retry when another program holds the key.
12. Quitting: clean exit, the startup and quit commands.
13. Video drivers `[Linux]`: the default Wayland driver, and `SDL_VIDEODRIVER=x11` under XWayland. This replaces the old Ubuntu checklist's pass A and pass B.

Rows are in execution order within a module, and modules are ordered to keep config resets few. The doc grows with the work: the SDL3 port, 3c and the streaming overlay (item 24) each add their rows or modules as they land.

### Row format

The same as ws-scrcpy-web's, so qa-harness's row parser carries over:

```
| ☐ <a id="t-3-2"></a> **3.2** `[All]` <name> | <steps> | <expected> |
```

- The anchor is `t-` plus the id with `.` and any suffix separator written as `-` (`5.3a` → `t-5-3a`; `4.2-system-gui` → `t-4-2-system-gui`).
- **Steps** say how to do it on each desktop the tag covers, where the mechanics differ (SendInput or uinput; `C:\StreamFlex` or `~/.config/streamflex`).
- **Expected** is the one observable claim, with the log line or frame that shows it where there is one.

### Index, retired ids, pass criteria

- **Index by module:** links to every row.
- **Retired ids:** a list of every id ever retired. The lint refuses an id on this list.
- **Global pass criteria:** a release passes when every row has a verdict on every desktop its tag covers, and none is `F`. Every `SKIP` is named, with its reason, in the results file.

## The coverage register: `tests/qa/automation-coverage.md`

### Summary (generated)

A block at the top, between `<!-- summary:begin -->` and `<!-- summary:end -->` markers, computed from the table by `check-smoke.py lint --write`: rows, row halves, halves per owner, and "automated today: N of M halves". The lint fails when the block is stale. No hand-kept counts.

### Every row

```
| Row | Platforms | Owner | Where it runs, or why not |
| 3.2 | `[Linux]` | streamflex-headless | `58-settings-pickers.sh` run `f58-hex` |
| 3.2 | `[Win]`   | qa-harness          | five-desktop lane, arc S3 |
```

- **Platforms** is one of the nine tags. A row's halves cover the desktops of the row's tag exactly once each.
- **Owner:**
  - `streamflex-ctest`: a unit test proves it; the last column names the test file and function.
  - `streamflex-headless`: the headless harness proves it; the last column names the check file and the run name (`f58-hex`). The `result` label text is not used, because it carries a changing `(exit N)` suffix.
  - `qa-harness`: the five-desktop lane proves it; the last column names the lane and arc, **never a qa-harness file path** (qa-harness is private, so a path would dangle in this public repo). Until item 75 has the arc, it says "five-desktop lane, planned".
  - `manual`: checked by hand; the last column says why it cannot be automated (a real TV or pad, a judgment by eye).
- **Where it runs, or why not** is never empty. Partial coverage is stated there in words.

**The honesty rule:** a half is credited to a check only when the check asserts the row's claim itself, not a neighbor of it. The headless harness proves the app's behavior in Debian and Fedora containers under a virtual display, not a desktop's: a desktop-specific claim (app-menu launch, Wayland focus, KDE compositing) is never `streamflex-headless`.

### Findings (optional)

Per-module notes on coverage gaps, as ws-scrcpy-web keeps them. They record; they do not queue work.

## Results: `tests/qa/results/vX.Y.Z.md`

`tests/qa/results/README.md` documents the format. The first results file is the next release's; v0.3.x and v0.4.x are not back-filled.

### Format

- **Header:** the StreamFlex tag and commit tested, the date, and one line per desktop: the OS and version, plus the base image digest when qa-harness ran it (qa-harness supplies its lock digests). The header also cites the `build-and-test` run id the automated cells come from.
- **Table:** `| Row | Win | Ubuntu | Kubuntu | Fedora | Fedora-KDE |`, one line per row id. A cell is one of:
  - `x`: pass;
  - `F<n>`: fail, with a finding id (`F1`);
  - `-`: the desktop is outside the row's tag;
  - `SKIP: NOT MEASURED -- <reason>`;
  - `x (vA.B.C)`: carried forward from an earlier release's pass (patch releases only, below).

  An empty cell is never a pass.
- **Findings:** `F1`, `F2`, ... each states what failed and what was seen, links its evidence (log or frame), and says where the fix went (a PR number or a patch release), or records the user's decision to ship with it.

### Who fills which cells

- **qa-harness halves:** pasted from qa-harness's rendered fragment.
- **`streamflex-headless` and `streamflex-ctest` halves:** `x` from the release commit's green `build-and-test` run.
- **`manual` halves:** whoever runs the pass by hand.

### The release flow

The results file is committed in the release PR (the version-bump PR), before the tag. A release with an unresolved `F` is not cut: it is fixed first, or the user decides to ship with it and the finding records that. A patch release gets its own file. It may carry forward rows its change cannot affect, written `x (vA.B.C)`, with the reason stated once in the header.

## The gate and the lint: `tests/qa/check-smoke.py`

One script, Python 3.12, standard library only, with three modes. The decisions are pure functions over parsed data (as ws-scrcpy-web's `evaluate()`), so they test without git or the network.

### `gate` (CI)

A PR that changes a watched path either changes `tests/qa/smoke-test.md` or carries, in its body,

```
<!-- smoke: none -- <why this change has no smoke impact> -->
```

A marker with an empty reason does not count. `gate` then runs the lint's completeness check on every `tests/qa/results/*.md` the PR adds or changes. The PR's file list (including the pre-rename path of a renamed file) and its **current** body are read through `gh api`, so editing the body and re-running the check sees the edit. The reason is printed as data, never as a format string.

**Watched paths:**

- `src/`, except `src/test_hooks.c` and `src/test_hooks.h`;
- `assets/` (the shipped fonts and icons);
- `config/` (the default `config.ini`, the `.desktop` file, the PKGBUILD, the Windows manifest and resources);
- `CMakeLists.txt` at the root (packaging and install rules);
- `vcpkg.json` (the Windows dependency versions).

Not watched: tests, `docs/`, `design/`, `branding/`, `.github/`. Dependabot's action bumps therefore never trip the gate. A release PR touches `CMakeLists.txt`, so it updates the doc or carries a marker such as `version bump; results in tests/qa/results/v0.5.0.md`.

### `lint` (CI and locally)

Fails on any of:

- an id that breaks the grammar, a duplicate id, or an anchor that does not match its id;
- a row with no tag, more than one tag, or a tag outside the nine;
- an id on the retired list;
- a row whose register halves do not cover its tag's desktops exactly once each;
- a register line for a row the doc does not have;
- an owner outside the four, or an empty "where" cell;
- a stale generated summary;
- in any results file: a malformed cell, an `F<n>` with no matching finding, or a `-` inside the row's tag;
- in a results file named with `--complete <file>`: a row of the doc with no line, or a cell left empty inside the row's tag. Every other results file is checked for form only, so a committed file stays frozen as the doc grows past it. In CI, `gate` passes `--complete` for each results file the PR adds or changes (it already has the PR's file list), so the release PR's file must be complete against the doc it ships with; locally, run `lint --complete tests/qa/results/vX.Y.Z.md` before opening the release PR.

### `lint --write`

Regenerates the register's summary block.

### Tests

`tests/qa/test_check_smoke.py`, unittest. Every gate verdict (not applicable, doc updated, opted out, empty reason, missing) and every lint rule has a passing and a failing case, on small fixture docs inside the test file.

### The workflow: `.github/workflows/smoke-coverage.yml`

A workflow of its own, not a job in `build.yml`, because the usual way to satisfy the gate is a body edit, which must re-run the check in seconds rather than re-run the ~30-minute build.

- `on: pull_request` to `master`, types `opened`, `synchronize`, `reopened`, `edited`.
- One job, named exactly `smoke-coverage`: check out (SHA-pinned, `persist-credentials: false`), set up Python 3.12 (the same pin as `build.yml`'s `library_check`), run the unit tests, then `lint`, then `gate`.
- Permissions: `contents: read`, `pull-requests: read`.
- A concurrency group per PR, canceling the run in progress.

### The required check

`smoke-coverage` (integration 15368, GitHub Actions) joins `build-and-test`, `CodeQL` and `Scorecard analysis` in the "Protect master" ruleset (`24046431`). The Rules API takes `PUT` and replaces the whole `rules` array, so the change resends every rule. It is made only after the build PR has merged green, so the check cannot block the PR that introduces it, and only after the user approves the exact payload.

### CONTRIBUTING.md

A short section: run `python tests/qa/check-smoke.py lint` before pushing a change to the doc, the register or a results file; when to use the `smoke: none` marker, and that it needs a real reason.

## What goes into the doc

1. **The four checklists, split to one claim per row.** Every claim a checklist made becomes a row; bookkeeping ("send back `marks.txt`") moves to pre-flight or is dropped. The build PR's body maps every old step id to its new rows (`W1 → 4.1, 4.2, ...`), so nothing is lost silently.
2. **The user-visible behavior the headless harness proves that no checklist covered:** the grid, menus, backgrounds, titles, shadows, the clock, parsing and fallbacks, chroma. Sources: `docs/configuration.md` and the 22 check files. This gives the register its real automated coverage, and leaves no headless check proving something no row names.

## PRs, in order

1. **Spec PR:** this file, on `design/smoke-doc-spec`.
2. **Plan PR:** `design/plans/2026-10-10-smoke-doc.md`, after the spec is reviewed.
3. **Build PR, one PR:** the doc, the register, `tests/qa/results/README.md` (format only, no results file yet), `check-smoke.py` and its tests, the workflow, the CONTRIBUTING section, the deleted checklists, the rewritten `tests/qa/README.md` (an index of the doc, the register, the results files and the guest scripts), and **the american-spelling pin bump** in `build.yml`'s `spelling` job: `ref: 4fe6f6e29181ae120545165643d81e601e376092 # v1.0.3`. (Verified 2026-10-10: tag `v1.0.3` is the signed tag object `04dbc527`, `verified: valid`, pointing at commit `4fe6f6e2`.) The PR changes no watched path, so it needs no marker; its own new workflow runs on it, so the doc and register must pass the lint.
4. **After the build PR merges:** the ruleset `PUT`, with the user's approval.
5. **Then the locked scheme goes to qa-harness**, and item 75's plan starts from it.

The SDL3 port follows (merge spec PR #42, write its plan). Its build PR adds the SDL3-specific rows or carries the marker.

## Building it

- Roles per the standing rules: Opus implements (`tier-implementer`, `Checkpoint mode: lone`), reviews run on Opus, fix-round re-reviews on Sonnet (`tier-rereviewer`). One implementer in the checkout.
- **The folding is the bulk of the work:** a few hundred rows from about 1,060 checklist lines and 22 check files. Each module's rows are reviewed against the checklist lines and checks they came from: every old claim present, one claim per row, every headless credit honest under the rule above.
- No product code changes, so the headless set is not run locally for this build; CI runs it on the PR regardless.

## Out of scope

- Packaging an `.rpm` (item 25's open question). Pre-flight builds from source on the Fedoras until that is decided.
- qa-harness's lane itself (its item 75), its JSON record and its renderer. This spec fixes only the interface: row ids, tags, the register's halves and the results cell grammar.
- Back-filling results for v0.3.x and v0.4.x.
