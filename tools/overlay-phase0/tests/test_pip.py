"""Tests for probe_spawn pip. PROBE_SPAWN names the built executable."""
import os
import subprocess
import sys
import tempfile
import threading
import time
import unittest

from test_spawn import EXIT_EVENTS, SPAWN, close_pipes, events

LINUX_SESSION_VARS = ("DISPLAY", "WAYLAND_DISPLAY", "XDG_SESSION_TYPE")


def run_spawn(test, args, env=None):
    """Runs probe_spawn with args and stdin closed; one that does not end inside the hang guard fails as HUNG."""
    try:
        return subprocess.run([SPAWN, *args], input="", capture_output=True, encoding="utf-8", timeout=120, env=env)
    except subprocess.TimeoutExpired:
        test.fail("HUNG: probe_spawn %s did not end" % " ".join(args[:2]))


def bare_env(**extra):
    """This environment without a display or a session type, plus extra."""
    env = {k: v for k, v in os.environ.items() if k not in LINUX_SESSION_VARS}
    env.update(extra)
    return env


class Lines:
    """Collects a running process's stdout lines on a thread, so a test can wait for one event at a time."""

    def __init__(self, proc):
        self.proc, self.lines = proc, []
        self.thread = threading.Thread(target=self.read, daemon=True)
        self.thread.start()

    def read(self):
        for line in self.proc.stdout:
            self.lines.append(line)

    def events(self):
        return events("".join(list(self.lines)))

    def wait_for(self, test, what, **fields):
        """Returns the first event whose fields equal fields; fails as HUNG, naming what, after the hang guard."""
        end = time.monotonic() + 120
        while True:
            for e in self.events():
                if all(e.get(k) == v for k, v in fields.items()):
                    return e
            if not self.thread.is_alive():
                test.fail("probe_spawn ended before %s: %s" % (what, "".join(self.lines)))
            if time.monotonic() > end:
                test.fail("HUNG: no %s" % what)
            time.sleep(0.05)


# A tkinter stand-in for a browser: a main window, then on cue a second window, its new title, and its close.
TK_CHILD = r'''
import os, sys, tkinter
tmp = sys.argv[1]
root = tkinter.Tk()
root.title("cptk main")
root.geometry("400x300+40+40")
phase = {"n": 0, "top": None}
def poll():
    n = phase["n"]
    if n == 0 and os.path.exists(os.path.join(tmp, "open")):
        phase["top"] = tkinter.Toplevel(root)
        phase["top"].title("cptk video")
        phase["top"].geometry("160x90+80+80")
        phase["n"] = 1
    elif n == 1 and os.path.exists(os.path.join(tmp, "retitle")):
        phase["top"].title("Picture-in-Picture")
        phase["n"] = 2
    elif n == 2 and os.path.exists(os.path.join(tmp, "shut")):
        phase["top"].destroy()
        phase["n"] = 3
    root.after(50, poll)
root.after(50, poll)
root.mainloop()
'''


