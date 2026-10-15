# Smoke results

One file per release, `vX.Y.Z.md`, committed in the release PR before the tag. The release PR also sets the smoke doc's **Smoke target** to the version. It records the verdict of every row of [`../smoke-test.md`](../smoke-test.md) on every desktop the row's tag covers. The first file is v0.6.0's, filled by qa-harness. v0.5.0 has none, by the user's ruling of 2026-10-15: the verdicts wait for v0.6.0's pass, where qa-harness runs every row but the Windows rows that need a real GPU, which the user runs, so v0.5.0's release PR commits no results file and leaves the **Smoke target** as it is: the target moves with the results file, from v0.6.0's release PR on. v0.3.x, v0.4.x and v0.5.0 are not back-filled.

## The file

1. **Header.** The StreamFlex tag and commit tested, the date, and the `build-and-test` run id the automated cells come from. Then the desktops table, with exactly this header and one line per desktop:

   | Desktop | OS | Image | Build |
   |---|---|---|---|

   **Desktop** is `Win`, `Ubuntu`, `Kubuntu`, `Fedora` or `Fedora-KDE`; **OS** the OS and version; **Image** qa-harness's base image digest when it ran that desktop, else `-`; **Build** the installed artifact's sha256, plus the CI run id on the Fedoras (`sha256:… (run 38…)`). qa-harness's renderer emits these lines. A patch release that carries rows forward says once, here, why its change cannot affect them.
2. **Results table**, with exactly this header:

   | Row | Win | Ubuntu | Kubuntu | Fedora | Fedora-KDE |
   |---|---|---|---|---|---|

   One line per row id. A cell is one of:

   | Cell | Means |
   |---|---|
   | `x` | pass |
   | `F1` | fail; its write-up is `F1` under Findings |
   | `S1` | not measured; its reason is `S1` under Not measured |
   | `-` | the desktop is outside the row's tag |
   | `x (v0.5.0)` | carried forward from that release's pass (patch releases only, from an earlier patch of the same major.minor: v0.5.2 may carry v0.5.0 or v0.5.1) |

   An empty cell is never a pass. A skip's reason never goes in the cell: reasons are long and often hold a `|`.
3. **`## Findings`**: `- **F1** <what failed, what was seen, the log or frame, and the fix PR or patch release, or the user's decision to ship with it>`.
4. **`## Not measured`**: `- **S1** <the full reason, verbatim from qa-harness's NOT MEASURED text where it came from there, and where it was measured afterwards or the user's decision to ship with it>`.

## Who fills which cells

- **qa-harness halves:** pasted from the fragment qa-harness renders from its run record and hands over through ClaudeScratch. qa-harness never pushes to this repo.
- **`streamflex-headless` and `streamflex-ctest` halves:** `x` from the release commit's green `build-and-test` run.
- **`manual` halves:** whoever runs the pass by hand.

## What holds a release

Any `F` or `S` inside a row's tag. A fail is fixed and a skip is measured first, or the user decides to ship with it and its entry records that.

## Checking a file

    python tests/qa/check-smoke.py lint --complete tests/qa/results/vX.Y.Z.md

A relative path is read from the repository root, wherever the command runs. `--complete` requires a line for every row of the doc, no empty cell inside a row's tag, and `-` in every cell outside it. CI runs it on every results file a PR adds or changes. Every other results file is checked for form only, so a committed file stays as it was when its release shipped.
