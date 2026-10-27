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
# How run reports the end: Linux also reports the leader's exit and the empty process group, both before exited.
EXIT_EVENTS = ["leader_exited", "group_empty", "exited"] if os.name == "posix" else ["exited"]


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
        self.assertEqual([e["event"] for e in evs], ["started"] + EXIT_EVENTS)
        self.assertEqual(evs[-1]["code"], 3)
        self.assertEqual(evs[-1]["ms_since_close"], -1)
        if os.name == "posix":
            self.assertEqual(evs[1]["code"], 3)
            self.assertEqual(evs[1]["ms_since_close"], -1)
            self.assertEqual(evs[2]["ms_since_close"], -1)

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
            self.assertEqual([e["event"] for e in evs], ["kill_sent"] + EXIT_EVENTS)
            self.assertEqual(evs[-1]["code"], 137)
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
        self.assertEqual([e["event"] for e in evs], ["started"] + EXIT_EVENTS)
        self.assertEqual(evs[-1]["code"], 0)
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
            # The grandchild stays in the leader's group, so on Linux the group is empty only once it is gone too.
            self.assertEqual([e["event"] for e in evs], ["started"] + EXIT_EVENTS)
            self.assertEqual(evs[1]["code"], 5)
            self.assertEqual(evs[-1]["code"], 5)
            self.assertTrue(os.path.exists(marker), "exited came while the grandchild was still running")

    @unittest.skipUnless(os.name == "posix", "leader_exited and group_empty are Linux run's lines")
    def test_group_empty_comes_before_a_descendant_that_left_the_group(self):
        with tempfile.TemporaryDirectory() as tmp:
            release, marker = os.path.join(tmp, "release"), os.path.join(tmp, "done")
            stray = ("import os, time\nwhile not os.path.exists(%r):\n    time.sleep(0.05)\nopen(%r, 'w').write('done')"
                     % (release, marker))
            leader = ("import subprocess, sys; subprocess.Popen([sys.executable, '-c', %r], start_new_session=True); "
                      "sys.exit(4)" % stray)
            proc = subprocess.Popen(
                [SPAWN, "run", sys.executable, "-c", leader], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True,
            )
            try:
                lines = []
                reader = read_until_exited(proc, lines)
                end = time.monotonic() + 120
                while not any('"group_empty"' in line for line in list(lines)):
                    if not reader.is_alive():
                        self.fail("run ended with no group_empty line: " + "".join(lines))
                    if time.monotonic() > end:
                        self.fail("HUNG: no group_empty line while the stray descendant waited")
                    time.sleep(0.05)
                early = [e["event"] for e in events("".join(list(lines)))]
                stray_done_early = os.path.exists(marker)
                open(release, "w").close()
                reader.join(120)
                if reader.is_alive():
                    self.fail("HUNG: no exited line after the stray descendant was released")
                stray_done = os.path.exists(marker)
            finally:
                open(release, "w").close()  # never leave the stray waiting
                if proc.poll() is None:
                    proc.kill()
                    proc.wait(timeout=120)
        self.assertEqual(early, ["started", "leader_exited", "group_empty"])
        self.assertFalse(stray_done_early, "the stray finished before group_empty was read")
        evs = events("".join(lines))
        self.assertEqual([e["event"] for e in evs], ["started", "leader_exited", "group_empty", "exited"])
        self.assertEqual(evs[1]["code"], 4)
        self.assertEqual(evs[3]["code"], 4)
        self.assertTrue(stray_done, "exited came while the stray descendant was still running")

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
        self.assertEqual([e["event"] for e in evs], ["started", "close_sent"] + EXIT_EVENTS)
        self.assertEqual(evs[2]["code"], 0)
        for e in evs[2:]:
            self.assertGreaterEqual(e["ms_since_close"], 0, e["event"])
        self.assertEqual(evs[-1]["code"], 0)


@unittest.skipUnless(SPAWN, "set PROBE_SPAWN to the built probe_spawn")
class KeysTest(unittest.TestCase):
    def test_dry_run_names_every_key_of_a_chord(self):
        proc = subprocess.run(
            [SPAWN, "keys", "--dry-run", "0", "Up", "Alt+F4"], capture_output=True, text=True, timeout=120
        )
        self.assertEqual(proc.returncode, 0)
        evs = events(proc.stdout)
        self.assertEqual([e["name"] for e in evs], ["Up", "Alt", "F4"])
        self.assertEqual(evs[0]["vk"], 0x26)
        self.assertEqual(evs[0]["evdev"], 103)
        self.assertTrue(all(e["dry"] for e in evs))

    def test_unknown_chord_part_injects_nothing(self):
        proc = subprocess.run(
            [SPAWN, "keys", "--dry-run", "0", "Up", "Alt+Foo"], capture_output=True, text=True, timeout=120
        )
        self.assertEqual(proc.returncode, 2)
        self.assertEqual([e["event"] for e in events(proc.stdout)], ["unknown_key"])

    def test_unknown_key_name_is_json_escaped(self):
        proc = subprocess.run(
            [SPAWN, "keys", "--dry-run", "0", 'Up+Fo"o\\'], capture_output=True, text=True, timeout=120
        )
        self.assertEqual(proc.returncode, 2)
        self.assertEqual([e.get("name") for e in events(proc.stdout)], ['Fo"o\\'])

    def test_negative_delay_is_refused(self):
        proc = subprocess.run(
            [SPAWN, "keys", "--dry-run", "-1", "Up"], capture_output=True, text=True, timeout=120
        )
        self.assertEqual(proc.returncode, 2)
        self.assertEqual(events(proc.stdout), [])

    def test_non_numeric_delay_is_refused(self):
        for delay in ["Up", "5x", ""]:
            with self.subTest(delay=delay):
                proc = subprocess.run(
                    [SPAWN, "keys", "--dry-run", delay, "Down"], capture_output=True, text=True, timeout=120
                )
                self.assertEqual(proc.returncode, 2)
                self.assertEqual(events(proc.stdout), [])
                self.assertIn("usage:", proc.stderr)

    def test_delay_is_decimal_and_bounded(self):
        proc = subprocess.run(
            [SPAWN, "keys", "--dry-run", "010", "Up"], capture_output=True, text=True, timeout=120
        )
        self.assertEqual(proc.returncode, 0)
        self.assertEqual([e["delay_ms"] for e in events(proc.stdout)], [10])  # not octal 8
        for delay in ["2147483648", "99999999999999999999"]:
            with self.subTest(delay=delay):
                proc = subprocess.run(
                    [SPAWN, "keys", "--dry-run", delay, "Up"], capture_output=True, text=True, timeout=120
                )
                self.assertEqual(proc.returncode, 2)
                self.assertEqual(events(proc.stdout), [])
                self.assertIn("usage:", proc.stderr)

    def test_keylog_refuses_a_bad_seconds_value(self):
        for seconds in ["x", "1x", "", "-1"]:
            with self.subTest(seconds=seconds):
                try:
                    proc = subprocess.run([SPAWN, "keylog", seconds], capture_output=True, text=True, timeout=120)
                except subprocess.TimeoutExpired:
                    self.fail("HUNG: keylog %r did not refuse its seconds value" % seconds)
                self.assertEqual(proc.returncode, 2)
                self.assertEqual(events(proc.stdout), [])
                self.assertIn("usage:", proc.stderr)

    def test_keylog_ends_on_time(self):
        proc = subprocess.run([SPAWN, "keylog", "1"], capture_output=True, text=True, timeout=120)
        self.assertEqual(events(proc.stdout)[-1]["event"], "keylog_end")


