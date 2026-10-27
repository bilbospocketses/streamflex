# CPTK Overlay Probe 0.0.1: the store steps

This is the checklist for publishing CPTK Overlay Probe 0.0.1, the Phase 0 measurement probe. It goes to three stores:

- the Chrome Web Store, as an unlisted item;
- Edge Add-ons, as a hidden item;
- AMO (addons.mozilla.org), which signs the Firefox build for self-distribution.

The probe has its own listing, separate from the planned product. **Do not create a CPTK Overlay listing (or claim
`overlay@cptk.app` on AMO) yet.** That listing waits for R1.

Keep `tools/overlay-phase0/listing.md` open beside this file. Every box you fill in a store comes from there. Open it
rendered, on GitHub or in a Markdown preview, and copy each paste block from that view, never from the raw file: the
raw file wraps the blocks as quoted lines, so a copy from it carries `> ` marks and line breaks into the store's
fields. Type every command in a Windows PowerShell window. Store review can take days, so start as soon as the
extension has landed.

## Before you start

1. **Find the checkout.** The commands below use one variable for it. Paste this line into the PowerShell window
   first, and keep that window open for the whole checklist:

   ```powershell
   $repo = 'C:\Users\jscha\ClaudeScratch\wt-streamflex-overlay-spec'
   ```

   Check the extension is there. This must print `True`:

   ```powershell
   Test-Path "$repo\tools\overlay-phase0\extension\build.py"
   ```

   If it prints `False`, the extension has not landed in this checkout yet. Stop and ask the streamflex tab.

2. **Have these ready:**
   - the Google account that will own CPTK's Chrome Web Store items, and a payment card for its one-time fee;
   - the Microsoft account that will own CPTK's Edge items (no fee);
   - a Mozilla account for AMO (no fee);
   - a password manager entry for the AMO keys you will create in step 7.

3. **The privacy policy address.** Both stores require one. Use
   `https://github.com/bilbospocketses/streamflex/blob/master/tools/overlay-phase0/privacy-policy.md`.
   That URL works once PR #73 has merged; a submission before then uses the file's permalink on PR #73 instead (the
   streamflex tab gives it to you).

## 1. Build the packages

```powershell
python "$repo\tools\overlay-phase0\extension\build.py"
```

It prints two lines:

```
built ...\dist\cptk-overlay-probe-chromium-0.0.1.zip
built ...\dist\cptk-overlay-probe-firefox-0.0.1.zip
```

Here is what each store gets:

| Store | What to upload | Where it is |
|---|---|---|
| Chrome Web Store | the Chromium zip | `$repo\tools\overlay-phase0\extension\dist\cptk-overlay-probe-chromium-0.0.1.zip` |
| Edge Add-ons | the same Chromium zip | the same file |
| AMO | nothing by hand: step 8 signs the folder | `$repo\tools\overlay-phase0\extension\dist\firefox\` |

The Firefox zip is not uploaded anywhere. Step 8 signs the `dist\firefox` folder directly. `make-kit.ps1` does not
build packages: it copies the finished kit, the signed XPI included, into one folder later (see step 10).

## 2. Make the screenshot

The Chrome Web Store needs at least one screenshot, exactly 1280 x 800 pixels. Edge takes the same file.

1. In Chrome, open `chrome://extensions`, turn on *Developer mode* (top right), click *Load unpacked*, and pick
   `$repo\tools\overlay-phase0\extension\dist\chromium`. This copy is only for the screenshot.
2. Open a second PowerShell window, paste the `$repo` line from *Before you start* into it, and start the test page
   server there. Leave that window running:

   ```powershell
   powershell -ExecutionPolicy Bypass -File "$repo\tools\overlay-phase0\pages\serve.ps1"
   ```

3. In Chrome, open a new tab, go to `http://127.0.0.1:8765/#cptk=0123456789abcdef0123456789abcdef` and press a few
   arrow keys. The probe's box, headed "CPTK Overlay Probe 0.0.1", appears in the bottom-left corner.
