# Fixture fonts

`noink.ttf` (400 bytes) is the font `tests/test_fontset.c` uses as a fallback that has a glyph and draws no ink
for it. Its cmap maps the narrow no-break space (U+202F), U+E500 in the Private Use Area (which no bundled font
has) and the star (U+2605), each to one empty glyph. Made from the repository's root with the headless harness's
generator, which needs Python 3 and no font tools:

```sh
python3 tests/headless/make_fonts.py blank tests/fixtures/fonts/noink.ttf NoInk "$(printf '\xe2\x80\xaf\xee\x94\x80\xe2\x98\x85')"
```

The same command makes the same bytes (SHA-256 `64435e6db6ac1dc9410b741b5d15db5da794eba9855e885de32643cc2384d52d`).
It is committed rather than made at build time because not every job that runs the unit tests has Python: the
Linux and Raspberry Pi jobs build in a Debian container that installs none.
