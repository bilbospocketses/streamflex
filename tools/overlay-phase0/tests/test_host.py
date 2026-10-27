"""Tests for the Phase 0 probe host. PROBE_HOST names the built executable."""
import json
import os
import struct
import subprocess
import tempfile
import unittest

HOST = os.environ.get("PROBE_HOST", "")


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
    def run_host(self, data):
        log_dir = tempfile.mkdtemp()
        env = dict(os.environ, CPTK_PROBE_LOG_DIR=log_dir)
        try:
            proc = subprocess.run(
                [HOST, "chrome-extension://abc/"], input=data, capture_output=True, env=env, timeout=120
            )
        except subprocess.TimeoutExpired:
            self.fail("HUNG: probe_host did not exit after its stdin closed")
        with open(os.path.join(log_dir, "probe.log"), encoding="utf-8") as f:
            lines = [json.loads(line) for line in f]
        return proc, lines

    def test_acks_each_message_and_logs_it(self):
        proc, lines = self.run_host(
            frame({"type": "report", "event": "start"}) + frame({"type": "report", "event": "key"})
        )
        self.assertEqual(proc.returncode, 0)
        self.assertEqual([r["seq"] for r in unframe(proc.stdout)], [1, 2])
        self.assertEqual([line["event"] for line in lines], ["start", "msg", "msg", "end"])
        self.assertEqual(lines[0]["argv"][1], "chrome-extension://abc/")
        self.assertEqual(lines[2]["body"]["event"], "key")

    def test_refuses_an_oversize_frame(self):
        proc, lines = self.run_host(struct.pack("=I", 0x7FFFFFFF))
        self.assertEqual(proc.returncode, 1)
        self.assertEqual(lines[-1]["event"], "oversize")
        self.assertEqual(lines[-1]["len"], 0x7FFFFFFF)

    def test_refuses_an_empty_frame(self):
        proc, lines = self.run_host(struct.pack("=I", 0))
        self.assertEqual(proc.returncode, 1)
        self.assertEqual(lines[-1]["event"], "empty")
        self.assertNotIn("len", lines[-1])

    def test_truncated_frame_ends_cleanly(self):
        proc, lines = self.run_host(struct.pack("=I", 100) + b'{"type":')
        self.assertEqual(proc.returncode, 0)
        self.assertEqual([line["event"] for line in lines], ["start", "end"])


if __name__ == "__main__":
    unittest.main()
