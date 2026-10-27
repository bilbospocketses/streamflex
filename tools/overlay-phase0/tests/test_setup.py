"""Tests for the Phase 0 setup scripts."""
import json
import os
import pathlib
import shutil
import subprocess
import sys
import tempfile
import unittest

KIT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(KIT / "setup"))
import firefox_profile  # noqa: E402
import linux_setup  # noqa: E402

IDS = {"chrome_web_store": "a" * 32, "edge_addons": "b" * 32, "gecko": "overlay@cptk.app"}
# A user line with a non-ASCII character: the profile scripts must keep it byte for byte.
USER_LINE = 'user_pref("browser.startup.homepage", "https://example.org/café");'


def write_ids(folder):
    path = pathlib.Path(folder) / "ids.json"
    path.write_text(json.dumps(IDS))
    return path


class LinuxSetupTest(unittest.TestCase):
    def setUp(self):
        self.root = pathlib.Path(tempfile.mkdtemp())
        self.home = pathlib.Path(tempfile.mkdtemp())
        self.ids = write_ids(tempfile.mkdtemp())

    def run_setup(self, *extra):
        return linux_setup.main(
            ["--ids", str(self.ids), "--host", "/usr/libexec/cptk/probe_host", "--root", str(self.root),
             "--home", str(self.home), *extra]
        )

    def test_writes_policy_and_host_files(self):
        self.assertEqual(self.run_setup(), 0)
        chrome = json.loads((self.root / "etc/opt/chrome/policies/managed/cptk-overlay.json").read_text())
        self.assertEqual(chrome["ExtensionInstallForcelist"], ["a" * 32 + ";https://clients2.google.com/service/update2/crx"])
        edge = json.loads((self.root / "etc/opt/edge/policies/managed/cptk-overlay.json").read_text())
        self.assertFalse(edge["StartupBoostEnabled"])
        host = json.loads((self.root / "etc/opt/edge/native-messaging-hosts/app.cptk.overlay.json").read_text())
        self.assertEqual(host["allowed_origins"], ["chrome-extension://" + "a" * 32 + "/", "chrome-extension://" + "b" * 32 + "/"])
        ff = json.loads((self.home / ".mozilla/native-messaging-hosts/app.cptk.overlay.json").read_text())
        self.assertEqual(ff["allowed_extensions"], ["overlay@cptk.app"])
        self.assertEqual(ff["path"], "/usr/libexec/cptk/probe_host")

    def test_setup_is_idempotent_and_keeps_foreign_entries(self):
        other = self.root / "etc/opt/chrome/policies/managed/zz-it.json"
        other.parent.mkdir(parents=True)
        other.write_text(json.dumps({"ExtensionInstallForcelist": ["c" * 32 + ";https://x"]}))
        self.run_setup()
        first = (self.root / "etc/opt/chrome/policies/managed/cptk-overlay.json").read_text()
        self.run_setup()
        self.assertEqual((self.root / "etc/opt/chrome/policies/managed/cptk-overlay.json").read_text(), first)
        self.assertIn("c" * 32, other.read_text())
        self.assertEqual(linux_setup.conflicts(self.root, "cptk-overlay.json"), [str(other)])

    def test_remove_deletes_only_its_files(self):
        other = self.root / "etc/opt/chrome/policies/managed/zz-it.json"
        other.parent.mkdir(parents=True)
        other.write_text(json.dumps({"ExtensionInstallForcelist": ["c" * 32 + ";https://x"]}))
        self.run_setup()
        self.run_setup("--remove")
        self.assertFalse((self.root / "etc/opt/chrome/policies/managed/cptk-overlay.json").exists())
        self.assertFalse((self.home / ".mozilla/native-messaging-hosts/app.cptk.overlay.json").exists())
        self.assertTrue(other.exists())

    def test_lists_the_folders_it_will_create(self):
        target = self.home / ".mozilla/native-messaging-hosts"
        self.assertEqual(linux_setup.missing_dirs(target), [self.home / ".mozilla", target])
        target.mkdir(parents=True)
        self.assertEqual(linux_setup.missing_dirs(target), [])

    def test_refuses_a_relative_host_path(self):
        with self.assertRaises(SystemExit):
            linux_setup.main(["--ids", str(self.ids), "--host", "probe_host", "--root", str(self.root),
                              "--home", str(self.home)])


