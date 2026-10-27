# CPTK Overlay Probe privacy policy

This policy covers CPTK Overlay Probe 0.0.1, a browser extension. The probe is a measurement tool for CPTK, a
launcher for living-room PCs. It is not CPTK Overlay, the planned product.

## What the probe handles

Only in a browser tab that CPTK opened, and only while that tab is open:

- **The page's origin:** its scheme, host and port, not its full address.
- **Key presses on the page.** Named keys, such as the arrow keys, Enter and the media keys, are recorded by name.
  A typed character, such as a letter or a digit, is recorded only as "char". The one exception is CPTK's own test
  pages at `http://127.0.0.1:8765`, which run on your computer: there, keys are recorded in full.
- **Full screen and playback results:** whether full screen and video playback worked when you pressed F or P.
- **Technical details:** the browser's tab and frame numbers, a time stamp, whether the extension may run on all
  sites, and whether the browser reports that it is under automation.

In any other tab the probe does nothing and handles nothing.

## Where it goes

The probe passes this data to the CPTK program on the same computer (native host `app.cptk.overlay`), through the
browser's native messaging. The CPTK program writes it to a log file on that computer:

- Windows: `%LOCALAPPDATA%\cptk-probe\probe.log`
- Linux: `~/.cptk-probe/probe.log`

Nothing is sent over the network, to CPTK's developers, or to anyone else. Nothing is sold, shared, or used for ads
or analytics.

## How long it is kept

- **In the browser:** the probe keeps only which tabs CPTK opened, and the CPTK program's last connection error. It
  keeps them in the browser's session storage. The browser clears that storage when it closes, and a tab's entry is
  removed when the tab closes. The probe keeps no browsing history and nothing on disk.
- **In the log file:** the log file stays on your computer until you delete it.

## How to delete it

- **To stop it:** close the tab, or remove the extension from your browser.
- **To erase what was recorded:** delete the log file listed above.

## Contact

Questions or problems: open an issue at `https://github.com/bilbospocketses/streamflex/issues`.

## Changes

If a later version handles data differently, this policy is updated before that version is published.
