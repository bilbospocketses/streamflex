"""Tests for make-kit.ps1, run against a fake kit source and a fake build folder in temp directories."""
import os
import pathlib
import shutil
import subprocess
import tempfile
import unittest

KIT = pathlib.Path(__file__).resolve().parent.parent

SOURCE_FILES = {
    "setup/windows.ps1": "setup",
    "setup/linux_setup.py": "linux",
    "setup/__pycache__/linux_setup.cpython-313.pyc": "cache",
    "pages/probe.html": "probe",
    "pages/serve.ps1": "serve",
    "extension/build.py": "build",
    "extension/src/content.js": "content",
    "extension/src/background.js": "background",
    "extension/dist/chromium/manifest.json": "stale build",
    "tests/test_host.py": "not in the kit",
}
BUILD_FILES = ["probe_host.exe", "probe_spawn.exe", "SDL3.dll", "probe_spawn.pdb", "probe_common.lib"]
# The fake System32: two of the three runtime DLLs, so the absent one shows it is skipped, plus one it must not take.
RUNTIME_FILES = ["vcruntime140.dll", "msvcp140.dll", "kernel32.dll"]
KIT_FILES = {
    "probe_host.exe", "probe_spawn.exe", "SDL3.dll", "vcruntime140.dll", "msvcp140.dll",
    "setup/windows.ps1", "setup/linux_setup.py", "pages/probe.html", "pages/serve.ps1",
    "extension/build.py", "extension/src/content.js", "extension/src/background.js",
}


@unittest.skipUnless(os.name == "nt" and shutil.which("powershell"), "Windows PowerShell only")
class MakeKitTest(unittest.TestCase):
    def temp_dir(self):
        path = pathlib.Path(tempfile.mkdtemp())
        self.addCleanup(shutil.rmtree, path, True)
        return path

    def setUp(self):
        self.source = self.temp_dir()
        shutil.copyfile(KIT / "make-kit.ps1", self.source / "make-kit.ps1")
        for rel, text in SOURCE_FILES.items():
            path = self.source / rel
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(text, encoding="utf-8")
        self.build = self.temp_dir()
        for name in BUILD_FILES:
            (self.build / name).write_bytes(name.encode())
        self.runtime = self.temp_dir()
        for name in RUNTIME_FILES:
            (self.runtime / name).write_bytes(b"system " + name.encode())
        self.kit = self.temp_dir() / "cptk-kit"

    def make_kit(self, *extra, env=None, kit=True):
        # -RuntimeDir always names the fake System32, so no test reads the real one.
        cmd = ["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", str(self.source / "make-kit.ps1"),
               "-Build", str(self.build), "-RuntimeDir", str(self.runtime),
               *(["-Kit", str(self.kit)] if kit else []), *extra]
        return subprocess.run(cmd, capture_output=True, text=True, env=env, timeout=120)

    def layout(self, kit):
        return {p.relative_to(kit).as_posix() for p in kit.rglob("*") if p.is_file()}

    def test_copies_the_kit_layout(self):
        (self.source / "ids.json").write_text('{"gecko": "overlay-probe@cptk.app"}', encoding="utf-8")
        xpi = self.temp_dir() / "cptk-overlay-probe-0.0.1.xpi"
        xpi.write_bytes(b"signed")
        proc = self.make_kit("-Xpi", str(xpi))
        self.assertEqual(proc.returncode, 0, proc.stderr)
        self.assertEqual(self.layout(self.kit), KIT_FILES | {"ids.json", "cptk-overlay-probe-0.0.1.xpi"})
        self.assertEqual((self.kit / "SDL3.dll").read_bytes(), b"SDL3.dll")
        self.assertEqual((self.kit / "vcruntime140.dll").read_bytes(), b"system vcruntime140.dll")
        self.assertEqual((self.kit / "cptk-overlay-probe-0.0.1.xpi").read_bytes(), b"signed")
        lines = proc.stdout.splitlines()
        self.assertIn(f"kit: {self.kit}", lines)
        self.assertIn("  extension\\src\\content.js", lines)
        self.assertIn("  vcruntime140.dll", lines)
        self.assertIn("  msvcp140.dll", lines)
        self.assertEqual(lines[-1], f"make-kit: copied {len(KIT_FILES) + 2} files")

    def test_default_kit_folder_without_ids_or_xpi(self):
        profile = self.temp_dir()
        proc = self.make_kit(env=dict(os.environ, USERPROFILE=str(profile)), kit=False)
        self.assertEqual(proc.returncode, 0, proc.stderr)
        self.assertEqual(self.layout(profile / "cptk-kit"), KIT_FILES)
        self.assertIn("make-kit: no ids.json yet (Task 7 records it)", proc.stdout.splitlines())

    def test_a_runtime_dll_from_the_build_wins(self):
        (self.build / "VCRUNTIME140.dll").write_bytes(b"build copy")
        proc = self.make_kit()
        self.assertEqual(proc.returncode, 0, proc.stderr)
        self.assertEqual((self.kit / "vcruntime140.dll").read_bytes(), b"build copy")
        self.assertEqual(proc.stdout.lower().count("vcruntime140.dll"), 1)

    def test_rerun_refreshes_changed_files_and_deletes_nothing(self):
        self.assertEqual(self.make_kit().returncode, 0)
        sentinel = self.kit / "extension" / "dist" / "chromium" / "manifest.json"
        sentinel.parent.mkdir(parents=True)
        sentinel.write_text("built in the kit", encoding="utf-8")
        (self.source / "pages/probe.html").write_text("probe v2", encoding="utf-8")
        self.assertEqual(self.make_kit().returncode, 0)
        self.assertEqual((self.kit / "pages/probe.html").read_text(encoding="utf-8"), "probe v2")
        self.assertEqual(sentinel.read_text(encoding="utf-8"), "built in the kit")

    def test_refuses_a_build_without_the_executables(self):
        (self.build / "probe_spawn.exe").unlink()
        proc = self.make_kit()
        self.assertNotEqual(proc.returncode, 0)
        self.assertIn("probe_spawn.exe", proc.stderr)
        self.assertFalse(self.kit.exists())


if __name__ == "__main__":
    unittest.main()
