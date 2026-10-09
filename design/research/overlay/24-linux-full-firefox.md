# StreamFlex: detecting and installing a full (non-snap) Firefox on Linux

Date: 2026-09-28. Read-only research; nothing installed or changed on any system.
Tags: VERIFIED = I fetched or measured it this session. REPORTED = a secondary source or my prior knowledge, not re-checked. UNKNOWN = not established.
Update 2026-09-28 (gap-fill pass): Mozilla's official APT instructions, the ExtensionSettings/Extensions policy docs, the native-messaging directory and the policies file precedence are now settled from primary sources (sections 2, 4, 5). Firefox versions cited below: tarball 156.0.1 (BuildID 20260921121718) and Mozilla deb 155.0~build1, both downloaded and inspected.
Limits: the original pass had no WebSearch budget, so it used direct fetches (WebFetch/curl) of primary sources; the gap-fill pass used 1 WebSearch. Snap behavior is deliberately not investigated (user decision); snap is only detected so it can be excluded.

## 0. StreamFlex's Linux packaging today (this repo)

- VERIFIED (`CMakeLists.txt:154-180`): `-DPACKAGE=DEB` uses CPack. `CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON`, no explicit `CPACK_DEBIAN_PACKAGE_DEPENDS`, section `video`, gzip. File name `streamflex_<ver>_<arch>.deb`. No epoch. There are NO `CPACK_DEBIAN_PACKAGE_RECOMMENDS` / `SUGGESTS` and no `CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA` (postinst) settings yet.
- VERIFIED (`.github/workflows/build.yml:134,182`): two deb jobs, amd64 and arm64 (`-DRPI=1` is the Raspberry Pi build). Arch: `PKGBUILD` generated from `config/PKGBUILD.in` (`depends=('sdl2' 'sdl2_image' 'sdl2_ttf' 'libinih')`, `#optdepends=()` is commented out). Fedora: none yet.
- VERIFIED (`docs/setup.md:38`): the current docs recommend Chrome/Chromium as the browser. The Firefox path is new.
- Consequence: `optdepends=('firefox: ...')` is the natural Arch hook. Debian has no clean equivalent (section 3).

## 1. Detection: full Firefox vs snap vs flatpak vs none

