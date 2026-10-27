# CPTK Overlay Probe: store listing text

This is the listing text for all three stores, so the Chrome Web Store, Edge Add-ons and AMO say the same thing. Every
answer here matches CPTK Overlay Probe 0.0.1 as `extension/build.py` builds it. If the manifest or the scripts change,
change this file in the same commit. The step-by-step upload checklist is
`design/research/overlay/phase0/store-steps.md`.

The probe has its own listing. It is not the first version of CPTK Overlay, the planned product: that listing is not
created in Phase 0, and the gecko id `overlay@cptk.app` stays unclaimed on AMO.

## The package

| Field | Value |
|---|---|
| Name | CPTK Overlay Probe |
| Version | 0.0.1 (Manifest V3) |
| Packages | `extension/dist/cptk-overlay-probe-chromium-0.0.1.zip` (Chrome Web Store, Edge Add-ons), `extension/dist/firefox/` (signed on AMO) |
| `permissions` | `nativeMessaging`, `storage` |
| `host_permissions` | `<all_urls>` |
| Content script | `content.js` on `<all_urls>`, `run_at: document_start` |
| Background | `background.js` (a service worker in Chromium, a background script in Firefox) |
| Icon | `icon128.png`, 128 x 128 |
| Native host | `app.cptk.overlay` (local to the computer, so it keeps the product's name) |
| Firefox id | `overlay-probe@cptk.app`, `strict_min_version` 140.0 |
| Firefox data collection | `"required": ["browsingActivity", "websiteActivity"]` |

## Store listing

**Name:** CPTK Overlay Probe

**Summary (132 characters max):** A measurement probe for CPTK's TV-remote overlay. It runs only in CPTK's own browser
profiles and does nothing anywhere else.

Both stores take the summary from the manifest's `description` (125 characters), and neither lets you edit it in
the dashboard. Changing it means changing `build.py` and uploading a new package.

**Visibility:** Unlisted (Chrome Web Store), Hidden (Edge Add-ons), unlisted self-distribution (AMO).

**Category:** Accessibility. In the Chrome Web Store it sits under *Make Chrome Yours*.

**Language:** English (United States).

**Description** (Edge requires 250 to 10,000 characters; this is about 1,600):

> CPTK Overlay Probe is a measurement tool, not a finished product. CPTK is a 10-foot launcher for living-room PCs,
> and its developers are planning a separate extension, CPTK Overlay, that will let a TV remote work on streaming
> sites. Before building it, they use this probe to measure how browsers behave: whether the extension is installed
> and reachable, which remote keys reach a page, and whether full screen and video playback can start from a key
> press.
>
> The probe does nothing in a tab unless CPTK opened that tab. CPTK adds a launch code to the end of the address, and
> the probe removes it from the address bar at once. Only that tab becomes active, and only until the tab closes or
> the browser quits. Every other tab, including your everyday browsing, is left alone.
>
> In an active tab, the probe shows its own box in the bottom-left corner of the page, headed "CPTK Overlay Probe
> 0.0.1", with its last few readings. It passes the remote's key presses to the CPTK program on the same computer. It
> passes named keys, such as the arrow keys, Enter and the media keys. It never passes the characters you type: a
> letter or a digit is reported only as "char". The F key enters full screen and the P key starts the page's video,
> and the probe reports whether each one worked. If the CPTK program cannot be reached, the box says why.
>
> The probe talks only to the CPTK program, through the browser's native messaging. The CPTK program writes each
> reading to a log file on the same computer. Nothing is sent over the network, to CPTK's developers or to anyone
> else, and the probe loads no code from anywhere.

**Graphic assets:**
- Store icon (Chrome Web Store) and extension logo (Edge): `extension/dist/chromium/icon128.png`. It is 128 x 128,
  which both stores accept (Edge recommends 300 x 300).
- Screenshot: the Chrome Web Store requires at least one, 1280 x 800. Edge makes them optional (640 x 480 or
  1280 x 800). Use a screenshot of the probe page with the probe's box showing.
- Promo tiles and video: none. They are optional in both stores.

**Website and support:** optional in both stores. Use `https://github.com/bilbospocketses/streamflex/issues` for
support, or leave both empty.

## Privacy fields

The Chrome Web Store's *Privacy practices* tab and Edge's *Privacy* page ask the same five things.

**Single purpose:**

> Measures how a TV remote can work on web pages in browser tabs that the CPTK launcher opens. In those tabs only, it
> passes the remote's key presses and the results of full screen and video playback to the CPTK program on the same
> computer, and shows them in an on-page box. It does nothing in any other tab.

**Permission justifications:**

- *nativeMessaging:* to talk to the CPTK program on the same computer (native host `app.cptk.overlay`). The probe
  passes its readings to it, and shows the connection state in the probe's on-page box.
- *storage:* to remember, for this browser session only, which tabs CPTK opened, so the probe stays inactive in
  every other tab. It uses `storage.session`, which the browser clears when it closes. A tab's entry is removed when
  the tab closes. The probe also keeps the CPTK program's last connection error there, to show it in the box. It
  keeps nothing on disk.
- *Host permission (`<all_urls>`, and the content script on `<all_urls>`):* CPTK users add their own streaming sites
  and media servers (Jellyfin, Emby) at addresses that are not known in advance, often on the home network, and the
  probe has to measure the same pages. The content script runs at `document_start` so it can remove CPTK's launch
  code from the address bar before the page reads it. On any page CPTK did not open, it does nothing: it asks the
  background whether the tab is active, gets "no", and stops.

**Remote code:** No, I am not using remote code.

> All of the probe's code is in the package. It fetches nothing from the network: no remote scripts, no remote data,
> no `eval` and no `new Function`. It runs no interpreter and takes no commands from any source. The only messages it
> receives come from the CPTK program on the same computer, through native messaging, and those are
> acknowledgments: the probe reads only the program's process id from the first one, to show "connected" in its box.

**Data usage:** both stores count data that an extension hands to a program on the same computer, so the probe does
handle user data, even though none of it leaves the computer.

Check these two:
- **Web history:** the origin (scheme, host and port, not the full address) of a page in a tab CPTK opened.
- **User activity:** in a tab CPTK opened, the key presses, and whether full screen and video playback worked after
  F or P. Named keys (ArrowUp, Enter, MediaPlayPause, F5) are passed by value. Any other key, which means any typed
  character, is passed only as `char`, with no key code. The one exception is the kit's own test pages at
  `http://127.0.0.1:8765`, which CPTK serves on the same computer: there, keys are passed in full.

Leave every other type unchecked: personally identifiable information, health, financial and payment,
authentication, personal communications, location and website content. The probe checks whether a page has a video
element and starts it on P, and reads no text from any page.

Where a store has a box for how the data is used, paste:

> Passed only to the CPTK program on the same computer (native host `app.cptk.overlay`), which writes it to a log
> file on that computer. Nothing leaves the computer. Nothing is sold or shared. Typed characters are redacted to
> "char".

Check all three certifications: the data is not sold or transferred to third parties, is not used for anything
outside the single purpose, and is not used to decide creditworthiness or for lending.

**Privacy policy URL:** required in both stores, because the probe handles user data.

`https://github.com/bilbospocketses/streamflex/blob/master/tools/overlay-phase0/privacy-policy.md`

The policy is `tools/overlay-phase0/privacy-policy.md`. That URL works once PR #73 has merged. A submission before
then uses the file's permalink on PR #73 instead (`store-steps.md`, *Before you start*).

## Notes for the reviewer

The Chrome Web Store's *Test instructions* tab and Edge's *Notes for certification* box both take this. Both are
optional. They are worth filling in here, because the probe does nothing on an ordinary page.

> CPTK Overlay Probe is a measurement tool for a planned product. It does nothing until CPTK, a desktop launcher,
> opens a tab with its launch code. To see it work without CPTK, open any web page with
> `#cptk=0123456789abcdef0123456789abcdef` added to the end of the address. A box appears in the bottom-left corner,
> headed "CPTK Overlay Probe 0.0.1". Its "host:" line says the native messaging host was not found, because the CPTK
> program is not installed. Press an arrow key and a `key` line appears. Press a letter and the line shows
> `"key":"char"`. F enters full screen. Open the same page without the code and nothing happens.

## Firefox (AMO, unlisted)

An unlisted add-on has no public listing on AMO, so AMO needs none of the text above. Its data disclosure lives in
the manifest instead, in `browser_specific_settings.gecko.data_collection_permissions`:

```json
"data_collection_permissions": { "required": ["browsingActivity", "websiteActivity"] }
```

- `browsingActivity` is for the page origin. Mozilla: "Information about the websites users visit, such as specific
  URLs, domains, or categories of pages users view."
- `websiteActivity` is for the key presses and the full screen and playback results. Mozilla: "interactions and mouse
  and keyboard activity, such as scrolling, clicking, typing, and actions".

Both are on Mozilla's list at
`https://extensionworkshop.com/documentation/develop/firefox-builtin-data-consent/`. AMO's add-on policies count data
sent to a native application as data transmission.
