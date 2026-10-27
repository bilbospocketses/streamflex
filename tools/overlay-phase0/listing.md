# CPTK Overlay: store listing text

One text for all three stores, so the Chrome Web Store, Edge Add-ons and AMO say the same thing. Every answer here
matches CPTK Overlay 0.0.1 as `extension/build.py` builds it. If the manifest or the scripts change, change this
file in the same commit. The step-by-step upload checklist is
`design/research/overlay/phase0/store-steps.md`.

## The package

| Field | Value |
|---|---|
| Name | CPTK Overlay |
| Version | 0.0.1 (Manifest V3) |
| Packages | `extension/dist/cptk-overlay-chromium-0.0.1.zip` (Chrome Web Store, Edge Add-ons), `extension/dist/firefox/` (signed on AMO) |
| `permissions` | `nativeMessaging`, `storage` |
| `host_permissions` | `<all_urls>` |
| Content script | `content.js` on `<all_urls>`, `run_at: document_start` |
| Background | `background.js` (a service worker in Chromium, a background script in Firefox) |
| Icon | `icon128.png`, 128 x 128 |
| Native host | `app.cptk.overlay` |
| Firefox id | `overlay@cptk.app`, `strict_min_version` 140.0 |

## Store listing

**Name:** CPTK Overlay

**Summary (132 characters max):** Drives streaming sites with a TV remote inside CPTK's own browser profiles. It does
nothing anywhere else.

Both stores take the summary from the manifest's `description` (106 characters), and neither lets you edit it in
the dashboard. Changing it means changing `build.py` and uploading a new package.

**Visibility:** Unlisted (Chrome Web Store), Hidden (Edge Add-ons), unlisted self-distribution (AMO).

**Category:** Accessibility. In the Chrome Web Store it sits under *Make Chrome Yours*.

**Language:** English (United States).

**Description** (Edge requires 250 to 10,000 characters; this is about 1,300):

> CPTK Overlay is part of CPTK, a 10-foot launcher for living-room PCs. CPTK opens streaming sites in a browser
> window of its own, and this extension lets a TV remote work on those pages.
>
> The extension does nothing in a tab unless CPTK opened that tab. CPTK adds a launch code to the end of the
> address, and the extension removes it from the address bar at once. Only that tab becomes active, and only until
> the tab closes or the browser quits. Every other tab, including your everyday browsing, is left alone.
>
> In an active tab, version 0.0.1 shows a small status box in the corner of the page and passes the remote's key
> presses to the CPTK program on the same computer. It passes named keys, such as the arrow keys, Enter and the
> media keys. It never passes the characters you type: a letter or a digit is reported only as "char". The F key
> enters full screen and the P key starts the page's video. If the CPTK program cannot be reached, the status box
> says why.
>
> The extension talks only to the CPTK program, through the browser's native messaging. It sends nothing over the
> network and loads no code from anywhere. In version 0.0.1 the CPTK program writes each report to a log file on
> the same computer, so the remote can be tested. Nothing goes to CPTK's developers or to anyone else.

**Graphic assets:**
- Store icon (Chrome Web Store) and extension logo (Edge): `extension/dist/chromium/icon128.png`. It is 128 x 128,
  which both stores accept (Edge recommends 300 x 300).
- Screenshot: the Chrome Web Store requires at least one, 1280 x 800. Edge makes them optional (640 x 480 or
  1280 x 800). Use a screenshot of the probe page with the status box showing.
- Promo tiles and video: none. They are optional in both stores.

**Website and support:** optional in both stores. Leave them empty, or use the StreamFlex repository's address.

## Privacy fields

The Chrome Web Store's *Privacy practices* tab and Edge's *Privacy* page ask the same five things.

**Single purpose:**

> Lets a TV remote control web pages in browser tabs that the CPTK launcher opens. It passes the remote's key
> presses from those tabs to the CPTK program on the same computer, and does nothing in any other tab.

**Permission justifications:**

- *nativeMessaging:* to talk to the CPTK program on the same computer (native host `app.cptk.overlay`). The
  extension passes the remote's key presses to it and shows its connection state in the page's status box. CPTK
  starts the browser and handles the remote's Home button.
- *storage:* to remember, for this browser session only, which tabs CPTK opened, so the extension stays inactive in
  every other tab. It uses `storage.session`, which the browser clears when it closes. A tab's entry is removed when
  the tab closes. The extension also keeps the CPTK program's last connection error there, to show it in the status
  box. It keeps nothing on disk.
- *Host permission (`<all_urls>`, and the content script on `<all_urls>`):* CPTK users add their own streaming sites
  and media servers (Jellyfin, Emby) at addresses that are not known in advance, often on the home network. The
  content script runs at `document_start` so it can remove CPTK's launch code from the address bar before the page
  reads it. On any page CPTK did not open, it does nothing: it asks the background whether the tab is active, gets
  "no", and stops.