class FirefoxProfileTest(unittest.TestCase):
    def setUp(self):
        self.profile = pathlib.Path(tempfile.mkdtemp())
        self.xpi = pathlib.Path(tempfile.mkdtemp()) / "probe.xpi"
        self.xpi.write_bytes(b"version-1")

    def test_profile_keeps_user_lines(self):
        (self.profile / "user.js").write_text(USER_LINE + "\n", encoding="utf-8")
        firefox_profile.prepare(self.profile, self.xpi)
        text = (self.profile / "user.js").read_text(encoding="utf-8")
        self.assertIn(USER_LINE, text)
        self.assertIn('user_pref("extensions.autoDisableScopes", 0); // cptk-overlay', text)
        self.assertIn('user_pref("extensions.startupScanScopes", 1); // cptk-overlay', text)
        firefox_profile.prepare(self.profile, self.xpi)
        self.assertEqual((self.profile / "user.js").read_text(encoding="utf-8"), text)

    def test_copies_only_when_the_xpi_differs(self):
        self.assertTrue(firefox_profile.prepare(self.profile, self.xpi))
        self.assertFalse(firefox_profile.prepare(self.profile, self.xpi))
        self.xpi.write_bytes(b"version-2")
        self.assertTrue(firefox_profile.prepare(self.profile, self.xpi))
        self.assertEqual((self.profile / "extensions/overlay@cptk.app.xpi").read_bytes(), b"version-2")

    def test_no_scan_scopes_variant(self):
        firefox_profile.prepare(self.profile, self.xpi, scan_scopes=False)
        self.assertNotIn("startupScanScopes", (self.profile / "user.js").read_text(encoding="utf-8"))


@unittest.skipUnless(os.name == "nt" and shutil.which("powershell"), "Windows PowerShell only")
class FirefoxProfilePsTest(unittest.TestCase):
    """The Windows twin of FirefoxProfileTest: setup/firefox-profile.ps1, for guests with no Python."""

    def setUp(self):
        self.profile = pathlib.Path(tempfile.mkdtemp())
        self.xpi = pathlib.Path(tempfile.mkdtemp()) / "probe.xpi"
        self.xpi.write_bytes(b"version-1")

    def prepare(self, *extra):
        cmd = ["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File",
               str(KIT / "setup" / "firefox-profile.ps1"), "-ProfileDir", str(self.profile), "-Xpi", str(self.xpi),
               *extra]
        proc = subprocess.run(cmd, capture_output=True, text=True, timeout=120)
        self.assertEqual(proc.returncode, 0, proc.stderr)
        return proc.stdout.strip()

    def test_profile_keeps_user_lines(self):
        (self.profile / "user.js").write_text(USER_LINE + "\n", encoding="utf-8")
        self.prepare()
        text = (self.profile / "user.js").read_text(encoding="utf-8")
        self.assertIn(USER_LINE, text)
        self.assertIn('user_pref("extensions.autoDisableScopes", 0); // cptk-overlay', text)
        self.assertIn('user_pref("extensions.startupScanScopes", 1); // cptk-overlay', text)
        self.prepare()
        self.assertEqual((self.profile / "user.js").read_text(encoding="utf-8"), text)

    def test_copies_only_when_the_xpi_differs(self):
        self.assertEqual(self.prepare(), "copied")
        self.assertEqual(self.prepare(), "unchanged")
        self.xpi.write_bytes(b"version-2")
        self.assertEqual(self.prepare(), "copied")
        self.assertEqual((self.profile / "extensions/overlay@cptk.app.xpi").read_bytes(), b"version-2")

    def test_no_scan_scopes_variant(self):
        self.prepare("-NoScanScopes")
        self.assertNotIn("startupScanScopes", (self.profile / "user.js").read_text(encoding="utf-8"))


