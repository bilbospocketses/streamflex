"""Tests for probe_spawn. PROBE_SPAWN names the built executable."""
import json
import os
import signal
import subprocess
import sys
import threading
import unittest

SPAWN = os.environ.get("PROBE_SPAWN", "")


def events(text):
    return [json.loads(line) for line in text.splitlines() if line.startswith("{")]


@unittest.skipUnless(SPAWN, "set PROBE_SPAWN to the built probe_spawn")
class RunTest(unittest.TestCase):
    def test_run_reports_exit_code(self):
        proc = subprocess.run(
            [SPAWN, "run", sys.executable, "-c", "import sys; sys.exit(3)"],
            input="", capture_output=True, text=True, timeout=120,
        )
        evs = events(proc.stdout)
        self.assertEqual([e["event"] for e in evs], ["started", "exited"])
        self.assertEqual(evs[1]["code"], 3)
        self.assertEqual(evs[1]["ms_since_close"], -1)

    def test_kill_ends_the_child(self):
        proc = subprocess.Popen(
            [SPAWN, "run", sys.executable, "-c", "import time; time.sleep(600)"],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True,
        )
        child = None
        try:
            first = []
            reader = threading.Thread(target=lambda: first.append(proc.stdout.readline()), daemon=True)
            reader.start()
            reader.join(120)
            if reader.is_alive():
                self.fail("HUNG: probe_spawn run printed no started line")
            started = json.loads(first[0])
            self.assertEqual(started["event"], "started")
            child = started["pid"]
            proc.stdin.write("kill\n")
            proc.stdin.flush()
            try:
                out, _ = proc.communicate(timeout=120)
            except subprocess.TimeoutExpired:
                self.fail("HUNG: probe_spawn run did not report the killed child's exit")
            evs = events(out)
            self.assertEqual([e["event"] for e in evs], ["kill_sent", "exited"])
            self.assertEqual(evs[1]["code"], 137)
        finally:
            if proc.poll() is None:
                proc.kill()  # on Windows the job closes with it and ends the child
                proc.wait(timeout=120)
            if child and os.name == "posix":
                try:
                    os.killpg(child, signal.SIGKILL)
                except ProcessLookupError:
                    pass

    def test_run_needs_a_program(self):
        proc = subprocess.run([SPAWN, "run"], capture_output=True, text=True, timeout=120)
        self.assertEqual(proc.returncode, 2)


if __name__ == "__main__":
    unittest.main()
