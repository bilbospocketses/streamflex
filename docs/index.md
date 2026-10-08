---
layout: default
---

<p><img src="{{ '/assets/branding/streamflex-banner.jpg' | relative_url }}" alt="StreamFlex: super flexible desktop streaming"></p>

StreamFlex is a customizable application launcher and front end designed with a TV-friendly [10 foot user interface](https://en.wikipedia.org/wiki/10-foot_user_interface), intending to mimic the look and feel of a streaming box or game console. StreamFlex allows you to launch applications on your HTPC or couch gaming PC entirely by use of a TV remote or a gamepad. No keyboard or mouse required!

StreamFlex is compatible with both Windows and Linux (including Raspberry Pi devices).

## Screenshots
<p><img src="{{ '/assets/screenshots/screenshot1.png' | relative_url }}"></p>
<p><img src="{{ '/assets/screenshots/screenshot2.png' | relative_url }}"></p>

## About
StreamFlex builds on Flex Launcher, which complexlogic first released in late 2021. Its creation was motivated by the puzzling lack of good HTPC control options at the time. StreamFlex is designed with the following principles in mind:
- **Flexibility:** Indicated in the name Stream*Flex*, the configuration should be highly flexible, with user having a great deal of control over the appearance and behavior of the program.
- **Simplicity:** The interface should be simple, yet beautiful.
- **Portability:** Many of the other existing solutions only support a single platform. StreamFlex aims to be portable and cross-platform, running on ARM-based SBCs such as Raspberry Pi, to high-end Windows PCs, and everything in between.
- **Ease of Navigation:** StreamFlex should be completely navigable using only directional keys, enter, and back. The launcher should include built-in support for gamepad devices.

StreamFlex is free and open source software, released under the GNU General Public License v3.0. You may modify and redistribute it under the terms of that licence. The source code is available on [GitHub](https://github.com/bilbospocketses/streamflex).

This project started from complexlogic's original Flex Launcher (v2.2) and is developed independently.

## Features
The following is a list of features supported by StreamFlex:

#### Custom Backgrounds
The background of StreamFlex is customizable. There are three supported modes for the background: a solid color, an image, or a slideshow of images. In the case of the slideshow, the user is able to specify how long to show the images, and how quickly to fade between them.

#### Application Icons
The user is able to pick the application icons, specify how large they should be, and how far apart they should be. StreamFlex has a built-in [icon library](icons) of streaming services, media apps and generic icons, each named in the config by a short name such as `netflix`, or you can use image files of your own. The user is also able to control the vertical centering of the application icons.

#### Submenus
StreamFlex supports submenus, which allows you to group your various applications in an organized fashion. For example, you can have separate menus for media and gaming applications.

#### Selection Highlighting
The user is able to change the size, color, opacity, and corner radius of the default highlight rectangle, as well as the border.

Additionally, StreamFlex also supports an icon-based selection mode where the user can specify a different icon that will display when a menu entry is selected. This allows the user to implement custom highlighting effects. 

#### Fonts
The font, size, color, and opacity of all texts are customizable. StreamFlex ships with a few royalty free fonts, but the user is able to substitute their own font instead.

#### Hotkeys
With the hotkeys feature, you can map custom commands to a button on your remote. This can be used to “speed dial” your favorite applications, add new controls, or exit a currently running application.

#### System Controls
There are built-in menu options to shut down, restart, or put your PC to sleep.

#### Gamepad Controls
StreamFlex has full support for gamepad controls, allowing easy menu navigation. The controls are on by default: connect your gamepad. The user can also add a custom mapping for a given gamepad.

#### Screensaver
StreamFlex includes a screensaver mode that will dim the screen after it has been idle. The user is able to adjust how long the idle time should be, and how much to dim the screen.

#### Clock
There is an available clock widget that displays the current time and, optionally, the current date. The user is able to adjust the size, font, placement, and time/date format.

## Support
If you believe you have found a bug in StreamFlex, or you have a feature request, open an issue on the [GitHub issue tracker](https://github.com/bilbospocketses/streamflex/issues).
