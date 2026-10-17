"""Unit tests for build-library.py: the pinned palette, the contrast rule, and the SVG geometry.
Run: python -m unittest discover -s branding/library -p "test_*.py" -v
"""
import importlib.util
import math
import pathlib
import unittest

HERE = pathlib.Path(__file__).resolve().parent
_spec = importlib.util.spec_from_file_location("build_library", HERE / "build-library.py")
bl = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(bl)

MEDIA = {
    "movies": "#E7364B", "news": "#C85D21", "kids": "#9E7522", "sports": "#7A8223", "music": "#328E23",
    "podcasts": "#308B69", "audiobooks": "#338984", "photos": "#358696", "tv-shows": "#3784A9",
    "live-tv": "#3A7EC9", "games": "#7B68EA", "emulators": "#CF37C7", "radio": "#DF3784",
}


def lightness(hex_color):
    """CIE L* of an sRGB hex color."""
    y = sum(w * c for w, c in zip((0.2126, 0.7152, 0.0722), bl.hex_to_linear(hex_color)))
    f = y ** (1 / 3) if y > 216 / 24389 else (24389 / 27 * y + 16) / 116
    return 116 * f - 16


class Palette(unittest.TestCase):
    def test_media_plates_match_the_spec(self):
        media = [row for row in bl.GENERIC if row[2] == "media"]
        self.assertEqual(len(media), 13)
        for name, _title, group, _glyph, slot, _trim in media:
            self.assertEqual(bl.plate_color(group, slot), MEDIA[name], name)

    def test_one_color_groups_match_the_spec(self):
        self.assertEqual(bl.plate_color("system", None), "#5C646D")
        self.assertEqual(bl.plate_color("general", None), "#07606C")
        self.assertEqual(bl.plate_color("devices", None), "#4D7189")

    def test_media_plates_share_one_lightness(self):
        for hex_color in MEDIA.values():
            self.assertAlmostEqual(lightness(hex_color), 52.0, delta=0.6, msg=hex_color)

    def test_every_plate_keeps_white_readable(self):
        for name, _title, group, _glyph, slot, _trim in bl.GENERIC:
            self.assertGreaterEqual(bl.contrast_with_white(bl.plate_color(group, slot)), 3.0, name)


class Geometry(unittest.TestCase):
    def test_trim_one_centers_the_em_box_at_sixty_percent(self):
        svg = bl.generic_svg("#123456", ["M0 0h1"], 1.0)
        self.assertIn('rx="112.64"', svg)
        self.assertIn('transform="translate(102.400 409.600) scale(0.320000)"', svg)

    def test_trim_scales_about_the_center(self):
        svg = bl.generic_svg("#123456", ["M0 0h1"], 0.5)
        self.assertIn('transform="translate(179.200 332.800) scale(0.160000)"', svg)


class ManifestGuards(unittest.TestCase):
    """inih reads a line into a 200-byte buffer and treats ' ;' as a comment: guard both at build time."""

    def build_with(self, keys):
        import tempfile
        with tempfile.TemporaryDirectory() as tmp:
            ini = pathlib.Path(tmp) / "brands.ini"
            ini.write_text("[probe]\n" + "".join(f"{k} = {v}\n" for k, v in keys.items()), encoding="utf-8")
            return bl.build_outputs(ini)[1]

    def base(self, **changes):
        keys = {"title": "Probe", "group": "video", "owner": "Probe, Inc.", "source": "https://example.com/p",
                "size": "512", "sha256": "0" * 64}
        keys.update(changes)
        return keys

    def test_a_long_manifest_line_is_refused(self):
        errors = self.build_with(self.base(source="https://example.com/" + "x" * 200))
        self.assertTrue(any("longer than" in e and "[probe]" in e for e in errors), errors)

    def test_an_inline_comment_marker_in_a_value_is_refused(self):
        for owner in ("Probe ; Inc.", "Probe #1"):
            errors = self.build_with(self.base(owner=owner))
            self.assertTrue(any("comment" in e and "[probe]" in e for e in errors), (owner, errors))

    def test_a_normal_brand_raises_no_guard_error(self):
        errors = self.build_with(self.base())
        self.assertFalse(any("longer than" in e or "comment" in e for e in errors), errors)


class Table(unittest.TestCase):
    def test_names_groups_and_slots(self):
        names = [row[0] for row in bl.GENERIC]
        self.assertEqual(len(names), 36)
        self.assertEqual(len(set(names)), 36)
        for name, _title, group, _glyph, slot, trim in bl.GENERIC:
            self.assertRegex(name, r"^[a-z0-9-]{1,32}$")
            self.assertIn(group, bl.libtools.GENERIC_GROUPS)
            self.assertEqual(slot is None, group != "media", name)
            self.assertTrue(0.5 <= trim <= 1.5, name)
        slots = sorted(row[4] for row in bl.GENERIC if row[4] is not None)
        self.assertEqual(slots, list(range(13)))


class Aliases(unittest.TestCase):
    """The match = aliases src/iconmatch.c compares an app's name with, beside each icon's name and title."""

    def test_the_manifest_carries_them(self):
        outputs, errors = bl.build_outputs()
        self.assertEqual(errors, [])
        manifest = outputs["icons.ini"]
        self.assertIn("[folder]\ntitle = Folder\ngroup = general\nfile = generic/folder.svg\nmatch = file explorer, files\n",
                      manifest)
        self.assertIn("file = brands/kodi.png\nmatch = xbmc\nowner = Kodi Foundation\n", manifest)
        self.assertIn("file = brands/netflix.png\nowner = Netflix, Inc.\n", manifest)   # No aliases, no line

    def test_every_alias_is_written_as_iconmatch_compares_it(self):
        for name, aliases in bl.MATCH.items():
            for alias in aliases:
                self.assertRegex(alias, bl.ALIAS_RE, name)
                self.assertEqual(bl.normalized(alias), alias, name)

    def test_the_real_aliases_raise_no_problem(self):
        icons = [(row[0], row[1]) for row in bl.GENERIC]
        icons += [(name, keys["title"]) for name, keys in bl.libtools.read_sections(bl.libtools.BRANDS_INI)]
        self.assertEqual(bl.match_problems(icons), [])

    def test_an_alias_for_no_icon_is_refused(self):
        problems = bl.match_problems([("kodi", "Kodi")], {"nosuch": ("x",)})
        self.assertTrue(any("'nosuch'" in p and "no icon has that name" in p for p in problems), problems)

    def test_an_alias_not_written_as_compared_is_refused(self):
        for alias in ("XBMC", "x-b", "two  spaces", " lead", ""):
            problems = bl.match_problems([("kodi", "Kodi")], {"kodi": (alias,)})
            self.assertTrue(any("is not lower case words" in p for p in problems), (alias, problems))

    def test_an_alias_ending_in_a_newline_is_refused(self):
        problems = bl.match_problems([("kodi", "Kodi")], {"kodi": ("xbmc\n",)})
        self.assertTrue(any("is not lower case words" in p for p in problems), problems)

    def test_an_alias_another_icon_has_is_refused(self):
        icons = [("kodi", "Kodi"), ("plex", "Plex"), ("tv-shows", "TV shows")]
        for alias in ("plex", "tv shows"):
            problems = bl.match_problems(icons, {"kodi": (alias,)})
            self.assertTrue(any("is already" in p for p in problems), (alias, problems))
        problems = bl.match_problems(icons, {"kodi": ("media",), "plex": ("media",)})
        self.assertTrue(any("'media' is already kodi's" in p for p in problems), problems)


if __name__ == "__main__":
    unittest.main()
