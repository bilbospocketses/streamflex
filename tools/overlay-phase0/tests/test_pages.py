"""Tests for the kit's pages: the React text probe, its vendored React, and serve.ps1."""
import hashlib
import http.client
import json
import os
import pathlib
import re
import shutil
import socket
import subprocess
import tempfile
import time
import unittest

KIT = pathlib.Path(__file__).resolve().parent.parent
PAGES = KIT / "pages"
VENDOR = PAGES / "vendor" / "react-18.3.1"
SCRIPTS = ["vendor/react-18.3.1/react.production.min.js", "vendor/react-18.3.1/react-dom.production.min.js"]


def page():
    return (PAGES / "text.html").read_text(encoding="utf-8")


class TextPageTest(unittest.TestCase):
    def test_loads_only_its_vendored_scripts(self):
        html = page()
        self.assertEqual(re.findall(r'<script[^>]*\ssrc="([^"]+)"', html), SCRIPTS)
        for src in SCRIPTS:
            self.assertTrue((PAGES / src).is_file(), src)
        self.assertNotRegex(html, r"https?://")
        for api in ("fetch(", "XMLHttpRequest", "WebSocket", "EventSource", "sendBeacon", "import("):
            self.assertNotIn(api, html, api)

    def test_its_policy_forbids_every_network_request(self):
        m = re.search(r'<meta http-equiv="Content-Security-Policy" content="([^"]+)">', page())
        self.assertIsNotNone(m)
        policy = [d.strip() for d in m.group(1).split(";")]
        self.assertEqual(policy, ["default-src 'none'", "script-src 'self' 'unsafe-inline'",
                                  "style-src 'unsafe-inline'"])
        self.assertNotIn("connect-src", m.group(1))  # default-src 'none' already covers every request

    def test_the_g18_string_is_the_kit_file(self):
        m = re.search(r'<script type="application/json" id="g18">(.*?)</script>', page(), re.S)
        expected = json.loads(m.group(1))
        raw = (PAGES / "text-g18.txt").read_bytes()
        self.assertFalse(raw.startswith(b"\xef\xbb\xbf"))
        self.assertFalse(raw.endswith(b"\n"))
        self.assertEqual(raw.decode("utf-8"), expected)
        self.assertEqual(len(expected), 156)
        self.assertEqual([c for c in expected if ord(c) < 0x20 or 0x7F <= ord(c) <= 0x9F], [])
        self.assertEqual(hashlib.sha256(raw).hexdigest(),
                         "f48989965246e2739c6e34e7e0b68e4106793bcfec1b1446671cea401edd92f6")

    @unittest.skipUnless(shutil.which("node"), "needs node")
    def test_its_inline_script_parses(self):
        scripts = re.findall(r"<script>(.*?)</script>", page(), re.S)
        self.assertEqual(len(scripts), 1)
        with tempfile.TemporaryDirectory() as tmp:
            path = os.path.join(tmp, "inline.js")
            pathlib.Path(path).write_text(scripts[0], encoding="utf-8")
            try:
                proc = subprocess.run(["node", "--check", path], capture_output=True, text=True, timeout=120)
            except subprocess.TimeoutExpired:
                self.fail("HUNG: node --check did not exit")
        self.assertEqual(proc.returncode, 0, proc.stderr)


class VendoredReactTest(unittest.TestCase):
    def recorded(self):
        table = (VENDOR / "README.md").read_text(encoding="utf-8")
        return dict(re.findall(r"^\| `([^`]+)` \|.*\| `([0-9a-f]{64})` \|$", table, re.M))

    def test_every_file_matches_its_recorded_sha256(self):
        recorded = self.recorded()
        present = {p.name for p in VENDOR.iterdir() if p.is_file() and p.name != "README.md"}
        self.assertEqual(set(recorded), present)
        for name, digest in recorded.items():
            self.assertEqual(hashlib.sha256((VENDOR / name).read_bytes()).hexdigest(), digest, name)

    def test_the_license_is_mit(self):
        self.assertTrue((VENDOR / "LICENSE").read_text(encoding="utf-8").startswith("MIT License"))


def free_port():
    with socket.socket() as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]


@unittest.skipUnless(os.name == "nt" and shutil.which("powershell"), "Windows PowerShell only")
class ServeTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.port = free_port()
        cls.proc = subprocess.Popen(["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File",
                                     str(PAGES / "serve.ps1"), "-Port", str(cls.port)],
                                    stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True)
        end = time.monotonic() + 120
        while True:
            try:
                with socket.create_connection(("127.0.0.1", cls.port), timeout=5):
                    return
            except OSError:
                if cls.proc.poll() is not None:
                    raise AssertionError("serve.ps1 exited before it listened: " + cls.proc.stderr.read())
                if time.monotonic() > end:
                    cls.proc.kill()
                    raise AssertionError("HUNG: serve.ps1 never listened on port %d" % cls.port)
                time.sleep(0.1)

    @classmethod
    def tearDownClass(cls):
        cls.proc.kill()
        try:
            cls.proc.wait(timeout=120)
        except subprocess.TimeoutExpired:
            raise AssertionError("HUNG: serve.ps1 did not exit after it was stopped")
        finally:
            cls.proc.stderr.close()

    def get(self, path):
        conn = http.client.HTTPConnection("127.0.0.1", self.port, timeout=120)
        try:
            conn.request("GET", path)
            r = conn.getresponse()
            return r.status, r.getheader("Content-Type"), r.read()
        except (TimeoutError, socket.timeout):
            self.fail("HUNG: serve.ps1 did not answer GET %s" % path)
        finally:
            conn.close()

    def test_serves_the_page_and_its_scripts(self):
        status, kind, body = self.get("/text.html")
        self.assertEqual((status, kind), (200, "text/html; charset=utf-8"))
        self.assertEqual(body, (PAGES / "text.html").read_bytes())
        for src in SCRIPTS:
            status, kind, body = self.get("/" + src)
            self.assertEqual((status, kind), (200, "text/javascript; charset=utf-8"), src)
            self.assertEqual(body, (PAGES / src).read_bytes(), src)

    def test_the_root_is_still_the_probe_page(self):
        status, _, body = self.get("/")
        self.assertEqual(status, 200)
        self.assertEqual(body, (PAGES / "probe.html").read_bytes())

    def test_refuses_everything_else(self):
        for path in ("/serve.ps1", "/vendor/react-18.3.1/LICENSE", "/vendor/react-18.3.1/README.md",
                     "/text-g18.txt", "/a/b/text.html", "/nosuch.html", "/vendor/", "/vendor/react-18.3.1/"):
            with self.subTest(path=path):
                self.assertEqual(self.get(path)[0], 404)
        # http.sys may refuse a path that climbs above the root with 400 or 403 (measured: 403) before serve.ps1
        # sees it; either way no file is served.
        for path, outside in (("/../setup/windows.ps1", KIT / "setup" / "windows.ps1"),
                              ("/vendor/../serve.ps1", PAGES / "serve.ps1")):
            with self.subTest(path=path):
                status, _, body = self.get(path)
                self.assertNotEqual(status, 200)
                self.assertNotIn(outside.read_bytes(), body)