4. Take a screenshot with Win+Shift+S and save it.
5. Fit it to 1280 x 800. In your first PowerShell window, set `$shot` to the file you saved, then paste the rest:

   ```powershell
   $shot = "$env:USERPROFILE\Pictures\Screenshots\<the file you saved>.png"
   $saveAs = "$env:USERPROFILE\Pictures\cptk-overlay-probe-1280x800.png"
   Add-Type -AssemblyName System.Drawing
   $src = [System.Drawing.Image]::FromFile($shot)
   $out = New-Object System.Drawing.Bitmap 1280, 800
   $g = [System.Drawing.Graphics]::FromImage($out)
   $g.Clear([System.Drawing.Color]::Black)
   $scale = [Math]::Min(1280 / $src.Width, 800 / $src.Height)
   $w = [int]($src.Width * $scale); $h = [int]($src.Height * $scale)
   $g.DrawImage($src, [int]((1280 - $w) / 2), [int]((800 - $h) / 2), $w, $h)
   $out.Save($saveAs, [System.Drawing.Imaging.ImageFormat]::Png)
   $g.Dispose(); $out.Dispose(); $src.Dispose()
   ```

   The result is `cptk-overlay-probe-1280x800.png` in your Pictures folder.
6. Stop the server with Ctrl+C in the second window, and remove the unpacked copy from `chrome://extensions`.

## 3. Chrome Web Store: the account

1. Sign in at `https://chrome.google.com/webstore/devconsole` with the Google account that will own CPTK's items.
2. Accept the developer agreement and pay the one-time registration fee. The page shows the current amount (it has
   long been US$5).
3. Open *Account* in the left menu. Set the publisher name to `CPTK`, and verify the contact email. The store sends
   a link to that address; click it.
4. If the dashboard asks you to turn on 2-Step Verification for the Google account, do that and come back.

## 4. Chrome Web Store: the upload

1. Click *New item* and upload `cptk-overlay-probe-chromium-0.0.1.zip`.
2. **The item id appears at once**, on the item's page and in the browser's address bar. It is 32 letters, all
   between `a` and `p`. Copy it into a note now; step 9 needs it.
3. **Store listing tab**, from `listing.md`, section *Store listing*:
   - Description: paste the description.
   - Category: Accessibility (under *Make Chrome Yours*).
   - Language: English (United States).
   - Store icon: `$repo\tools\overlay-phase0\extension\dist\chromium\icon128.png`.
   - Screenshot: `cptk-overlay-probe-1280x800.png` from step 2.
   - The name and summary come from the package and cannot be edited here. That is expected.
4. **Privacy practices tab**, from `listing.md`, section *Privacy fields*:
   - Single purpose: paste it.
   - Permission justifications: one box each for `nativeMessaging`, `storage` and the host permission. Paste each.
   - Remote code: pick *No, I am not using remote code*, and paste the text that follows it if a box appears.
   - Data usage: check *Web history* and *User activity* only. Check all three certification boxes.
   - Privacy policy URL: the address from *Before you start*.
5. **Distribution tab:** set *Visibility* to **Unlisted**. Leave the regions at *All regions*.
6. **Test instructions tab:** paste the *Notes for the reviewer* text.
7. Click *Submit for review*.

## 5. Edge Add-ons: the account

1. Sign in at `https://partner.microsoft.com/dashboard/microsoftedge/overview` with the Microsoft account that will
   own CPTK's items.
2. Register for the Microsoft Edge program. There is no fee.
3. Pick the account type with care: it **cannot be changed later**. *Individual* is for one person. *Company* needs a
   company approver and a check of the company, which takes longer.
4. Set the publisher display name to `CPTK`.

## 6. Edge Add-ons: the upload

1. Click *Create new extension* and upload the same `cptk-overlay-probe-chromium-0.0.1.zip`.
2. **Availability:** set *Visibility* to **Hidden**. Leave *Markets* at all markets.
3. **Properties:** Category: Accessibility. Leave *Mature content* unchecked.
4. **Privacy**, from `listing.md`, section *Privacy fields*: single purpose, the three permission justifications,
   *No, I am not using remote code*, the same two data types (web history, user activity) and the certifications,
   and the privacy policy URL.
5. **Store listings:** click *Edit details* on the English row.
   - Description: paste it. Edge wants at least 250 characters; this one has about 1,800.
   - Extension logo: `icon128.png`, the same file as the Chrome store icon.
   - Screenshot: `cptk-overlay-probe-1280x800.png` (optional here, but use it).
   - The name and the short description come from the package. That is expected.
