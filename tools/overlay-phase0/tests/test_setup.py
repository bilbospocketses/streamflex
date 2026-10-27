"""Tests for the Phase 0 setup scripts."""
import io
import json
import os
import pathlib
import shutil
import subprocess
import sys
import tempfile
import types
import unittest
from unittest import mock

if os.name == "nt":
    import winreg

KIT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(KIT / "setup"))
import firefox_profile  # noqa: E402
import linux_setup  # noqa: E402

IDS = {"chrome_web_store": "a" * 32, "edge_addons": "b" * 32, "gecko": "overlay-probe@cptk.app"}
CWS_ENTRY = "a" * 32 + ";https://clients2.google.com/service/update2/crx"
EDGE_ENTRY = "b" * 32 + ";https://edge.microsoft.com/extensionwebstorebase/v1/crx"
HOST_EXE = r"C:\cptk\probe_host.exe"
# A user line with a non-ASCII character: the profile scripts must keep it byte for byte.
USER_LINE = 'user_pref("browser.startup.homepage", "https://example.org/café");'


def write_ids(folder, ids=IDS):
    path = pathlib.Path(folder) / "ids.json"
    path.write_text(json.dumps(ids))
    return path


def run_powershell(test, cmd, cwd=None):
    """Runs Windows PowerShell; a run that does not exit inside the hang guard fails the test as HUNG."""
    try:
        return subprocess.run(cmd, capture_output=True, text=True, timeout=120, cwd=cwd)
    except subprocess.TimeoutExpired:
        test.fail(f"HUNG: no exit from {cmd[5]} inside 120 s")


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
        self.assertEqual(ff["allowed_extensions"], ["overlay-probe@cptk.app"])
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

    def test_refuses_bad_ids(self):
        for bad in ({**IDS, "chrome_web_store": "A" * 32}, {**IDS, "edge_addons": "b" * 31},
                    {k: v for k, v in IDS.items() if k != "gecko"}, {**IDS, "gecko": "overlay@cptk.app"}):
            ids = write_ids(tempfile.mkdtemp(), bad)
            argv = ["--ids", str(ids), "--host", "/usr/libexec/cptk/probe_host", "--root", str(self.root),
                    "--home", str(self.home)]
            with mock.patch("sys.stderr", new_callable=io.StringIO) as err:
                self.assertEqual(linux_setup.main(argv), 1, bad)
            self.assertIn("ids file", err.getvalue())
        self.assertEqual(os.listdir(self.root), [])
        self.assertEqual(os.listdir(self.home), [])

    def test_under_sudo_gives_the_user_back_what_it_made_in_their_home(self):
        def getpwnam(name):
            self.assertEqual(name, "alice")
            return types.SimpleNamespace(pw_dir=str(self.home), pw_uid=1001, pw_gid=1002)

        fake_pwd = types.ModuleType("pwd")
        fake_pwd.getpwnam = getpwnam
        calls = []
        argv = ["--ids", str(self.ids), "--host", "/usr/libexec/cptk/probe_host", "--root", str(self.root)]
        with mock.patch.dict(sys.modules, {"pwd": fake_pwd}), mock.patch.dict(os.environ, {"SUDO_USER": "alice"}), \
                mock.patch.object(os, "chown", lambda p, u, g: calls.append((pathlib.Path(p), u, g)), create=True):
            self.assertEqual(linux_setup.main(argv), 0)
            hosts = self.home / ".mozilla/native-messaging-hosts"
            self.assertEqual(calls, [(self.home / ".mozilla", 1001, 1002), (hosts, 1001, 1002),
                                     (hosts / "app.cptk.overlay.json", 1001, 1002)])
            calls.clear()
            self.assertEqual(linux_setup.main(argv), 0)
            self.assertEqual(calls, [(hosts / "app.cptk.overlay.json", 1001, 1002)])
        self.assertTrue((self.root / "etc/opt/chrome/policies/managed/cptk-overlay.json").exists())


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
        self.assertEqual((self.profile / "extensions/overlay-probe@cptk.app.xpi").read_bytes(), b"version-2")

    def test_no_scan_scopes_variant(self):
        # An explicit 0, not a missing line: a profile that ran the default way keeps the 1 in prefs.js otherwise.
        firefox_profile.prepare(self.profile, self.xpi)
        firefox_profile.prepare(self.profile, self.xpi, scan_scopes=False)
        text = (self.profile / "user.js").read_text(encoding="utf-8")
        self.assertIn('user_pref("extensions.startupScanScopes", 0); // cptk-overlay', text)
        self.assertNotIn('user_pref("extensions.startupScanScopes", 1)', text)


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
        proc = run_powershell(self, cmd)
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
        self.assertEqual((self.profile / "extensions/overlay-probe@cptk.app.xpi").read_bytes(), b"version-2")

    def test_no_scan_scopes_variant(self):
        self.prepare()
        self.prepare("-NoScanScopes")
        text = (self.profile / "user.js").read_text(encoding="utf-8")
        self.assertIn('user_pref("extensions.startupScanScopes", 0); // cptk-overlay', text)
        self.assertNotIn('user_pref("extensions.startupScanScopes", 1)', text)


