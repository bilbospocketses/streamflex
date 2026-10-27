# StreamFlex overlay: native-side libraries and APIs (todo item 24)

Researched 2026-10-27. Desk research only; nothing installed. Grades: **VERIFIED** = the primary page or source file was fetched and read in this pass; **REPORTED** = a secondary source, a search snippet, or a primary page that did not state it outright; **UNKNOWN** = not found. Facts about package versions are as the package index showed them on 2026-10-27.

Caveat on method: WebFetch answers through a small summarizing model. Where a number or a quote carries design weight I either fetched the raw page or note that it came through the summarizer. One discrepancy found that way is called out in section 5 (Chrome's extension-to-host size limit).

## Summary table and recommendations

| # | Need | Recommendation | Why (one line) |
|---|---|---|---|
| 1 | HTTPS client | **libcurl on both OSes**, vcpkg `curl` with its default `ssl` feature (Schannel on Windows, OpenSSL on Linux); set `CURLSSLOPT_REVOKE_BEST_EFFORT` on Windows | One code path, default-on cert verification, every distro floor ships it (trixie 8.14.1, Fedora 44 8.18.0, Arch 8.23.0). WinHTTP is the alternative only if PAC/WPAD proxies must work: curl cannot do PAC. |
| 2 | Ed25519 verify + SHA-256 | **libsodium** (`crypto_sign_verify_detached` + `crypto_hash_sha256`) | One dependency does both, ISC license, in vcpkg and all four distro families (trixie 1.0.18, Fedora 44 1.0.22, Arch 1.0.22). Fallback if zero new runtime deps is the goal: vendor Monocypher (+ its optional Ed25519/SHA-512 file) and a small public-domain SHA-256, because Monocypher has no SHA-256. |
| 3 | JSON | **yyjson** (vendor it, or use distro/vcpkg), MIT | Explicit-length input, unescapes strings, strict. The raw-span requirement does not need parser support (see section 3). cJSON is the safe-everywhere alternative (trixie 1.7.18). |
| 4 | QR code | **Nayuki QR-Code-generator, C version, vendored** (`qrcodegen.c/.h`), MIT | Two files, no heap, 3918-byte worst-case buffer, v1.8.0. libqrencode is LGPL-2.1+ and heavier. |
| 5 | Native Messaging host | Host can be the same exe only by sniffing argv; **recommend a console-subsystem host mode or tiny sibling exe**; exit on stdin EOF | Browsers pass fixed args and the manifest cannot add any. Chromium closes the pipes then terminates the host about 2 s later (Windows). |
| 6 | SDL3 | `SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS` and `SDL_HINT_FORCE_RAISEWINDOW` both exist since **3.2.0**; HDR properties and the HDR window event since **3.2.0**; nothing HDR-related new in 3.4.0 | All available at the 3.2.10 floor. |
| 7 | Job Objects | Nested jobs work on Windows 8+; **do not set UI restrictions or either breakaway flag** on the StreamFlex job; use `KILL_ON_JOB_CLOSE` | Per Microsoft's nested-jobs page. Whether Edge/Chrome helpers escape by other means is UNKNOWN. |
| 8 | Trusted keys | SendInput is subject to UIPI (equal or lower integrity only); uinput needs a shipped udev rule (systemd ships none) | `isTrusted` for SendInput keys is not confirmed by any primary source. Test it. |
| 9 | Home key | `WH_KEYBOARD_LL`: timeout default is silent removal on Windows 7+, cap 1000 ms from Windows 10 1709; GlobalShortcuts portal is fully supported in GNOME 48; evdev `EVIOCGRAB` is exclusive | See section 9. |
| 10 | Portable ZIP, no admin | **Plan on HKCU for everything**: Chromium and Firefox both load user-scope policy from HKCU (source-verified); NM host registration is HKCU-capable on all three browsers (doc-verified). Do not plan on a UAC prompt a remote can answer. | Store-hosted force-install is permitted on non-domain Windows (Chrome: only off-store is restricted; Edge: limited to **Edge Add-ons** listings). Default ACL on `HKCU\Software\Policies` and UAC keyboard behavior are not documented: test. See section 10. |

---

## 1. HTTPS client for C

### libcurl
- **vcpkg port `curl` 8.23.0** (VERIFIED, https://raw.githubusercontent.com/microsoft/vcpkg/master/ports/curl/vcpkg.json). Default feature is `ssl`. That feature resolves to `sspi` on Windows (non-UWP) and `openssl` elsewhere. The port's own comment: "Defaults to schannel on Windows, unless http3 is also enabled". There is no feature literally named `schannel`; `sspi` is the Windows TLS selection. Other features that matter: `http2` (adds nghttp2), `brotli`, `zstd`, `psl`, `idn`/`winidn`. Not needed for a small JSON-over-HTTPS client.
- **Linux package versions:**
  - Debian trixie: `libcurl4-openssl-dev` 8.14.1-2+deb13u7 (VERIFIED, https://packages.debian.org/trixie/libcurl4-openssl-dev). GnuTLS and NSS flavors also exist (`libcurl4-gnutls-dev`, `libcurl4-nss-dev`). Runtime package `libcurl4t64` has the same version, 1,013 kB installed on amd64, and depends on libssl3t64, libnghttp2, libnghttp3, libssh2, libldap2, libgssapi-krb5, librtmp1, libpsl5t64, libzstd1, libbrotli1, zlib1g; it Recommends ca-certificates (VERIFIED, https://packages.debian.org/trixie/libcurl4t64).
  - Fedora 44: `libcurl-devel` 8.18.0-14.fc44 (VERIFIED, https://packages.fedoraproject.org/pkgs/curl/libcurl-devel/). TLS backend of the Fedora build: UNKNOWN from these pages (REPORTED to be OpenSSL).
  - Arch: `curl` 8.23.0-1 in core, depends on openssl (so OpenSSL backend), ca-certificates; 2.3 MB installed for the whole package including the tool (VERIFIED, https://archlinux.org/packages/core/x86_64/curl/).
  - Raspberry Pi OS trixie and Ubuntu 25.10: not fetched; both derive from Debian/Ubuntu archives (UNKNOWN versions here).
- **Certificate validation defaults:** `CURLOPT_SSL_VERIFYPEER` defaults to 1 (VERIFIED, https://curl.se/libcurl/c/CURLOPT_SSL_VERIFYPEER.html). The default CA bundle path is fixed at build time and overridable with `CURLOPT_CAINFO`/`CURLOPT_CAPATH`. The doc page covers the tool: "If curl was built with Schannel, then curl uses the certificates that are built into the OS" (VERIFIED, https://curl.se/docs/sslcerts.html), so on Windows with the vcpkg default there is no CA file to ship. Schannel also runs CRL checks unless verification is off (same page).
  - `CURLSSLOPT_REVOKE_BEST_EFFORT` (since 7.70.0, Schannel only) ignores revocation failures when a distribution point is missing or offline; `CURLSSLOPT_NO_REVOKE` disables checks; NO_REVOKE wins if both are set (VERIFIED, https://curl.se/libcurl/c/CURLOPT_SSL_OPTIONS.html). **Recommendation:** set BEST_EFFORT on Windows so an offline CRL point does not look like "adapter source down".
  - `CURLSSLOPT_NATIVE_CA` is per backend: OpenSSL on Windows since 7.71.0, GnuTLS 8.5.0 (same page). Not needed on Linux where the distro bundle path is compiled in.
- **Proxy support:**
  - libcurl honors `https_proxy`/`HTTPS_PROXY`, `ALL_PROXY` and `NO_PROXY` (CIDR in NO_PROXY since 7.86.0) (VERIFIED for the tool's behavior, https://everything.curl.dev/usingcurl/proxies/env.html; the page describes the tool, and libcurl's own env handling is REPORTED to match).
  - **curl does not support PAC files:** "Since curl does not support JavaScript, it cannot support .pac proxy configuration either." (VERIFIED, https://curl.se/docs/faq.html).
  - Whether libcurl reads the Windows/Internet Options proxy automatically: **UNKNOWN** (the FAQ and the proxy page do not say). Treat it as "no" and test on a machine with a system proxy.
- **Binary size cost:** UNKNOWN for a static vcpkg build (not measured). Reference points only: the Debian runtime lib is about 1 MB plus its dependency chain; the Arch package is 2.3 MB with the tool.

### WinHTTP (Windows only)
- Ships in the OS, no dependency (VERIFIED, https://learn.microsoft.com/en-us/windows/win32/winhttp/winhttp-start-page).
- `WinHttpOpen` with `WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY` "uses system and per-user proxy settings (including the Internet Explorer proxy configuration)" and handles failover and authentication; supported Windows 8.1 and newer. `WINHTTP_ACCESS_TYPE_DEFAULT_PROXY` "does not inherit browser proxy settings" and is deprecated on 8.1+ (VERIFIED, https://learn.microsoft.com/en-us/windows/win32/api/winhttp/nf-winhttp-winhttpopen).
- PAC/WPAD is available through `WinHttpGetProxyForUrl`, but it is not fully integrated: the app calls it per URL and sets `WINHTTP_OPTION_PROXY` on the request (VERIFIED, https://learn.microsoft.com/en-us/windows/win32/winhttp/winhttp-autoproxy-api).
- `WINHTTP_FLAG_SECURE_DEFAULTS` requires TLS 1.2+ (same WinHttpOpen page). Microsoft recommends async mode.
- Cost: a second, Windows-only HTTP implementation next to libcurl.

### Recommendation
**libcurl on both**, because a second client doubles the test surface for a few small GETs. Choose WinHTTP-on-Windows only if corporate PAC/WPAD proxies are a stated requirement; that is the one thing it does that libcurl cannot. Pin the vcpkg baseline so the version is predictable, and code to the 7.x/8.x API that the trixie floor (8.14.1) has.

---

## 2. Ed25519 verify and SHA-256 in C

| Library | Ed25519 verify | SHA-256 | License | Size / form | Availability |
|---|---|---|---|---|---|
| **libsodium** | `crypto_sign_verify_detached` (VERIFIED documented, https://doc.libsodium.org/public-key_cryptography/public-key_signatures) | `crypto_hash_sha256` plus `_init/_update/_final` (VERIFIED, header fetched) | ISC (vcpkg, Arch) | Arch installed 657.5 KB (VERIFIED) | vcpkg 1.0.22 port-version 1 (VERIFIED); Debian trixie `libsodium-dev` 1.0.18-1+deb13u1 with runtime `libsodium23` (VERIFIED); Fedora 44 `libsodium-devel` 1.0.22-1.fc44 (VERIFIED); Arch `libsodium` 1.0.22-1 (VERIFIED) |
| **Monocypher** 4.0.3 | `crypto_ed25519_check(sig[64], pk[32], msg, len)` lives in the **optional** `src/optional/monocypher-ed25519.c/.h` (16,526 + 5,493 bytes). The core `crypto_eddsa_check` uses BLAKE2b and is **not** Ed25519-compatible. (VERIFIED, https://monocypher.org/manual/eddsa, https://monocypher.org/manual/ed25519, GitHub directory listing) | **None.** The optional header has SHA-512, SHA-512 HMAC and HKDF only (VERIFIED, header fetched). | CC0-1.0 or BSD-2-Clause, your choice (VERIFIED) | Core under 2,000 SLOC, no dependencies, vendor `monocypher.c/.h` (VERIFIED, https://monocypher.org/) | Not in Debian trixie (a name search returned nothing; VERIFIED). vcpkg: the path `ports/monocypher` returned 404 (VERIFIED, so no port under that name). Vendor-only. |
| **TweetNaCl** | Yes (`crypto_sign_open`) | `crypto_hash` is SHA-512 only | Public domain (REPORTED) | About 100 tweets of C | Not researched further; slower and not packaged in this survey. UNKNOWN availability. |
| **OpenSSL 3 EVP** | `EVP_DigestVerify` with Ed25519 (REPORTED, not fetched) | `EVP_sha256` | Apache-2.0 (REPORTED) | Large | Trixie `libssl-dev` 3.5.7-1~deb13u3 (VERIFIED). **Windows is the problem:** the vcpkg curl default uses Schannel, so OpenSSL would be a new Windows dependency. |

Notes:
- The adapter source signs with Ed25519 over the exact UTF-8 `payload` string, standard base64 with padding, public key a raw 32-byte value, selected by `key_id` (per the adapter source's interface). Any RFC 8032 Ed25519 verify works; libsodium's `crypto_sign_verify_detached` takes the 64-byte signature, message, and 32-byte key. Interop against the service's Node signer is **untested**; add a test vector from the service's signer.
- libsodium requires `sodium_init()` once. Its docs page has a TODO where "Ed25519 compatibility" and "ZIP215/strict verification" notes should be (VERIFIED), so the strictness rules are not documented there; for signatures produced by the operator's own key this does not matter.
- The trixie floor is libsodium 1.0.18. Both functions used here are long-standing libsodium APIs, so they should exist at that floor (REPORTED; the docs page dates only Ed25519ph, "added in libsodium 1.0.12", which is VERIFIED).

**Recommendation: libsodium.** One dependency for both primitives, on every target. If the project prefers zero new runtime dependencies, vendor Monocypher with `monocypher-ed25519.c` and add a vendored public-domain SHA-256 (not researched here; test against NIST vectors).

---

## 3. JSON in C

| | cJSON | yyjson | jsmn |
|---|---|---|---|
| License | MIT | MIT | MIT (copyright Serge Zaitsev; VERIFIED in header) |
| vcpkg | 1.7.19 (feature `utils` optional) | 0.13.0 (features reader/writer on by default) | port exists: version-date 2019-04-27, port-version 2 (VERIFIED; its license field was empty in the manifest) |
| Debian trixie | `libcjson-dev` 1.7.18-3.1+deb13u1 (VERIFIED) | `libyyjson-dev` 0.10.0+ds-1+b1 (VERIFIED) | `libjsmn-dev` 1.1.0-2, header-only (VERIFIED) |
| Fedora | `cjson-devel` 1.7.19-1.fc44 (VERIFIED) | `yyjson-devel` 0.12.0-2.fc44 (VERIFIED) | not confirmed (UNKNOWN) |
| Arch (extra) | `cjson` 1.7.19-1 (VERIFIED) | `yyjson` 0.13.0-1 (VERIFIED) | not checked (UNKNOWN) |
| Raw byte span of a value | **No.** README exposes only error position and parse end (VERIFIED). | **No source offset per value.** `yyjson_doc_get_read_size` returns bytes consumed; `YYJSON_READ_NUMBER_AS_RAW`/`yyjson_get_raw` returns a number's text, not its position (VERIFIED, the adapter source's interface). | **Yes:** each `jsmntok_t` carries `start`/`end` offsets into the input (VERIFIED). |
| Unescapes strings | Yes (README is silent; standard behavior, REPORTED) | Yes (REPORTED) | **No**, tokenizer only; you must unescape yourself (VERIFIED: no decode routine). |
| Other | Cannot hold `\u0000`/embedded NUL; numbers are `double` with `valueint` saturating (VERIFIED). | Explicit length (`yyjson_read_opts`, REPORTED), tracks lengths so embedded NUL is OK (REPORTED). | Needs token array sizing; no tree. |

**The raw-span requirement does not need parser support.** the adapter source's interface says: a plain adapter file is hashed as received (no parsing needed); an envelope's `payload` is itself a JSON string, and "`payload` always ends with `,"adapter":<the stored bytes>}`", the first `,"adapter":` in `payload` is that key, and the stored bytes are everything after it up to but not including the final `}`. So:
1. Parse the envelope (`payload`, `signature`, `key_id`); the parser unescapes `payload`, giving the exact bytes that were signed.
2. Verify Ed25519 over those bytes.
3. `memmem` for `,"adapter":` in the payload bytes and hash the slice from there to the last byte minus one.
4. Then parse `payload` separately for its other fields.
JSON.stringify escapes only `"`, `\`, control characters and lone surrogates, so unescaping the envelope string recovers the original UTF-8 (REASONED from the interface's own description; test against the service's vectors). A parser that rejects embedded NUL (cJSON) is fine because the payload has none, but yyjson's length-carrying strings avoid the question.

The native side probably never needs to parse the adapter body itself (the browser extension does), only cache and hash it.

**Recommendation: yyjson**, vendored (single .c/.h pair, REPORTED) so the trixie floor of 0.10.0 is irrelevant; or cJSON if a "boring, smallest-API, packaged everywhere" choice is preferred. Avoid jsmn: no unescape and no value tree means more code for no gain once the raw-span trick is used.

---

## 4. QR code in C

- **Nayuki QR-Code-generator, C version** (https://github.com/nayuki/QR-Code-generator/tree/master/c): files `qrcodegen.c`, `qrcodegen.h` (VERIFIED listing); **MIT** (VERIFIED in the header and vcpkg's `nayuki-qr-code-generator` 1.8.0 manifest, which exists); latest release **v1.8.0** (VERIFIED, releases page; the page showed "17 Apr" without a year). API (VERIFIED, header fetched):
  ```c
  bool qrcodegen_encodeText(const char *text, uint8_t tempBuffer[], uint8_t qrcode[],
      enum qrcodegen_Ecc ecl, int minVersion, int maxVersion, enum qrcodegen_Mask mask, bool boostEcl);
  int  qrcodegen_getSize(const uint8_t qrcode[]);
  bool qrcodegen_getModule(const uint8_t qrcode[], int x, int y);
  ```
  `qrcodegen_BUFFER_LEN_MAX` is 3918 bytes (VERIFIED, header), so both buffers can be static arrays; no heap. Versions 1-40; `Ecc` LOW/MEDIUM/QUARTILE/HIGH; `Mask` AUTO = -1.
- **libqrencode:** license LGPL-2.1 or later (VERIFIED, README); Debian trixie `libqrencode-dev` 4.1.1-2 (VERIFIED); vcpkg 4.1.1 port-version 3, optional `tool` feature pulls libpng (VERIFIED). Fedora, Arch: not checked (UNKNOWN). Heavier, and LGPL adds a static-linking obligation on Windows.
- **Rendering as SDL rectangles** (design note, not a researched fact): compute `n = qrcodegen_getSize(qr)`, add a quiet zone of at least 4 modules on each side per the QR spec (REPORTED, the standard's rule; not fetched), choose an integer pixel scale `k` so `(n + 8) * k` fits, fill a white rect for the whole code, then fill dark modules black. Use `SDL_RenderFillRects` in one batch or draw once into a target texture with nearest scaling; avoid anti-aliased scaling. Keep dark-on-light even in a dark theme, because many scanners fail on inverted codes (REPORTED).
- For a short device-code URL, a small version (low tens of modules) is expected; exact version for the real URL: UNKNOWN until the URL is fixed.

**Recommendation: vendor Nayuki's C version.**

---

## 5. Native Messaging host protocol

### Framing and limits (all three browsers)
- Messages are JSON, UTF-8, preceded by a 32-bit length **in native byte order** (VERIFIED: Chrome https://developer.chrome.com/docs/extensions/develop/concepts/native-messaging; Edge https://learn.microsoft.com/en-us/microsoft-edge/extensions/developer-guide/native-messaging; Firefox https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/Native_messaging, which says "unsigned 32-bit value").
- **Host to extension: 1 MB** in all three (VERIFIED). The length field in a host reply must not exceed 1024*1024 (Edge troubleshooting text).
- **Extension to host:** MDN says **4 GB** (VERIFIED); the Edge page says **4 GB** (VERIFIED); the **current Chrome page, as fetched twice, says 64 MiB** ("The maximum size of the message sent to the native messaging host is 64 MiB"). That fetch came through the summarizer, but the second pass was targeted at that sentence and returned the same. Treat 64 MiB as the safe upper bound for Chrome and Edge messages (the Edge page is an older copy of Google's text) and keep real messages far smaller. Re-verify by hand.
- **Windows stdio must be binary:** Chrome and Edge docs say set `O_BINARY` with `__setmode` (the pages write the double-underscore name; `_setmode` is the CRT's modern spelling) or `\n` becomes `\r\n` and corrupts framing (VERIFIED). MDN's equivalent is Python's `-u` flag. The more robust route for a C host is `ReadFile`/`WriteFile` on `GetStdHandle(STD_INPUT_HANDLE/STD_OUTPUT_HANDLE)`, which sidesteps the CRT text mode (design inference).

### How each browser launches the host
- **Chrome and Edge:** each host is started "in a separate process", current directory set to the directory containing the host binary. First argument is the caller's origin (`chrome-extension://<id>/`). **On Windows a second argument `--parent-window=<decimal HWND>` is added**, which is 0 when the caller is a service worker (VERIFIED, Chrome and Edge pages). Linux/macOS get one argument.
- **Firefox:** two arguments: the complete path to the host manifest, and (since Firefox 55) the add-on ID. No origin string, no parent window handle (VERIFIED, MDN).
- **The host manifest has no argument-list key.** The keys are `name`, `description`, `path`, `type` (only `stdio`), and `allowed_origins` (Chrome/Edge) or `allowed_extensions` (Firefox) (VERIFIED, Edge and MDN key tables). So an app cannot add its own flag; a same-exe host must recognize its caller by argv: Chromium has `argv[1]` starting `chrome-extension://`; Firefox has `argv[1]` ending `.json` and `argv[2]` an add-on ID.
- Chromium launches `.exe` hosts directly since **Chromium 113**; earlier it wrapped everything in `cmd.exe`, which broke termination (VERIFIED, https://textslashplain.com/2023/03/16/improving-native-message-host-reliability-on-windows/).

### Lifetime of the host
- **One process per `connectNative` port**, kept "until the port is destroyed" (Chrome, Edge, MDN: "until the extension calls `Port.disconnect()` or the page that connected to it is closed"). `sendNativeMessage` starts a new process per message (VERIFIED).
- **Chromium on port close:** closes the stdin and stdout pipes, then if the process has not exited it terminates it about **2 seconds later** (Windows; `TerminateProcess`), but **only if Chromium itself is still running** (VERIFIED, textslashplain post, an Edge/Chromium engineer's write-up; Chromium source files for this could not be fetched). The Linux/macOS equivalent is UNKNOWN from a primary source; assume SIGPIPE/EOF and exit on stdin EOF.
- **Firefox on port close:** on Linux/macOS it sends SIGTERM, then SIGKILL; on Windows it puts the host in a **Job object and kills the job** (VERIFIED, MDN). A host that wants children to outlive it must create them with `CREATE_BREAKAWAY_FROM_JOB`. Matters for StreamFlex: if the host process is also the launcher (below), killing it kills the launcher.
- Hosts should exit on stdin EOF. A Windows trap: closing the stdin handle while a read is pending blocks, so do not `CloseHandle` it; the OS closes it at exit (REPORTED, same post).

### stderr
- Chrome page: "write to `stderr`" for debug output (VERIFIED). Edge: output to stderr is written to the browser's error log (VERIFIED). Firefox: "the browser will redirect it to the browser console" (VERIFIED).

### Manifest registration (verified locations)
| Browser | Windows | Linux |
|---|---|---|
| Chrome | `HKCU\SOFTWARE\Google\Chrome\NativeMessagingHosts\<name>` or HKLM; 32-bit view checked first | system `/etc/opt/chrome/native-messaging-hosts/`; user `~/.config/google-chrome/NativeMessagingHosts/` |
| Edge | `HKCU\SOFTWARE\Microsoft\Edge\NativeMessagingHosts\<name>`, then Chromium, then Google\Chrome keys as **fallbacks**; HKCU before HKLM | system `/etc/opt/edge/native-messaging-hosts`; user `~/.config/microsoft-edge/NativeMessagingHosts` |
| Firefox | `HKCU\SOFTWARE\Mozilla\NativeMessagingHosts\<name>` or HKLM; Wow6432Node view first since Firefox 64 | system `/usr/lib/mozilla/native-messaging-hosts/` or `/usr/lib64/mozilla/native-messaging-hosts/`; user `~/.mozilla/native-messaging-hosts/` |

All from the three pages above (VERIFIED). Other facts: `path` may be relative to the manifest on Windows, must be absolute on Linux; `name` is lowercase alphanumerics, underscores and dots (Chromium) and the file is `<name>.json`; Edge reads only the manifest from the **first registry location found**, so both store IDs must be in `allowed_origins` if the extension is in both stores; `allowed_origins` takes no wildcards (VERIFIED).

### Same executable as the SDL GUI app?
- **Possible, with design constraints.** The browser spawns `path` with the fixed arguments above, stdin/stdout pipes inherited. Constraints: (1) argv sniffing as above; (2) a Windows **GUI-subsystem** executable gets no console and may not initialize CRT stdio sensibly, so use `GetStdHandle` raw I/O (design inference, UNKNOWN whether SDL's entry shim interferes; test); a console-subsystem build avoids that but Windows may flash a console window when a browser launches it (UNKNOWN whether Chromium creates hosts with no window; the pages do not say); (3) Firefox's job-kill and Chromium's 2-second terminate would kill a host that is also the long-lived launcher, so a **separate short-lived host process that talks to the running launcher over a named pipe/Unix socket** is the safer design; (4) the host must exit on stdin EOF.
- **Recommendation:** ship the host as the same binary in a headless "native-host" mode (or a tiny sibling exe), relaying to the resident launcher; never let the browser-spawned process be the launcher itself.

---

## 6. SDL3 hints and APIs

Versions are from the `release-3.2.x` header text (VERIFIED, https://raw.githubusercontent.com/libsdl-org/SDL/release-3.2.x/include/SDL3/SDL_hints.h) and `WhatsNew.txt` (headings 3.4.0, 3.2.22, 3.2.16, 3.2.10, 3.2.4, 3.2.0 on main; the `main` branch header reads 3.5.0, i.e. a development version).

### `SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS`
- Exists, **since SDL 3.2.0**, default `0`, "can be set anytime" (VERIFIED).
- **Mechanism** (VERIFIED, `release-3.2.x/src/joystick/SDL_joystick.c`): `SDL_PrivateJoystickShouldIgnoreEvent` returns "ignore" when the hint is off **and** `SDL_HasWindows() && SDL_GetKeyboardFocus() == NULL`. The filter sits in the shared `SDL_SendJoystick{Axis,Ball,Hat,Button}` path, so it applies to every driver, evdev included. With the hint off and an SDL window existing but unfocused: button presses are dropped (releases pass), axis motion away from center is dropped (return to center passes), hats only pass centered. **If the app has no SDL windows at all, events pass regardless.**
- **Windows backends** (VERIFIED, SDL source comments):
  - XInput has no focus check in `SDL_xinputjoystick.c` and "continues to provide data in the background".
  - RawInput registers with `RIDEV_INPUTSINK` ("Receive messages when in background") and the file says Windows.Gaming.Input and DirectInput "do not get inputs from XBox One controllers when not in the foreground".
  - WGI's update code treats all-zero axes as focus loss.
  - Which backend SDL 3.2.10 actually selects by default for a given pad (HIDAPI vs RawInput vs XInput vs WGI): **UNKNOWN**; test with the target controllers while the browser is foreground.
- **Linux evdev:** the hint filter above is the only SDL-side gate found; whether the evdev driver otherwise reports in the background was not confirmed by test (UNKNOWN beyond the source reading). Device access needs permission (section 9).
- **Caveat for the design:** the filter only drops events while an SDL window exists without keyboard focus; if the launcher keeps a hidden or minimized window, set the hint.

### `SDL_HINT_FORCE_RAISEWINDOW`
- Exists, **since 3.2.0**, default `0`; "At present, this is only an issue under MS Windows" (VERIFIED).
- Windows implementation (VERIFIED, `release-3.2.x/src/video/windows/SDL_windowswindow.c`, `WIN_RaiseWindow`): when set, `ShowWindow(SW_RESTORE)`, `AttachThreadInput` to the foreground window's thread, toggles `HWND_TOPMOST` then `HWND_NOTOPMOST`, then `SetForegroundWindow`, then detaches and calls `SetFocus`/`SetActiveWindow`. Also the separate `SDL_HINT_WINDOW_ACTIVATE_WHEN_RAISED` (default true) controls whether it activates at all.
- Microsoft's own rules for `SetForegroundWindow` (VERIFIED, https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setforegroundwindow): allowed only if, among other things, the foreground lock timeout expired, **the caller is the foreground process, was started by it, there is no foreground window, or it received the last input event**; otherwise Windows flashes the taskbar button. "It is possible for a process to be denied ... even if it meets these conditions." After the browser is closed there is no foreground window, which is the favorable case; raising over a still-running browser is the unfavorable one.
- Behavior specifically on **Windows 11** beyond this documentation: UNKNOWN (not researched; test).

### `SDL_RaiseWindow`
- **Since 3.2.0**; "Request that a window be raised above other windows and gain the input focus"; result is "subject to desktop window manager policy" (VERIFIED, https://wiki.libsdl.org/SDL3/SDL_RaiseWindow and header).
- **Wayland** (VERIFIED, `release-3.2.x/src/video/wayland/SDL_waylandwindow.c`): `Wayland_RaiseWindow` calls the same path as FlashWindow, `Wayland_activate_window`, which gets an `xdg_activation_v1` token, sets the serial from `input->last_implicit_grab_serial` on the seat, commits, and then calls `xdg_activation_v1_activate`. The source comment says the protocol's general policy "is to go with #2 [flash the titlebar] unless the client can prove ... that raising the window will not be malicious." Because the serial is the app's own last input grab, a launcher that has not just received input on that seat cannot prove it. (Inference: GNOME will likely only flash. The SYNTHESIS already treats GNOME focus-stealing prevention as verified; hence "Home closes the browser" rather than raising.) SDL also reads `XDG_ACTIVATION_TOKEN` from the environment at first show (VERIFIED, from the fetch note).
- The GlobalShortcuts portal's `Activated` signal can carry an `activation_token` option (VERIFIED, portal docs), which is a route to a token a launcher could pass to `xdg_activation`; whether SDL exposes setting it: UNKNOWN.

### HDR properties and event
- `SDL_PROP_DISPLAY_HDR_ENABLED_BOOLEAN` ("true if the display has HDR headroom above the SDR white point"), documented under `SDL_GetDisplayProperties`, **since 3.2.0** (VERIFIED, `release-3.2.x/include/SDL3/SDL_video.h`).
- Per-window: `SDL_PROP_WINDOW_HDR_ENABLED_BOOLEAN`, `SDL_PROP_WINDOW_SDR_WHITE_LEVEL_FLOAT`, `SDL_PROP_WINDOW_HDR_HEADROOM_FLOAT`, documented under `SDL_GetWindowProperties`, **since 3.2.0** (VERIFIED). (An earlier guess of `SDL_PROP_DISPLAY_SDR_WHITE_POINT_FLOAT` / `SDL_PROP_DISPLAY_HDR_HEADROOM_FLOAT` was **ABSENT** in both 3.2.x and main.)
- **Change event:** `SDL_EVENT_WINDOW_HDR_STATE_CHANGED` ("Window HDR properties have changed") is in the 3.2.x `SDL_events.h` enum, whose enum-level note is "available since SDL 3.2.0" (VERIFIED). There is **no display-level HDR change event**: the `SDL_EVENT_DISPLAY_*` list is ORIENTATION, ADDED, REMOVED, MOVED, DESKTOP_MODE_CHANGED, CURRENT_MODE_CHANGED, CONTENT_SCALE_CHANGED (VERIFIED), plus `SDL_EVENT_DISPLAY_USABLE_BOUNDS_CHANGED` added in 3.4.0 (VERIFIED, WhatsNew).
- 3.4.0 HDR-related news is only a renderer item ("Added support for YUV textures and HDR colorspaces to the GPU 2D renderer"); no new display property (VERIFIED, WhatsNew).
- **Per-platform behavior of these properties** (Windows, Wayland, X11): UNKNOWN from this pass. Note that on a window that exists in the launcher, these report the launcher's own window; the browser's HDR state is separate.

---

## 7. Windows process supervision with Job Objects

- **Nested jobs exist from Windows 8 / Server 2012**; earlier a process could be in only one job (VERIFIED, https://learn.microsoft.com/en-us/windows/win32/procthread/nested-jobs and the job-objects page).
- **Nesting rule:** a process already in a job that is added to another becomes nested "by default if the system can form a valid job hierarchy **and neither job sets UI limits** (`JobObjectBasicUIRestrictions`)" (VERIFIED). **So do not set UI restrictions on StreamFlex's job.** The rule for the browser's own jobs is the same: Chromium's sandbox sets UI restrictions plus an active-process limit of 1 on **each renderer's own job** (REPORTED, Chromium sandbox design doc via a search summary; the doc fetch returned 503). It is a documented Chromium design that renderers work when the broker is already inside a job on Windows 8+ (REPORTED, 2014 Chromium code-review snippets: "Disallow breakaway from job on Windows 8" because Windows 8 supports nested jobs). I could not read the current source, so whether a UI-limit child job nests under a parent job in practice: **UNKNOWN**; test Chrome and Edge launched inside a job.
- **Inheritance and breakaway** (VERIFIED, job-objects and nested-jobs pages): by default a child created with `CreateProcess` joins every job in the parent's chain. It escapes only if the immediate job has `JOB_OBJECT_LIMIT_BREAKAWAY_OK` **and** the creator passed `CREATE_BREAKAWAY_FROM_JOB`, or if the job has `JOB_OBJECT_LIMIT_SILENT_BREAKAWAY_OK`. With neither set, "the tool can monitor the entire process tree", and a child's `AssignProcessToJobObject` calls fail if they cannot nest. Processes started through other mechanisms are **not** children (the page names `Win32_Process.Create`); COM/service/Task Scheduler launches (updaters, elevation services) are outside any job (inference).
- **Notifications:** messages go to the completion port of **every job in the parent chain**, so a child job need not have its own port; `JOB_OBJECT_MSG_ACTIVE_PROCESS_ZERO` means the active process count reached 0; messages "are intended only as notifications and their delivery ... is not guaranteed" except for notification-limit messages; associate the port when the job is inactive to avoid missed messages (VERIFIED, https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-jobobject_associate_completion_port). That makes the completion port a good primary signal and the main-PID wait a backstop, as SYNTHESIS §4.10 already says.
- **Termination:** terminating a job terminates its child jobs; `JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE` on a nested job kills the job and its children when the last handle closes (VERIFIED). Set it, so a crashing launcher cannot leave a fullscreen browser behind.
- **Chrome/Edge setting `JOB_OBJECT_LIMIT_BREAKAWAY_OK` on their own jobs:** UNKNOWN (not found in any fetched source). **Edge Startup Boost/background mode keeping `msedge.exe` alive**, and Chromium's single-instance hand-off (a second launch against an already-running profile can exit after handing off): not verified here; a private `--user-data-dir` per service reduces both (REPORTED, SYNTHESIS).
- **Firefox on Windows:** with `-wait-for-browser` the launcher process puts itself and the browser process into a **job** (bug 1740619, VERIFIED, https://bugzilla.mozilla.org/show_bug.cgi?id=1740619), so StreamFlex's job has to nest that one; this is the nested-job case above and should work as long as StreamFlex sets no UI limits. **Regression bug 1859445**: with that switch, the browser's *children* were also in the job and were killed on browser exit unless spawned with breakaway; fixed in **Firefox 120** ("wontfix" for 118, 119 and ESR 115) (VERIFIED, https://bugzilla.mozilla.org/show_bug.cgi?id=1859445). Which Firefox builds StreamFlex will see on Windows: release channel, so the fix applies (inference).

---

## 8. Trusted key injection

### Windows `SendInput`
- "This function is subject to UIPI. Applications are permitted to inject input only into applications that are at an **equal or lesser integrity level**", and it fails when blocked by UIPI with neither the return value nor `GetLastError` saying so (VERIFIED, https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-sendinput). A zero return can also mean "input was already blocked by another thread" (same page). It does not reset keyboard state: held keys interfere (same page).
- A medium-integrity launcher into a medium-integrity browser is allowed. **Does `--app=` Edge run at medium?** No source fetched states Edge's integrity level for any launch mode; the expectation is that the browser process runs at the launching user's integrity (medium unless the launcher is elevated), and `--app` only changes window chrome (REASONED, UNKNOWN as a documented fact). The risk case is an **elevated** launcher: a high-integrity launcher can still inject into medium, so the practical failure is the reverse (a medium launcher against an elevated browser).
- **Do SendInput keys arrive with `isTrusted === true` in the page?** **UNKNOWN**: no primary source confirms it; a search found only indirect, secondary evidence (a W3C bug thread listing SendInput as one way to emulate input; a project asserting trusted results for CDP key dispatch, which is different). Expectation: yes, because the browser sees an ordinary OS keyboard event, but a Phase 0 test with a logging listener and a user-activation-gated action (`requestFullscreen`) settles it.

### Linux `/dev/uinput`
- **Creation sequence** (VERIFIED, https://www.kernel.org/doc/html/latest/input/uinput.html): write to `/dev/uinput` (or `/dev/input/uinput`); `UI_SET_EVBIT`, `UI_SET_KEYBIT`, `UI_DEV_SETUP`, `UI_DEV_CREATE`; the kernel example pauses after creation "so that userspace has time" to notice the new device before events are sent. The kernel page says nothing about permissions.
- **Permissions: nothing in systemd grants it by default.** systemd's `70-uaccess.rules` input rule is for **joysticks only** (`SUBSYSTEM=="input", ENV{ID_INPUT_JOYSTICK}=="?*", TAG+="uaccess"`), `50-udev-default.rules` sets `SUBSYSTEM=="input", GROUP="input"` and has no uinput rule (VERIFIED, systemd main branch; I could not fetch `70-uaccess.rules` itself, only `.rules.in`; absence of a uinput line was reported for both). A known-good rule from a shipping project (Solaar; VERIFIED, `rules.d-uinput/42-logitech-unify-permissions.rules`):
  ```
  KERNEL=="uinput", SUBSYSTEM=="misc", TAG+="uaccess", OPTIONS+="static_node=uinput"
  ```
  `static_node=uinput` makes udev create the node at boot (needed when the module loads late), and `uaccess` grants an ACL to the user on the active local seat. The kernel module may also need loading at boot (`/etc/modules-load.d/uinput.conf` with `uinput`; REPORTED, third-party guide).
- **Debian, Ubuntu, Fedora, Arch differences:** the rule above works identically on all of them because it is udev/systemd-logind, not distro policy (REASONED; not tested). An alternative is `GROUP="input", MODE="0660"` and adding the user to the `input` group (REPORTED, Arch forums and a xremap guide); that is a worse default for an appliance that auto-logs in. `uaccess` needs a logind seat session, so a service launched outside a user session will not get the ACL (REASONED).
- **Do GNOME and KDE Wayland deliver uinput keys to the focused app?** A uinput device is a kernel input device like any other; the compositor reads it through libinput and routes it to the focused client (REASONED, consistent with xdotool-free tools such as ydotool/xremap working on Wayland; **not confirmed by a primary source in this pass**). Test on GNOME and KDE.

---

## 9. Low-level Home hook

### Windows
- **`WH_KEYBOARD_LL` timeout:** the hook proc should finish within the `LowLevelHooksTimeout` registry value (`HKCU\Control Panel\Desktop`, milliseconds). On timeout the message is passed on, and "**on Windows 7 and later, the hook is silently removed** without being called. There is no way for the application to know whether the hook is removed." From **Windows 10 1709** the maximum allowed is 1000 ms (a larger value falls back to 1000). The thread that installed it must have a message loop; Microsoft recommends doing the work on a worker thread and returning at once, and says raw input is usually better for observing keyboard input (VERIFIED, https://learn.microsoft.com/en-us/windows/win32/winmsg/lowlevelkeyboardproc). The callback runs before `GetAsyncKeyState` updates. Re-arming on a timer (SYNTHESIS) is the right workaround; no API reports removal.
- **`RegisterHotKey` limits** (VERIFIED, https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-registerhotkey): the parameter docs do not say a modifier is required, so a bare key is not ruled out (not tested); fails if the key combination is already registered by another hot key; **"Keyboard shortcuts that involve the WINDOWS key are reserved for use by the operating system"**; F12 is reserved for debuggers; `MOD_NOREPEAT` is not supported on Vista; it cannot be associated with a window made by another thread; "some pre-existing, default hotkeys registered by the OS (such as PrintScreen) may be overridden ... when one of the app's windows is in the foreground". The hot key is delivered as `WM_HOTKEY` to the registering thread. The remote's Home as an HID consumer-control code ("AC Home", usage page 0x000C, usage 0x0223, scan code 0xE032) is listed in Microsoft's scan-code table (VERIFIED, https://learn.microsoft.com/en-us/windows/win32/inputdev/about-keyboard-input); whether it reaches `RegisterHotKey` as a virtual key (`VK_BROWSER_HOME`): UNKNOWN, test.
- Chrome/Edge's media-key hook ordering versus a launcher hook: UNKNOWN (SYNTHESIS §9).

### Linux
- **GlobalShortcuts portal (`org.freedesktop.portal.GlobalShortcuts`)** (VERIFIED, https://flatpak.github.io/xdg-desktop-portal/docs/doc-org.freedesktop.portal.GlobalShortcuts.html and the XML on GitHub): methods `CreateSession`, `BindShortcuts` ("An application can only attempt to bind shortcuts of a session once"), `ListShortcuts`, `ConfigureShortcuts` (added in interface **version 2**); signals `Activated`/`Deactivated` carry a millisecond timestamp with undefined base and optional options such as an `activation_token`, plus `ShortcutsChanged`. `preferred_trigger` is optional; the portal typically shows a dialog where the user configures shortcuts, so the trigger is a preference, not a guarantee (page does not say "suggestion" outright). The page does not say which xdg-desktop-portal release added it (UNKNOWN here).
- **GNOME:** "With GNOME 48, it is now possible for apps to register system-wide global shortcuts ... fully supported by GNOME 48" (VERIFIED, https://release.gnome.org/48/developers/). Users on GNOME 48 reported no permission prompt and no obvious settings place for some apps (REPORTED, GNOME Discourse and Fedora forum threads).
- **KDE:** supported through `kglobalacceld`; a forum post says "for years" (REPORTED, 1Password community); the **first Plasma version is UNKNOWN**. One report claims that since xdg-desktop-portal-kde 6.7.4 a shortcut is active only if the app calls `BindShortcuts` in the current session (REPORTED, unverified), which matches the "one `BindShortcuts` per session" rule: call it at every launcher start.
- **Which GNOME and xdg-desktop-portal versions the target distros ship** (trixie, Raspberry Pi OS trixie, Ubuntu 25.10, Fedora 44, Arch): not checked (UNKNOWN). The portal needs both a GNOME 48+ (or KDE) backend and a portal frontend that has the interface.
- **Reading the remote's evdev device directly:** `/dev/input/event*` nodes are group `input` via `SUBSYSTEM=="input", GROUP="input"` in `50-udev-default.rules` (VERIFIED), with no uaccess ACL for keyboards (the uaccess rule is joystick-only), so a non-root launcher needs `input` group membership or its own udev rule granting `uaccess` to the specific remote's `ID_VENDOR_ID/ID_MODEL_ID` (REASONED). **`EVIOCGRAB`** (VERIFIED, Linux `drivers/input/evdev.c`): a nonzero argument grabs the device **exclusively**; if a grab already exists it returns **`-EBUSY`**; releasing by a non-holder returns `-EINVAL`; the grab is dropped when the file is closed (`evdev_release` calls `evdev_ungrab`). Grabbing means the compositor and the browser stop receiving that device's events, which is the wrong default for Home-only handling: read without a grab and let events also reach the browser, or grab only if a remote key must be swallowed. A grab can fail if another process (a compositor with the device) already holds it. For consumer-control "Home", the evdev code is `KEY_HOMEPAGE` (172) or `KEY_HOME` depending on the device (REPORTED, not confirmed; the kernel event-codes page fetched did not list it).

---

## 10. Portable ZIP, no installer: per-user policy and UAC (added at the lead's request)

### (a) Can a standard user write `HKCU\Software\Policies\{Google\Chrome, Microsoft\Edge, Mozilla\Firefox}`?
- **UNKNOWN from any primary source.** No Microsoft page fetched documents the default ACL on `HKCU\Software\Policies`. What the search found:
  - Microsoft Q&A: a user account has full read/write on its own HKCU (REPORTED, secondary).
  - Helge Klein's listing of Windows 7 default HKCU permissions: the user has full access at the HKCU root; the listing was truncated and does not show the Policies subkey (REPORTED).
  - One forum poster (Advanced Installer) says writing under `HKCU\Software\Policies` "is not allowed for a normal user", reasoning that Group Policy owns the key (REPORTED, anecdotal, unverified).
- Design consequence: this is a 30-second test on a stock Windows 11 Home and Pro box as a standard user (`reg add "HKCU\Software\Policies\Google\Chrome" /v X /t REG_DWORD /d 1`, then check the exit code and `reg query`). Do it in Phase 0 before relying on it. Also expect that on a domain-joined or MDM-managed machine, a Group Policy refresh rewrites the user-policy hive and can wipe hand-written values (REASONED, not sourced).

### (b) Do the browsers honor HKCU policy, and does a store-hosted force-install work on a non-domain machine?
- **Chromium (Chrome and, by shared code, Edge): HKCU is read as user-scope policy.** `policy_loader_win.cc` iterates `{POLICY_SCOPE_MACHINE, HKEY_LOCAL_MACHINE}` and `{POLICY_SCOPE_USER, HKEY_CURRENT_USER}` and reads the Chrome policy key from each; the file has no condition that skips HKCU (VERIFIED, https://raw.githubusercontent.com/chromium/chromium/main/components/policy/core/common/policy_loader_win.cc). Domain-join and enterprise checks in that file feed only UMA histograms. Edge's registry path is documented only as `SOFTWARE\Policies\Microsoft\Edge\...` with no hive named (VERIFIED, https://learn.microsoft.com/en-us/deployedge/microsoft-edge-policies/extensioninstallforcelist), and Edge is Chromium-based, so HKCU working there is **REPORTED by inference, not read in Edge's own source**. Precedence: machine scope overrides user scope (VERIFIED, Google's policy-precedence doc, https://support.google.com/chrome/a/answer/9037717).
- **Firefox: HKCU is read, HKLM wins.** `WindowsGPOPoliciesProvider` reads `ROOT_KEY_CURRENT_USER` first and then `ROOT_KEY_LOCAL_MACHINE` ("Machine policies override user policies, so we read user policies first and then replace them if necessary") (VERIFIED, https://raw.githubusercontent.com/mozilla/gecko-dev/master/toolkit/components/enterprisepolicies/EnterprisePoliciesParent.sys.mjs). The key read is `SOFTWARE\Policies\Mozilla\<app name>`. Firefox's documented `ExtensionSettings` registry key form (value vs subkey) remains the open item from SYNTHESIS §9; not resolved here.
- **`ExtensionInstallForcelist` on a non-domain Windows machine:**
  - Chrome: the policy definition says off-store apps and extensions "can be force-installed only if" the device is joined to an AD or Azure AD domain or enrolled in Chrome Enterprise Core; the restriction is on **off-store** items; `per_profile: true`, `dynamic_refresh: true`; supported from Chrome 9 (VERIFIED, https://raw.githubusercontent.com/chromium/chromium/main/components/policy/resources/templates/policy_definitions/Extensions/ExtensionInstallForcelist.yaml). So a Chrome Web Store item is permitted on an unmanaged machine, consistent with SYNTHESIS §4.4. The loader also has a `FilterSensitivePolicies` step that "separately marks non-allowlisted force-install and extension-settings entries" (VERIFIED from the fetch summary of `policy_loader_common.cc`; the allowlist contents and the condition that triggers it were not visible, UNKNOWN). Treat that as the mechanism behind the off-store rule and test with the real extension.
  - **Edge is stricter in wording:** "For Windows instances not joined to a Microsoft Active Directory domain, forced installation is limited to apps and extensions listed in the **Microsoft Edge Add-ons website**" (VERIFIED, same Edge page). So on non-domain Windows the Edge force-install requires an **Edge Add-ons listing** of the extension; a Chrome Web Store listing may not satisfy it (the Edge docs' own CWS example is in a different article, https://learn.microsoft.com/en-us/deployedge/microsoft-edge-manage-extensions-policies, and does not state non-domain behavior). Whether the CWS update URL works on non-domain Edge: UNKNOWN; test.
  - Edge force-install does not apply to InPrivate (VERIFIED, same page).
  - Net: "store-hosted force-install works from HKCU on non-domain Home/Pro" is **supported by the source and policy docs for Chrome, and by inference for Edge, but not demonstrated**; run it on a clean Windows 11 Home VM as a Phase 0 item.
- **Native-messaging policies from HKCU:** same loader, so the Chrome-side `NativeMessagingAllowlist`/`NativeMessagingBlocklist` work from user scope (inference). For Edge, `NativeMessagingAllowlist` is `SOFTWARE\Policies\Microsoft\Edge\NativeMessagingAllowlist\1,2,...` (VERIFIED, https://learn.microsoft.com/en-us/deployedge/microsoft-edge-policies/nativemessagingallowlist). They are only needed if a deny-all blocklist exists; by default "All native messaging hosts are allowed" (same page), so StreamFlex normally writes none.

### (c) If HKLM is required: can a `runas` UAC prompt be answered with a TV remote?
- **What is documented** (VERIFIED, https://learn.microsoft.com/en-us/windows/security/application-security/application-control/user-account-control/how-it-works):
  - A **standard** user gets the **credential prompt** (must type an administrator's name and password); an **administrator in Admin Approval Mode** gets the **consent prompt** (Yes/No). A TV user is usually the latter, but a standard-user account would require typing a password, which is a dead end for a remote.
  - Both prompts are shown on the **secure desktop** by default ("Only Windows processes can access the secure desktop"; the user desktop is dimmed; "When the user selects Yes or No, the desktop switches back"). Clipboard paste does not work on it.
  - Prompts are color-coded: gray for Windows or verified-publisher binaries, **yellow for unsigned or untrusted** (an unsigned portable ZIP helper gets the yellow warning prompt).
- **Keyboard navigability, Tab/arrow order, and default button: UNKNOWN from any Microsoft source.** No page found documents it. Third-party only: a community AutoHotkey project (UAC-Focus) is built around **Alt+Y / Alt+N** and says the prompt window is always in focus and "easy to control using the keyboard" (REPORTED, search summary of https://github.com/lightproof/UAC-Focus; the README itself returned 404 on my fetch). A Microsoft Q&A answer used UI Automation, not keys (REPORTED). **Which button has default focus is UNKNOWN here**; from general use I expect "Yes", but that is memory, not a source. Test with the real remote (arrows, Tab, Enter) on Windows 11 for both prompt types.
- **`ShellExecuteExW` with `runas`:** the page documents `ERROR_CANCELLED` ("the user canceled the request") for a declined prompt (VERIFIED, https://learn.microsoft.com/en-us/windows/win32/api/shellapi/nf-shellapi-shellexecuteexw); handle it. The page says nothing about the verb's UAC behavior itself.
- **Recommendation:** make HKCU policy and HKCU NM registration the default path with no prompt; offer a one-time, explicit "needs admin" step (keyboard/mouse) only for anything that truly needs HKLM, and do not put it on the 10-foot critical path. If the remote's keys do not reach the secure desktop dialog, the user is stranded with no way to dismiss it (UNKNOWN until tested with the real remote).

### (d) NM host registration under HKCU without elevation, all three browsers
- **Chrome:** the installer registers the manifest path under `SOFTWARE\Google\Chrome\NativeMessagingHosts\<name>` "in either HKEY_LOCAL_MACHINE or HKEY_CURRENT_USER" (VERIFIED, https://developer.chrome.com/docs/extensions/develop/concepts/native-messaging).
- **Edge:** `HKEY_CURRENT_USER\SOFTWARE\Microsoft\Edge\NativeMessagingHosts\<name>` is first in the documented search order, then HKCU Chromium and HKCU Google\Chrome keys as fallbacks, then HKLM (VERIFIED, https://learn.microsoft.com/en-us/microsoft-edge/extensions/developer-guide/native-messaging). **Policy `NativeMessagingUserLevelHosts`**: "If you set this policy to Enabled or leave it unset, Microsoft Edge can use native messaging hosts installed at the user level"; Disabled restricts to system-level hosts (VERIFIED, https://learn.microsoft.com/en-us/deployedge/microsoft-edge-policies/nativemessaginguserlevelhosts). Chrome has a policy of the same name, not fetched (UNKNOWN text).
- **Firefox:** `HKEY_CURRENT_USER\Software\Mozilla\NativeMessagingHosts\<name>` or HKLM; the Wow6432Node view is checked first since Firefox 64 (VERIFIED, https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/Native_manifests).
- The key creation itself needs no elevation because it is the caller's own hive; the one caveat is the same ACL question as (a) for `Software\Policies`, which does not apply to `Software\<Vendor>\NativeMessagingHosts` (REASONED; NM keys are ordinary per-user keys, and installers for NM hosts routinely write them as the user, REPORTED not sourced).

---

## What I could not fetch or confirm

- (Section 10) The default ACL on `HKCU\Software\Policies`; Edge's own source for HKCU policy; whether Edge on a non-domain machine accepts a Chrome Web Store ID; the allowlist contents behind Chromium's sensitive-policy filter; UAC consent dialog keyboard navigation and default-button focus (no Microsoft source; the one third-party repo README returned 404); the Firefox `ExtensionSettings` registry shape.

- Chromium source for `native_message_process_host.cc` and `native_process_launcher*.cc` (404/503 on every path tried): Chromium's port-close behavior comes from a 2023 Edge/Chromium engineer's blog (secondary-primary), and Linux behavior is unconfirmed.
- Chromium's sandbox design doc (503): the per-renderer job with UI limits and active-process limit of 1 is from search summaries and 2014 code-review snippets.
- Whether libcurl reads Windows proxy settings automatically (not stated by the FAQ or the proxy docs).
- Fedora's libcurl TLS backend, Fedora and Arch jsmn, Raspberry Pi OS and Ubuntu 25.10 package versions of every library above.
- Edge/Chrome integrity level under `--app=`, and any use of `JOB_OBJECT_LIMIT_BREAKAWAY_OK` by Chrome or Edge themselves.
- `isTrusted` for SendInput keys; GNOME/KDE Wayland delivery of uinput keys; KDE's first version with the GlobalShortcuts portal.
- Per-platform behavior (Windows/Wayland/X11) of SDL's HDR properties and event; SDL's default Windows joystick backend for a given pad at 3.2.10; Windows 11's foreground rules beyond Microsoft's SetForegroundWindow page.
- `SDL_HINT_JOYSTICK_RAWINPUT` default: the summarizer reported "0 (default)", which conflicts with my recollection; left out of the conclusions.
- Monocypher in vcpkg (path `ports/monocypher` is 404; a differently named port is possible).
- Binary size of a static vcpkg libcurl, libsodium or yyjson build.