6. Click *Publish*. In *Notes for certification*, paste the *Notes for the reviewer* text. Click *Publish* again.
7. **Find the id.** Open the extension's *Extension overview* page. It shows a listing URL that ends in a 32-letter
   id, all letters between `a` and `p`. That id is the one you want. Partner Center also shows a Product ID and a
   Store ID, which look different. Do not use either. Copy the 32-letter id into your note.
8. Edge says certification can take up to seven business days.

## 7. AMO: the account and the keys

1. Sign in at `https://addons.mozilla.org/developers/` with your Mozilla account, and accept the distribution
   agreement.
2. Open `https://addons.mozilla.org/developers/addon/api/key/` and generate credentials. You get two values: a
   **JWT issuer** (it looks like `user:12345:678`) and a **JWT secret** (a long string). Save both in the password
   manager now. AMO shows the secret only once.

## 8. Sign the Firefox build

A signed file cannot be changed, and AMO will not sign version 0.0.1 a second time. So check first that the build
is version 0.0.1 and carries the probe's id and its data declaration. This must print `True`:

```powershell
$m = Get-Content -Raw "$repo\tools\overlay-phase0\extension\dist\firefox\manifest.json" | ConvertFrom-Json
$g = $m.browser_specific_settings.gecko
$req = @($g.data_collection_permissions.required)
$m.version -ceq '0.0.1' -and $g.id -ceq 'overlay-probe@cptk.app' -and $req.Count -eq 2 -and $req[0] -ceq 'browsingActivity' -and $req[1] -ceq 'websiteActivity'
```

`False` means the build is not the reviewed one. Stop and ask the streamflex tab.

1. Put the two keys into this PowerShell window only. `Read-Host` asks for each one. The secret is hidden as you
   type, and neither one is saved in a file or in the command history:

   ```powershell
   $env:WEB_EXT_API_KEY = Read-Host 'AMO JWT issuer'
   $secret = Read-Host 'AMO JWT secret' -AsSecureString
   $env:WEB_EXT_API_SECRET = [System.Net.NetworkCredential]::new('', $secret).Password
   ```

2. Make the output folder and sign:

   ```powershell
   $handoff = 'C:\Users\jscha\ClaudeScratch\streamflex-handoff\overlay-phase0'
   New-Item -ItemType Directory -Force $handoff | Out-Null
   npx.cmd --yes web-ext@8 sign --source-dir "$repo\tools\overlay-phase0\extension\dist\firefox" --channel unlisted --artifacts-dir $handoff
   ```

   `npx.cmd`, not `npx`: in Windows PowerShell a plain `npx` can be blocked by the script policy. `web-ext` reads
   the two keys from `WEB_EXT_API_KEY` and `WEB_EXT_API_SECRET` by itself.
3. Wait. AMO checks and signs unlisted add-ons on its own, usually within minutes, and `web-ext` waits up to 15
   minutes. When it finishes, a new `.xpi` file is in the handoff folder. AMO picks its name.
4. If `web-ext` stops before the file arrives, do not run it again. Open `https://addons.mozilla.org/developers/`, pick
   CPTK Overlay Probe, then version 0.0.1, and download the signed file into the handoff folder by hand.
5. Rename the file to `cptk-overlay-probe-0.0.1.xpi`:

   ```powershell
   Get-ChildItem $handoff -Filter *.xpi
   Rename-Item (Join-Path $handoff '<the name it showed>.xpi') 'cptk-overlay-probe-0.0.1.xpi'
   ```