@unittest.skipUnless(os.name == "nt" and shutil.which("powershell"), "Windows PowerShell only")
class WindowsSetupTest(unittest.TestCase):
    """windows.ps1 against a stand-in root under HKCU; it never touches real policy."""

    TEST_ROOT = r"HKCU:\Software\CPTK-Phase0-Test"
    SUB = r"Software\CPTK-Phase0-Test"

    def setUp(self):
        self.ids = write_ids(tempfile.mkdtemp())
        self.manifests = tempfile.mkdtemp()
        self.locked = []

    def tearDown(self):
        # A locked key is opened up again first (its owner may always change its ACL), or it outlives the cleanup.
        unlock = "".join(
            f"$k = [Microsoft.Win32.Registry]::CurrentUser.OpenSubKey('{self.SUB}\\{sub}', 'ReadWriteSubTree', "
            "'ChangePermissions'); $a = New-Object System.Security.AccessControl.RegistrySecurity; "
            "$a.AddAccessRule((New-Object System.Security.AccessControl.RegistryAccessRule("
            "[Security.Principal.WindowsIdentity]::GetCurrent().User, 'FullControl', 'ContainerInherit', 'None', "
            "'Allow'))); $k.SetAccessControl($a); $k.Close(); "
            for sub in self.locked)
        subprocess.run(["powershell", "-NoProfile", "-Command",
                        unlock + f"Remove-Item -LiteralPath '{self.TEST_ROOT}' -Recurse -ErrorAction SilentlyContinue"],
                       timeout=120)

    def lock(self, sub):
        """Leaves this user only ReadKey on the stand-in key, as a stock Windows 11 does on HKCU\\Software\\Policies."""
        # .NET, not Set-Acl: 5.1 cannot load Set-Acl's module when PowerShell 7's modules come first on the path.
        self.locked.append(sub)
        key = f"{self.SUB}\\{sub}"
        script = (f"[Microsoft.Win32.Registry]::CurrentUser.CreateSubKey('{key}').Close(); "
                  f"$k = [Microsoft.Win32.Registry]::CurrentUser.OpenSubKey('{key}', 'ReadWriteSubTree', "
                  "'ChangePermissions'); $a = New-Object System.Security.AccessControl.RegistrySecurity; "
                  "$a.SetAccessRuleProtection($true, $false); "
                  "$a.AddAccessRule((New-Object System.Security.AccessControl.RegistryAccessRule("
                  "[Security.Principal.WindowsIdentity]::GetCurrent().User, 'ReadKey', 'ContainerInherit', 'None', "
                  "'Allow'))); $k.SetAccessControl($a); $k.Close()")
        proc = subprocess.run(["powershell", "-NoProfile", "-Command", script], capture_output=True, text=True,
                              timeout=120)
        self.assertEqual(proc.returncode, 0, proc.stderr)

    def ps(self, *extra, ids=None, host=HOST_EXE, manifests=None, root=TEST_ROOT, cwd=None):
        cmd = ["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", str(KIT / "setup" / "windows.ps1"),
               "-IdsFile", str(ids or self.ids), "-HostPath", host, "-ManifestDir", manifests or self.manifests,
               "-RegistryRoot", root, *extra]
        return run_powershell(self, cmd, cwd=cwd)

    def ok(self, *extra, **kw):
        proc = self.ps(*extra, **kw)
        self.assertEqual(proc.returncode, 0, f"stderr: {proc.stderr}\nstdout: {proc.stdout}")
        return proc

    def values(self, sub):
        with winreg.OpenKey(winreg.HKEY_CURRENT_USER, rf"{self.SUB}\{sub}") as k:
            return {winreg.EnumValue(k, i)[0]: winreg.EnumValue(k, i)[1] for i in range(winreg.QueryInfoKey(k)[1])}

    def typed(self, sub, name):
        with winreg.OpenKey(winreg.HKEY_CURRENT_USER, rf"{self.SUB}\{sub}") as k:
            return winreg.QueryValueEx(k, name)

    def seed(self, sub, name, kind, data):
        with winreg.CreateKey(winreg.HKEY_CURRENT_USER, rf"{self.SUB}\{sub}") as k:
            winreg.SetValueEx(k, name, 0, kind, data)

    def exists(self, sub=None):
        try:
            winreg.OpenKey(winreg.HKEY_CURRENT_USER, rf"{self.SUB}\{sub}" if sub else self.SUB).Close()
            return True
        except FileNotFoundError:
            return False

    def manifest_of(self, host_key):
        with winreg.OpenKey(winreg.HKEY_CURRENT_USER, rf"{self.SUB}\{host_key}") as k:
            path = winreg.QueryValue(k, None)
        return path, json.loads(pathlib.Path(path).read_text(encoding="utf-8-sig"))

    def test_setup_is_idempotent_and_keeps_foreign_entries(self):
        chrome = r"Policies\Google\Chrome\ExtensionInstallForcelist"
        edge = r"Policies\Microsoft\Edge\ExtensionInstallForcelist"
        self.seed(chrome, "1", winreg.REG_SZ, "c" * 32 + ";https://x")
        self.ok()
        self.ok()
        self.assertEqual(self.values(chrome), {"1": "c" * 32 + ";https://x", "2": CWS_ENTRY})
        self.assertEqual(self.values(edge), {"1": EDGE_ENTRY})
        self.ok("-Remove")
        self.assertEqual(self.values(chrome), {"1": "c" * 32 + ";https://x"})
        self.assertFalse(self.exists(edge))

    def test_writes_the_edge_dwords_and_all_three_host_keys(self):
        self.ok()
        for name in ("StartupBoostEnabled", "BackgroundModeEnabled"):
            self.assertEqual(self.typed(r"Policies\Microsoft\Edge", name), (0, winreg.REG_DWORD), name)
        chromium_origins = ["chrome-extension://" + "a" * 32 + "/", "chrome-extension://" + "b" * 32 + "/"]
        for host_key in (r"Google\Chrome\NativeMessagingHosts\app.cptk.overlay",
                         r"Microsoft\Edge\NativeMessagingHosts\app.cptk.overlay"):
            path, manifest = self.manifest_of(host_key)
            self.assertEqual(pathlib.Path(path), pathlib.Path(self.manifests) / "app.cptk.overlay.chromium.json")
            self.assertEqual(manifest["path"], HOST_EXE)
            self.assertEqual(manifest["allowed_origins"], chromium_origins)
        path, manifest = self.manifest_of(r"Mozilla\NativeMessagingHosts\app.cptk.overlay")
        self.assertEqual(pathlib.Path(path), pathlib.Path(self.manifests) / "app.cptk.overlay.firefox.json")
        self.assertEqual(manifest["path"], HOST_EXE)
        self.assertEqual(manifest["allowed_extensions"], ["overlay-probe@cptk.app"])

    def test_remove_leaves_no_policy_keys_behind(self):
        self.ok()
        self.ok("-Remove")
        for gone in (r"Policies\Google", r"Policies\Microsoft\Edge",
                     r"Google\Chrome\NativeMessagingHosts\app.cptk.overlay",
                     r"Microsoft\Edge\NativeMessagingHosts\app.cptk.overlay",
                     r"Mozilla\NativeMessagingHosts\app.cptk.overlay"):
            self.assertFalse(self.exists(gone), gone)
        self.assertEqual(os.listdir(self.manifests), [])

    def test_remove_puts_back_what_was_there_before(self):
        edge = r"Policies\Microsoft\Edge"
        it_entry = "b" * 32 + ";https://it.example/crx"
        it_manifest = r"C:\it\other-host.json"
        self.seed(edge, "StartupBoostEnabled", winreg.REG_DWORD, 1)
        self.seed(edge, "BackgroundModeEnabled", winreg.REG_SZ, "it-set")
        self.seed(edge, "HomepageLocation", winreg.REG_SZ, "https://intranet.example")
        self.seed(edge + r"\ExtensionInstallForcelist", "5", winreg.REG_SZ, it_entry)
        self.seed(r"Mozilla\NativeMessagingHosts\app.cptk.overlay", "", winreg.REG_SZ, it_manifest)
        self.ok()
        self.ok()  # a second run must not record setup's own values as the prior ones
        self.assertEqual(self.typed(edge, "StartupBoostEnabled"), (0, winreg.REG_DWORD))
        self.assertEqual(self.values(edge + r"\ExtensionInstallForcelist"), {"5": it_entry})
        self.ok("-Remove")
        self.assertEqual(self.typed(edge, "StartupBoostEnabled"), (1, winreg.REG_DWORD))
        self.assertEqual(self.typed(edge, "BackgroundModeEnabled"), ("it-set", winreg.REG_SZ))
        self.assertEqual(self.typed(edge, "HomepageLocation"), ("https://intranet.example", winreg.REG_SZ))
        self.assertEqual(self.values(edge + r"\ExtensionInstallForcelist"), {"5": it_entry})
        self.assertEqual(self.values(r"Mozilla\NativeMessagingHosts\app.cptk.overlay"), {"": it_manifest})
        self.assertFalse(self.exists(r"Google\Chrome\NativeMessagingHosts\app.cptk.overlay"))
        self.assertFalse(self.exists(r"Policies\Google"))
        self.assertEqual(os.listdir(self.manifests), [])

    def test_remove_without_a_state_file_removes_by_name(self):
        self.ok()
        os.remove(os.path.join(self.manifests, "setup-state.json"))
        proc = self.ok("-Remove")
        self.assertIn("removing by name", proc.stdout)
        for gone in (r"Policies\Google", r"Policies\Microsoft\Edge", r"Mozilla\NativeMessagingHosts\app.cptk.overlay"):
            self.assertFalse(self.exists(gone), gone)

    def test_resolves_a_relative_manifest_dir(self):
        cwd = tempfile.mkdtemp()
        self.ok(manifests=r"rel\hosts", cwd=cwd)
        path, manifest = self.manifest_of(r"Google\Chrome\NativeMessagingHosts\app.cptk.overlay")
        self.assertEqual(pathlib.Path(path), pathlib.Path(cwd) / "rel" / "hosts" / "app.cptk.overlay.chromium.json")
        self.assertEqual(manifest["path"], HOST_EXE)
        self.ok("-Remove", manifests=r"rel\hosts", cwd=cwd)
        self.assertEqual(os.listdir(pathlib.Path(cwd) / "rel" / "hosts"), [])

    def test_refuses_a_relative_host_path(self):
        proc = self.ps(host=r"cptk\probe_host.exe")
        self.assertEqual(proc.returncode, 1, f"stderr: {proc.stderr}\nstdout: {proc.stdout}")
        self.assertIn("-HostPath must be an absolute path", proc.stderr)
        self.assertFalse(self.exists())
        self.assertEqual(os.listdir(self.manifests), [])

    def test_refuses_bad_ids(self):
        for bad in ({**IDS, "chrome_web_store": "A" * 32}, {**IDS, "edge_addons": "b" * 31},
                    {k: v for k, v in IDS.items() if k != "gecko"}, {**IDS, "gecko": "overlay@cptk.app"}):
            proc = self.ps(ids=write_ids(tempfile.mkdtemp(), bad))
            self.assertEqual(proc.returncode, 1, f"{bad}\nstderr: {proc.stderr}\nstdout: {proc.stdout}")
            self.assertIn("ids file", proc.stderr)
        self.assertFalse(self.exists())
        self.assertEqual(os.listdir(self.manifests), [])

    def test_a_read_only_policy_key_stops_setup_before_any_write(self):
        self.lock("Policies")
        proc = self.ps()
        self.assertEqual(proc.returncode, 1, f"stderr: {proc.stderr}\nstdout: {proc.stdout}")
        self.assertIn(rf"setup: {self.TEST_ROOT}\Policies\Google\Chrome\ExtensionInstallForcelist is not writable "
                      "from this shell; run it from a PowerShell opened with Run as administrator", proc.stderr)
        self.assertEqual(os.listdir(self.manifests), [])
        with winreg.OpenKey(winreg.HKEY_CURRENT_USER, self.SUB) as k:
            self.assertEqual([winreg.EnumKey(k, i) for i in range(winreg.QueryInfoKey(k)[0])], ["Policies"])
        with winreg.OpenKey(winreg.HKEY_CURRENT_USER, rf"{self.SUB}\Policies") as k:
            self.assertEqual(winreg.QueryInfoKey(k)[:2], (0, 0))

    def test_a_read_only_policy_key_stops_remove_before_any_change(self):
        chrome = r"Policies\Google\Chrome\ExtensionInstallForcelist"
        edge = r"Policies\Microsoft\Edge\ExtensionInstallForcelist"
        self.ok()
        self.lock(chrome)
        proc = self.ps("-Remove")
        self.assertEqual(proc.returncode, 1, f"stderr: {proc.stderr}\nstdout: {proc.stdout}")
        self.assertIn(rf"setup: {self.TEST_ROOT}\{chrome} is not writable from this shell", proc.stderr)
        self.assertEqual(self.values(chrome), {"1": CWS_ENTRY})
        self.assertEqual(self.values(edge), {"1": EDGE_ENTRY})
        self.assertTrue(self.exists(r"Mozilla\NativeMessagingHosts\app.cptk.overlay"))
        self.assertIn("setup-state.json", os.listdir(self.manifests))

    def test_hklm_scope_puts_policy_and_host_keys_under_its_root(self):
        # The stand-in for HKLM:\Software; under -Scope HKLM the host keys share the policy root.
        root = self.TEST_ROOT + r"\HKLM"
        proc = self.ok("-Scope", "HKLM", root=root)
        self.assertIn(rf"policy and host keys under {root},", proc.stdout)
        with winreg.OpenKey(winreg.HKEY_CURRENT_USER, self.SUB) as k:
            self.assertEqual([winreg.EnumKey(k, i) for i in range(winreg.QueryInfoKey(k)[0])], ["HKLM"])
        self.assertEqual(self.values(r"HKLM\Policies\Google\Chrome\ExtensionInstallForcelist"), {"1": CWS_ENTRY})
        for host_key in (r"HKLM\Google\Chrome\NativeMessagingHosts\app.cptk.overlay",
                         r"HKLM\Microsoft\Edge\NativeMessagingHosts\app.cptk.overlay",
                         r"HKLM\Mozilla\NativeMessagingHosts\app.cptk.overlay"):
            self.assertEqual(self.manifest_of(host_key)[1]["path"], HOST_EXE)
        self.ok("-Scope", "HKLM", "-Remove", root=root)
        self.assertFalse(self.exists(r"HKLM\Policies\Google"))
        self.assertFalse(self.exists(r"HKLM\Mozilla\NativeMessagingHosts\app.cptk.overlay"))


if __name__ == "__main__":
    unittest.main()
