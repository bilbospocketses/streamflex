# Hands-on checks

| File | What it is |
|---|---|
| [`smoke-test.md`](smoke-test.md) | The smoke doc: every release's hands-on check, one observable claim per row, on five desktops (Windows 11, Ubuntu GNOME, Kubuntu, Fedora Workstation, Fedora KDE). Pre-flight per desktop is at its top. |
| [`automation-coverage.md`](automation-coverage.md) | The coverage register: for each row and desktop, the unit test or headless check that proves it, or why a person checks it. |
| [`results/`](results/README.md) | One results file per release: every row's verdict on every desktop. |
| [`check-smoke.py`](check-smoke.py) | Lints the three, and is the `smoke-coverage` CI check. `python tests/qa/check-smoke.py lint`. |
| `virtual-gamepad.py` | A standard-library uinput Xbox 360 pad for the Linux guests: `press`, `sequence`, `axis`, `serve --keep-open S` with `send`, `list` and `selftest`. |
| `make-test-images.py` | Makes the Linux picture folder the background rows use (expected count 3). |
| `ubuntu-guest.sh` | Guest-side setup for the Linux desktops: `facts`, `setup`, `reset-pass`, `add-cafe-entry`, `pad-node`. |

The rows are scripted by qa-harness's five-desktop lane (its item 75), which keys on the row ids, so an id is never renumbered or reused (see CONTRIBUTING.md, Smoke Tests).

## How the gamepad script was tested (2026-09-29)

On the Docker Desktop VM (kernel 6.18.33.2-microsoft-standard-WSL2), `uinput` and `evdev` are modules and were not loaded. I loaded both for the test, and unloaded them after. The test ran in an `ubuntu:26.04` container (`--privileged -v /dev:/dev`), with Ubuntu's own `libsdl2-classic` 2.32.10, the SDL the guest will load.

- **The layout self-test.** `selftest` passes, and every size, offset and ioctl number matches a C program compiled against that container's `<linux/uinput.h>`:
  - `input_event` is 24 bytes, `uinput_setup` 92 and `uinput_abs_setup` 28; `absinfo` sits at offset 4 and `ff_effects_max` at 88;
  - the ioctls: `UI_DEV_SETUP` 0x405c5503, `UI_ABS_SETUP` 0x401c5504, `UI_SET_*BIT` 0x4004556x, `UI_GET_SYSNAME(64)` 0x8040552c.
- **SDL sees a game controller.** An SDL2 probe reported `SDL_IsGameController=1`, named the device *X360 Controller*, and picked up the mapping `a:b0,b:b1,back:b6,guide:b8,start:b7,…,dpup:h0.1,…,lefttrigger:a2,righttrigger:a5`. It saw every one of the 15 buttons press and release, and every stick direction and both triggers at full range. That held both when the device existed before SDL started (`serve`) and when it was hot-plugged afterwards.
- **Teardown.** SIGTERM, including SIGTERM in the middle of a 30 s hold, and `send quit` all destroyed the device; `/proc/bus/input/devices` then listed no pad.
- **The error paths.** A missing `/dev/uinput` exits 2 and says to `modprobe uinput` and run as root. A non-root user exits 2 with "permission denied; run as root". A bad button name exits 1, `send` with no server exits 3, and an out-of-range axis value exits 1.
- **What the container could not show.** The container ran SDL with `SDL_JOYSTICK_DISABLE_UDEV=1`, because there is no udevd. The guest uses udev, so U0 checks that udev tags the pad `ID_INPUT_JOYSTICK=1` and gives `qa` an ACL on its event node.
- `make-test-images.py` and `ubuntu-guest.sh` were exercised in the same image, with a stub system config: the files, ownership, the section-scoped Gamepad edit, `add-cafe-entry`, `reset-pass` and the wrapper. `ubuntu-guest.sh` passes shellcheck at warning level. Nothing was run against a real StreamFlex build or a GNOME session.
