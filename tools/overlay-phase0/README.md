# Streaming overlay Phase 0 probe kit

Throwaway tools for the measurements in `design/specs/2026-10-27-streaming-overlay-design.md` (Phase 0). Nothing
here ships, and StreamFlex's own build never compiles it. The protocol that uses it is
`design/research/overlay/phase0/guest-protocol.md`; the user's run sheet is `design/research/overlay/phase0/run-sheet.md`.

## Build

Windows (vcpkg; the repo root's `vcpkg.json` supplies SDL3):

```powershell
cmake -S tools/overlay-phase0 -B build-phase0 -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" -DVCPKG_MANIFEST_DIR="$PWD"
cmake --build build-phase0 --config Release
```

Linux (the distro's SDL3 development package):

```sh
cmake -S tools/overlay-phase0 -B build-phase0 -DCMAKE_BUILD_TYPE=Release
cmake --build build-phase0
```

## Test

Windows (PowerShell, from the repo root; set the variables in the same command, or every test skips):

```powershell
$env:PROBE_HOST = "$PWD\build-phase0\Release\probe_host.exe"; $env:PROBE_SPAWN = "$PWD\build-phase0\Release\probe_spawn.exe"; python -m unittest discover -s tools/overlay-phase0/tests -v
```

Linux:

```sh
PROBE_HOST=build-phase0/probe_host PROBE_SPAWN=build-phase0/probe_spawn python3 -m unittest discover -s tools/overlay-phase0/tests -v
```

## Assemble the kit (Windows)

The run sheet and the Windows guests use one folder holding the whole kit. After the build, from the repo root:

```powershell
powershell -ExecutionPolicy Bypass -File tools\overlay-phase0\make-kit.ps1 -Build build-phase0\Release -Xpi <signed xpi>
```

It copies `probe_host.exe`, `probe_spawn.exe` and every DLL beside them (SDL3.dll), the Visual C++ runtime DLLs
(`vcruntime140.dll`, `vcruntime140_1.dll`, `msvcp140.dll`, from `-RuntimeDir`, default System32, when there) so the
kit runs on a clean Windows 11, `setup\`, `pages\`, `extension\`
(`src\` and `build.py`), `ids.json` when it exists and the XPI when `-Xpi` names one into `$env:USERPROFILE\cptk-kit`
(`-Kit` picks another folder), then lists what it copied. Re-running refreshes every file and deletes nothing.

## What is here

| Piece | Job |
|---|---|
| `probe_host` | The native-messaging host `app.cptk.overlay`: logs every message to `probe.log` and acknowledges it. |
| `probe_spawn` | `run` (supervise a browser), `keys` (inject keys), `keylog` (log raw keys), `pad` (gamepad in the background), `focus` (does the launcher get focus back). |
| `extension/` | CPTK Overlay Probe 0.0.1 (Gecko id `overlay-probe@cptk.app`), the probe extension under its own store listings, and `build.py`, which makes the Chromium and Firefox packages. The signed Firefox package is `cptk-overlay-probe-0.0.1.xpi`; the native host is still `app.cptk.overlay`. |
| `pages/` | `probe.html` (fullscreen and media-start targets), `keylog.html` (what a page sees), `serve.ps1` (a page server for Windows guests). |
| `setup/` | `windows.ps1`, `linux_setup.py`: policy and host registration. `firefox-profile.ps1`, `firefox_profile.py`: a Firefox profile with the sideloaded XPI. |
| `ids.json` | The three store ids, recorded when the probe is published. |
| `listing.md` | The store listing text, the same for all three stores; `design/research/overlay/phase0/store-steps.md` says where each part goes. |
| `privacy-policy.md` | The probe's privacy policy, which the store listings link to. |
| `ci/` | The Linux check: `Dockerfile.linux` makes the image, and `linux-check.sh` builds the kit in it and runs the whole tests folder. |
| `make-kit.ps1` | Assembles the built kit into one folder for the run sheet and the Windows guests. |
