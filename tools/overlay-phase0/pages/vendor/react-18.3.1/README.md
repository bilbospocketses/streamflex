# Vendored React 18.3.1

The text probe page (`../../text.html`) loads these two files from the kit's own page server; nothing is fetched
from a network. They are the npm packages' UMD production builds, unchanged. React 18.3.1 is the last release with
UMD builds (React 19 ships none), and a UMD build runs from a plain `<script>` tag with no bundler; its
controlled-input path (`onChange` from the native `input` event) is the one React 19 keeps.

- **Upstream:** https://github.com/facebook/react, npm packages `react` and `react-dom`
- **Version:** 18.3.1
- **License:** MIT, in `LICENSE` (the same file in both packages)
- **Package integrity (npm):** `react-18.3.1.tgz`
  `sha512-wS+hAgJShR0KhEvPJArfuPVN1+Hz1t0Y6n5jLrGQbkb4urgPE/0Rve+1kMB1v/oWgHgm4WIcV+i7F2pTVj+2iQ==`;
  `react-dom-18.3.1.tgz`
  `sha512-5m4nQKp+rZRb09LNH59GM4BxTh9251/ylbKIbpe7TpGxfJ+9kv6BLkLBXIjjspbgbnIBNqlI23tRnTWT0snUIw==`

| File | From | sha256 |
|---|---|---|
| `react.production.min.js` | `react-18.3.1.tgz`, `package/umd/react.production.min.js` | `d949f1c3687aedadcedac85261865f29b17cd273997e7f6b2bfc53b2f9d4c4dd` |
| `react-dom.production.min.js` | `react-dom-18.3.1.tgz`, `package/umd/react-dom.production.min.js` | `35f4f974f4b2bcd44da73963347f8952e341f83909e4498227d4e26b98f66f0d` |
| `LICENSE` | `react-18.3.1.tgz`, `package/LICENSE` | `52412d7bc7ce4157ea628bbaacb8829e0a9cb3c58f57f99176126bc8cf2bfc85` |

`tests/test_pages.py` checks every file here against this table, and that the table names every file.
`.gitattributes` marks the folder `-text`, so no checkout rewrites a line ending.

## Updating

1. `npm view react@<version> dist.tarball dist.integrity` and the same for `react-dom`. A version without a
   `umd/` folder (any 19.x) cannot be vendored this way.
2. Download both tarballs into a new, empty folder outside the repo, check each against its `dist.integrity`
   (`sha512-` and the base64 SHA-512 of the `.tgz`), and extract them.
3. Replace the three files, rename this folder to the new version, and update the page's two `<script src>`
   lines, `serve.ps1` needs no change.
4. Update this file: the version, both integrities and the three sha256 values.
