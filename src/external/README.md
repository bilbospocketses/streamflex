# Vendored code

Third-party sources StreamFlex builds in, as they came from upstream apart from the local changes listed below. The policy, from `CONTRIBUTING.md`: take the latest upstream and read what changed; patch a CodeQL alert only where the patch changes no behavior; mark every patch with a `// streamflex:` comment and list it here; never patch a compiler warning, but silence the file where it is included (`#pragma warning(push, 0)` in `src/image.c` and `tests/test_library_svg.c`).

## nanosvg

SVG parser (`nanosvg.h`) and rasterizer (`nanosvgrast.h`), used for SVG icons, the highlight and the scroll arrows.

- **Upstream:** https://github.com/memononen/nanosvg
- **Commit:** `239e102ec2c691f2902e20ace2ed36ee4a35cfe6` (2026-07-09, "Prevent zero size malloc when parsing file (#291)")
- **License:** zlib, in the header of each file. The license asks for altered versions to be plainly marked; the `// streamflex:` comments and this file do that.

| File | Upstream path | Upstream blob | Ours |
|---|---|---|---|
| `nanosvg.h` | `src/nanosvg.h` | `291ecbf8a0c7033109550ce4abd0be3f7ff0f160` | patched, see below |
| `nanosvgrast.h` | `src/nanosvgrast.h` | `ba512c74e41b341ec4881e63ec11da6d5e8bb9e2` | unchanged |

`git hash-object src/external/nanosvgrast.h` prints the blob above while the file is unchanged. Until this update (2026-09-29) both files were unpatched upstream copies: `nanosvg.h` at `0ce2e2bee` (2022-12-04) and `nanosvgrast.h` at `93ce879dc` (2023-12-29).

### Local changes

| File | Marker | What | Why |
|---|---|---|---|
| `nanosvg.h` | `// streamflex: name and value always point into s here` (in `nsvg__parseElement`) | Removed the `if (name && value)` around the two `attr[nattr++]` stores; the stores are kept. | CodeQL alert #7, `cpp/redundant-null-check-simple`. Both pointers are set from `s` on every path that reaches the test (the loop breaks before `value` is set when the string ends), so the test was always true and removing it changes nothing. |

`grep -n "streamflex:" src/external/*.h` lists every local change, one line each.

## Updating

1. Find the new upstream commit: `gh api repos/memononen/nanosvg/commits/master --jq .sha`.
2. See our patches exactly: fetch each file at the commit recorded above (`https://raw.githubusercontent.com/memononen/nanosvg/<recorded sha>/src/<file>`) and run `git diff --no-index <that file> src/external/<file>`. The diff should hold only the changes listed above.
3. Read upstream's own changes between the recorded commit and the new one (`gh api repos/memononen/nanosvg/compare/<recorded>...<new>`), for anything that changes what StreamFlex draws, and any API change.
4. Replace each file with the new upstream copy (keep LF line endings), then re-apply each local change. For each, check the alert still applies to the new code: drop the patch if upstream fixed it, and re-check that it is still inert if the code around it changed.
5. Build with `-DEXTRA_WARNINGS=ON` on Windows and Linux, run `ctest` and the headless harness, and confirm the pragmas still keep the build at no warnings.
6. Update this file: the commit, its date and subject, both blob SHAs, and the local changes.