@unittest.skipUnless(SPAWN, "set PROBE_SPAWN to the built probe_spawn")
class SdlProbeTest(unittest.TestCase):
    """Every run uses SDL's dummy video driver, so no window reaches the desktop."""

    def probe(self, args, **env):
        try:
            return subprocess.run(
                [SPAWN] + args, capture_output=True, text=True, timeout=120,
                env=dict(os.environ, SDL_VIDEO_DRIVER="dummy", **env),
            )
        except subprocess.TimeoutExpired:
            self.fail("HUNG: probe_spawn %s did not end" % " ".join(args))

    def test_pad_runs_without_a_display(self):
        proc = self.probe(["pad", "1", "default"])
        self.assertEqual(proc.returncode, 0)
        evs = events(proc.stdout)
        # One focus line, the start state: the window's own opening churn is not reported. A plugged-in pad may add
        # pad_added lines, so only the focus lines are pinned.
        self.assertEqual(evs[0]["event"], "focus")
        self.assertEqual([e["event"] for e in evs].count("focus"), 1)
        self.assertEqual(evs[-1]["event"], "pad_end")

    def test_pad_reports_a_renderer_failure(self):
        proc = self.probe(["pad", "1", "default"], SDL_RENDER_DRIVER="nosuchrenderer")
        self.assertEqual(proc.returncode, 1)
        self.assertEqual([e["event"] for e in events(proc.stdout)], ["sdl_failed"])

    def test_pad_refuses_an_unknown_backend(self):
        proc = self.probe(["pad", "1", "dinput"])
        self.assertEqual(proc.returncode, 2)
        self.assertEqual(events(proc.stdout), [])
        self.assertIn("usage:", proc.stderr)

    def test_pad_refuses_a_bad_seconds_value(self):
        for seconds in ["x", "1x", "", "-1", "86401"]:
            with self.subTest(seconds=seconds):
                proc = self.probe(["pad", seconds, "default"])
                self.assertEqual(proc.returncode, 2)
                self.assertEqual(events(proc.stdout), [])
                self.assertIn("usage:", proc.stderr)

    def test_focus_reports_a_renderer_failure(self):
        proc = self.probe(["focus", sys.executable, "-c", "pass"], SDL_RENDER_DRIVER="nosuchrenderer")
        self.assertEqual(proc.returncode, 1)
        self.assertEqual([e["event"] for e in events(proc.stdout)], ["sdl_failed"])

    def test_focus_reports_a_missing_program(self):
        with tempfile.TemporaryDirectory() as tmp:
            proc = self.probe(["focus", os.path.join(tmp, "no-such-program")])
        self.assertEqual(proc.returncode, 1)
        self.assertEqual([e["event"] for e in events(proc.stdout)], ["spawn_failed"])

    def test_focus_reports_only_what_it_measured(self):
        code = "import sys; print('child-was-here'); sys.stderr.write('child-err'); sys.exit(7)"
        proc = self.probe(["focus", sys.executable, "-c", code])
        self.assertEqual(proc.returncode, 0)
        self.assertEqual([line for line in proc.stdout.splitlines() if not line.startswith("{")], [])
        self.assertIn("child-err", proc.stderr)
        evs = events(proc.stdout)
        self.assertEqual([e["event"] for e in evs], ["started", "exited", "focus_after_exit", "key_after_exit"])
        self.assertEqual(evs[1]["code"], 7)
        # The dummy driver never moves focus, so the probe must not claim it was lost or won back.
        self.assertFalse(evs[2]["lost"])
        self.assertEqual(evs[2]["lost_ms"], -1)
        self.assertFalse(evs[2]["back_before_raise"])
        self.assertFalse(evs[2]["gained"])
        self.assertEqual(evs[2]["ms"], -1)
        self.assertIsInstance(evs[2]["raised"], bool)
        self.assertEqual(evs[3]["ms"], -1)


if __name__ == "__main__":
    unittest.main()
