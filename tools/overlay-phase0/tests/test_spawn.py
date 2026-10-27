"""Tests for probe_spawn. PROBE_SPAWN names the built executable."""
import json
import os
import signal
import subprocess
import sys
import tempfile
import threading
import time
import unittest

SPAWN = os.environ.get("PROBE_SPAWN", "")


def events(text):
    return [json.loads(line) for line in text.splitlines() if line.startswith("{")]


def read_until_exited(proc, lines):
    """Starts a thread that appends proc's stdout lines to lines until its exited event or EOF."""
    def run():
        for line in proc.stdout:
            lines.append(line)
            if line.startswith("{") and json.loads(line).get("event") == "exited":
                return
    reader = threading.Thread(target=run, daemon=True)
    reader.start()
    return reader


def wait_for_file(path, what):
    end = time.monotonic() + 120
    while not os.path.exists(path):
        if time.monotonic() > end:
            raise AssertionError("HUNG: " + what)
        time.sleep(0.05)


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

    def test_child_runs_while_stdin_is_open_and_stays_off_stdout(self):
        with tempfile.TemporaryDirectory() as tmp:
            marker = os.path.join(tmp, "ran.txt")
            code = "print('child-was-here', flush=True); open(%r, 'w').write('ran')" % marker
            proc = subprocess.Popen(
                [SPAWN, "run", sys.executable, "-c", code],
                stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True,
            )
            try:
                lines = []
                reader = read_until_exited(proc, lines)
                reader.join(120)
                if reader.is_alive():
                    self.fail("HUNG: the child did not exit while probe_spawn's stdin was open")
                ran = os.path.exists(marker)
                proc.stdin.close()
                try:
                    rest, err = proc.communicate(timeout=120)
                except subprocess.TimeoutExpired:
                    self.fail("HUNG: probe_spawn run did not end after its stdin closed")
            finally:
                if proc.poll() is None:
                    proc.kill()
                    proc.wait(timeout=120)
        self.assertTrue(ran)
        lines += rest.splitlines(True)
        self.assertEqual([line for line in lines if not line.startswith("{")], [])
        evs = events("".join(lines))
        self.assertEqual([e["event"] for e in evs], ["started", "exited"])
        self.assertEqual(evs[1]["code"], 0)
        self.assertIn("child-was-here", err)

    def test_exited_waits_for_a_grandchild(self):
        with tempfile.TemporaryDirectory() as tmp:
            marker = os.path.join(tmp, "grandchild.txt")
            grandchild = "import time; time.sleep(2); open(%r, 'w').write('done')" % marker
            leader = "import subprocess, sys; subprocess.Popen([sys.executable, '-c', %r]); sys.exit(5)" % grandchild
            try:
                proc = subprocess.run(
                    [SPAWN, "run", sys.executable, "-c", leader],
                    input="", capture_output=True, text=True, timeout=120,
                )
            except subprocess.TimeoutExpired:
                self.fail("HUNG: probe_spawn run did not report the leader's exit")
            evs = events(proc.stdout)
            self.assertEqual([e["event"] for e in evs], ["started", "exited"])
            self.assertEqual(evs[1]["code"], 5)
            self.assertTrue(os.path.exists(marker), "exited came while the grandchild was still running")

    def close_child(self, program, code, marker):
        """Runs program -c code, waits for it to write marker, then sends close; returns run's events."""
        proc = subprocess.Popen(
            [SPAWN, "run", program, "-c", code], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True,
        )
        try:
            wait_for_file(marker, "the child never wrote " + marker)
            proc.stdin.write("close\n")
            proc.stdin.flush()
            try:
                out, _ = proc.communicate(timeout=120)
            except subprocess.TimeoutExpired:
                self.fail("HUNG: probe_spawn run did not report the closed child's exit")
        finally:
            if proc.poll() is None:
                proc.kill()  # on Windows the job closes with it and ends the child
                proc.wait(timeout=120)
        return events(out)

    @unittest.skipUnless(os.name == "nt", "a Windows close posts WM_CLOSE to the child's windows")
    def test_close_posts_wm_close_to_the_childs_window(self):
        pythonw = os.path.join(os.path.dirname(sys.executable), "pythonw.exe")
        with tempfile.TemporaryDirectory() as tmp:
            marker = os.path.join(tmp, "up.txt")
            code = ("import tkinter; root = tkinter.Tk(); root.geometry('160x40'); root.update(); "
                    "open(%r, 'w').write('up'); root.mainloop()" % marker)
            evs = self.close_child(pythonw, code, marker)
        self.assertEqual([e["event"] for e in evs], ["started", "close_sent", "exited"])
        self.assertGreaterEqual(evs[1]["windows"], 1)
        self.assertEqual(evs[2]["code"], 0)
        self.assertGreaterEqual(evs[2]["ms_since_close"], 0)

    @unittest.skipUnless(os.name == "posix", "a Linux close sends SIGTERM to the child's process group")
    def test_close_sends_sigterm_to_the_group(self):
        with tempfile.TemporaryDirectory() as tmp:
            marker = os.path.join(tmp, "up.txt")
            code = ("import signal, sys, time; signal.signal(signal.SIGTERM, lambda *a: sys.exit(0)); "
                    "open(%r, 'w').write('up'); time.sleep(600)" % marker)
            evs = self.close_child(sys.executable, code, marker)
        self.assertEqual([e["event"] for e in evs], ["started", "close_sent", "exited"])
        self.assertEqual(evs[2]["code"], 0)
        self.assertGreaterEqual(evs[2]["ms_since_close"], 0)


if __name__ == "__main__":
    unittest.main()
