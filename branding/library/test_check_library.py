"""Unit tests for check-library.py's brand and listing checks. Needs Pillow."""
import importlib.util
import pathlib
import shutil
import tempfile
import unittest

from PIL import Image

HERE = pathlib.Path(__file__).resolve().parent
_spec = importlib.util.spec_from_file_location("check_library", HERE / "check-library.py")
cl = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(cl)


class CheckLibrary(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.library = pathlib.Path(self.tmp.name) / "library"
        shutil.copytree(cl.libtools.LIBRARY, self.library)

    def tearDown(self):
        self.tmp.cleanup()

    def test_the_real_library_passes(self):
        self.assertEqual(cl.check_files(self.library), [])

    def test_an_unlisted_file_is_reported(self):
        (self.library / "generic" / "stray.svg").write_text("<svg/>", encoding="utf-8")
        self.assertTrue(any("stray.svg is not listed" in p for p in cl.check_files(self.library)))

    def test_a_non_square_brand_is_reported(self):
        brand = next((self.library / "brands").glob("*.png"))
        Image.new("RGBA", (600, 500), (0, 0, 0, 0)).save(brand)
        problems = cl.check_files(self.library)
        self.assertTrue(any(brand.name in p and "square" in p for p in problems), problems)

    def test_a_brand_with_pixels_near_the_chroma_key_is_reported(self):
        # Transparent mode on Windows shows through #010101, and scaling averages pixels a step off it onto
        # it: five opaque pixels within one step in every channel are named and counted
        brand = next((self.library / "brands").glob("*.png"))
        image = Image.new("RGBA", (512, 512), (10, 20, 30, 255))
        for x, color in enumerate(((1, 1, 1), (0, 0, 0), (2, 2, 2), (0, 2, 1), (1, 1, 1))):
            image.putpixel((200 + x, 256), color + (255,))
        image.putpixel((210, 256), (1, 1, 1, 254))   # not fully opaque: not counted
        image.putpixel((211, 256), (3, 1, 1, 255))   # two steps off in red: not counted
        image.save(brand)
        problems = cl.check_files(self.library)
        self.assertTrue(any(brand.name in p and "5 opaque pixel(s) within one step of the chroma key" in p
                            for p in problems), problems)

    def test_a_brand_opaque_in_its_corner_is_reported(self):
        brand = next((self.library / "brands").glob("*.png"))
        Image.new("RGBA", (512, 512), (10, 20, 30, 255)).save(brand)
        problems = cl.check_files(self.library)
        self.assertTrue(any(brand.name in p and "outside the outline" in p for p in problems), problems)


if __name__ == "__main__":
    unittest.main()