@unittest.skipUnless(SPAWN, "set PROBE_SPAWN to the built probe_spawn")
class PipTest(unittest.TestCase):
    def test_classify_names_the_rule(self):
        cases = [
            (["Picture in picture"], True, "title"),  # Chrome and Edge, English
            (["Picture-in-Picture"], True, "title"),  # Firefox, English
            (["  PICTURE-IN-PICTURE  "], True, "title"),
            (["", "PictureInPicture"], True, "role"),  # Firefox on X11
            (["Shaka Player Demo - Google Chrome"], False, None),
            (["Picture in picture demo - Mozilla Firefox"], False, None),
            ([""], False, None),
        ]
        for args, pip, rule in cases:
            with self.subTest(args=args):
                proc = run_spawn(self, ["pip", "--classify", *args])
                self.assertEqual(proc.returncode, 0, proc.stderr)
                (ev,) = events(proc.stdout)
                self.assertEqual((ev["event"], ev["pip"], ev["rule"]), ("classify", pip, rule))

    def test_pip_needs_a_program(self):
        for args in (["pip"], ["pip", "--no-raise"], ["pip", "--no-close"], ["pip", "--no-close", "--no-raise"],
                     ["pip", "--classify"], ["pip", "--classify", "a", "b", "c"]):
            with self.subTest(args=args):
                proc = run_spawn(self, args)
                self.assertEqual(proc.returncode, 2)
                self.assertEqual(events(proc.stdout), [])
                self.assertIn("usage: probe_spawn pip", proc.stderr)

    def test_pip_supervises_like_run(self):
        proc = run_spawn(self, ["pip", sys.executable, "-c", "import sys; sys.exit(3)"], env=bare_env())
        self.assertEqual(proc.returncode, 0, proc.stderr)
        evs = [e for e in events(proc.stdout) if not e["event"].startswith("window")]
        self.assertEqual([e["event"] for e in evs], ["pip_session", "started"] + EXIT_EVENTS + ["pip_summary"])
        self.assertEqual(evs[-2]["code"], 3)
        self.assertEqual(evs[0]["session"], "windows" if os.name == "nt" else "none")
        self.assertEqual(evs[0]["can_list"], os.name == "nt")
        self.assertEqual(evs[-1]["listed"], os.name == "nt")
        self.assertEqual((evs[-1]["pip"], evs[-1]["closed"], evs[-1]["raised"], evs[-1]["foreground"]), (0, 0, 0, 0))

    def watch_tk(self, tmp, flags, steps):
        """Runs pip <flags> on TK_CHILD: waits for the main window, opens the second one, retitles it
        Picture-in-Picture and checks the watch re-classifies it, then calls steps(lines, main, video). Closes the
        child through run's close and returns every event."""
        pythonw = os.path.join(os.path.dirname(sys.executable), "pythonw.exe")
        proc = subprocess.Popen([SPAWN, "pip", *flags, pythonw, "-c", TK_CHILD, tmp],
                                stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                encoding="utf-8")
        try:
            lines = Lines(proc)
            main = lines.wait_for(self, "the main window's line", event="window", title="cptk main")
            self.assertFalse(main["pip"])
            open(os.path.join(tmp, "open"), "w").close()
            video = lines.wait_for(self, "the second window's line", event="window", title="cptk video")
            self.assertFalse(video["pip"])
            open(os.path.join(tmp, "retitle"), "w").close()
            changed = lines.wait_for(self, "the retitled window's line", event="window_changed", id=video["id"])
            self.assertEqual((changed["title"], changed["pip"], changed["rule"]), ("Picture-in-Picture", True, "title"))
            seen = lines.wait_for(self, "the pip_seen line", event="pip_seen", id=video["id"])
            self.assertEqual(seen["main"], main["id"])
            self.assertGreaterEqual(seen["ms_since_seen"], 0)
            steps(lines, main, video)
            proc.stdin.write("close\n")
            proc.stdin.flush()
            lines.thread.join(120)
            if lines.thread.is_alive():
                self.fail("HUNG: probe_spawn pip did not end after close")
            proc.wait(timeout=120)
        finally:
            if proc.poll() is None:
                proc.kill()  # the job closes with it and ends the child
                proc.wait(timeout=120)
            close_pipes(proc)
        evs = lines.events()
        self.assertEqual([e["event"] for e in evs][-2:], ["exited", "pip_summary"])
        self.assertEqual([e for e in evs if e["event"] == "raised"], [])
        return evs

    @unittest.skipUnless(os.name == "nt", "the Windows watch lists the job's windows")
    def test_pip_closes_a_retitled_window_and_names_the_main_one(self):
        def steps(lines, main, video):
            closed = lines.wait_for(self, "the pip_closed line", event="pip_closed", id=video["id"])
            self.assertEqual((closed["call_ok"], closed["method"]), (True, "WM_CLOSE"))
            self.assertGreaterEqual(closed["ms"], 0)
            skipped = lines.wait_for(self, "the raise_skipped line", event="raise_skipped", id=video["id"])
            self.assertEqual(skipped["why"], "no_raise")
            # tkinter destroys a toplevel on WM_CLOSE, as a browser closes its picture-in-picture window.
            lines.wait_for(self, "the closed window's window_gone line", event="window_gone", id=video["id"])
            gone = lines.wait_for(self, "the pip_gone line", event="pip_gone", id=video["id"])
            self.assertTrue(gone["closed"])
            self.assertGreaterEqual(gone["ms"], closed["ms"])

        with tempfile.TemporaryDirectory() as tmp:
            # --no-raise: the test never takes the focus; the close reaches only the test's own tkinter window.
            evs = self.watch_tk(tmp, ["--no-raise"], steps)
        summary = evs[-1]
        self.assertEqual((summary["listed"], summary["pip"], summary["closed"], summary["raised"]), (True, 1, 1, 0))
        self.assertGreaterEqual(summary["windows"], 2)

    @unittest.skipUnless(os.name == "nt", "the Windows watch lists the job's windows")
    def test_no_close_leaves_the_window_open(self):
        def steps(lines, main, video):
            skipped = lines.wait_for(self, "the close_skipped line", event="close_skipped", id=video["id"])
            self.assertEqual(skipped["why"], "no_close")
            lines.wait_for(self, "the raise_skipped line", event="raise_skipped", id=video["id"])
            open(os.path.join(tmp, "shut"), "w").close()
            gone = lines.wait_for(self, "the pip_gone line", event="pip_gone", id=video["id"])
            self.assertFalse(gone["closed"])

        with tempfile.TemporaryDirectory() as tmp:
            evs = self.watch_tk(tmp, ["--no-close", "--no-raise"], steps)
        self.assertEqual([e for e in evs if e["event"] == "pip_closed"], [])
        self.assertEqual((evs[-1]["pip"], evs[-1]["closed"]), (1, 0))


if __name__ == "__main__":
    unittest.main()
