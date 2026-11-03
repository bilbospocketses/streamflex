"""Tests for the probe extension's build script and, where node is on PATH, its scripts' behavior."""
import json
import pathlib
import shutil
import subprocess
import sys
import unittest
import zipfile

KIT = pathlib.Path(__file__).resolve().parent.parent
DIST = KIT / "extension" / "dist"
SIM = pathlib.Path(__file__).resolve().parent / "extension_sim.js"
NODE = shutil.which("node")
FLAVORS = ("chromium", "firefox")
FILES = ("background.js", "content.js", "icon128.png", "manifest.json")


def setUpModule():
    try:
        subprocess.run([sys.executable, str(KIT / "extension" / "build.py")], check=True, timeout=120)
    except subprocess.TimeoutExpired as exc:
        raise AssertionError(f"HUNG: build.py did not exit within {exc.timeout} s") from exc


def scripts():
    """Every copy of the two scripts: the sources and both built flavors."""
    dirs = [KIT / "extension" / "src"] + [DIST / flavor for flavor in FLAVORS]
    return [(d / name, (d / name).read_text(encoding="utf-8")) for d in dirs for name in ("content.js", "background.js")]


class ExtensionBuildTest(unittest.TestCase):
    def manifest(self, flavor):
        return json.loads((DIST / flavor / "manifest.json").read_text(encoding="utf-8"))

    def test_both_flavors_share_the_listing_fields(self):
        for flavor in FLAVORS:
            m = self.manifest(flavor)
            self.assertEqual(m["manifest_version"], 3)
            self.assertEqual(m["name"], "CPTK Overlay Probe")
            self.assertLessEqual(len(m["description"]), 132, "Chrome's description limit")
            self.assertIn("measurement probe", m["description"])
            # The gate is per tab, by the launch code; there is no profile check.
            self.assertIn("launch code", m["description"])
            self.assertNotIn("profile", m["description"])
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
        self.assertEqual(gecko["id"], "overlay-probe@cptk.app")
        self.assertEqual(gecko["strict_min_version"], "140.0")
        self.assertEqual(
            gecko["data_collection_permissions"], {"required": ["browsingActivity", "websiteActivity"]}
        )

    def test_zips_hold_the_built_files_at_the_root(self):
        for flavor in FLAVORS:
            with zipfile.ZipFile(DIST / f"cptk-overlay-probe-{flavor}-0.0.1.zip") as z:
                self.assertEqual(sorted(z.namelist()), list(FILES))
                self.assertEqual(json.loads(z.read("manifest.json")), self.manifest(flavor))

    def test_no_dynamic_code(self):
        for path, text in scripts():
            self.assertNotIn("eval(", text, path)
            self.assertNotIn("new Function", text, path)

    def test_nothing_uses_storage_local(self):
        for path, text in scripts():
            self.assertNotIn("storage.local", text, path)
            self.assertNotIn("cptkAllUrlsGranted", text, path)

    def test_both_scripts_redact_keys(self):
        for path, text in scripts():
            self.assertIn("function redactKey(", text, path)


@unittest.skipUnless(NODE, "node is not on PATH")
class ExtensionBehaviorTest(unittest.TestCase):
    """Runs the built scripts against a fake browser (extension_sim.js), one scenario per test."""

    def scenario(self, name):
        try:
            result = subprocess.run(
                [NODE, str(SIM), str(DIST / "chromium"), name],
                capture_output=True,
                encoding="utf-8",
                errors="replace",
                timeout=120,
            )
        except subprocess.TimeoutExpired as exc:
            raise AssertionError(f"HUNG: extension_sim.js {name} did not exit within {exc.timeout} s") from exc
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn(f"ok {name}", result.stdout)

    def test_an_inactive_tab_does_nothing(self):
        self.scenario("inactive-tab-does-nothing")

    def test_a_token_activates_only_its_own_tab(self):
        self.scenario("token-activates-only-its-tab")

    def test_typed_characters_are_redacted(self):
        self.scenario("keys-are-redacted")

    def test_ime_and_dead_keys_are_redacted(self):
        self.scenario("ime-and-dead-keys-are-redacted")

    def test_the_probe_pages_keep_full_key_values(self):
        self.scenario("probe-pages-keep-full-keys")

    def test_the_background_redacts_again_and_drops_inactive_tabs(self):
        self.scenario("background-redacts-again")

    def test_the_background_redacts_ime_and_dead_keys(self):
        self.scenario("background-redacts-ime-and-dead-keys")

    def test_all_urls_is_read_at_relay_time(self):
        self.scenario("allurls-is-read-at-relay-time")

    def test_a_host_failure_is_shown_in_the_box(self):
        self.scenario("host-failure-is-shown")

    def test_a_host_connection_is_shown_in_the_box(self):
        self.scenario("host-connection-is-shown")

    def test_the_box_draws_on_an_already_parsed_page(self):
        self.scenario("box-draws-on-an-already-parsed-page")

    def test_a_closed_tab_is_forgotten(self):
        self.scenario("closed-tab-is-forgotten")

    def test_the_token_is_stripped_on_a_double_slash_path(self):
        self.scenario("token-is-stripped-on-a-double-slash-path")

    def test_the_token_is_stripped_from_the_fragment_only(self):
        self.scenario("token-is-stripped-from-the-fragment-only")


if __name__ == "__main__":
    unittest.main()
