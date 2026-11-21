---
layout: default
title: Compilation Guide
---
# Compilation Guide
## Table of Contents
1. [Overview](#overview)
2. [Linux](#linux)
3. [Windows](#windows)

## Overview
 StreamFlex builds natively on Linux and Windows, and features a cross-platform CMake build system. The following external dependencies are required:
 - SDL3 ≥ 3.2.10
 - SDL3_image ≥ 3.2.4
 - SDL3_ttf ≥ 3.2.2
 - fontconfig (Linux only)
 - libdbus-1 (Linux only)
 - inih
 - getopt (Windows only; provided by vcpkg)

## Linux
StreamFlex on Linux builds with GCC. This guide assumes you already have the development tools Git, CMake, pkg-config, and GCC installed on your system. If not, consult your distro's documentation. 

First, install the dependencies. The steps to do so are dependent on your distro:

#### APT-based Distributions (Debian trixie, Ubuntu 25.10, Raspberry Pi OS trixie or newer, Mint, etc.)
```bash
sudo apt install libsdl3-dev libsdl3-image-dev libsdl3-ttf-dev libfontconfig-dev libinih-dev libdbus-1-dev
```

#### Pacman-based Distributions (Arch, Manjaro, etc.)
```bash
sudo pacman -S sdl3 sdl3_image sdl3_ttf fontconfig libinih dbus
```

#### DNF-based Distributions (Fedora)
```bash
sudo dnf install SDL3-devel SDL3_image-devel SDL3_ttf-devel fontconfig-devel inih-devel dbus-devel
```

### Building
Clone the master repo and create a build directory:
```bash
git clone https://github.com/bilbospocketses/streamflex.git
cd streamflex
mkdir build && cd build
```
Generate the Makefile:
```bash
cmake .. -DCMAKE_BUILD_TYPE=Release
```
If you're building on Raspberry Pi, it's recommended to pass `-DRPI=1` to cmake, which tweaks the default configuration to be more Pi-centric.

Build and test the program:
```bash
make
./streamflex
```
Optionally, install it into your system directories:
```bash
sudo make install
```
By default, this will install the program and assets with a prefix of `/usr/local`. If you wish to use a different prefix, re-run the cmake generation step with `-DCMAKE_INSTALL_PREFIX=prefix`.

## Windows
StreamFlex on Windows builds with Visual Studio's MSVC compiler. [vcpkg](https://vcpkg.io/en/index.html) builds the dependencies listed in `vcpkg.json` as static libraries. The steps below reproduce the CI build in `.github/workflows/build.yml`: the same compiler, the same vcpkg version, and the same libraries.

Before starting, make sure the following are installed:
- **Visual Studio 2022.** The free Community edition is fine. Install the "Desktop development with C++" workload, which includes:
  - MSVC v143 build tools
  - a Windows 10/11 SDK
  - C++ CMake tools for Windows

  CI builds with Visual Studio 2022 on GitHub's `windows-2022` runner. A newer Visual Studio also works, but its compiler differs from the one the release packages are built with.
- **Git,** in your `Path` environment variable.
- **CMake 3.18 or newer,** in your `Path`. The easiest way is `winget install Kitware.CMake`. Visual Studio's "C++ CMake tools" component includes its own copy, but doesn't add it to `Path`. To use that copy instead, add it for the current PowerShell session:
  ```powershell
  $env:Path += ";C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin"
  ```
  (Change `Community` to your edition.)

Run the commands below in an ordinary PowerShell window. You don't need a Visual Studio developer prompt: CMake's Visual Studio generator finds the compiler on its own.

### Building
Clone the repository and create a build directory:
```powershell
git clone https://github.com/bilbospocketses/streamflex.git
cd streamflex
mkdir build
cd build
```
Clone vcpkg and check out the exact version CI uses. That commit is `VCPKG_COMMITTISH` in `.github/workflows/build.yml`, so read it from there rather than copying it by hand:
```powershell
git clone https://github.com/microsoft/vcpkg
$pin = (Select-String -Path ..\.github\workflows\build.yml -Pattern 'VCPKG_COMMITTISH:\s*(\w+)').Matches[0].Groups[1].Value
git -C vcpkg checkout $pin
```
Generate the Visual Studio project files. On the first run, vcpkg sets itself up and builds every dependency in `vcpkg.json`, which takes a while:
```powershell
cmake .. -G "Visual Studio 17 2022" -DCMAKE_TOOLCHAIN_FILE=".\vcpkg\scripts\buildsystems\vcpkg.cmake" -DVCPKG_TARGET_TRIPLET="x64-windows-static"
```
Build and test the program:
```powershell
cmake --build .
.\Debug\streamflex.exe
```
Optionally, build the same zipped Release package that CI publishes, which may then be extracted to a directory of your choosing:
```powershell
cmake --build . --config Release --target package
```
The `build` directory is ignored by git, so nothing in it shows up as a change to the repository.
