<div align="center">
  <img src="docs/assets/branding/streamflex-banner.jpg" alt="StreamFlex: super flexible desktop streaming" width="800">


# StreamFlex
</div>
<details open>
  <summary>Table of Contents</summary>
  <ol>
    <li>
      <a href="#about">About</a>
    </li>
    <li>
      <a href="#screenshots">Screenshots</a>
    </li>
    <li>
      <a href="#installation">Installation</a>
      <ul>
        <li><a href="#windows">Windows</a></li>
        <li><a href="#linux">Linux</a></li>
      </ul>
    </li>
    <li><a href="#usage">Usage</a></li>
      <ul>
        <li><a href="#controls">Controls</a></li>
        <li><a href="#debugging">Debugging</a></li>
      </ul>
    <li><a href="#development-status">Development Status</a></li>
    <li><a href="#documentation">Documentation</a></li>
    <li><a href="#credits">Credits</a></li>
    <li><a href="#license">License</a></li>
  </ol>
</details>

## About
StreamFlex is a customizable application launcher and front end designed with a TV-friendly [10 foot user interface](https://en.wikipedia.org/wiki/10-foot_user_interface), intending to mimic the look and feel of a streaming box or game console. StreamFlex allows you to launch applications on your HTPC or couch gaming PC entirely by use of a TV remote or a gamepad. No keyboard or mouse required!

StreamFlex is compatible with both Windows and Linux (including Raspberry Pi devices).

## Screenshots
![Screenshot 1](docs/assets/screenshots/screenshot1.png "Screenshot 1")

https://user-images.githubusercontent.com/95071366/208355237-11f00cbb-9cc3-436b-98f7-8de350e584a7.mp4

## Installation
Executables are available for Windows 64 bit, Linux x86-64, and Raspberry Pi. You can also compile the program yourself using the [compilation guide](docs/compilation.md).

### Windows
Download the win64 .zip file from the [latest release](https://github.com/bilbospocketses/streamflex/releases/latest) and extract the contents to a directory of your choice. StreamFlex should be run on an up-to-date Windows 10 system, or Windows 11.

### Linux
Binary packages for APT and pacman based distributions are available from the [latest release](https://github.com/bilbospocketses/streamflex/releases/latest) and the [download page](https://bilbospocketses.github.io/streamflex/download). Download the package for your system, then install it from the directory you saved it to.

#### APT-based x86-64 Distributions (Debian, Ubuntu, etc.)
This package is compatible with Debian 12 (Bookworm) and later, Ubuntu 22.04 and later.
```bash
sudo apt install ./streamflex_*_amd64.deb
```
#### Pacman-based x86-64 Distributions (Arch, Manjaro, etc.)
```bash
sudo pacman -U streamflex-*-x86_64.pkg.tar.zst
```
#### Raspberry Pi
This package is compatible with Raspberry Pi OS 12 (Bookworm) and later, 64 bit only.
```bash
sudo apt install ./streamflex_*_arm64.deb
```
#### Copying Assets to Home Directory
The Linux packages install a default config file and assets to `/usr/share/streamflex`. It is strongly recommended to *not* edit this config file directly, as it will be overwritten if you upgrade to a later version of StreamFlex. Instead, copy these files to your home directory and edit it there.
```bash
cp -r /usr/share/streamflex ~/.config
sed -i "s|/usr/share/streamflex|$HOME/.config/streamflex|g" ~/.config/streamflex/config.ini
```

## Usage
StreamFlex uses an INI file to configure the menus and settings. Upon  startup, the program will search for a file named `config.ini` in the following locations in order:
1. The current working directory
2. The directory containing the `streamflex` executable
3. Linux only: `~/.config/streamflex`
4. Linux only: `/usr/share/streamflex`

If your config file is in one of the above locations, StreamFlex can be started simply by double clicking the executable file or adding it to autostart. If your config file is in a non-standard location, you must specify the path via command line argument:
```bash
streamflex -c /path/to/config.ini
```
StreamFlex ships with a default config file which is intended strictly for demonstration purposes. If you try to start one of the applications, it is possible that nothing will happen because the install path is different on your system, or you don't have the application installed at all. See the [configuration file documentation](docs/configuration.md#configuring-streamflex) for instuctions on how to change the menus and settings.

### Controls
The keyboard arrow keys move the highlight cursor: left and right along a row, and up and down between the rows of a grid. Enter selects the current entry, backspace goes back to the previous menu (if applicable), and Esc quits the program. 

#### TV Remotes
StreamFlex does not feature built-in decoding of IR or CEC signals. If you plan to use a TV remote to control the device, it is assumed that these signals are decoded by the OS or another program and mapped to keyboard presses, which can then be received by StreamFlex.

#### Gamepads
Gamepad controls are built-in to the program, and are enabled by default: the gamepad controls should "Just Work" for most users. To turn them off, open your configuration file and, under the "Gamepad" section, set "Enabled" to false, or turn them off on the settings screen's Controls page. If your gamepad is not recognized automatically, or you want to change the default controls, see the [gamepad controls documentation](docs/configuration.md#gamepad-controls).

### Debugging
StreamFlex has a debug mode which may be enabled as follows:
```bash
streamflex -d
```
This will output a logfile named `streamflex.log` in the same directory as `streamflex.exe` on Windows, and in `~/.local/share/streamflex` on Linux. 

## Development Status
This project started from complexlogic's original Flex Launcher at v2.2 and is developed independently; it does not sync with or contribute back to that project. Versioning restarted at 0.1.0, which was published under the name Flex Launcher; the project has been StreamFlex since. A major overhaul is under way, and the project stays below 1.0 until it is in place and working, so expect changes between 0.x releases. See [CHANGELOG.md](CHANGELOG.md) for what has changed.

## Documentation
Here is a list of available documentation:
- [Configuration](docs/configuration.md#configuring-streamflex)
- [Icon Library](https://bilbospocketses.github.io/streamflex/icons): every built-in icon and the name to use in your config
- [General Setup Guide](docs/setup.md#setup-guide)
  - [Windows-specific Setup Guide](docs/setup_windows.md#windows-setup-guide)
  - [Linux-specific Setup Guide](docs/setup_linux.md#linux-setup-guide)
- [Compilation Guide](docs/compilation.md#compilation-guide)

## Credits
StreamFlex started from Flex Launcher, created by [complexlogic](https://github.com/complexlogic), who released it into the public domain under the Unlicense. This project is built on that work.

StreamFlex is made possible by the following projects:
- [SDL](https://github.com/libsdl-org/SDL), including the subprojects:
  - [SDL_image](https://github.com/libsdl-org/SDL_image)
  - [SDL_ttf](https://github.com/libsdl-org/SDL_ttf)
- [Nanosvg](https://github.com/memononen/nanosvg)
- [inih](https://github.com/benhoyt/inih)
- [Material Symbols](https://github.com/google/material-design-icons) (Apache-2.0), the glyphs of the generic library icons

The streaming and media service icons in the icon library are the trademarks of their owners; see [the brand notice](assets/icons/library/brands/NOTICE.md).

StreamFlex's design, inherited from Flex Launcher, was strongly influenced by the excellent desktop application launcher [xlunch](https://github.com/Tomas-M/xlunch).

## License
StreamFlex is released under the [GNU General Public License v3.0](LICENSE).

---

**Disclaimer:** StreamFlex is an independent open-source project and is not affiliated, associated, authorized, endorsed by, or in any way officially connected with Comcast/Xfinity, Streamflex Labs, RA Apps, BHSD Trading LLC, Streamflex (streamflex.com), or any other commercial entities operating under similar names. All product names, logos and trademarks are the property of their respective owners.