6. **Sign a 0.0.2 copy for guest row G13, while the keys are still in this window.** G13 checks that Firefox picks
   up a replaced XPI, so it needs the same probe signed a second time, as version 0.0.2. Like 0.0.1 it is signed
   with `--channel unlisted`, so it never appears on any listing. The version changes on this PC only and is put
   back afterwards; never commit it.

   ```powershell
   $build = "$repo\tools\overlay-phase0\extension\build.py"
   $text = [IO.File]::ReadAllText($build)
   [IO.File]::WriteAllText($build, $text.Replace('VERSION = "0.0.1"', 'VERSION = "0.0.2"'))
   python $build
   npx.cmd --yes web-ext@8 sign --source-dir "$repo\tools\overlay-phase0\extension\dist\firefox" --channel unlisted --artifacts-dir "$handoff\0.0.2"
   ```

   The build prints two `0.0.2` lines. Wait for `web-ext` as in 3 above; if it stops before the file arrives,
   download version 0.0.2 by hand as in 4 above, into `$handoff\0.0.2`. Then move the file next to 0.0.1 under its
   new name:

   ```powershell
   Get-ChildItem "$handoff\0.0.2" -Filter *.xpi
   Move-Item (Join-Path "$handoff\0.0.2" '<the name it showed>.xpi') (Join-Path $handoff 'cptk-overlay-probe-0.0.2.xpi')
   ```

   Put the version back, rebuild 0.0.1 and delete the 0.0.2 zips:

   ```powershell
   [IO.File]::WriteAllText($build, $text)
   python $build
   Remove-Item "$repo\tools\overlay-phase0\extension\dist\*-0.0.2.zip"
   ```

   Run the check from the top of this section (the one before 1) again. It must print `True`: the build is back at
   0.0.1. `False` means the version did not go back. Stop and ask the streamflex tab.

7. Clear the keys from the window:

   ```powershell
   Remove-Item Env:WEB_EXT_API_KEY, Env:WEB_EXT_API_SECRET
   Remove-Variable secret
   ```

The signed XPIs are now at
`C:\Users\jscha\ClaudeScratch\streamflex-handoff\overlay-phase0\cptk-overlay-probe-0.0.1.xpi` and, for G13,
`C:\Users\jscha\ClaudeScratch\streamflex-handoff\overlay-phase0\cptk-overlay-probe-0.0.2.xpi`.

## 9. Record the ids

1. Create `$repo\tools\overlay-phase0\ids.json` with the two ids from your note. The `gecko` line stays as it is:

   ```json
   {
     "chrome_web_store": "<the 32-letter id from step 4>",
     "edge_addons": "<the 32-letter id from step 6>",
     "gecko": "overlay-probe@cptk.app"
   }
   ```

2. Check both ids have the right shape. This must print `True`:

   ```powershell
   $ids = Get-Content -Raw "$repo\tools\overlay-phase0\ids.json" | ConvertFrom-Json
   $ids.chrome_web_store -cmatch '^[a-p]{32}$' -and $ids.edge_addons -cmatch '^[a-p]{32}$' -and $ids.gecko -ceq 'overlay-probe@cptk.app'
   ```

   `False` means an id is mistyped, an Edge Product ID or Store ID was copied by mistake, or the `gecko` line is not
   `overlay-probe@cptk.app`.
3. Do not commit the file yourself. The streamflex tab commits it with `listing.md`.

## 10. Tell the streamflex tab

Paste this into the streamflex tab, filled in:

```
CPTK Overlay Probe 0.0.1 is submitted.
Chrome Web Store id: <id>  (submitted <date>)
Edge Add-ons id: <id>  (submitted <date>)
Signed XPI: C:\Users\jscha\ClaudeScratch\streamflex-handoff\overlay-phase0\cptk-overlay-probe-0.0.1.xpi
Signed 0.0.2 XPI (G13): C:\Users\jscha\ClaudeScratch\streamflex-handoff\overlay-phase0\cptk-overlay-probe-0.0.2.xpi / not made
ids.json: written and checked (True) / not written
Privacy policy URL used: <address>
```

The tab then commits `ids.json` and builds the kit. The run sheet covers that, with a line like this, run from
the checkout:

```powershell
powershell -ExecutionPolicy Bypass -File "$repo\tools\overlay-phase0\make-kit.ps1" -Build "$repo\build-phase0\Release" -Xpi "C:\Users\jscha\ClaudeScratch\streamflex-handoff\overlay-phase0\cptk-overlay-probe-0.0.1.xpi"
```

## 11. Wait for both reviews

Each store emails you when the item is published. Tell the streamflex tab when each one goes live. Guest rows G1, G5,
G10 and G15 need both live, and so do Part B, H3 and H4 of your run sheet. Guest row G13 needs neither: only the two
signed XPIs and `ids.json`. The other rows need no listing.

If you submitted with the PR #73 permalink for the privacy policy, then once PR #73 has merged, replace the privacy
policy URL in both stores with
`https://github.com/bilbospocketses/streamflex/blob/master/tools/overlay-phase0/privacy-policy.md`. The permalink keeps
working, but it freezes the policy at that commit.