**Remote code:** No, I am not using remote code.

> All of the extension's code is in the package. It fetches nothing from the network: no remote scripts, no remote
> data, no `eval` and no `new Function`. It runs no interpreter and takes no commands from any source. The only
> messages it receives come from the CPTK program on the same computer, through native messaging, and those are
> acknowledgments: the extension reads only the program's process id from the first one, to show "connected" in the
> status box.

**Data usage:** the Chrome Web Store and Edge count data the extension only hands to a program on the same computer,
so 0.0.1 does handle user data, even though none of it leaves the computer.

Check these two:
- **Web history:** the origin (scheme, host and port, not the full address) of a page in a tab CPTK opened.
- **User activity:** key presses in a tab CPTK opened. Named keys (ArrowUp, Enter, MediaPlayPause, F5) are passed by
  value. Any other key, which means any typed character, is passed only as `char`, with no key code. The one exception
  is the kit's own test pages at `http://127.0.0.1:8765`, which CPTK serves on the same computer: there, keys are
  passed in full.

Leave every other type unchecked: personally identifiable information, health, financial and payment,
authentication, personal communications, location and website content. The extension checks whether a page has a
video element and starts it on P, and reads no text from any page.

Check all three certifications: the data is not sold or transferred to third parties, is not used for anything
outside the single purpose, and is not used to decide creditworthiness or for lending.

**Privacy policy URL:** required in both stores, because the extension handles user data. Use the hosted copy of the
privacy policy below.

## Notes for the reviewer

The Chrome Web Store's *Test instructions* tab and Edge's *Notes for certification* box both take this. Both are
optional. They are worth filling in here, because the extension does nothing on an ordinary page.

> CPTK Overlay does nothing until CPTK, a desktop launcher, opens a tab with its launch code. To see it work without
> CPTK, open any web page with `#cptk=0123456789abcdef0123456789abcdef` added to the end of the address. A status box
> appears in the bottom-left corner with "CPTK probe 0.0.1". Its "host:" line says the native messaging host was not
> found, because the CPTK program is not installed. Press an arrow key and a `key` line appears. Press a letter and the
> line shows `"key":"char"`. F enters full screen. Open the same page without the code and nothing happens.

## Privacy policy text

Host this page before submitting, and paste its address into both stores' *Privacy policy URL* field. It names no
browser on purpose: Edge's policies ask that a policy given to Edge not refer to other browsers.

> # CPTK Overlay privacy policy
>
> This policy covers CPTK Overlay 0.0.1, a browser extension that is part of CPTK, a launcher for living-room PCs.
>
> **What the extension handles.** Only in a browser tab that CPTK opened, and only while that tab is open:
> - the origin of the page (its scheme, host and port, not its full address);
> - the keys pressed on the page: named keys such as the arrow keys, Enter and the media keys by name, and any
>   typed character only as "char" (keys on CPTK's own test pages at `http://127.0.0.1:8765` are recorded in full);
> - whether full screen and video playback worked when you pressed F or P;
> - technical details: the browser's tab and frame numbers, a time stamp, whether the extension may run on all
>   sites, and whether the browser reports that it is under automation.
>
> **Where it goes.** To the CPTK program on the same computer, through the browser's native messaging. In version
> 0.0.1 the CPTK program writes it to a log file on that computer (`%LOCALAPPDATA%\cptk-probe\probe.log` on
> Windows, `~/.cptk-probe/probe.log` on Linux), so the remote can be tested. Nothing is sent over the network, to
> CPTK's developers, or to anyone else.
>
> **What the extension keeps.** Which tabs CPTK opened, and the CPTK program's last connection error, in the
> browser's session storage. The browser clears it when it closes, and a tab's entry is removed when the tab closes.
> The extension keeps no browsing history and nothing on disk.
>
> **Sharing.** None. No data is sold, shared, or used for ads or analytics.
>
> **Your control.** Close the tab to stop it, or remove the extension. Delete the log file to erase what the CPTK
> program recorded.
>
> **Changes.** A version that handles data differently will update this policy first.

## Firefox (AMO, unlisted)

An unlisted add-on has no public listing on AMO, so AMO needs none of the text above. Its data disclosure lives in
the manifest instead: `browser_specific_settings.gecko.data_collection_permissions`.

**Pending a ruling.** `build.py` writes `{"required": ["none"]}`. AMO's add-on policies say that data sent to a
native application counts as data transmission. By that rule, 0.0.1 transmits browsing activity (the page origin)
and website activity (key presses), so `["browsingActivity", "websiteActivity"]` may be the accurate value. Settle
this before the Firefox build is signed, because a signed XPI cannot be changed.
