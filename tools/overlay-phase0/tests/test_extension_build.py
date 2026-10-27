"""Tests for the probe extension's build script."""
import json
import pathlib
import subprocess
import sys
import unittest
import zipfile

KIT = pathlib.Path(__file__).resolve().parent.parent
DIST = KIT / "extension" / "dist"


class ExtensionBuildTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        try:
            subprocess.run([sys.executable, str(KIT / "extension" / "build.py")], check=True, timeout=120)
        except subprocess.TimeoutExpired as exc:
            raise AssertionError(f"HUNG: build.py did not exit within {exc.timeout} s") from exc

    def manifest(self, flavor):
        return json.loads((DIST / flavor / "manifest.json").read_text(encoding="utf-8"))

    def test_both_flavors_share_the_listing_fields(self):
        for flavor in ("chromium", "firefox"):
            m = self.manifest(flavor)
            self.assertEqual(m["manifest_version"], 3)
            self.assertEqual(m["name"], "CPTK Overlay")
            self.assertEqual(m["version"], "0.0.1")
            self.assertEqual(m["permissions"], ["nativeMessaging", "storage"])
            self.assertEqual(m["host_permissions"], ["<all_urls>"])
            self.assertEqual(m["content_scripts"][0]["run_at"], "document_start")
            self.assertTrue((DIST / flavor / "icon128.png").read_bytes().startswith(b"\x89PNG"))

    def test_flavor_specific_fields(self):
        self.assertEqual(self.manifest("chromium")["background"], {"service_worker": "background.js"})
        ff = self.manifest("firefox")
        self.assertEqual(ff["background"], {"scripts": ["background.js"]})
        gecko = ff["browser_specific_settings"]["gecko"]
        self.assertEqual(gecko["id"], "overlay@cptk.app")
        self.assertEqual(gecko["strict_min_version"], "140.0")
        self.assertEqual(gecko["data_collection_permissions"], {"required": ["none"]})

    def test_zips_hold_the_manifest_at_the_root(self):
        for flavor in ("chromium", "firefox"):
            with zipfile.ZipFile(DIST / f"cptk-overlay-{flavor}-0.0.1.zip") as z:
                self.assertIn("manifest.json", z.namelist())
                self.assertIn("content.js", z.namelist())

    def test_no_dynamic_code(self):
        for name in ("content.js", "background.js"):
            text = (KIT / "extension" / "src" / name).read_text(encoding="utf-8")
            self.assertNotIn("eval(", text)
            self.assertNotIn("new Function", text)


if __name__ == "__main__":
    unittest.main()