### Facts that drive the design
- VERIFIED (curl of packages.ubuntu.com): Ubuntu `firefox` on jammy `1:1snap1-0ubuntu2` and noble `1:1snap1-0ubuntu5` is described "Transitional package - firefox -> firefox snap". For resolute (26.04) the description is "Installs Firefox snap and provides some system integration". So a `firefox` deb being installed on Ubuntu (22.04/24.04/26.04) says nothing about a real Firefox. https://packages.ubuntu.com/noble/firefox , https://packages.ubuntu.com/search?keywords=firefox&searchon=names&suite=resolute&section=all
- REPORTED: that deb ships a `/usr/bin/firefox` stub that starts the snap. So `command -v firefox` returning `/usr/bin/firefox` does NOT prove a full Firefox, and a `/snap/bin/firefox` check alone misses it. PATH is the wrong thing to probe.
- VERIFIED (I downloaded and unpacked Mozilla's `firefox_155.0~build1_amd64.deb`, no install): the real deb puts the browser in `/usr/lib/firefox/` (`firefox`, `firefox-bin`, `application.ini`-style data, `distribution/distribution.ini`), with `/usr/bin/firefox -> ../lib/firefox/firefox`. It ships no policies.json and no native-messaging manifests. Maintainer is `Mozilla <release@mozilla.com>`. https://packages.mozilla.org/apt/dists/mozilla/main/binary-amd64/Packages
- VERIFIED (curl of packages.debian.org filelist): Debian `firefox-esr` uses `/usr/lib/firefox-esr/`, `/etc/firefox-esr/firefox-esr.js`, `/usr/bin/firefox-esr`, `/usr/bin/firefox`. https://packages.debian.org/trixie/amd64/firefox-esr/filelist
- VERIFIED (archlinux.org file list): Arch `firefox` 156.0.1 uses `/usr/lib/firefox/{firefox,firefox-bin}`, `/usr/bin/firefox`, `usr/lib/firefox/distribution/distribution.ini`. https://archlinux.org/packages/extra/x86_64/firefox/files/
- REPORTED: Fedora uses `/usr/lib64/firefox/` (x86_64) and `/usr/bin/firefox`. Not re-checked (the Fedora package page has no file list).
- Snap firefox lives under `/snap/firefox/current/...` (its binary is never on the real filesystem in `/usr/lib*`). REPORTED.
- Flatpak: `flatpak info org.mozilla.firefox` succeeds; files under `/var/lib/flatpak/app/org.mozilla.firefox` or `~/.local/share/flatpak/app/...`. REPORTED.

### Design rule
Probe for a real Firefox application directory (an ELF `firefox` binary next to `application.ini`), not for a command name or a package name. A snap or the Ubuntu stub can never satisfy that test; Mozilla deb, Debian ESR, Arch, Fedora, Mint, Pi OS and a tarball all do. Package-manager queries are only used to explain what was found.

### Pseudo-code (C or shell equivalent)

```
struct ff { char dir[PATH_MAX]; char kind[16]; };   // kind: deb-mozilla|distro|tarball|user-tarball

// Step 1: candidate application dirs, in this order
candidates = [
  "/usr/lib/firefox", "/usr/lib64/firefox",            // Mozilla deb, Arch, Fedora, Mint, Ubuntu-with-real-deb, Debian "firefox"
  "/usr/lib/firefox-esr", "/usr/lib64/firefox-esr",    // Debian ESR, Pi OS
  "/usr/lib/x86_64-linux-gnu/firefox", "/usr/lib/aarch64-linux-gnu/firefox",   // defensive; unverified layout
  "/opt/firefox", "/opt/firefox/firefox",              // Mozilla tarball convention
  "$HOME/.local/opt/firefox", "$HOME/firefox",         // user tarball
]
// Also: resolve `command -v firefox firefox-esr` via PATH, realpath() it, and add dirname(realpath) as a candidate.
// (realpath of the Ubuntu stub gives /usr/bin, which fails Step 2, so it is harmless.)

for d in candidates:
    if is_regular_elf(d + "/firefox") or is_regular_elf(d + "/firefox-bin"):   // read first 4 bytes == 0x7f 'E' 'L' 'F'; a shell script fails
        if exists(d + "/application.ini") or exists(d + "/omni.ja"):
            return FULL(d)

// Step 2: none found. Classify what IS there, only to word the message
if exists("/snap/firefox/current") or system("snap list firefox") == 0:   -> SNAP_ONLY   (treated as "no full Firefox")
if system("flatpak info org.mozilla.firefox") == 0:                       -> FLATPAK_ONLY (treated as "no full Firefox")
else                                                                       -> NONE
```

Rules that keep it correct:
- Never accept `dpkg -s firefox` / `dpkg -S` / `rpm -q firefox` / `pacman -Q firefox` as proof. On Ubuntu the deb named `firefox` is the transitional one (VERIFIED above).
- Never key on `/snap/bin/firefox` alone (misses the `/usr/bin/firefox` stub) and never assume `/usr/bin/firefox` is real.
- Optional explanatory check on Debian family: `dpkg-query -W -f='${Version} ${Maintainer}' firefox` gives `1:1snap1-...` (transitional) vs `155.0~build1 / Mozilla` (real). Use only for the message, not the decision.
- Per-distro outcome of the algorithm (VERIFIED paths for Debian/Arch/Mozilla deb, REPORTED for the rest):

| Distro | Result |
|---|---|
| Ubuntu 22.04 / 24.04 / 26.04, default | Step 1 fails (stub is a script, browser is in the snap) -> SNAP_ONLY -> prompt |
| Ubuntu + Mozilla apt deb | `/usr/lib/firefox/firefox` ELF -> FULL |
| Debian 12 / 13 | `firefox-esr` -> `/usr/lib/firefox-esr` -> FULL (if installed; it is the default browser in the desktop task, REPORTED) |
| Linux Mint | real Firefox deb (REPORTED; not re-checked) -> `/usr/lib/firefox` -> FULL |
| Fedora | `/usr/lib64/firefox` -> FULL (REPORTED) |
| Arch | `/usr/lib/firefox` only if the user installed it (not installed by default) |
| Raspberry Pi OS | see section 2, no Firefox by default |

## 2. Installing a full Firefox

### Ubuntu / Debian family: Mozilla's APT repo
- VERIFIED (curl of the repo): the suite is `mozilla`, component `main`, architectures `all amd64 arm64 i386`. There is NO `armhf`, so 32-bit Raspberry Pi OS cannot use it. Package `firefox` 155.0 in the index, with `Depends` limited to plain libs (libgtk-3-0 etc.). https://packages.mozilla.org/apt/dists/mozilla/InRelease
- Recipe. VERIFIED verbatim from Mozilla's official article "Install Firefox on Linux", section "Install Firefox DEB package for Debian-based and Ubuntu-based distributions (recommended)", read in a real browser this session (plain HTTP fetch is blocked by a client challenge; it resolves in a browser): https://support.mozilla.org/en-US/kb/install-firefox-linux

```sh
sudo install -d -m 0755 /etc/apt/keyrings
wget -q https://packages.mozilla.org/apt/repo-signing-key.gpg -O- | sudo tee /etc/apt/keyrings/packages.mozilla.org.asc > /dev/null

# Debian Bookworm / Ubuntu Noble and older: one-line file
echo "deb [signed-by=/etc/apt/keyrings/packages.mozilla.org.asc] https://packages.mozilla.org/apt mozilla main" | sudo tee -a /etc/apt/sources.list.d/mozilla.list > /dev/null

# Debian Trixie / Ubuntu Resolute (26.04) and newer: /etc/apt/sources.list.d/mozilla.sources
Types: deb
URIs: https://packages.mozilla.org/apt
Suites: mozilla
Components: main
Signed-By: /etc/apt/keyrings/packages.mozilla.org.asc

# /etc/apt/preferences.d/mozilla
Package: *
Pin: origin packages.mozilla.org
Pin-Priority: 1000

# Ubuntu only, to replace the snap: Mozilla's article writes the SAME file name /etc/apt/preferences.d/mozilla again
Package: firefox
Pin: release o=Ubuntu
Pin-Priority: -1

sudo apt-get update
sudo apt-get install firefox     # or firefox-esr, -beta, -nightly, -devedition
```
  - Key fingerprint published by Mozilla in that article: `35BAA0B33E9EB396F59CA838C0BA5CE6DC6315A3`. VERIFIED independently: I downloaded `https://packages.mozilla.org/apt/repo-signing-key.gpg` and `gpg --show-keys` printed `35BA A0B3 3E9E B396 F59C A838 C0BA 5CE6 DC63 15A3` (rsa2048, created 2021-05-04, uid "Artifact Registry Repository Signer"); the current `InRelease` is signed by the same key. Mozilla also prints a `gpg ... awk` one-liner that checks the fingerprint.
  - Caveat for StreamFlex's helper (my analysis): the article's Ubuntu step tells the reader to write the pin to the same path as the `Package: *` pin, which would overwrite it. Write the two stanzas into ONE `/etc/apt/preferences.d/mozilla` file (separated by a blank line), or use a second file name. Mozilla's `Package: *` pin at 1000 means every package that exists in the Mozilla repo prefers it; it also has `firefox-l10n-*` packages (VERIFIED in the article) and other Firefox editions, so the earlier `firefox*`-scoped pin idea is not Mozilla's form (mine, untested).
  - Older third-party sources (REPORTED, superseded by the above): https://www.omgubuntu.co.uk/2022/04/how-to-install-firefox-deb-apt-ubuntu-22-04 , https://ubuntuhandbook.org/index.php/2022/04/install-firefox-deb-ubuntu-22-04/ . The earlier version of this file showed `Pin-Priority: 1001` and `firefox*`; that was a third-party variant, not Mozilla's.
  - The same article (VERIFIED) also gives a Fedora/RHEL RPM repo (`https://packages.mozilla.org/rpm/firefox`, dnf5 `config-manager addrepo` form for Fedora 41+ and a `/etc/yum.repos.d/mozilla.repo` for older dnf) and a tarball section ("System Firefox installation (for advanced users)" and a per-user install). Not read in detail; read it before writing the Fedora/tarball helper branch.
- Why the pin matters: VERIFIED that Ubuntu's `firefox` is `1:1snap1-...`. The epoch `1:` outranks Mozilla's `155.0~build1`, so without a priority above the archive's, apt would choose the transitional package and reinstall the snap. omgubuntu states exactly this (REPORTED).
- Must the snap be removed first? Not required for the deb to install and be preferred. REPORTED: after the deb is installed, keeping the snap only duplicates icons; `sudo snap remove --purge firefox` is the tidy-up. If the transitional Ubuntu package is already installed, `apt install firefox` with the pin in place replaces it (a "downgrade" by epoch is avoided because the pin of -1 removes the Ubuntu candidate; omgubuntu shows `apt remove firefox` first, REPORTED). Recommendation for StreamFlex: do not remove the snap automatically; tell the user it is optional.
- VERIFIED from the Mozilla deb's maintainer scripts: postinst only registers `update-alternatives` for `x-www-browser` and `gnome-www-browser`, and removes an obsolete AppArmor conffile. It does not touch policies.

Raspberry Pi OS
- VERIFIED (curl of archive.raspberrypi.com Packages for trixie and bookworm, arm64): the Raspberry Pi archive carries `chromium` (153.x) and NO `firefox` package. So a Pi OS box has Chromium by default and Firefox, if any, comes from Debian's `firefox-esr` (VERIFIED it exists in trixie for amd64/arm64/armhf: https://packages.debian.org/trixie/firefox-esr) or from Mozilla's arm64 apt repo. https://archive.raspberrypi.com/debian/dists/trixie/InRelease
- Recipe for Pi OS: 64-bit -> `sudo apt install firefox-esr` (simplest, works, no third-party repo) or Mozilla's repo (arm64 only). 32-bit (armhf) -> `firefox-esr` only. Whether the Pi OS desktop image preinstalls firefox-esr: UNKNOWN.
- Caveat: ESR lags the extension APIs; native messaging has been stable, so this is not expected to matter (REPORTED).

Mozilla tarball (any distro; the fallback for Fedora-less or unsupported combos)
- VERIFIED (HTTP 302 probes): `https://download.mozilla.org/?product=firefox-latest-ssl&os=linux64&lang=en-US` -> `.../releases/156.0.1/linux-x86_64/en-US/firefox-156.0.1.tar.xz`; `os=linux64-aarch64` -> `.../linux-aarch64/...tar.xz`; `os=linux` (32-bit) -> 404. So the tarball covers x86_64 and aarch64 only.
- Recipe: `sudo tar -C /opt -xJf firefox-*.tar.xz` gives `/opt/firefox/`; optional `sudo ln -sf /opt/firefox/firefox /usr/local/bin/firefox`. Per-user install to `~/.local/opt/firefox` needs no root. The tarball's in-app updater is on for user-writable dirs (REPORTED).
- Downside: no distro updates and no desktop entry unless StreamFlex writes one. Offer this only when apt/dnf is unavailable or the user declines the repo.

Fedora and Arch
- Fedora: `dnf install firefox` (Fedora's own package, current 156.0.1 on rawhide, VERIFIED at https://packages.fedoraproject.org/pkgs/firefox/firefox/). It is the default browser of Fedora Workstation (REPORTED), so usually already present.
- Arch: `pacman -S firefox` (VERIFIED in `extra`, 156.0.1). Not installed by default on a minimal Arch. The right hook is `optdepends=('firefox: opens streaming services through the StreamFlex extension')` in `config/PKGBUILD.in`.
- Both ship a normal (non-snap) Firefox, so the prompt should be "install Firefox" with the native package command, not a special repo.

## 3. Where the prompt can live

- Debian policy VERIFIED (https://www.debian.org/doc/debian-policy/ch-maintainerscripts.html): "Maintainer scripts are not guaranteed to run with a controlling terminal and may not be able to interact with the user. They must be able to fall back to noninteractive behavior if no controlling terminal is available." debconf is the accepted mechanism ("packages interacting with users using debconf in the postinst script should install a config script"). Aborting for lack of an answer is called a packaging defect that prevents unattended installs.
- Practical consequences for StreamFlex (my analysis, not sourced):
  - debconf in postinst: allowed but wrong fit. This is an HTPC, often installed over SSH or unattended (Pi image builds); a debconf prompt would need a sane default of "do nothing", and it can't tell whether the user has a display. Also changing the system's APT sources from a postinst is invasive.
  - `Recommends`/`Suggests: firefox`: BAD on Ubuntu. VERIFIED that the deb named `firefox` there is the snap-installing transitional package, so `Recommends: firefox` would pull the snap on 22.04 to 26.04, exactly what the user wants to avoid. `Recommends: firefox-esr` is correct on Debian/Pi OS but does not exist in Ubuntu's archive. Use `Suggests: firefox-esr | firefox` at most, and never `Recommends` on `firefox`. Since dpkg-shlibdeps drives `Depends` (CMakeLists.txt:167-173), `CPACK_DEBIAN_PACKAGE_SUGGESTS` can be added without disturbing it (not tested).
  - Desktop notification from the package: no session at install time; unreliable.
  - First-run check inside the app: the only place that knows the real runtime state (which Firefox is installed, whether a display exists) and that can re-check on every start without touching packaging. Recommended.
- Comparable apps: I could not verify any from primary sources this session. REPORTED from prior knowledge and not re-checked: Chrome/Chromium-family debs add their own apt repo from postinst without prompting; most media apps simply document the dependency and print an error at launch when the browser is missing.
- **Recommendation for StreamFlex**
  1. Run the detection algorithm (section 1) at startup and whenever the Settings screen opens. It is a few `stat()` calls, so it is cheap and needs no state file.
  2. Only if the result is not FULL, show a one-time (dismissable, re-openable from Settings) fullscreen dialog: "Firefox (full version) is highly recommended", with a distro-specific explanation (Ubuntu: "the default Firefox is a snap and can't talk to StreamFlex").
  3. The action button runs a privileged helper, never the launcher itself: install `/usr/libexec/streamflex/install-firefox` (or under `/usr/share/streamflex/`) and run it with `pkexec`. It branches on `/etc/os-release` (`ID`, `ID_LIKE`): Ubuntu ->  Mozilla apt repo + pin; Debian and Pi OS -> `apt install firefox-esr` (or the Mozilla repo on arm64); Fedora -> `dnf install firefox`; Arch -> `pacman -S firefox`; otherwise the tarball. If `pkexec` is unavailable, show the commands as text for the user to copy.
  4. Add the Arch `optdepends` line and the Debian `Suggests: firefox-esr | firefox` (not Recommends). Nothing in postinst.
  5. Never show the prompt when a FULL Firefox is detected, and never when the user chose a Chromium-family browser in settings.
- UNKNOWN: whether a controller-driven fullscreen SDL UI can host a `pkexec` polkit prompt on every target session (Wayland/kiosk); that needs a test on a Pi/HTPC.

## 4. Native Messaging host manifest paths (Firefox on Linux)

- VERIFIED (MDN, https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/Native_manifests): global `/usr/lib/mozilla/native-messaging-hosts/<name>.json` or `/usr/lib64/mozilla/native-messaging-hosts/<name>.json`; per-user `~/.mozilla/native-messaging-hosts/<name>.json`. `<name>` must equal the manifest's `name`.
- VERIFIED (Firefox source, `toolkit/xre/nsXREDirProvider.cpp` `GetSystemParentDirectory`): the system directory is `/usr/lib64/mozilla` when the build defines `HAVE_USR_LIB64_DIR`, else `/usr/lib/mozilla` (`/usr/local/lib/mozilla` on BSDs). It is ONE path per build, not both. https://raw.githubusercontent.com/mozilla-firefox/firefox/main/toolkit/xre/nsXREDirProvider.cpp
- Consequence per flavor:

| Flavor | System dir read | Per-user dir |
|---|---|---|
| Mozilla apt deb (Ubuntu/Debian/Mint) | `/usr/lib/mozilla/native-messaging-hosts` only. VERIFIED: I unpacked `firefox_155.0~build1_amd64.deb` (SHA256 matches the repo's Packages entry) and its `libxul.so` contains the single string `/usr/lib/mozilla` and no `/usr/lib64/mozilla` | `~/.mozilla/native-messaging-hosts` |
| Mozilla tarball in `/opt` or `~` | `/usr/lib/mozilla/native-messaging-hosts` only. VERIFIED: `libxul.so` of `firefox-156.0.1.tar.xz` (linux64) contains only `/usr/lib/mozilla`. The path is absolute, so it does NOT depend on where the tarball is unpacked | same; recommended |
| Debian ESR, Arch | `/usr/lib/mozilla/...` (REPORTED; not tested. The source's default branch is `/usr/lib/mozilla`; distro builds may define `HAVE_USR_LIB64_DIR`) | same |
| Fedora / other lib64 distros | `/usr/lib64/mozilla/...` only when the build defines `HAVE_USR_LIB64_DIR` (per the source above; Fedora not tested) | same |
| Snap / flatpak | out of scope; not reliably reading the host paths (REPORTED). Detection excludes them | UNKNOWN |

- Note (VERIFIED, same MDN page and `nsXREDirProvider.cpp`): MDN lists both `/usr/lib/mozilla/...` and `/usr/lib64/mozilla/...` as "global" locations without saying a given build reads only one; the source shows it is exactly one, fixed at compile time. For Mozilla's own deb and tarball that one is `/usr/lib/mozilla` (measured above), so a StreamFlex `.deb` shipping the manifest to `/usr/lib/mozilla/native-messaging-hosts/` works with them.
- Recommendation: write the per-user manifest `~/.mozilla/native-messaging-hosts/<name>.json` for every flavor; it is read by all non-sandboxed builds (VERIFIED in MDN) and avoids the `lib` vs `lib64` question and needs no root. Additionally install the system-wide manifest from the package: `/usr/lib/mozilla/native-messaging-hosts/` in the deb and `/usr/lib64/mozilla/native-messaging-hosts/` in a Fedora RPM. On Arch, `/usr/lib/mozilla/native-messaging-hosts/` (REPORTED convention). The manifest's `path` must be an absolute path to the StreamFlex host binary; `allowed_extensions` holds the extension ID.

## 5. Enterprise policy to force-install the extension

- VERIFIED (Mozilla policy-templates docs): "On Linux, the file goes into `firefox/distribution`, where `firefox` is the installation directory for firefox, which varies by distribution or you can specify system-wide policy by placing the file in `/etc/firefox/policies`." https://raw.githubusercontent.com/mozilla/policy-templates/master/docs/index.md
- VERIFIED (Firefox source, `EnterprisePoliciesParent.sys.mjs` `_getLocalConfigurationFile`): on Linux, when the build has `MOZ_SYSTEM_POLICIES`, it reads `SysConfD/policies/policies.json` first (SysConfD is `/etc/firefox`, per the build option text "reading policies from ... /etc/firefox") and returns it if it exists; otherwise it falls back to `XREAppDist/policies.json`, i.e. `<app dir>/distribution/policies.json`. It is a first-found file, not a merge. https://raw.githubusercontent.com/mozilla-firefox/firefox/main/toolkit/components/enterprisepolicies/EnterprisePoliciesParent.sys.mjs , https://raw.githubusercontent.com/mozilla-firefox/firefox/main/toolkit/moz.configure (`--disable-system-policies` exists, so the feature is on by default).
- Per flavor:

| Flavor | `/etc/firefox/policies/policies.json` | `<app dir>/distribution/policies.json` |
|---|---|---|
| Mozilla apt deb | works. VERIFIED: `omni.ja` -> `modules/AppConstants.sys.mjs` has `MOZ_SYSTEM_POLICIES: true` in deb 155.0~build1 (and in tarball 156.0.1). Reads `/etc/firefox/policies/policies.json`; `SysConfD` = `/etc` + lower-cased app name (`SpecialSystemDirectory.cpp`, `GetUnixSystemConfigDir`) | `/usr/lib/firefox/distribution/` exists and holds `distribution.ini` (VERIFIED); a file there would be package-owned territory. Used only if `/etc/firefox/policies/policies.json` does not exist |
| Debian ESR | REPORTED | `/usr/lib/firefox-esr/distribution` exists (VERIFIED as a path in the filelist) |
| Arch | REPORTED to work | `/usr/lib/firefox/distribution/` (VERIFIED, holds `distribution.ini`) |
| Fedora | REPORTED to work | `/usr/lib64/firefox/distribution/` (REPORTED) |
| Tarball `/opt/firefox` | works. VERIFIED: tarball 156.0.1 `AppConstants` has `MOZ_SYSTEM_POLICIES: true`, so `/etc/firefox/policies/policies.json` is read even though the app dir is `/opt/firefox`. The tarball's `distribution/` holds only `distribution.ini` (VERIFIED) | `/opt/firefox/distribution/policies.json` (needs create; root-owned) |
| Snap/flatpak | out of scope | out of scope |

- Recommendation: use `/etc/firefox/policies/policies.json` for package installs (survives Firefox upgrades, package-neutral, owned by StreamFlex or its helper), and `<app dir>/distribution/policies.json` only for a tarball install whose build ignores `/etc`. After writing, verify at runtime with `about:policies` in that Firefox (REPORTED). Note the file is first-found, so if another product already owns `/etc/firefox/policies/policies.json`, StreamFlex must MERGE its `ExtensionSettings` entry into it rather than replace the file.
- Precedence, confirmed (VERIFIED from the `_getLocalConfigurationFile` source above): first found wins, no file-level merge. Order on Linux with `MOZ_SYSTEM_POLICIES`: (1) `/etc/firefox/policies/policies.json` if it EXISTS, else (2) `<XREAppDist>/policies.json` = `<appdir>/distribution/policies.json`. If (1) exists, (2) is ignored completely. Holds for Mozilla's APT deb and tarball (both have `MOZ_SYSTEM_POLICIES: true`, measured). Also VERIFIED: with more than one policy PROVIDER (JSON file, Windows GPO, macOS), `CombinedProvider` uses `Object.assign` over the providers' `policies` objects, so provider overrides are per top-level policy key, and the JSON provider has the lowest precedence; on Linux only the JSON provider applies. https://raw.githubusercontent.com/mozilla-firefox/firefox/main/toolkit/components/enterprisepolicies/EnterprisePoliciesParent.sys.mjs
- Docs location. VERIFIED: the old `mozilla/policy-templates` `docs/index.md` now carries a banner "The documentaton below is NOT current" and points to the Firefox administrator reference, https://firefox-admin-docs.mozilla.org/ (source repo `mozilla/enterprise-admin-reference`). Its page for this policy, read this session (last-updated stamp "Sep 28, 2026"): https://firefox-admin-docs.mozilla.org/reference/policies/extensionsettings/ . The old `mozilla.github.io/policy-templates` links are stale. That old `docs/index.md` still says (VERIFIED) that on Linux the file goes into `firefox/distribution` (the install dir) "or you can specify system-wide policy by placing the file in `/etc/firefox/policies`".
- **Force-install: verified shape.** Top-level is `{"policies": { "ExtensionSettings": { "<extension id>": { ... } } } }`. Fields VERIFIED from the admin reference page:
  - `installation_mode`: `allowed` (default), `blocked`, `force_installed` ("Automatically installs the extension and prevents it from being removed by the user"; not valid for the default `"*"` entry and needs `install_url`), `normal_installed` (installs but the user may disable it).
  - `install_url`: "The URL from which Firefox can download a force_installed or normal_installed extension. Firefox automatically installs, updates, or re-installs the extension when the XPI file's internal version changes." A `file:///` URL is allowed: "Firefox will update or re-install the extension whenever the XPI file at that path changes." As of Firefox 153 `install_url` is optional for AMO-hosted extensions (Firefox then installs the latest from AMO by ID); for our own XPI it is still required.
  - Extension IDs: an email-style ID (`name@example.com`) or a brace UUID (`{...}`); UUID IDs need the braces.
  - `updates_disabled`: (Firefox 89, ESR 78.11) per-extension boolean; as of Firefox 152, `false` keeps updates on and stops the user disabling them, and force-installed extensions always auto-update regardless.
  - `update_url`: (Firefox 151) URL Firefox checks for updates, overriding the manifest's `update_url`.
  - Also: `private_browsing` (Firefox 136 / ESR 128.8), `default_area` (`navbar` or `menupanel`, Firefox 113), `temporarily_allow_weak_signatures` (Firefox 127).
```json
{
  "policies": {
    "ExtensionSettings": {
      "streamflex@example.com": {
        "installation_mode": "force_installed",
        "install_url": "https://example.com/downloads/streamflex-1.0.0.xpi",
        "private_browsing": true,
        "default_area": "navbar"
      }
    }
  }
}
```
  The shape and every field name are VERIFIED (the admin reference's own example uses `"installation_mode": "force_installed"` with an `"install_url"` for `uBlock0@raymondhill.net`); the id, URL and the `private_browsing`/`default_area` choices above are placeholders of mine. Local-file variant: `"install_url": "file:///usr/share/streamflex/firefox/streamflex.xpi"` (`file:///` VERIFIED as allowed by the docs above; the path is mine).
- The `Extensions` policy form (VERIFIED, https://firefox-admin-docs.mozilla.org/reference/policies/extensions/): `{"policies":{"Extensions":{"Install":["https://.../file.xpi","//path/to/xpi"],"Uninstall":["id@x"],"Locked":["id@x"]}}}`. `Install` is "a list of URLs or native paths for extensions to be installed"; `Locked` is "a list of extension IDs that the user cannot disable or uninstall". Available since Firefox 60 (ESR 60). The page recommends ExtensionSettings instead (added in 69, "closer in compatibility to Chrome and Edge"). Recommendation: use `ExtensionSettings`; `Extensions.Install` has no per-extension controls and the docs steer away from it.
- Versions (VERIFIED, compatibility table on the ExtensionSettings page): ExtensionSettings is available from Firefox 69 and ESR 68.1 (and "Firefox Enterprise" 149). `Extensions` from 60. The latest release checked is 156.0.1, so every Firefox StreamFlex would recommend has it.
- Can the user remove a force-installed extension? VERIFIED (docs): `force_installed` "prevents it from being removed by the user". Root can, by editing/removing the policy file (that is how policy removal works; with the entry gone Firefox no longer forces it; whether the extension is then uninstalled or left behind is UNKNOWN, and the `Extensions.Uninstall` list or `installation_mode: blocked` is the documented way to remove one). The user can still open `about:addons`, but remove/disable is blocked for it (REPORTED behavior, not tested).
- Can a self-distributed, signed, AMO-unlisted XPI be force-installed from our own URL? YES in principle: VERIFIED that `install_url` is any URL ("The URL from which Firefox can download..."), that AMO offers "self-distribution" signing that gives "a signed add-on without it being listed in the public add-ons directory" (https://extensionworkshop.com/documentation/enterprise/enterprise-distribution/ ), and that the policy docs' own advice for enterprise is the ExtensionSettings policy. Requirements VERIFIED from https://extensionworkshop.com/documentation/publish/self-distribution/ : the web server must send the XPI with `Content-Type: application/x-xpinstall`; automatic updates come from the manifest's `update_url` (Firefox installs a higher version found there), and if there is no `update_url` Firefox checks AMO for a LISTED update (so an unlisted XPI without `update_url` will not self-update from AMO). Signing: "All add-ons must be signed before they can be installed into Firefox's standard or beta versions"; unsigned add-ons work only on Developer Edition, Nightly and ESR with `xpinstall.signatures.required` toggled (VERIFIED, enterprise-distribution page), so StreamFlex should ship a signed XPI. Not tested end to end (no Firefox was run). If the XPI is shipped inside the StreamFlex package, the `file:///` route avoids hosting and TLS entirely and Firefox re-installs it when the file changes.

## 6. Linux Chrome and Edge (added 2026-09-28; the user's like-for-like decision: Chrome, Edge and full Firefox on Linux)

Scope: the OFFICIAL Google Chrome and Microsoft Edge packages, plus their Flathub repackagings. Distro Chromium is out of scope (its paths differ, section 6.4). Nothing was run in a real Chrome or Edge; every claim is from docs, source, package metadata and downloaded package scripts. Versions seen: Chrome stable 154.0.8037.57, Edge stable 154.0.4258.37.

### 6.1 Managed policy directories, and how several files combine
- VERIFIED (Chromium source, `components/policy/core/common/policy_paths.cc`): Google-branded Chrome reads `/etc/opt/chrome/policies` (Chrome for Testing: `/etc/opt/chrome_for_testing/policies`; unbranded Chromium: `/etc/chromium/policies`). Under it are `managed/` (mandatory) and `recommended/`. https://raw.githubusercontent.com/chromium/chromium/main/components/policy/core/common/policy_paths.cc . Google's own text agrees: `/etc/opt/chrome/policies/managed/` and `/recommended/`, and "Make sure that the files under /managed are not writable by non-admin users" (https://www.chromium.org/administrators/linux-quick-start/ , read this session).
- REPORTED for Edge: `/etc/opt/edge/policies/managed/` (and `recommended/`), any `*.json`. Sources: a Microsoft Q&A answer (community-written, not by Microsoft staff) https://learn.microsoft.com/en-us/answers/questions/2005942/configure-trusted-websites-for-microsoft-edge-on-l ; the third-party repo https://github.com/TommyTran732/Microsoft-Edge-Policies ("mandatory policies should be put in /etc/opt/edge/policies/managed/managed.json"); and the Flathub Edge launcher script (below), which bridges exactly `/etc/opt/edge/policies/{managed,recommended,enrollment}`. Microsoft's Learn policy pages do not mention Linux at all (a search of the policy index and the ExtensionInstallForcelist page found zero hits for "Linux"). So the Edge path is consistent across three independent third-party sources but has no Microsoft page.
- The file is a plain JSON object whose keys are policy names, with no wrapper (unlike Firefox's `{"policies": ...}`). VERIFIED by Google's example `{ "ShowHomeButton": true }` at the Chromium quick-start page above.
- **Several files ARE read and combined, per policy, not per file.** VERIFIED (Chromium source `config_dir_policy_loader.cc`, `LoadFromPath`): it enumerates every FILE in the directory (no `.json` filter in the loader), sorts the names lexicographically, and merges them with "priority to the last file in lexicographic order". The merge is by top-level policy key, so a policy named in two files takes the value from the later file whole; a list such as `ExtensionInstallForcelist` is NOT concatenated across files (my reading of the loader's `MergeFrom` comment, "gives priority to existing keys"). Google's documentation is weaker than the code: "you should not be setting the same policy in more than one file. If you do, it is undefined which of the values you specified prevails." So: never rely on the ordering; give StreamFlex its own file (for example `streamflex.json`), and if another file already sets `ExtensionInstallForcelist` (an IT-managed box), the helper must detect that and tell the user instead of silently shadowing or being shadowed. Files that fail JSON parsing are skipped with a syslog warning (VERIFIED in the loader). Trailing commas and Chromium's JSON extensions are accepted (VERIFIED, `JSON_ALLOW_TRAILING_COMMAS`).
- Any policy file makes the browser show "managed" status (REPORTED: https://github.com/flathub/com.google.Chrome/issues/43 , where a policy file caused Chrome's managed-mode warning). Expect a "Managed by your organization" indicator in `chrome://management` / the menu on Chrome and Edge once StreamFlex writes a policy (UNKNOWN for exact wording; not tested). This is a user-visible side effect of the force-install route; the Firefox policy route has the same kind of effect (`about:policies`), untested.
- Policies reload while the browser runs (`dynamic_refresh: true` and per-profile in the Chromium policy definition, VERIFIED) and the loader watches the directory. Tell users to restart anyway (Google's quick-start says so).

### 6.2 Does `ExtensionInstallForcelist` force-install an UNLISTED store extension on Linux?
- VERIFIED (Chromium policy definition, https://raw.githubusercontent.com/chromium/chromium/main/components/policy/resources/templates/policy_definitions/Extensions/ExtensionInstallForcelist.yaml ): supported on `chrome.*` from 9 (all desktop platforms including Linux). Each entry is a string `<32-letter extension id>[;<update url>]`; "By default, the Chrome Web Store's update URL is used" when no URL is given; users "can't uninstall or turn off" it; and "If a previously force-installed app or extension is removed from this list, Chrome automatically uninstalls it." The domain rule is stated for Windows and macOS ONLY: "On Windows instances, apps and extensions from outside the Chrome Web Store can only be forced installed if the instance is joined to ... Active Directory ... or enrolled in Chrome Enterprise Core", and "On macOS ... managed via MDM ...". **No such rule is written for Linux.**
- VERIFIED (Chrome for Developers, https://developer.chrome.com/docs/extensions/how-to/distribute/host-on-linux ): "Linux is the only platform where Chrome users can install extensions that are hosted outside of the Chrome Web Store." So on Linux even an OFF-store CRX from our own URL is allowed by the platform; the update URL must serve an update-manifest XML and the CRX must be served with an accepted content type.
- Unlisted store items: VERIFIED (https://developer.chrome.com/docs/webstore/cws-dashboard-distribution ) that an Unlisted item "does not create a listing on the Chrome Web Store, but does allows anyone to install your item if they know its Chrome Web Store URL". The pages I could read do not say in so many words "an unlisted item can be force-installed by ID", so that step is an inference: the default update URL is the store's, the store serves unlisted items to anyone with the link, and an ON-store item never hits the off-store rule anyway. Verdict: works on Linux without any domain or enrollment step: VERIFIED that no Linux restriction exists in the policy text, INFERRED that unlisted store items resolve; not tested end to end.
- Entry forms (VERIFIED from the policy definition's example): `"<id>;https://clients2.google.com/service/update2/crx"` (explicit store URL) or just `"<id>"`.
```json
/etc/opt/chrome/policies/managed/streamflex.json
{
  "ExtensionInstallForcelist": [
    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa;https://clients2.google.com/service/update2/crx"
  ]
}
```
  (The 32 a's are a placeholder id; the shape is VERIFIED.) `ExtensionSettings` with `installation_mode: "force_installed"` and `override_update_url` is the other route (Edge's docs list it; Chromium's has the same schema, not re-read); use the simple list form.
- **Edge on Linux: UNDOCUMENTED, not disproved.** VERIFIED (Microsoft Learn, https://learn.microsoft.com/en-us/deployedge/microsoft-edge-browser-policies/extensioninstallforcelist , updated 2026-07-09): "Supported versions" lists Windows >= 77, macOS >= 77, Android >= 149, iOS >= 149. Linux is not listed. The same page: "For Windows instances not joined to a Microsoft Active Directory domain, forced installation is limited to apps and extensions listed in the Microsoft Edge Add-ons website" and "On macOS ... apps and extensions from outside the Microsoft Edge Add-ons website can only be force installed if the instance is managed via MDM". Entry form VERIFIED there: `gbchcmhmhahfdphkhkmpfmihenigjmpp;https://edge.microsoft.com/extensionwebstorebase/v1/crx` (Edge Add-ons update URL) or a bare id, "By default, the Microsoft Edge Add-ons website's update URL is used". Edge is Chromium and reads the same directory layout (6.1), so the policy very likely works, but Microsoft does not document Linux support for this policy, so treat it as UNKNOWN until tested on a real Edge for Linux (`edge://policy` shows which policies loaded and whether the force-install was applied). Important subtlety: Microsoft's statement says an extension "listed in" Edge Add-ons; an UNLISTED item is on the site, and the user has decided to ship unlisted, so this is another thing the Phase 0 test must confirm on Windows Home AND Linux.
- Edge policy file name when two browsers are managed: use a separate file per browser directory; the two browsers do not share `/etc/opt/*`.

### 6.3 Native-messaging host manifest directories
- Chrome, VERIFIED (https://developer.chrome.com/docs/extensions/develop/concepts/native-messaging ): system-wide `/etc/opt/chrome/native-messaging-hosts/<name>.json`; per-user `~/.config/google-chrome/NativeMessagingHosts/<name>.json`. (Chromium: `/etc/chromium/native-messaging-hosts/`, `~/.config/chromium/NativeMessagingHosts/`.)
- Edge, VERIFIED (Microsoft Learn, https://learn.microsoft.com/en-us/microsoft-edge/extensions/developer-guide/native-messaging , tab "Linux"): system-wide `/etc/opt/edge/native-messaging-hosts`; per-user `~/.config/microsoft-edge/NativeMessagingHosts`. The page says the per-user location is inside the "user data directory", so a non-default `--user-data-dir` moves it.
- Manifest keys (VERIFIED, both pages): `name` (lowercase letters, digits, underscore, dot), `description`, `path` (**absolute on Linux and macOS**), `type: "stdio"`, `allowed_origins` = `["chrome-extension://<id>/"]` with a trailing slash and no wildcards. "On Linux and macOS: provide read permissions on the manifest and run permissions on the host." Edge's page says the browser starts the host with its working directory set to the host binary's directory. The first argument the host receives is the calling origin; message limits: host to browser 1 MB, browser to host 4 GB.
- Both stores, two ids: the extension is a different listing in each store and gets a different id, so `allowed_origins` needs the Chrome id AND the Edge id (Microsoft says so for the Windows registry case; on Linux each browser reads its own directory, so the same manifest file can be dropped into both directories with both origins, as I read it: not tested). The manifest `name` must equal the name the extension passes to `connectNative`.
- Fallback: on Windows, Edge falls back to the Chrome/Chromium registry keys (VERIFIED, same page). The page gives NO Linux fallback, so StreamFlex should write the host manifest into BOTH `/etc/opt/chrome/native-messaging-hosts/` and `/etc/opt/edge/native-messaging-hosts/` (and the Firefox one, section 4).
- The host binary is the same StreamFlex helper for all three browsers; only the manifest directory and the `allowed_origins` / `allowed_extensions` field differ (Firefox uses `allowed_extensions` and an extension id like `name@example.com`).

### 6.4 Detection: official packages against Flatpak
- Official Chrome deb/rpm: files under `/opt/google/chrome/`. VERIFIED indirectly: the Chrome deb's postinst installs icons from `/opt/google/chrome/product_logo_*.png` and creates NSS symlinks in `/opt/google/chrome`; Flathub's `apply_extra.sh` extracts `./opt/google/chrome` from Google's deb. Launcher symlinks `/usr/bin/google-chrome-stable` -> alternatives (VERIFIED by the prerm's `update-alternatives --remove ... /usr/bin/google-chrome-stable`). Main ELF is `/opt/google/chrome/chrome` (REPORTED; the Flathub command is `chrome`; not opened in the deb).
- Official Edge deb/rpm: `/opt/microsoft/msedge/` (VERIFIED: postinst references `/opt/microsoft/msedge/product_logo_*.png`; Flathub Edge `cobalt.ini` has `EntryPoint=/app/extra/msedge`, so the ELF is `msedge`; `/usr/bin/microsoft-edge-stable` VERIFIED via prerm).
- Design rule, same as Firefox's (section 1): probe for an ELF at `/opt/google/chrome/chrome` and `/opt/microsoft/msedge/msedge` (optionally the `-beta`/`-unstable` and `-beta`/`-dev` sibling directories; REPORTED, not needed). A `PATH` lookup of `google-chrome` finds wrapper scripts and, on some distros, Chromium under other names.
- **Flatpak builds: `com.google.Chrome` and `com.microsoft.Edge` on Flathub** (manifests read this session, master branch). They are repackagings of the vendors' own debs via `extra-data`, both proprietary (`LicenseRef-proprietary`), not marked vendor-verified in the Flathub appstream API (`verification` metadata all null; REPORTED reading). Chrome Flatpak has x86_64 and aarch64 builds; Edge Flatpak is x86_64 only (VERIFIED in the manifests).
  - **Policies: the host's policy IS honored.** VERIFIED: both launcher scripts (`chrome.sh` in flathub/com.google.Chrome, `edge.sh` in flathub/com.microsoft.Edge) loop over `managed recommended enrollment` under `/etc/opt/chrome/policies` (Edge: `/etc/opt/edge/policies`) and symlink every file from `/run/host/etc/opt/<browser>/policies/<type>` into the sandbox, with the comment "Merge the policies with the host ones", and both manifests grant `--filesystem=host-etc`. The Chrome script takes every file; the Edge script only `*.json`, depth 1. So a StreamFlex `streamflex.json` in the host directory reaches a Flatpak Chrome or Edge.
  - **Native messaging: NOT reachable, treat as unsupported.** The launchers only bridge policies. The sandbox's own `/etc/opt/*/native-messaging-hosts` is not the host's, and the host manifest's `path` (for example `/usr/libexec/streamflex/host`) does not exist inside the sandbox, whose `/usr` is the runtime's. REPORTED (third-party reports and the design gap): https://github.com/flatpak/xdg-desktop-portal/issues/655 ("confinement prevents the browser from executing random executables on the user's host"), and the search-result excerpts of https://github.com/openai/codex/issues/42953 and https://github.com/anthropics/claude-code/issues/15587 (Chrome Flatpak cannot reach a host native-messaging app; the per-user location becomes `~/.var/app/com.google.Chrome/config/google-chrome/NativeMessagingHosts/`, and the workarounds are `flatpak override` filesystem grants or a wrapper using `flatpak-spawn --host`, which the Chrome Flatpak does not permit: I did not verify the last point in the manifest, which has no `org.freedesktop.Flatpak` talk-name; VERIFIED absence in the `finish-args` list above). VERIFIED (https://flatpak.github.io/xdg-desktop-portal/docs/ , interface list read this session): there is no `org.freedesktop.portal.NativeMessaging` interface in the documented portals; the 2021 proposal is a closed issue.
  - Detection: `flatpak info com.google.Chrome` / `com.microsoft.Edge`, or files under `/var/lib/flatpak/app/<id>` or `~/.local/share/flatpak/app/<id>` (REPORTED). Classify as FLATPAK_ONLY and treat as "no usable browser", exactly like snap Firefox. Do not write manifests into `~/.var/app/...` (workarounds need a user-owned override and a wrapper; not recommended).
- Ubuntu's `chromium-browser` is a snap and Debian's `chromium` uses `/etc/chromium/policies` (Ubuntu's chromium.org note: `/etc/chromium-browser/policies`, VERIFIED at the quick-start page): all out of scope, and never accepted as "Chrome".

### 6.5 Installing Chrome and Edge: official repos and packages
- Both vendors' debs configure the apt repo themselves on install. VERIFIED by reading the maintainer scripts I downloaded (first ~400 KB of each deb): Chrome's `postinst` runs `install_key` then `install_deb822_sources`, which writes a `.sources` file with `URIs: https://dl.google.com/linux/chrome-stable/deb/`, `Suites: stable`, `Components: main`, `Architectures: amd64`, `Signed-By: /usr/share/keyrings/...` (the key data is embedded in the script; a `repo_add_once` flag in `/etc/default/google-chrome` controls it). Edge's `postinst` is the same structure: `install_key`, `install_deb822_sources`, `microsoft-edge.sources`, `URIs: https://packages.microsoft.com/repos/edge-stable`, `Suites: stable`, `Architectures: amd64`, key at `/usr/share/keyrings/microsoft-edge.gpg`. The `postrm` scripts also show the older one-line forms (`deb [arch=amd64] https://dl.google.com/linux/chrome/deb/ stable main`; `deb [arch=amd64] https://packages.microsoft.com/repos/edge stable main`). So StreamFlex needs no apt-source or pin step for Chrome or Edge; installing the vendor `.deb` is enough and keeps it updated. (There is NO Ubuntu-snap-style trap: Ubuntu does not ship a `google-chrome` or `microsoft-edge` package.)
- Direct downloads (HTTP 200 on a HEAD request this session; not downloaded in full): `https://dl.google.com/linux/direct/google-chrome-stable_current_amd64.deb`, `.../google-chrome-stable_current_x86_64.rpm`; Edge: `https://packages.microsoft.com/repos/edge/pool/main/m/microsoft-edge-stable/microsoft-edge-stable_154.0.4258.37-1_amd64.deb` (latest version read from the repo's Packages index).
- Suggested user command on Debian/Ubuntu (my form, `apt` resolves the dependencies of a local file): `sudo apt install ./google-chrome-stable_current_amd64.deb` (or the Edge deb).
- **Architectures (VERIFIED from each repo's `InRelease` and `Packages`):** Chrome's repo lists `amd64` and `arm64`, and `google-chrome-stable` 154.0.8037.57 exists in both (Filenames `..._amd64.deb` and `..._arm64.deb`). Edge's repo lists `amd64 arm64 armhf all`, but the `arm64` and `armhf` `Packages` files are EMPTY (0 bytes; the `.gz` is 29 bytes), so **Edge for Linux is amd64 only**. A Raspberry Pi 64-bit box can run Chrome (arm64 deb) and Firefox but not Edge; 32-bit ARM can run neither Chrome nor Edge. (Chrome's own postinst-written `.sources` file says `Architectures: amd64` only, so on arm64 the auto-added source will not update Chrome; UNKNOWN whether that is intended; a StreamFlex arm64 helper may have to fix the file.)
- Signing keys (VERIFIED by downloading and `gpg --show-keys`): Google `https://dl.google.com/linux/linux_signing_key.pub` primary key `EB4C 1BFD 4F04 2F6D DDCC EC91 7721 F63B D38B 4796` (rsa4096, 2016-04-12, "Google Inc. (Linux Packages Signing Authority)"; the same fingerprint appears in the deb's script comment). Microsoft `https://packages.microsoft.com/keys/microsoft.asc` primary key `BC52 8686 B50D 79E3 39D3 721C EB3E 94AD BE12 29CF` (rsa2048, 2015-10-28, "Microsoft (Release signing)"; same fingerprint in the Edge script comment). Neither is needed if the deb is installed directly.
- RPM (Fedora, RHEL): both vendors publish yum repos. HEAD 200 (existence only) for `https://dl.google.com/linux/chrome/rpm/stable/x86_64/repodata/repomd.xml`, `.../aarch64/repodata/repomd.xml` (an aarch64 Chrome RPM repo exists; contents not read), and `https://packages.microsoft.com/yumrepos/edge/repodata/repomd.xml`. The repo-setup commands and GPG key handling were NOT read from an official page: UNKNOWN. Google's RPM is expected to install its own repo file (REPORTED, by analogy with the deb; not checked). Fedora also ships Chromium natively; that is not Chrome.
- **RPM/dnf repo setup (closed 2026-09-28).** VERIFIED from the `%post` scriptlets inside the vendors' own x86_64 RPMs (I downloaded the first 1.5 MB of `google-chrome-stable_current_x86_64.rpm` and `microsoft-edge-stable-154.0.4258.37-1.x86_64.rpm`; no install). Both RPMs configure their yum repo themselves on install (`install_rpm_key`, then write the repo file), the same self-configuring pattern as the debs, so on Fedora `sudo dnf install ./google-chrome-stable_current_x86_64.rpm` (or the Edge rpm) is enough (the dnf command is my form). The repo files they write:
  ```ini
  # /etc/yum.repos.d/google-chrome.repo   (Chrome; DEFAULT_ARCH="x86_64", REPOCONFIG="https://dl.google.com/linux/chrome/rpm/stable")
  [google-chrome]
  name=google-chrome
  baseurl=https://dl.google.com/linux/chrome/rpm/stable/x86_64
  enabled=1
  gpgcheck=1
  gpgkey=https://dl.google.com/linux/linux_signing_key.pub

  # /etc/yum.repos.d/microsoft-edge.repo   (Edge; REPOCONFIG="https://packages.microsoft.com/yumrepos/edge-stable")
  [microsoft-edge]
  name=microsoft-edge
  baseurl=https://packages.microsoft.com/yumrepos/edge-stable
  enabled=1
  gpgcheck=1
  gpgkey=https://packages.microsoft.com/keys/microsoft.asc
  ```
  Both scriptlets also write a zypper repo file under `/etc/zypp/repos.d/` (openSUSE). Chrome's scriptlet has a `repo_add_once` flag in `/etc/default/google-chrome` like the deb. Key fingerprints (VERIFIED with `gpg --show-keys` on the downloaded keys): Google `linux_signing_key.pub` primary `EB4C 1BFD 4F04 2F6D DDCC EC91 7721 F63B D38B 4796`; Microsoft `microsoft.asc` primary `BC52 8686 B50D 79E3 39D3 721C EB3E 94AD BE12 29CF`. Microsoft also publishes an older repo `https://packages.microsoft.com/yumrepos/edge/` whose `config.repo` (VERIFIED, fetched) is `[edge-yum] baseurl=https://packages.microsoft.com/yumrepos/edge/ gpgcheck=0 repo_gpgcheck=0 gpgkey=https://packages.microsoft.com/yumrepos/edge/repodata/repomd.xml.key`; that repomd key has the same fingerprint `BC52 ... 29CF` (VERIFIED). Prefer the scriptlet's `edge-stable` form with `gpgcheck=1` (`edge-stable/repodata/repomd.xml` answers HTTP 200). The aarch64 Chrome RPM was not inspected (its scriptlet presumably sets `DEFAULT_ARCH` to aarch64: UNKNOWN); Edge has no arm64 build (above). These are package-derived, not from an official how-to page.
- Official install pages were not readable: Google's Chrome download page and Microsoft's Edge for Linux page were not fetched. Treat the instructions above as derived from the packages themselves, which are primary, not from Google's or Microsoft's how-to text.

### 6.6 Should the installer's "offer full Firefox" prompt become "offer any of the three"? Evidence only; the user decides
Facts that bear on it (all sourced above):
- The reason a Firefox prompt was needed is the Ubuntu snap trap (sections 1-2). Chrome and Edge have no equivalent: Ubuntu does not ship them, so the only way onto a box is the vendor deb/rpm, and that deb is a plain single-file install that configures its own repo (6.5). Installing either is simpler than the Firefox path on Ubuntu (no key, no `sources`, no pin).
- Architecture limits: Edge is amd64-only (6.5), so on a Raspberry Pi or any ARM box the prompt cannot offer Edge; Chrome has arm64.
- DRM: Chrome and Firefox are the DRM candidates on Linux (Netflix's help page lists both; not a measurement). Edge is treated as non-DRM on Linux until the Phase 0 test (section "Linux Edge and Widevine"). If StreamFlex's main services are DRM, the offer should lead with Chrome or Firefox.
- Policy/native-messaging cost per browser: Firefox needs an app-directory-independent `/etc/firefox/policies/policies.json` plus `/usr/lib/mozilla/native-messaging-hosts/` (or the per-user dir); Chrome needs `/etc/opt/chrome/policies/managed/` plus `/etc/opt/chrome/native-messaging-hosts/`; Edge needs the Edge equivalents, with Edge's Linux policy support undocumented (6.2). Adding Chrome or Edge to the helper is one more pair of directories each, not a new mechanism. Firefox keeps "one file, first found wins" while Chrome/Edge merge files, so the helper's merge logic differs.
- Sandboxed builds (snap Firefox, Flatpak Chrome/Edge) are excluded by detection in every case (6.4); the prompt should say so for all three.
- What the user has to see: one "recommended" browser prevents a choice screen on a couch UI, while "any of the three" needs a controller-driven list and a per-browser probe. Both are possible; the evidence does not favor either on feasibility alone. If the prompt becomes "any of the three", the ordering that the facts support is: Chrome (works everywhere the vendor ships, DRM candidate, simplest install), Firefox (all architectures via distro or Mozilla, DRM candidate), Edge last (amd64 only, non-DRM on Linux, Linux policy support undocumented).

## Open items
- NEW (Chrome/Edge): UNKNOWN: Edge for Linux applying `ExtensionInstallForcelist` from `/etc/opt/edge/policies/managed/` (Microsoft documents Windows/macOS/mobile only); and that an UNLISTED Chrome Web Store / Edge Add-ons item force-installs by id on Linux. Both need a hands-on test on a real box (`chrome://policy`, `edge://policy`, `chrome://extensions`).
- NEW: UNKNOWN: whether the last-file-wins loader behavior (code) is stable; Google's docs call it undefined.
- NEW: UNKNOWN: the exact "managed" UI wording and whether force-install removes a per-user "Add to Chrome" prompt; not tested.
- SETTLED (section 6.5): RPM repo files, key URLs and fingerprints for Chrome and Edge, from the RPMs' own scriptlets. Still UNKNOWN: the aarch64 Chrome RPM's repo arch line; Google/Microsoft official how-to pages not read.
- NEW: UNKNOWN: arm64 Chrome `.sources` file arch line, and Chrome/Edge behavior under a non-default `--user-data-dir` for the per-user manifest.
- SETTLED this pass: native-messaging system dir for Mozilla deb and tarball (`/usr/lib/mozilla`); Mozilla's official APT file names, contents and key fingerprint; ExtensionSettings and Extensions syntax and versions; policies file precedence (first found, no merge).
- UNKNOWN: whether Debian's `firefox-esr`, Arch, Fedora and Mint builds define `HAVE_USR_LIB64_DIR` / enable `MOZ_SYSTEM_POLICIES` (only Mozilla's own builds were measured). A cheap test is the same `libxul.so` string check on each distro package.
- UNKNOWN: whether `/etc/firefox/policies` applies to Debian `firefox-esr` (the app name it reports may differ from `firefox`; `SysConfD` is built from the app-info name lower-cased).
- UNKNOWN: an end-to-end test that a signed unlisted XPI is force-installed from our URL or a `file:///` path on a real Firefox; and what happens to it when the policy entry is later removed.
- UNKNOWN: Mozilla's article has separate tarball sections ("System Firefox installation", "Local Firefox installation"); not read in detail, so the tarball recipe in section 2 is still the earlier one.
- UNKNOWN: Pi OS desktop image default browsers on trixie (only the archive contents were checked).
- UNKNOWN: Linux Mint's Firefox packaging; assumed a real deb.
- Tooling note: two of the fetched Ubuntu how-to pages tripped the prompt-injection scanner on the words "Pin-Priority". The content was ordinary apt pinning syntax and was treated as data only.


## Linux Edge and Widevine (resolved 2026-09-28)

Verdict: **UNKNOWN, leaning "does not work out of the box on current stable"; treat Edge on Linux as NOT a DRM target until a Phase 0 hands-on test says otherwise.** The one primary source saying it works (Netflix's page) is a support-matrix claim, not a test; the evidence that it is disabled is Microsoft's own Q&A plus a user report on 143. Nobody in the sources found has actually confirmed working Widevine on the stable .deb in 2026 (the one 2026 success is a Flatpak test, below).

Evidence FOR (works):
- VERIFIED (fetched 2026-09-28) https://help.netflix.com/en/node/30081 lists, for Linux: "Chrome 117 or later: Up to Full HD (1080p)", "Firefox 129 or later: Up to Full HD (1080p)", "Edge 118 or later: Up to Full HD (1080p)". Same page says Netflix cannot guarantee Linux functionality and gives no Linux troubleshooting. This is a stated support claim, undated on the page, and it contradicts the Microsoft answer below. Note Chrome/Firefox 1080p on Linux is also doubtful in practice (Widevine L3 is normally capped near 720p); the page is not a reliable measurement for any of the three.
- REPORTED (single test, 2026-08-11 to 2026-09-11) https://github.com/flathub/com.microsoft.Edge/pull/871: a contributor got Netflix playing in the Flatpak build of stable Edge (test build 151.0.4129.78, Widevine CDM 4.10.3050.1 downloaded into the profile) once the sandbox was fixed. Before the fix: Netflix error D7702-1003, "libwidevinecdm.so: cannot open shared object file: Operation not permitted". PR still open; this is Flatpak, not the packages.microsoft.com .deb, and one author's log. It does show the CDM can load in a current Edge on Linux, which weakens "Microsoft disabled it entirely".
- REPORTED (2021, single forum post) https://forum.endeavouros.com/t/how-to-watch-netflix-on-microsoft-edge/21501: Arch user got Netflix in Edge after `yay -S chromium-widevine`. Old, non-.deb, manual CDM install.

Evidence AGAINST (does not work):
- VERIFIED (fetched) https://learn.microsoft.com/en-us/answers/questions/2241881/widevine-drm-disabled-for-linux-environments : asked 2025-03-30, Microsoft-affiliated answer 2025-03-31: "Unfortunately, Widevine DRM is not available on Linux now." Flag text quoted: `edge://flags/#edge-widevine-drm` shows "Not available on your platform. (Enables the Widevine content decryption module in Microsoft Edge. - Mac, Windows)". No Edge version stated. Answerer offers `microsoft-edge --enable-features=msWidevinePlatform` (all Edge instances closed first) as only a "possible workaround", adding "If this workaround does not work, I'm afraid it is simply disabled for some reasons." Nobody in the thread confirms it works.
- REPORTED (single forum post, 2025-12-28) https://community.learnlinux.tv/t/microsoft-disabled-widevine-drm-in-edge-143-for-linux/4938 : user says DRM playback (Prime, Netflix) broke on Edge 143; rolling back to 142 failed; switching to Chrome fixed it. No Microsoft response. One post; does not prove 142 worked or that 143 removed anything.

Answers to the lead's sub-questions:
- Out of the box on the stable .deb/.rpm? Not established. Microsoft's own answer says no (as of Mar 2025, version unspecified); one Dec 2025 user report says it broke in 143.
- Supported/unsupported way? `--enable-features=msWidevinePlatform` is UNKNOWN (suggested by Microsoft-side answerer, unconfirmed by anyone). Manual CDM install (2021 Arch report) and the Flatpak/Zypak path (2026) are unsupported and distro/packaging specific.
- Insider builds gaining Widevine around 2021, then stable, then removal: UNKNOWN. I found no primary source (Edge blog, release notes) for any of it in five searches; the only 2021 evidence is the forum post above and an unrelated Microsoft Tech Community thread title "MS Edge Beta can't play Netflix on Ubuntu" (2021, not opened). Do not assert this history.

Phase 0 hands-on test (one line): on a clean Ubuntu/Fedora VM install stable Edge from packages.microsoft.com, open `edge://flags/#edge-widevine-drm` and `edge://components` (look for a Widevine CDM version), then try Netflix or the Bitmovin/Shaka DRM demo with and without `--enable-features=msWidevinePlatform`, recording the Edge version and result.
