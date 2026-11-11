"""Tests for the Phase 0 probe host. PROBE_HOST names the built executable."""
import json
import os
import shutil
import struct
import subprocess
import tempfile
import unittest

HOST = os.environ.get("PROBE_HOST", "")
MAX_IN = 64 * 1024 * 1024


def frame(obj):
    body = json.dumps(obj).encode("utf-8")
    return struct.pack("=I", len(body)) + body


def unframe(data):
    out = []
    while data:
        (n,) = struct.unpack("=I", data[:4])
        out.append(json.loads(data[4 : 4 + n]))
        data = data[4 + n :]
    return out


@unittest.skipUnless(HOST, "set PROBE_HOST to the built probe_host")
class HostTest(unittest.TestCase):
    def temp_dir(self):
        path = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, path, True)
        return path

    def run_host(self, data, code=0, args=("chrome-extension://abc/",), env=None, log_dir=None):
        if env is None:
            log_dir = self.temp_dir()
            env = dict(os.environ, CPTK_PROBE_LOG_DIR=log_dir)
        try:
            proc = subprocess.run([HOST, *args], input=data, capture_output=True, env=env, timeout=120)
        except subprocess.TimeoutExpired:
            self.fail("HUNG: probe_host did not exit after its stdin closed")
        self.assertEqual(proc.returncode, code, proc.stderr)
        with open(os.path.join(log_dir, "probe.log"), encoding="utf-8") as f:
            lines = [json.loads(line) for line in f]
        return proc, lines

    def test_acks_each_message_and_logs_it(self):
        proc, lines = self.run_host(
            frame({"type": "report", "event": "start"}) + frame({"type": "report", "event": "key"})
        )
        acks = unframe(proc.stdout)
        self.assertEqual([r["seq"] for r in acks], [1, 2])
        self.assertEqual([r["type"] for r in acks], ["ack", "ack"])
        self.assertEqual([r["pid"] for r in acks], [lines[0]["pid"]] * 2)
        self.assertEqual([line["event"] for line in lines], ["start", "msg", "msg", "end"])
        self.assertEqual(lines[0]["argv"][1], "chrome-extension://abc/")
        self.assertEqual(lines[2]["body"]["event"], "key")

    def test_refuses_an_oversize_frame(self):
        proc, lines = self.run_host(struct.pack("=I", 0x7FFFFFFF), code=1)
        self.assertEqual(lines[-1]["event"], "oversize")
        self.assertEqual(lines[-1]["len"], 0x7FFFFFFF)

    def test_accepts_a_frame_of_exactly_the_limit(self):
        body = b'"' + b"a" * (MAX_IN - 2) + b'"'
        proc, lines = self.run_host(struct.pack("=I", len(body)) + body)
        self.assertEqual([line["event"] for line in lines], ["start", "msg", "end"])
        self.assertEqual(len(lines[1]["body"]), MAX_IN - 2)
        self.assertEqual([r["seq"] for r in unframe(proc.stdout)], [1])

    def test_refuses_a_frame_one_byte_over_the_limit(self):
        proc, lines = self.run_host(struct.pack("=I", MAX_IN + 1), code=1)
        self.assertEqual(lines[-1]["event"], "oversize")
        self.assertEqual(lines[-1]["len"], MAX_IN + 1)

    def test_refuses_an_empty_frame(self):
        proc, lines = self.run_host(struct.pack("=I", 0), code=1)
        self.assertEqual(lines[-1]["event"], "empty")
        self.assertNotIn("len", lines[-1])

    def test_truncated_frame_ends_cleanly(self):
        proc, lines = self.run_host(struct.pack("=I", 100) + b'{"type":')
        self.assertEqual([line["event"] for line in lines], ["start", "end"])

    def test_partial_length_header_ends_cleanly(self):
        for size in (1, 2, 3):
            with self.subTest(size=size):
                proc, lines = self.run_host(struct.pack("=I", 100)[:size])
                self.assertEqual([line["event"] for line in lines], ["start", "end"])

    def test_argv_is_escaped_and_kept_intact(self):
        arg = 'q"b\\c\x01t\té日本'
        proc, lines = self.run_host(b"", args=(arg,))
        self.assertEqual(lines[0]["argv"][1], arg)

    def test_a_long_argument_is_cut_on_a_character_boundary(self):
        arg = "é" * 3000
        proc, lines = self.run_host(b"", args=(arg,))
        logged = lines[0]["argv"][1]
        self.assertLess(len(logged), len(arg))
        self.assertTrue(arg.startswith(logged))

    def test_an_empty_log_dir_falls_back_to_the_default(self):
        home = self.temp_dir()
        env = dict(os.environ, CPTK_PROBE_LOG_DIR="", LOCALAPPDATA=home, HOME=home)
        log_dir = os.path.join(home, "cptk-probe" if os.name == "nt" else ".cptk-probe")
        proc, lines = self.run_host(b"", env=env, log_dir=log_dir)
        self.assertEqual([line["event"] for line in lines], ["start", "end"])

    def test_an_unopenable_log_says_so_on_stderr(self):
        missing = os.path.join(self.temp_dir(), "no", "such", "dir")
        env = dict(os.environ, CPTK_PROBE_LOG_DIR=missing)
        try:
            proc = subprocess.run([HOST], input=b"", capture_output=True, env=env, timeout=120)
        except subprocess.TimeoutExpired:
            self.fail("HUNG: probe_host did not exit after its stdin closed")
        self.assertEqual(proc.returncode, 2)
        self.assertIn(b"probe.log", proc.stderr)


if __name__ == "__main__":
    unittest.main()
