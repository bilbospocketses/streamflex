---
layout: default
title: Configuration
---
# Configuring StreamFlex
## Table of Contents

1. [Overview](#overview)
2. [The Settings Screen](#the-settings-screen)
3. [Settings](#settings)
    - [General](#general)
    - [Background](#background)
    - [Layout](#layout)
    - [Titles](#titles)
    - [Highlight](#highlight)
    - [Scroll Indicators](#scroll-indicators)
4. [Creating Menus](#creating-menus)
    - [Special Commands](#special-commands)
    - [Desktop Files (Linux Only)](#desktop-files-linux-only)
5. [Clock](#clock)
6. [Screensaver](#screensaver)
7. [Hotkeys](#hotkeys)
8. [Gamepad Controls](#gamepad-controls)
9. [Transparent Backgrounds](#transparent-backgrounds)

## Overview
StreamFlex uses an [INI file](https://en.wikipedia.org/wiki/INI_file) to configure settings and menus. The INI file consists of sections enclosed in square brackets, and in each section there are entries which consist of a key and a value. Example:
```ini
[Section]
Key1=value
Key2=value
...
```
A line that starts with `#` or `;` is a comment, and is ignored. A comment can also follow a value, after a space and a semicolon: `Columns=4 ; four across`. The [settings screen](#the-settings-screen) keeps every comment when it saves. Here are a few things to note about the configuration settings for StreamFlex:
- All keys and values are case sensitive.
- Full UTF-8 character set is supported for titles.
- The following image formats are supported: JPEG, PNG, WebP and SVG
- Relative paths are evaluated with respect to the *current working directory*, which may not be the same as the directory that the config file is located in. It is recommended to use absolute paths whenever possible to eliminate any confusion. Fonts are the exception: a relative path in either `Font` setting that isn't found there is also looked for in the folder containing the StreamFlex executable. Icons from the built-in [Icon Library](icons) are given by name rather than path, so they don't depend on the working directory at all.
- Color is specified in 24 bit RGB HEX format prefixed with the # character, e.g. the color red should be `#FF0000`. The letters can be uppercase or lowercase. HEX color pickers can be easily found online to assist color choices.
- Several settings allow for values to be specified in pixels *or* as a percentage of another value: [IconSpacing](#iconspacing), the titles' [Padding](#padding) and the clock's [Margin](#margin). In this case, if no percent sign is detected it will be interpreted as pixels, and if the percent sign is present, than it will be interpreted as a percent value e.g. "5" means 5 pixels and "5%" means 5 percent. The opacities, `VCenter` and `Intensity` take a percentage only, so a plain number there is invalid.
- A percentage in those settings, and in `IconSpacing` and `Margin`, is written `N%`, `N.N%` or `N.NN%`: it may have up to two decimals, such as `12.5%`. A percentage over 100% is invalid. The titles' `Padding` takes a whole percentage, at most 50%.
- A value a setting cannot take is ignored, and the log says so: `Invalid <key> value '<value>' in [<section>], ignoring it`. The setting keeps its default. Pixels and percentages are read strictly, so a value such as `IconSpacing=40px` is ignored rather than read as 40.
- Shell variable expansion is generally not supported, e.g. you cannot use the ~ character to refer to your home directory. The exception is for commands, since those are passed through to your system shell.

## The Settings Screen
The settings screen changes every setting on this page from the remote: each section under [Settings](#settings), each menu's grid, the [Clock](#clock), the [Screensaver](#screensaver), the [Hotkeys](#hotkeys) and the [Gamepad](#gamepad-controls). It shows each change in a preview as you make it, and saves your changes into your config file when you leave. The menus' entries are still written in the config file.

### Opening it
- Press the **Menu** key on the remote (the context-menu key on a keyboard), or **Start** on a gamepad when [gamepad controls](#gamepad-controls) are enabled, as they are by default. Either works unless your config gives that key or button something else to do. SDL reports the Menu key as one of two keycodes, `#40000065` (a keyboard's context-menu key) or `#40000076` (a remote's Menu button), and both open settings; a hotkey on either code takes that code over. The name the debug log gives `#40000065` depends on the SDL version: it may read `Key Application (#40000065)` or `Key Menu (#40000065)`, so go by the keycode.
- Or run the `:settings` [special command](#special-commands) from a menu entry, a hotkey or a gamepad control. The default config's System menu has a Settings tile.

Holding the key or button opens settings once. Settings don't open while an application is launching or running.

### Using it
The settings are in a column on the left; the rest of the screen is a live preview of your launcher.
- **Up and Down** move between rows.
- **Left and Right** change the highlighted value. On a colour, the default menu or the gamepad's device they step through its choices; a font or a command is chosen with OK.
- **OK** opens a row marked ›: a page, the [folder browser](#the-folder-browser), a [picker](#the-pickers) or a [binding](#key-and-button-bindings). On any other row it does what the row says, such as *Discard changes*.
- **Back** goes back a page. On the first page, it saves your changes and closes settings.
- **Menu** (or Start) closes settings from any page, saving your changes, except two: the *Couldn't save* page, where you choose *Try again* or *Leave without saving*, and the [restart question](#restart-now), where you choose *Yes* or *No*. A `:home` command does the same, then shows the menu set by `DefaultMenu`.

A row that depends on a switch that is off, such as *Shadow colour* while *Shadows* is off, is greyed. It cannot be changed, but the cursor can rest on it, and the note under the preview says why.

While settings are open, a hotkey or gamepad control works only when its command is one of the keys above (`:up`, `:down`, `:left`, `:right`, `:select`, `:back`, `:home` or `:settings`). Any other command, such as `:quit`, is ignored. The screensaver does not start while settings are open.

### What it changes
The first page has a row for each page below, in this order, and *Discard changes*. Each page's rows are listed here with the setting each one writes and the steps Left and Right take. A value from your config file that is not one of the steps, such as `Opacity=12.5%`, stays among the choices in its place, so you can step back to it.

#### The General page
- *Default menu* ([DefaultMenu](#defaultmenu)): your menus, in the order the config file has them.
- *Wrap around* ([WrapEntries](#wrapentries)), *Reset on Back* ([ResetOnBack](#resetonback)), *Mouse select* ([MouseSelect](#mouseselect)) and *Block the OS screensaver* ([InhibitOSScreensaver](#inhibitosscreensaver)): On or Off.
- *VSync* ([VSync](#vsync)): On or Off.
- *FPS limit* ([FPSLimit](#fpslimit)): Off, 30, 60, 75, 120, 144, 165 or 240. Greyed while VSync is on.
- *After launching an app* ([OnLaunch](#onlaunch)): *Blank screen*, *Keep showing* or *Quit*.
- *App timeout* ([ApplicationTimeout](#applicationtimeout)): 3, 5, 10, 15, 20 or 30 seconds.
- *Startup command* and *Quit command* ([StartupCmd](#startupcmd), [QuitCmd](#quitcmd)): chosen in the [command picker](#the-command-picker). *None* removes the line.

VSync and the FPS limit take effect at once, with no restart.

#### The Background page
- *Mode* ([Mode](#mode)): *Colour*, *Image*, *Slideshow* or *Transparent*. The rows under it follow the mode:
  - Colour: *Colour* ([Color](#color)). Left and Right step through ten presets, Black, Charcoal, Graphite, Slate, Midnight, Navy, Teal, Forest, Plum and Burgundy; OK opens the [colour picker](#the-colour-picker). A colour of your own is shown as *Custom #RRGGBB*.
  - Image: *Image* ([Image](#image)), chosen in the [folder browser](#the-folder-browser).
  - Slideshow: *Folder* ([SlideshowDirectory](#slideshowdirectory)), chosen in the folder browser; *Change every* ([SlideshowImageDuration](#slideshowimageduration)): 5, 10, 15 or 30 seconds, or 1, 2, 5, 10, 30 or 60 minutes; and *Fade* ([SlideshowTransitionTime](#slideshowtransitiontime)): 0 to 3 seconds, in steps of 0.5.
  - Transparent: a note that the desktop shows through, and *See-through colour* ([ChromaKeyColor](#chromakeycolor)).
- *Overlay* ([Overlay](#overlay)): On or Off.
- *Overlay colour* ([OverlayColor](#overlaycolor)) and *Overlay opacity* ([OverlayOpacity](#overlayopacity)): 0% to 100%, in steps of 5. Both are greyed until Overlay is on.

If you choose Image or Slideshow but leave the Background page without choosing an image or folder, the mode goes back to what it was when you opened the page.

#### The Menus page
The grid of every menu (*All menus*, which is the `[Layout]` section) and of each menu on its own:
- *Rows* ([Rows](#rows)): 1 to 10.
- *Columns* ([Columns](#columns)): 1 to 12.
- *Largest button* ([IconSize](#iconsize)): 64, 96, 128, 160, 192, 256, 320, 384, 512, 768 or 1024 px. On *All menus*, *Fill* lets buttons grow as large as the grid allows.
- On *All menus* only: *Icon spacing* ([IconSpacing](#iconspacing)): 0% to 10% of the screen width, in steps of 1; and *Vertical centre* ([VCenter](#vcenter)): 25% to 75%, in steps of 5.

On a menu's own page, the lowest step, *All menus*, makes that menu follow the shared grid again. A menu with no entries cannot be shown in the preview, but its grid can still be changed. The Menus page lists up to 62 menus; with more, it lists 61, and a note at its end counts the rest, whose grids you set in the config file.

#### The Titles page
- *Size* ([FontSize](#fontsize)): Small, Medium or Large. Titles scale with each menu's buttons. A fixed size from your config file stays among the choices, shown as *Fixed*.
- *Show titles* ([Enabled](#enabled)): On or Off.
- *Font* ([Font](#font)): chosen in the [font picker](#the-font-picker).
- *Colour* ([Color](#color-1)): the ten presets, or the colour picker. The note under the preview gives the [contrast warning](#the-contrast-warning).
- *Opacity* ([Opacity](#opacity)): 0% to 100%, in steps of 5.
- *Shadows* ([Shadows](#shadows)): On or Off.
- *Shadow colour* ([ShadowColor](#shadowcolor)): greyed until Shadows is on.
- *Too long* ([OversizeMode](#oversizemode)): *Truncate* or *Shrink*. A `None` from your config file stays among the choices, shown as *Leave as is*.
- *Padding* ([Padding](#padding)): 0% to 20%, in steps of 2.

Every row but *Show titles* is greyed while titles are off.

#### The Highlight page
- *Show* ([Enabled](#enabled-1)): On or Off.
- *Fill colour* ([FillColor](#fillcolor)), and *Fill opacity* ([FillOpacity](#fillopacity)): 0% to 100%, in steps of 5.
- *Outline size* ([OutlineSize](#outlinesize)): 0 to 10 px.
- *Outline colour* ([OutlineColor](#outlinecolor)) and *Outline opacity* ([OutlineOpacity](#outlineopacity)): 0% to 100%, in steps of 5. Both are greyed while the outline's size is 0.
- *Corner radius* ([CornerRadius](#cornerradius)): 0 to 100, in steps of 5. Greyed while there is an outline, since rounded corners cannot be drawn with one.
- *Vertical padding* ([VPadding](#vpadding)) and *Horizontal padding* ([HPadding](#hpadding)): 0 to 100 px, in steps of 5.

Every row but *Show* is greyed while the highlight is off.

#### The Scroll indicators page
- *Show* ([Enabled](#enabled-2)): On or Off.
- *Fill colour* ([FillColor](#fillcolor-1)).
- *Outline size* ([OutlineSize](#outlinesize-1)): 0 to 10 px.
- *Outline colour* ([OutlineColor](#outlinecolor-1)): greyed while the outline's size is 0.
- *Opacity* ([Opacity](#opacity-1)): 0% to 100%, in steps of 5.

Every row but *Show* is greyed while the scroll indicators are off.

#### The Clock page
- *Show* ([Enabled](#enabled-3)) and *Show date* ([ShowDate](#showdate)): On or Off.
- *Weekday* ([IncludeWeekday](#includeweekday)): On or Off. Greyed until Show date is on.
- *Alignment* ([Alignment](#alignment)): Left or Right.
- *Font* ([Font](#font-1)): chosen in the font picker.
- *Size* ([FontSize](#fontsize-1)): 20 to 120, in steps of 5.
- *Colour* ([FontColor](#fontcolor)): the ten presets, or the colour picker, with the contrast warning.
- *Opacity* ([Opacity](#opacity-2)): 0% to 100%, in steps of 5.
- *Shadows* ([Shadows](#shadows-1)), and *Shadow colour* ([ShadowColor](#shadowcolor-1)), which is greyed until Shadows is on.
- *Margin* ([Margin](#margin)): 0% to 10%, in steps of 1.
- *Time* ([TimeFormat](#timeformat)): *14:05* (24hr), *2:05 PM* (12hr) or *Auto*.
- *Date* ([DateFormat](#dateformat)): *Sep 28* (Big), *28 Sep* (Little) or *Auto*. Greyed until Show date is on.

Every row but *Show* is greyed while the clock is off.

#### The Screensaver page
- *On* ([Enabled](#enabled-4)): On or Off.
- *Idle time* ([IdleTime](#idletime)): 3, 5, 10, 15 or 30 seconds, or 1, 2, 5, 10 or 15 minutes.
- *Dim level* ([Intensity](#intensity)): 10% to 100%, in steps of 10. While this page is open, the preview is dimmed as the screensaver would dim the screen.
- *Pause slideshow* ([PauseSlideshow](#pauseslideshow)): On or Off.

Every row but *On* is greyed while the screensaver is off.

#### The Controls page
- *Keyboard*: the [hotkeys](#hotkeys). Its page lists them; see [Key and button bindings](#key-and-button-bindings).
- *Gamepad*: its page has:
  - *On* ([Enabled](#enabled-5)): On or Off;
  - *Device* ([DeviceIndex](#deviceindex)): *Any*, or one of the gamepads connected, by name. A device index from your config file with no gamepad on it stays among the choices, shown as *Pad N (not connected)*;
  - *Mappings file* ([ControllerMappingsFile](#controllermappingsfile)), chosen in the folder browser. It applies at next start: the note under the preview says *This applies at next start* when you choose one, the page says why, and settings offer to [restart StreamFlex](#restart-now) when you leave;
  - the gamepad's [controls](#key-and-button-bindings).

  *Device* and *Mappings file* are greyed while the gamepad is off.

#### The folder browser
Choosing an image or a slideshow folder opens a folder browser. It starts in the folder of the image or slideshow you have now, or else in your Pictures folder (on Linux, the one your desktop names in its `user-dirs.dirs` file, whatever its language, such as `~/Bilder`), and its list of places reaches your home folder and your drives (on Linux, `/` and the drives and shares mounted in `/media` and `/mnt`).
- **Up and Down** move; **Left and Right** move a page at a time.
- **OK** opens a folder, or chooses the highlighted image, or *Use this folder* for a slideshow.
- **Back** goes up a folder. Past the top it shows the places, and from the places it closes the browser without choosing.

The preview shows each image as you move over it, and the first image of a highlighted folder. The browser lists JPEG, PNG and WebP images by the same rule a slideshow uses (see [SlideshowDirectory](#slideshowdirectory)). A slideshow folder needs at least two images; the Background page shows the chosen folder's name and how many images it holds. An image that cannot be opened is shown but cannot be chosen, and the browser says it cannot be opened; the debug log (`-d`) gives the reason. A path too long for one line of the config file is shown but cannot be chosen, and the browser says why. Network drives and shares, and on Linux what is mounted in `/media` and `/mnt`, are listed without being opened, so a server that is off does not hold up the list of places; opening one that cannot be reached waits for the network to give up, then says so. The exception is an NFS share mounted `hard`: there the wait may never end, because the system keeps retrying until the server answers. The same wait applies when your current image or slideshow folder is on such a share, since the browser starts there.

For the gamepad's *Mappings file*, the browser lists every file, not only images, and previews none of them. It lists files only, so a pipe or a device never appears.

### The pickers
A row whose value is a colour, a font, a command, the default menu or the gamepad's device opens a picker with OK. The picker takes the place of the rows; the key hint at the bottom says which keys it takes. **Menu** and `:home` leave a picker without choosing, and close settings as they would from the page.

#### The colour picker
The colour picker shows 24 swatches, six across and four down:
- Black `#000000`, Charcoal `#1E1E1E`, Graphite `#33383D`, Slate `#2E3440`, Midnight `#121A2E`, Navy `#0B1F3A`, Teal `#07606C`, Forest `#1E3B2F`, Plum `#3B1F3A` and Burgundy `#4A1520`, the ten that Left and Right step through on a colour row;
- White `#FFFFFF`, Light grey `#C8C8C8`, Grey `#808080` and Dark grey `#4A4A4A`;
- Red `#D03030`, Orange `#E07020`, Amber `#F0B000`, Yellow `#F0E040`, Lime `#80C040`, Green `#30A050`, Cyan `#20B0C0`, Blue `#3070D0`, Indigo `#5048C0` and Pink `#D04890`.

Below them is a *Custom #RRGGBB* row, for any other colour. The swatch of the colour you have now is marked.
- **The arrows** move between the swatches, and down onto the Custom row. The preview shows the colour under the cursor as you move, and the note under the preview names it.
- **OK** on a swatch chooses it. On the Custom row, OK opens the hex editor, which starts at the colour you have now: **Left and Right** choose a digit, **Up and Down** change it (past F it goes back to 0), **OK** keeps the colour, and **Back** leaves the editor without choosing.
- **Back** closes the picker without choosing, and puts the colour back as it was when the picker opened.

#### The contrast warning
For the titles' and the clock's colour, the note under the preview warns when the colour stands out too little from what lies behind it: `Low contrast: 1.8:1 against the background; 3:1 or more reads well`. It warns below 3:1, the contrast ratio WCAG asks of large text. What lies behind is the background colour, or for an image or a slideshow, the image's mean luminance (its average brightness); when the overlay is on, it is laid over either. A transparent background gets no warning. The warning is advice only: the colour can still be chosen. It shows in the colour picker, for the colour under the cursor, and whenever the cursor rests on the colour's row.

#### The command picker
The startup and quit commands, and a binding's command, are chosen from a list:
- *None*;
- the special commands: Left, Right, Up, Down, OK, Back, Home, Settings, *Quit StreamFlex*, *Shut down*, *Restart* (the computer) and *Sleep*;
- *Open submenu:* each of your menus;
- on Windows only, *Close the app on show*, the [`:exit`](#exit) hotkey's command;
- every command your menus' entries run, each named by its entry's title, and each once.

A command from your config file that none of these runs is listed first, as *Custom: ...*, so choosing it keeps it. **Up and Down** move, **Left and Right** move a page at a time, **OK** chooses, and **Back** closes the list without choosing.

#### The font picker
The titles' and the clock's *Font* rows open a list of the fonts installed, one row for each family, each drawn in its own face: StreamFlex's bundled fonts first, then the others by name. It lists TrueType and OpenType fonts (`.ttf` and `.otf`) and font collections (`.ttc` and `.otc`), from:
- the folder of StreamFlex's bundled fonts;
- on Windows, the fonts installed for every user and for you (a font installed for one user is in your own folder, and listed too);
- on Linux, `/usr/share/fonts`, `/usr/local/share/fonts`, `~/.local/share/fonts` and `~/.fonts`, and the folders inside them.

A face that cannot draw the letters A, a and 0, such as a symbol font's, is left out. The first time the picker opens, it reads the font files while it shows *Loading fonts… (N)*, N being the files read so far; **Back** closes it meanwhile. The list is then kept until StreamFlex quits, so it opens at once from then on.

Choosing a family writes its Regular style's file to `Font`, or the first style found if it has no Regular. A face inside a font collection, other than its first, is also written as [`FontFace`](#fontface). A font in your config file that is not in the list is listed first, as *Custom:* and its file name. The keys are the command picker's.

#### The list pickers
*Default menu* lists your menus, in the order the config file has them. *Device* lists *Any*, then each gamepad connected, by name; it is made again when a gamepad is plugged in or pulled out. The keys are the command picker's, and Left and Right on these rows step through the same choices without opening the list.

### Key and button bindings
*Controls › Keyboard* lists the [hotkeys](#hotkeys), and *Controls › Gamepad* lists the gamepad's [controls](#controls) under its settings. Each list starts with *Add binding*, and then has a row for each binding: its key or button, and the command it runs. A binding's page has three rows:
- *Key*: OK captures the key or button (below);
- *Command*: OK opens the [command picker](#the-command-picker);
- *Remove*, or *Cancel* for a binding that is new.

A binding is set as soon as it has both a key and a command: you are then back on the list, and it takes effect at once. On Windows the exit hotkey is registered again, as the first `:exit` hotkey on a key Windows can register (F1 to F11, and F13 to F24).

If the hotkeys and controls cannot be read from the config file, the Keyboard page and the gamepad's controls are left out, and the log says why.

#### Capture
OK on *Key* asks you to press the key or button, and the note under the preview counts down the 5 seconds you have. The key that started the capture (OK, held down) does not count: its repeats and its release are ignored, so the next key pressed is the one captured. A gamepad's capture takes a button, a stick pushed one way, or a trigger. The page then shows *Captured:* and the key's name, with *Keep*, *Try again* and *Cancel*.

A capture that ends with nothing to keep leaves the binding as it was, with the reason in the note:
- nothing was pressed in 5 seconds;
- the key has no code StreamFlex can store. On Linux, a CEC remote's OK and Back arrive this way;
- the key is the Left or Right arrow, Enter (OK) or Backspace (Back), which keep their own meaning.

#### The safety floor
The Left and Right arrows, Enter (OK) and Backspace (Back) always keep their meaning. Beyond that, settings refuse a change, or a removal, that would leave Left, Right, Up, Down, OK, Back or Settings without a key on the Keyboard page, or without a button on the Gamepad page while the gamepad is on. The note says which: `That would leave no key for Up`. The Up and Down arrows and the Menu key count while no hotkey takes them over, and so do the gamepad's built-in Up, Down and Start (see [Gamepad Controls](#gamepad-controls)) while nothing else is bound to them. On Windows, the key the exit hotkey is registered on never reaches StreamFlex, so another hotkey on that key counts for nothing.

On Windows, `:exit` can only be bound to F1 to F24, and not F12. A hotkey line that has no name in the config file (`=#...`) cannot be removed, since removing it would change how the lines after it are read; its *Remove* row says so.

#### The 10 second confirmation
A hotkey on the Up or Down arrow or the Menu key that runs something other than that key's own command (`:up`, `:down` or `:settings`) takes the key's job away. Such a change takes effect, and the note asks you to press that key again: `Press Up again within 10 s to keep it`. Pressing it keeps the change. Otherwise, after 10 seconds, the change goes back. It goes back too if settings close first. Until the change is kept or goes back, no binding's page opens, and the note says why.

#### How bindings are written
A new hotkey is written as `HotkeyN=#<keycode>;<command>`, numbered one above the highest `HotkeyN` in the file, such as `Hotkey2=#4000003E;:quit`. A new gamepad control is written as the button's name and its command, such as `ButtonY=:home`. A changed binding rewrites its own line, keeping its name, and a removed one removes its line.

### Saving
- Only the settings you changed are written. Everything else in your config file stays as it was: comments, blank lines and order included. Setting a menu back to *All menus* removes its line.
- The file is read again when you save, so an edit made to it by hand while settings were open is kept.
- If nothing changed, nothing is written.
- The previous version is kept beside it as `config.ini.bak`. The new file is written beside the old one and then swapped in whole, so a failed save never leaves half a file. On Windows, a config file you have hidden stays hidden.
- A file that sets the columns with the older name, `MaxButtons`, keeps that name when they are saved. A file that has both `Columns` and `MaxButtons` in `[Layout]` is left with `Columns` alone.
- Hotkeys and gamepad controls are saved line by line: only the lines of the bindings you changed, added or removed are written. A binding's line that was changed by hand while settings were open keeps the hand edit, and your change is written beside it as a new line; where two lines bind one key or button, the first in the file is the one that runs. A line removed by hand stays removed, and the removal is skipped. The debug log (`-d`) says so in a `Settings: not saved as asked: ...` line for each.
- **Discard changes**, on the first page, puts everything back as it was when you opened settings, the bindings included.
- **On Linux**, the config installed with the package (in `/usr/share/streamflex`) cannot be changed. Your first save writes your own copy to `~/.config/streamflex/config.ini`, which StreamFlex reads from then on, and later saves change that copy. If that copy exists but cannot be read, the save fails and says why rather than replace it.
- If the file cannot be written, settings say why, and offer to try again or to leave without saving; Back returns to the settings instead. The config file stays as it was.
- Quitting StreamFlex while settings are open saves nothing.

### Restart now?
One setting applies only at the next start: the gamepad's *Mappings file* ([ControllerMappingsFile](#controllermappingsfile)). When a save writes it, settings ask, before they close, *Restart StreamFlex now to apply the mappings file?* They ask after every save that writes it, however you leave: with Back, with the Menu key, with `:home`, or with *Try again* after a save that failed.
- **Yes**, under the cursor when the question opens, restarts StreamFlex. On Linux the new start takes the place of the running program; on Windows a new copy starts once this one's window, and its exit hotkey, are gone.
- **No**, or **Back**, closes settings. The change waits for the next start.

A restart is not a quit: the [QuitCmd](#quitcmd) does not run, and the restarted copy does not run the [StartupCmd](#startupcmd) again. The restarted copy is started with `--restarted`, an option for StreamFlex's own use, and carries on the same log file instead of starting a new one; the debug log (`-d`) says `Restarting StreamFlex to apply the mappings file`. If the program cannot be found to start again, the log says why, and StreamFlex carries on running. This has nothing to do with the [`:restart`](#restart) command, which restarts the computer.

## Settings
The following sections contain settings that control the look and behavior of the launcher:
- [General](#general)
- [Background](#background)
- [Layout](#layout)
- [Titles](#titles)
- [Highlight](#highlight)
- [Scroll Indicators](#scroll-indicators)

#### General
The settings in this section control the general behavior of the launcher.

- [DefaultMenu](#defaultmenu)
- [VSync](#vsync)
- [FPSLimit](#fpslimit)
- [OnLaunch](#onlaunch)
- [ResetOnBack](#resetonback)
- [MouseSelect](#mouseselect)
- [InhibitOSScreensaver](#inhibitosscreensaver)
- [StartupCmd](#startupcmd)
- [QuitCmd](#quitcmd)

##### DefaultMenu
This is the title of the main menu that shows when StreamFlex is started. The value *must* match the name of one of your menu sections, or there will be an error and StreamFlex will refuse to start. See the [Creating Menus](#creating-menus) section for more information.

##### VSync
Defines whether VSync will be used to synchronize the frame rate with the refresh rate of your monitor. This setting is a boolean "true" or "false"

Default: true

##### FPSLimit
When `VSync` is set to false, this setting defines the maximum number of frames per second that StreamFlex will render. The minimum is 10, and the maximum is the same as the refresh rate of your monitor. The file may hold a whole number from 10 to 1000; any other value is ignored, and the log says so. A limit above your monitor's refresh rate leaves VSync on, as no `FPSLimit` at all does (*Off* on the settings screen).

Both `VSync` and `FPSLimit` take effect at once when the settings screen changes them. If the renderer refuses to turn VSync on, or off, StreamFlex paces each frame itself and writes an error line to the log: `The renderer refused VSync: each frame is paced to <N> ms instead`.

Default: none (Off)

##### ApplicationTimeout
Defines the time in seconds that the launcher will wait for an application to launch, from 3 to 30. If the launcher does not lose the window focus before the timeout occurs, it assumes there was an error with the launched application.

Default: 15

##### OnLaunch
Defines the action that StreamFlex will take upon the launch of an application. Possible values: "None", "Blank", and "Quit"
- None: StreamFlex will maintain its window while waiting for the launched application to initialize.
- Blank: StreamFlex will change to a blank, black screen while waiting for the launched application to initialize.
- Quit: StreamFlex will quit immediately after the successful launch of an application.

Default: Blank

##### WrapEntries
Defines whether the highlight will wrap to the other side of the screen after reaching its leftmost or rightmost position. This setting is a boolean "true" or "false".

Default: false

##### ResetOnBack
Defines whether StreamFlex will remember the previous entry position when going back to a previous menu. If set to true, the highlight will be reset to the first entry in the menu when going back. This setting is a boolean "true" or "false".

Default: false

##### MouseSelect
Defines whether the left mouse button can be used to select the highlighted entry. This setting is intended to support gyroscopic mouse devices where the enter/ok button functions as a mouse left click instead of the keyboard enter button. This setting is a boolean "true" or "false".

Default: false

##### InhibitOSScreensaver
Defines whether StreamFlex will prevent your default OS screensaver from activating while it is running. On Windows, this will also inhibit any power saving features as well (e.g. autosleep). This setting is a boolean "true" or "false".

Default: true

##### StartupCmd
Defines a command that StreamFlex will execute immediately upon startup. This can be used to autostart your favorite application.

##### QuitCmd
Defines a command that StreamFlex will execute immediately before quitting. This can be used to do any mode switching or appplication starting to prepare your desktop, e.g. for maintenance.

#### Background
The settings in this section control what StreamFlex will display in the background.

- [Mode](#mode)
- [Color](#color)
- [Image](#image)
- [SlideshowDirectory](#slideshowdirectory)
- [SlideshowImageDuration](#slideshowimageduration)
- [SlideshowTransitionTime](#slideshowtransitiontime)
- [ChromaKeyColor](#chromakeycolor)
- [Overlay](#overlay)
- [OverlayColor](#overlaycolor)
- [OverlayOpacity](#overlayopacity)

##### Mode
Defines what mode the background will be. Possible values: "Color", "Image", and "Slideshow"
- Color: The background will be a solid color.
- Image: The background will be an image.
- Slideshow: The background will be a series of images displayed in random order, with a fading transition between each image.
- Transparent: The background will be transparent. This is an advanced feature; users should read the [Transparent Backgrounds](#transparent-backgrounds) section before proceeding.

Default: Color

##### Color
When `Mode` is set to "Color", this setting defines the color of the background.

Default: #000000 (Black)

##### Image
When `Mode` is set to "Image", this setting defines the image to be displayed in the background. The value should be a path to an image file. If the image is not the same resolution as your desktop, it will be stretched accordingly.

##### SlideshowDirectory
When `Mode` is set to "Slideshow", this setting defines the directory (folder) which contains the images to display in the background. The value should be a path to a directory on your filesystem. The slideshow shows the files whose names end in `.jpg`, `.jpeg`, `.png` or `.webp`, in any case (`DSC_0001.JPG` counts), and leaves out hidden files: on Linux a name starting with a dot, and on Windows a file with the hidden or system attribute. Folders inside it are not searched.

##### SlideshowImageDuration
When `Mode` is set to "Slideshow", this setting defines the amount of time in seconds to display each image. Must be an integer value, from 5 to 3600.

Default: 30

##### SlideshowTransitionTime
When `Mode` is set to "Slideshow", this setting defines the amount of time in seconds that the next background image will fade in, at most 3. The fading transition may be disabled by setting this to 0, which will yield a "hard" transition between images. Decimal values are acceptable.

Default: 1.5 (the sample config's commented-out line shows 3)

##### ChromaKeyColor
When `Mode` is set to "Transparent", this setting defines the color that will be applied to the background for chroma key transparency.

Default: #010101

##### Overlay
Defines whether the background overlay feature is enabled. The background overlay is a solid color, typically black, that is painted over your background to improve the contrast between the background and the text/icons. This setting is a boolean "true" or "false".

Default: false

##### OverlayColor
Defines the color of the background overlay.

Default: #000000 (Black)

##### OverlayOpacity
Defines the opacity of the background overlay. Must be a percent value, which may have up to two decimals (`12.5%`).

Default: 50%

#### Layout
The settings in this section define the geometric layout of the launcher. A menu shows its buttons in a grid of `Rows` × `Columns`, and the buttons are sized to fill it. Any menu can override these settings; see [Menu Layouts](#menu-layouts).

- [Rows](#rows)
- [Columns](#columns)
- [IconSize](#iconsize)
- [IconSpacing](#iconspacing)
- [VCenter](#vcenter)

##### Rows
The number of rows of buttons shown at once. With 1 row the menu is a strip, which slides one button at a time when you move past its edge. With 2 or more the menu is a grid: Left and Right stop at the end of a row, Up and Down move between rows, and the grid scrolls one row at a time. See [Moving Around](#moving-around).

Default: 1

##### Columns
The number of buttons in each row. The buttons are sized so that `Columns` of them fit across the screen and `Rows` of them fit down it, titles included. For a single row, 3-5 is sensible for a typical TV at a typical viewing distance.

`MaxButtons`, the older name for this setting, is still accepted. If both are set, `Columns` is used.

Default: 4

##### IconSize
The largest size of a button, in pixels: a whole number from 32 to 1024. Buttons are sized to fill the grid but never grow past this. Leave it out to let them grow as large as the grid allows. Any other value, such as `200px`, is ignored and noted in the log. An icon image that is not square is stretched to fit. SVG icons are drawn at the button's size, so they stay sharp at any size.

Default: none (the sample config sets 256)

##### IconSpacing
The gap between buttons, across and down, in pixels or percent of the screen width. A percentage may have up to two decimals (`12.5%`). The gap is kept as set, up to the width of the screen: if it is too large for the grid to fit at `IconSize`, the buttons shrink instead.

Default: 5%

##### VCenter
The vertical centre of the buttons, in percent of the screen height, which may have up to two decimals (`52.5%`). A value of 50% centres them halfway down the screen; a higher value lowers them and a lower value raises them. The centre is kept between 25% and 75% of the height: a value outside that is drawn at the nearer end. The rows that have buttons are centred on this line, but a tall grid is kept on the screen and below the clock.

Default: 50%

#### Titles
The settings in this section affect the application titles that display below the icons.

- [Enabled](#enabled)
- [Font](#font)
- [FontFace](#fontface)
- [FontSize](#fontsize)
- [Color](#color-1)
- [Shadows](#shadows)
- [ShadowColor](#shadowcolor)
- [Opacity](#opacity)
- [OversizeMode](#oversizemode)
- [Padding](#padding)

##### Enabled
Defines whether or not application titles are enabled. This setting is a boolean "true" or "false".

Default: true

##### Font
Defines the font to use for the titles of the menu entries. The value should be the path to a TrueType or OpenType font file (`.ttf` or `.otf`), or a font collection (`.ttc`). StreamFlex ships with a handful of libre fonts. The settings screen's [font picker](#the-font-picker) lists the fonts installed.

`FontFace` picks a face inside a font collection (`.ttc`), counted from 0. Absent means 0. A config moved to another machine whose font is not there falls back to the bundled font, as its image paths do.

Default: OpenSans

##### FontFace
The face to use inside the titles' `Font`, when that is a font collection: a whole number, counted from 0. The font picker writes it when you choose a face that is not a collection's first, and removes it when you choose one that is. A face the file does not have falls back to the bundled font, and the log says so.

Default: 0

##### FontSize
Defines the size of the menu entry titles, in one of two ways:
- **A percentage** of the button size, such as `14%`. Each menu's titles follow its buttons, so a dense grid gets smaller titles and a row of large buttons gets larger ones. Titles never get smaller than 2% of the screen height, so they stay readable from the couch. The settings screen's Small, Medium and Large are `11%`, `14%` and `17%`.
- **A fixed size**, such as `36`: the same size in every menu, as in earlier versions.

Default: 14%

##### Color
Defines the color of the menu entry titles.

Default: #FFFFFF (White)

##### Shadows
Defines whether shadows are enabled for the menu titles. Shadows give a 3D textured appearance to the text to improve the contrast from the background. This setting is a boolean "true" or "false".

Default: false

##### ShadowColor
Defines the color of the title shadows.

Default: #000000 (Black)

##### Opacity
Defines the opacity of the menu entry titles. Must be a percent value, which may have up to two decimals (`12.5%`).

Default: 100%

##### OversizeMode
Defines the behavior when the width of a menu entry title exceeds the width of its button, which is set by the menu's grid (see [Layout](#layout)). Possible values: "Truncate", "Shrink", and "None"
- Truncate: Truncates the title at the maximum width and adds "..." to the end. ("Truncated" is accepted too.)
- Shrink: Shrinks an oversized title to a smaller size than `FontSize` so that it fits, but never below 2% of the screen height. A title that still does not fit is truncated.
- None: No action is taken to limit the width of titles. Overlaps with other titles may occur, and it is the user's responsibility to manually handle any such case.

Default: Truncate

##### Padding
Defines the vertical spacing between an icon and its title: a whole percentage of the button size up to 50%, such as `8%`, or a number of pixels. A number of pixels is capped at half the button's size.

Default: 8%

#### Highlight
The settings in this section control the menu highlight.

- [Enabled](#enabled-1)
- [FillColor](#fillcolor)
- [FillOpacity](#fillopacity)
- [OutlineSize](#outlinesize)
- [OutlineColor](#outlinecolor)
- [OutlineOpacity](#outlineopacity)
- [CornerRadius](#cornerradius)
- [VPadding](#vpadding)
- [HPadding](#hpadding)

##### Enabled
Defines whether or not the highlight is enabled. If a user disables the highlight, it is assumed that they will be using the [Sected Icon Overrides](#selected-icon-overrides) feature instead. This setting is a boolean "true" or "false".

Default: true

##### FillColor
Defines the fill color of the highlight cursor.

Default: #FFFFFF (White)

##### FillOpacity
Defines the fill opacity of the highlight cursor. Must be a percent value, which may have up to two decimals (`12.5%`).

Default: 25%

##### OutlineSize
Defines the stroke width in pixels of the outline of the highlight cursor. Setting this to 0 will disable the outline. The outline is drawn at most as wide as the smaller of `VPadding` and `HPadding`, so it stays inside the highlight.

Default: 0

##### OutlineColor
Defines the outline color of the highlight cursor.

Default: #0000FF (Blue)

##### OutlineOpacity
Defines the outline opacity of the highlight cursor. Must be a percent value, which may have up to two decimals (`12.5%`).

Default: 100%

##### CornerRadius
Defines the corner radius of the highlight cursor, in pixels. A value of 0 will yield a plain rectangle. Increasing the value will yield a rounded rectangle with increasingly round corners. The value of `HighlightOutlineSize` must be 0, otherwise this setting will be ignored.

Default: 0

##### VPadding
Defines the amount of vertical distance that the highlight cursor extends beyond the top and bottom of the menu entry icon, in pixels.

Default: 30

##### HPadding
Defines the amount of horizontal distance that the highlight cursor extends beyond the left and right of the menu entry icon, in pixels. It is drawn at most half as wide as the gap between buttons ([IconSpacing](#iconspacing)), so the highlight never reaches the next button.

Default: 30

#### Scroll Indicators
The settings in this section pertain to scroll indicators. Scroll indicators are arrows that show when a menu has more buttons than fit on the screen. A one-row menu shows them in the bottom left and/or bottom right corners. A grid shows them centred at the top and/or bottom of the screen, pointing up or down.

- [Enabled](#enabled-2)
- [FillColor](#fillcolor-1)
- [OutlineSize](#outlinesize-1)
- [OutlineColor](#outlinecolor-1)
- [Opacity](#opacity-1)

##### Enabled
Defines whether scroll indicators will be enabled when a menu has more buttons than fit on the screen. This setting is a boolean "true" or "false".

Default: true

##### OutlineSize
Defines the stroke width in pixels of the scroll indicator outline. Setting this to 0 will disable the outline. It is drawn at most 1% of the screen height wide.

Default: 0

##### FillColor
Defines the fill color of the scroll indicators.

Default: #FFFFFF (White)

##### OutlineColor
Defines the color of the scroll indicator outline.

Default: #000000 (Black)

##### Opacity
Defines the opacity of the scroll indicators. Must be a percent value, which may have up to two decimals (`12.5%`).

Default: 100%

## Creating Menus
At least one menu must be defined in the configuration file, and the title must match the `DefaultMenu` setting value. The title of a menu is its section name. Any title may be used that is not reserved for another section, such as "Settings", "Gamepad", etc. The entries of the menu are implemented as key=value pairs. The name of the key will be ignored by the program, and is therefore arbtrary. However, it is recommended to pick something intutitive such as Entry1, Entry2, Entry3, etc. The entry information is contained in the value.

Each entry value contains 3 parts of information in order: the title, the icon, and the command to run when the button is clicked. These are delimited by semicolons:
```ini
Entry=title;icon;command
```
An entry missing any of the three is ignored, and the log says why.

The icon is either the name of an icon from StreamFlex's built-in [Icon Library](icons), such as `netflix` or `movies`, or the path to an image file of your own (PNG, JPEG, WebP or SVG). A name is lowercase letters, digits and hyphens only; anything else is read as a path. To use a file of your own whose name looks like an icon name, write it as a path, for example `./kodi`.

The command is typically one of the following:
1. The path to the program executable that you want to launch 
2. Windows: the path to a program shortcut (.lnk file)
3. Linux: the path to a [.desktop file](#desktop-files-linux-only)
4. A [special command](#special-commands)
5. The path to an executable script, in the case that you want to perform multiple actions upon program launch.

 A simple example menu titled `Media` is shown below:
```ini
[Media]
Entry1=Kodi;kodi;"C:\Program Shortcuts\kodi.lnk"
Entry2=Netflix;netflix;"C:\Program Shortcuts\netflix.lnk"
Entry3=Plex;plex;"C:\Program Shortcuts\plex.lnk"
Entry4=Home Videos;C:\Pictures\Icons\camera.png;"C:\Program Shortcuts\videos.lnk"
Entry5=Back;back;:back
```

### Menu Layouts
A menu can set its own `Rows`, `Columns` and `IconSize`, which override the [Layout](#layout) settings for that menu only. For example, big buttons on the main menu and a denser grid for games:
```ini
[Main]
Entry1=Games;games;:submenu Games

[Games]
Rows=3
Columns=6
Entry1=...
```
These three names count as layout settings only when their value is a number. A key with one of these names whose value is an entry (`title;icon;command`) is still read as an entry, and the log notes it.

### Moving Around
- **A one-row menu** is a strip. Left and Right move along it, and it slides one button at a time at its edges. With `WrapEntries`, moving past the last button selects the first, and the other way round.
- **A menu with two or more rows** is a grid.
  - Left and Right move within a row and stop at its ends. With `WrapEntries`, they wrap within the row.
  - Up and Down move between rows, keeping the column. Moving down into a shorter last row lands on its last button.
  - The grid scrolls one row at a time. With `WrapEntries`, moving down from the last row goes to the first, and up from the first goes to the last.
- Each menu remembers its selected button and scroll position when you come back to it, unless `ResetOnBack` is set.

### Selected Icon Overrides
The Selected Icon Override feature allows the user to define a different icon for the launcher to display when an entry is highlighted. To use this feature, name the path of the selected icon the same as the default entry icon path, but with a suffix of `_selected` (not including the file extension).

For example, if the icon path for an entry is defined as `C:\icons\kodi.png`, then the program will check for the existence of `C:\icons\kodi_selected.png` and, if it exists, this icon will be shown when the entry is selected instead of the default. This feature allows the user to implement custom highlight effects such as glowing, color changes, etc. Icons from the library have no selected versions; the highlight shows which one is selected.

### Special Commands
Special commands are commands that are internal to StreamFlex and begin with a colon. The following is a list of special commands:

#### :submenu
Change to a different menu. Requires a menu title as an argument. For example, the command `:submenu Games` will change to the menu `Games`. The argument must be a valid menu title that is defined elsewhere in the config file.

#### :fork
Forks a new process and executes a command in it without exiting the launcher. This is typically used in combination with a [hotkey](#hotkeys). Use this special command when you want to execute a command on your system for some reason other than launching a graphical application. Example use cases:
- Change a Wi-Fi connection
- Pair or connect a Bluetooth device
- Start or stop some system service/daemon

The :fork special command requires a command as an argument. For example `:fork command arguments` will execute `command arguments` without leaving the launcher.

Windows users should invoke a command line interpreter such as Command Prompt and pass the command to run as an argument, e.g. `:fork cmd.exe /c "command arguments"`

#### :exit
Quits the currently running application. Windows only, as a hotkey. Anywhere else (a Linux hotkey, or a menu entry on either platform) StreamFlex logs that it does nothing. See the [Exit Hotkey](#exit-hotkey-windows-only) section for more information.

#### :back
Go back to the previous menu.

#### :home
Change to the menu defined in the `DefaultMenu` setting.

#### :quit
Quit StreamFlex.

#### :left
Move the highlight cursor left.

#### :right
Move the highlight cursor right.

#### :up
Move the highlight cursor up one row. Only a menu with two or more [Rows](#rows) has rows to move between.

#### :down
Move the highlight cursor down one row.

#### :select
Press enter on the current selection. This special command is only available as a gamepad or hotkey command, it is forbidden for menu entries: a menu entry that uses it is ignored, and the log says why.

#### :shutdown
Shut down the computer.<sup>1</sup>

#### :restart
Restart the computer.<sup>1</sup>

#### :sleep
Put the computer to sleep.<sup>1</sup>

<sup>1</sup> *Linux: Works in systemd-based distros only. Non-systemd distro users need to implement the command manually for their init system.*

#### :settings
Opens the [settings screen](#the-settings-screen). Running it again while settings are open saves your changes and closes them (on the *Couldn't save* page it does nothing).

### Desktop Files (Linux Only)
If the application you want to launch was installed via your distro's package manager, a .desktop file was most likely provided. The command to launch a Linux application can simply be the path to its .desktop file, and StreamFlex will run the Exec command that the developers have specified in the file. Desktop files are located in /usr/share/applications.

#### Desktop Actions
Some .desktop files contain "Actions", which affect how the program is launched. An action may be specified by delimiting it from the path to the .desktop file with a semicolon. For example, Steam has a mode called "Big Picture Mode", which provides an interface similar to a game console and is ideal for a living room PC. The action in the .desktop file is called "BigPicture". A sample menu entry to launch Steam in Big Picture mode is shown below:
```
Entry1=Steam;/path/to/steamicon.png;/usr/share/applications/steam.desktop;BigPicture
```

## Clock
StreamFlex contains a clock widget, which displays the current time, and, optionally, the current date. The following settings may be used to control the behavior of the clock.

#### Enabled
Defines whether or not the clock is enabled. This setting is a boolean "true" or "false".

Default: false

#### ShowDate
Defines whether or not the current date should be shown in addition to the current time. This setting is a boolean "true" or "false".

Default: false

#### Alignment
Defines which side of the screen the clock text should align to. Possible values: "Left" and "Right"

Default: Left

#### Font
Defines the font to use for the clock text. The value should be the path to a TrueType or OpenType font file (`.ttf` or `.otf`), or a font collection (`.ttc`). The settings screen's [font picker](#the-font-picker) lists the fonts installed.

`FontFace` picks a face inside a font collection (`.ttc`), counted from 0. Absent means 0. A config moved to another machine whose font is not there falls back to the bundled font, as its image paths do.

Default: SourceSansPro

#### FontFace
The face to use inside the clock's `Font`, when that is a font collection: a whole number, counted from 0. The font picker writes it when you choose a face that is not a collection's first, and removes it when you choose one that is. A face the file does not have falls back to the bundled font, and the log says so.

Default: 0

#### FontSize
Defines the font size of the clock text: a whole number, at least 1. Any other value, such as a negative one, is ignored, and the log says so. The settings screen steps it from 20 to 120, in steps of 5.

Default: 50

#### Margin
Defines the distance of the clock text from the top and side of the screen, in pixels or percent of the screen height. A percentage may have up to two decimals (`12.5%`). The margin is at most 10% of the screen height.

Default: 5%

#### FontColor
Defines the color of the clock text.

Default: #FFFFFF (White)

#### Shadows
Defines whether shadows are enabled for the clock text. Shadows give a 3D textured appearance to the text to improve the contrast from the background. This setting is a boolean "true" or "false".

Default: false

#### ShadowColor
Defines the color of the clock text shadows.

Default: #000000 (Black)

#### Opacity
Defines the opacity of the clock text. Must be a percent value, which may have up to two decimals (`12.5%`).

Default: 100%

#### TimeFormat
Defines the format of the current time. Possible values: "24hr", "12hr", and "Auto"
- 24hr: The clock will be a 24 hour format.
- 12hr: The clock will be a 12 hour format, including AM/PM designation in your locale.
- Auto: Automatically determine the time format based on your system locale.

Default: Auto

#### DateFormat
Defines the order of the month and day in the date. Possible values: "Little", "Big", "Auto"
- Little: The day will come before the month.
- Big: The month will come before the day.
- Auto: Automatically determine the date format based on your system locale.

#### IncludeWeekday
Defines whether the date format should include the abbreviated weekday in your system locale. This setting is a boolean "true" or "false"

Default: true

## Screensaver
StreamFlex contains a screensaver feature, which will dim the screen after the input has been idle for the specified amount of time. Here are the settings that control the behavior of the screensaver

#### Enabled
Defines whether or not the screensaver is enabled. This setting is a boolean "true" or "false".

Default: false

#### IdleTime
Defines the amount of time in seconds that the input should be idle before activating the screensaver, from 3 to 900 (15 minutes).

Default: 300 (5 minutes)

#### Intensity
Defines the amount to dim the screen. Must be a percent value, which may have up to two decimals (`12.5%`). An intensity too low to dim the screen at all (below 0.4%) is logged, and the screensaver is not started.

Default: 70%

#### PauseSlideshow
When the background's `Mode` is set to "Slideshow", this setting defines whether or not the slideshow should be paused while the screensaver is active. This setting is a boolean "true" or "false".

Default: true

## Hotkeys
StreamFlex supports configurable hotkeys, which executes a command when a specified key is pressed. Each hotkey consists of a key=value pair, where the key is an arbitrary name, and the value contains the SDL keycode of the hotkey and the command to run when it is pressed, delimited by a semicolon:
```ini
Hotkey=keycode;command
```
The settings screen's *Keyboard* page adds, changes and removes hotkeys from the remote, capturing the key for you; see [Key and button bindings](#key-and-button-bindings).

The keycode is a HEX prefixed with the # character. There are two ways to find a keycode for a given key. The first is to use the [lookup table provided by SDL](https://wiki.libsdl.org/SDLKeycodeLookup). The name of each key is in the right column of the table, and the corresponding HEX keycode is in the center column. The second is to run StreamFlex in debug mode, press the key, then check the log. For each keystroke, the name of the key will be printed and the HEX value will be in parenthesis next to it.

Any key can be set as a hotkey, except keys that are reserved for the default controls: the left and right arrow keys, enter/return, and backspace. The up and down arrow keys move between rows of a grid, unless a hotkey is bound to them, in which case the hotkey is used. The Menu key opens the [settings screen](#the-settings-screen) the same way in either of its two keycodes, `#40000065` (a keyboard's context-menu key) and `#40000076` (a remote's Menu button), unless a hotkey is bound to that code; a hotkey on one code leaves the other still opening settings. The debug log's name for `#40000065` varies with the SDL version (`Application` or `Menu`); the keycode is what identifies it. Hotkeys may be used to "speed dial" your favorite applications, or to add controls via [special commands](#special-commands). As an example configuration below, the first hotkey is mapped to F1 and will launch Kodi when it is pressed, and the second hotkey is mapped to F12 and will cause StreamFlex to quit when it is pressed:
```ini
[Hotkeys]
Hotkey1=#4000003A;"C:\Program Shortcuts\kodi.lnk"
Hotkey2=#40000045;:quit
```

### Exit Hotkey (Windows only)
The exit hotkey feature allows a user to quit the running application using a button on their remote. This is especially useful for applications that don't have a quit button, such as a web browser operating in fullscreen mode.

Only the function keys F1-F24 may be used as an exit hotkey, with the exception of F12 which is forbidden by Windows. Pressing an exit hotkey is functionally equivalent to using the Alt+F4 keyboard shortcut on the active window; it is not a forceful method, so the application is able to close cleanly. However, the application could also choose to ignore it, display a confirmation dialog, or not respond if it's hung.

The following example maps F10 as an exit hotkey:
```ini
Hotkey=#40000043;:exit
```
Linux users that desire similar functionality should check the documentation of their desktop environment and/or window manager. Most support global hotkeys that can be configured to close the active window.

## Gamepad Controls
StreamFlex has built-in support for gamepad controls through SDL. All settings for gamepads will be in a section titled `Gamepad`. Within the section, there are key=value pairs which define the gamepad settings and the commands to be run when a button or axis is pressed.

### Settings
The following settings are available in the `Gamepad` section to define the behavior of gamepads

#### Enabled
Defines whether or not gamepad controls are enabled. This setting is a boolean "true" or "false". Set it to false to turn gamepad controls off.

Default: true

#### DeviceIndex
Defines the device index of the gamepad in SDL. If this value is negative, any gamepad may be used to control the launcher.

Default: -1

#### ControllerMappingsFile
A path to a text file that contains 1 or more controller mappings to override the default. This is usually not necessary, but if you want to change the mapping for your controller, or there is no default mapping for your controller in SDL, it can be specified via this interface. A community database of mappings for many common controllers can be found [here](https://github.com/gabomdq/SDL_GameControllerDB). Alternatively, you may create a custom mapping using a GUI tool such as the [SDL2 Gamepad Tool](https://generalarcade.com/gamepadtool/).

Applies at next start: SDL can add mappings while running, but not take one back. When the settings screen changes it, it offers to [restart StreamFlex](#restart-now) as it closes.

### Controls
The controls are defined in key=value pairs, where the key is the name of the axis or button that is pressed, and the value is the command that is to be run, which is typically a [special command](#special-commands). An axis is an analog stick or a trigger. For analog sticks, negative (-) represents left for the x axis and up for the y axis, and postive (+) represents right for the x axis and down for the y axis. 

The [SDL GameController](https://wiki.libsdl.org/CategoryGameController) interface is an abstraction which conceptualizes a controller as having an Xbox-style layout. The mapping names in SDL are based on the *location* of the buttons on an Xbox controller, and may not correspond to the actual labelling of the buttons on your controller. For example, `ButtonA` is for the "bottom" button, `ButtonB` is for the "right" button of the 4 main control buttons. If you have a Playstation-style controller, those mapping names will correspond to the X button and the Circle button, respectively. 

The default controls in StreamFlex allow the user to move the highlight cursor with the left stick or the DPad, select an entry by pressing A, and go back to the previous menu by pressing B. These controls are simple and will suffice for the vast majority of use cases. The settings screen's *Gamepad* page adds, changes and removes controls, capturing the button from the gamepad; see [Key and button bindings](#key-and-button-bindings).

Up and down have defaults of their own. If your config maps nothing to `:up` or `:down`, the DPad's up and down buttons and the left stick's vertical axis run them, unless your config already uses those controls for something else. A config written before grids existed can still move between rows.

Start has a default too: if your config maps nothing to `:settings`, Start opens the [settings screen](#the-settings-screen), unless your config already uses Start for something else.

The following axis and buttons are available for control in StreamFlex:
- LStickX-
- LStickX+
- LStickY-
- LStickY+
- RStickX-
- RStickX+
- RStickY-
- RStickY+
- LTrigger
- RTrigger
- ButtonA
- ButtonB
- ButtonX
- ButtonY
- ButtonBack
- ButtonGuide
- ButtonStart
- ButtonLeftStick
- ButtonRightStick
- ButtonLeftShoulder
- ButtonRightShoulder
- ButtonDPadUp
- ButtonDPadDown
- ButtonDPadLeft
- ButtonDPadRight

## Transparent Backgrounds
*Note for Linux users only: this feature requires compositor implementation. See the [Linux Setup Guide](https://bilbospocketses.github.io/streamflex/setup_linux#transparent-backgrounds) for details. To see whether SDL chose X11 or Wayland, and which renderer, run StreamFlex with debug logging (`-d`): the log's `Video:` line at startup names both.*

StreamFlex supports transparent backgrounds using the chroma key technique. This method works by setting a strategically chosen color to the background, which is removed later. In film production, this technique is often refered to as "blue screening" or "green screening".

Every pixel of the window that is exactly the chroma key color becomes transparent. StreamFlex keeps icons clear of it: as an icon loads, any opaque pixel within one step of the key in every channel is moved two steps away from it (with the default key, near-black becomes `#030303`). Text and the colors you configure are not changed, so choose a key color that your fonts, highlight and other colors do not use. The color is set with the `ChromaKeyColor` setting in the Background section. The default is `#010101`, a slight off-shade of black.

A limitation of this method is that your icons and text must be fully opqaue or fully transparent. Any semi-transparent pixels will blend with the chroma key background so that it does not produce a color match, and consequently will not be removed from the background.

StreamFlex's text rendering is anti-aliased, which gives the text a "feathered" look with semi-transparent pixels on the edges. Normally, this is desirable, but in the case of chroma keying, semi-transparent pixels will cause your chroma key background color to "bleed through" on the edges of the text. If bright blue or bright green is chosen as the chroma key, this will result in a blue or green glowing effect around the text, which is usually undesirable. This is the reason why a dark color is chosen as the default chroma key. The bleed though appears as a dark outline rather than as a bright glowing.

Some icons have shadows which are intended to provide a textured look. The shadows are usually semi-transparent, which will cause them to not render properly with the chroma key technique. You should choose icons without shadows, or manually erase the shadows from an icon if there are no other icons available for the given application. Some icons are also heavily anti-aliased, which can give a glowing or outline effect similar to the text rendering described above. Icons from the [Icon Library](icons) have smooth, partly transparent edges, so a faint outline in the chroma key color can show around them.

Another common issue is the highlight. The default highlight is semi-transparent, which will not render properly when blended with the chroma key background. There are a few ways to address with this:
- Set the `FillOpacity` setting to 0%, and use an outline-only highlight instead
- Use custom [Selected Icons](#selected-icon-overrides) in place of StreamFlex's highlight
- Linux only: use a shader to recover the highlight's transparency. See the [Linux Setup Guide](https://bilbospocketses.github.io/streamflex/setup_linux#transparent-backgrounds) for details.

Transparent backgrounds will require some effort to obtain a setup that looks good and works well. Be prepared to do a significant amount of tinkering if you wish to use this feature.

### Windows Implenetation 
The Windows implementation of transparency is not hardware accelerated. If your refresh rate is very high, this can result in a signficant load on the CPU. If you find that the trasparent background is causing a high load on your system, consider changing the `VSync` setting to false and setting `FPSLimit` to 30 or lower, which will reduce the amount of computation required.

### Animated Backgrounds
Transparent backgrounds can be used to implement animated backgrounds in combination with another program. On Windows, [Wallpaper Engine](https://www.wallpaperengine.io) and [Lively](https://github.com/rocksdanister/lively) are popular choices. For Linux, I recommend [anipaper](https://github.com/Theldus/anipaper); see the [Linux Setup Guide](https://bilbospocketses.github.io/streamflex/setup_linux#animated-backgrounds) for details.

### Custom Widgets
StreamFlex offers a simple clock widget which can show the current time and date. For more advanced functionality, you can combine a transparent background with a third party widget program. For example, you can have a widget that displays weather, news, etc. in addition to the time. [Rainmeter](https://www.rainmeter.net/) is a popular option on Windows, and [Conky](https://github.com/brndnmtthws/conky) for Linux.