@unittest.skipUnless(os.name == "nt" and shutil.which("powershell"), "Windows PowerShell only")
class WindowsSetupTest(unittest.TestCase):
    TEST_ROOT = r"HKCU:\Software\CPTK-Phase0-Test"

    def ps(self, *args):
        cmd = ["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", str(KIT / "setup" / "windows.ps1"), *args]
        return subprocess.run(cmd, capture_output=True, text=True, timeout=120)

    def tearDown(self):
        subprocess.run(["powershell", "-NoProfile", "-Command",
                        f"Remove-Item -LiteralPath '{self.TEST_ROOT}' -Recurse -ErrorAction SilentlyContinue"],
                       timeout=120)

    def test_setup_is_idempotent_and_keeps_foreign_entries(self):
        import winreg
        ids = write_ids(tempfile.mkdtemp())
        manifests = tempfile.mkdtemp()
        key = r"Software\CPTK-Phase0-Test\Policies\Google\Chrome\ExtensionInstallForcelist"
        with winreg.CreateKey(winreg.HKEY_CURRENT_USER, key) as k:
            winreg.SetValueEx(k, "1", 0, winreg.REG_SZ, "c" * 32 + ";https://x")
        args = ["-IdsFile", str(ids), "-HostPath", r"C:\cptk\probe_host.exe", "-ManifestDir", manifests,
                "-RegistryRoot", self.TEST_ROOT]
        self.assertEqual(self.ps(*args).returncode, 0)
        self.assertEqual(self.ps(*args).returncode, 0)
        with winreg.OpenKey(winreg.HKEY_CURRENT_USER, key) as k:
            values = {winreg.EnumValue(k, i)[0]: winreg.EnumValue(k, i)[1] for i in range(winreg.QueryInfoKey(k)[1])}
        self.assertEqual(values, {"1": "c" * 32 + ";https://x", "2": "a" * 32 + ";https://clients2.google.com/service/update2/crx"})
        host = r"Software\CPTK-Phase0-Test\Microsoft\Edge\NativeMessagingHosts\app.cptk.overlay"
        with winreg.OpenKey(winreg.HKEY_CURRENT_USER, host) as k:
            manifest_path = winreg.QueryValue(k, None)
        self.assertEqual(json.loads(pathlib.Path(manifest_path).read_text(encoding="utf-8-sig"))["path"], r"C:\cptk\probe_host.exe")
        self.assertEqual(self.ps(*args, "-Remove").returncode, 0)
        with winreg.OpenKey(winreg.HKEY_CURRENT_USER, key) as k:
            self.assertEqual(winreg.QueryInfoKey(k)[1], 1)

    def test_remove_leaves_no_policy_keys_behind(self):
        import winreg
        ids = write_ids(tempfile.mkdtemp())
        args = ["-IdsFile", str(ids), "-HostPath", r"C:\cptk\probe_host.exe", "-ManifestDir", tempfile.mkdtemp(),
                "-RegistryRoot", self.TEST_ROOT]
        self.assertEqual(self.ps(*args).returncode, 0)
        self.assertEqual(self.ps(*args, "-Remove").returncode, 0)
        for gone in (r"Policies\Google", r"Policies\Microsoft\Edge",
                     r"Microsoft\Edge\NativeMessagingHosts\app.cptk.overlay"):
            with self.assertRaises(FileNotFoundError, msg=gone):
                winreg.OpenKey(winreg.HKEY_CURRENT_USER, rf"Software\CPTK-Phase0-Test\{gone}")


if __name__ == "__main__":
    unittest.main()
