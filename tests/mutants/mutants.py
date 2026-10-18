#!/usr/bin/env python3
"""The mutant driver (plan Task 1.3; unit entries and controls, 3c Task 16). For each entry of a JSON list
it puts one mutation into a copy of the source and judges a run of it. It runs in the streamflex-test
image: /src the repository (read-only; the lists are in /src/tests/mutants), /out the logs and the verdicts.
    python3 /src/tests/mutants/mutants.py [--controls] [--kind unit|headless] [--find-only] /src/tests/mutants/<list>.json
A list with unit entries needs the seccomp profile off (docker run --security-opt seccomp=unconfined), as
tests/unit.sh does.

A headless entry (one with no "unit") builds the launcher with the harness's test hooks (no sanitizers, for
speed), runs it on a fixture under Xvfb with -d, presses the entry's keys a second apart, ends it (TERM,
then KILL 10 s later), and judges its log.

verdict "holds": an unmutated control, whose expectations all hold. "fires": a failure forced, whose
failure branch's expectations all hold. "caught": the old form put back, whose expectations (the
check's) must NOT all hold: the check catches the mutant. A run that never got far enough to be
judged is NOT-RUN, which is wrong whatever its verdict: one with no log at all, one with keys to press
whose wait line never came, and one whose entry sets wait when that line is not in its log. A "caught"
entry's expectations cannot show that the launcher reached the site, as one failing is enough, so a
"caught" entry with no keys must set wait to a line the run prints only if it reached the site: at the
site, or just past it on the path the mutated form takes, or just before it when nothing at or past it
is printed; never a line only the success path the old form skips prints. A mutant that stops the
launcher before then is NOT-RUN, never caught. One that sets no wait is wrong, and not run. A keyed
entry's wait is the line its first key waits for (Begin program loop by default).
Each verdict is followed by every expectation, and whether it held.

A key is an X key name, pressed a second before the next. Words with spaces between them ("Left Right")
are pressed in one xdotool call, as fast as it sends them. A key starting with + is a step instead,
followed by no second's wait: "+write PATH TEXT" puts TEXT and a newline in PATH in one step (a rename),
for a hook's file; "+wait LINE" waits up to 30 s for LINE in the log, and a LINE that never comes ends
the keys and makes the run NOT-RUN, so a +wait names only a line a correct run and its mutant both
print; "+stop" and "+cont" stop the launcher and let it go on, so the keys between reach it in one
pass of its loop; "+sleep S" waits S seconds. For check 84's runs (3c Task 12): "+type TEXT" types TEXT
(xdotool type); "+hold KEY SECONDS" holds KEY down that long, then lets it go; "+key KEYSYM" presses KEYSYM
by its keycode with no modifier (tests/headless/key.py); "+setxkb LAYOUT" sets the X keyboard layout;
"+pixel TAG x,y=r,g,b ..." looks for those colors at those points for up to 10 s and adds the line
"PIXEL TAG yes" or "PIXEL TAG no" to the text judged, after the log.

A unit entry ("unit": a ctest -R regex) runs tests/unit.sh with that filter, building only the tests it
names ("targets", or test_<name> for each name in the regex), its find put into unit.sh's copy through
MUTANT_FIND and MUTANT_REPLACE, and judges unit.sh's output. "holds": unit.sh exits 0 and every
expectation is in its output. "caught" (or "fires"): unit.sh exits non-zero and every expectation, the
failure a test must report, is in its output; a run that failed another way is WRONG. A run that stopped
before its tests (MUTANT NOT APPLIED, CONFIGURE FAILED, BUILD FAILED) is NOT-RUN. Each verdict gives
ctest's count of failed tests. Every unit entry runs before the first headless one, since unit.sh makes
/work afresh.

--controls: each "caught" or "fires" entry is also judged against its control, the same run with nothing
mutated (for a unit entry its filter and targets; for a headless one its fixture, keys, env, wait and
seconds), which runs once for each setup and is kept; an unmutated "holds" entry with that setup is that
run. A "caught" headless entry's control must hold the entry's own expectations, a "fires" entry's must
not, and a unit entry's control must exit 0 with none of the entry's expectations in its output;
otherwise the entry is CONTROL-FAILED, which is wrong whatever the mutant did, since its verdict could
then be the driver's or the expectation's, not the check's.
--kind runs only that kind of entry, so the two kinds can run at once in two containers.
--find-only counts each entry's find in /src and runs nothing: a find that is not there "count" times
(1 when unset) is wrong."""
import argparse
import json
import os
import platform
import re
import shutil
import signal
import subprocess
import sys
import time

SRC, WORK, OUT = "/src", "/work", "/out"
LOG = os.path.expanduser("~/.local/share/streamflex/streamflex.log")
STOPPED = ("MUTANT NOT APPLIED", "CONFIGURE FAILED", "BUILD FAILED")   # unit.sh's lines for a run that never tested
SETUP = ("fixture", "keys", "env", "wait", "seconds")                  # What a headless entry's control shares
SIDE = []   # Lines a run's steps add to the text judged (a +pixel's verdict)


def tree():
    shutil.rmtree(WORK, ignore_errors=True)
    os.makedirs(WORK)
    tar = subprocess.Popen(["tar", "-C", SRC, "--exclude=./build", "--exclude=./.git", "--exclude=./headless-out",
                            "--exclude=./.superpowers", "-cf", "-", "."], stdout=subprocess.PIPE)
    subprocess.run(["tar", "-C", WORK, "-xf", "-"], stdin=tar.stdout, check=True)
    tar.wait()
    subprocess.run(["cmake", "-S", WORK, "-B", f"{WORK}/build", "-DCMAKE_BUILD_TYPE=Debug",
                    "-DCMAKE_C_FLAGS=-DSTREAMFLEX_TEST_HOOKS"], check=True, stdout=subprocess.DEVNULL)


def mutate(entry):
    if entry.get("find") is None:
        return True
    path = f"{WORK}/{entry['file']}"
    text = open(path, encoding="utf-8").read()
    found = text.count(entry["find"])
    if found != entry.get("count", 1):
        print(f"MUTANT NOT APPLIED {entry['name']}: the text is in {entry['file']} {found} times")
        return False
    open(path, "w", encoding="utf-8", newline="\n").write(text.replace(entry["find"], entry["replace"]))
    return True


def log_text():
    return open(LOG, encoding="utf-8", errors="replace").read() if os.path.exists(LOG) else ""


def step(key, process, env):
    """Runs a + step of a key list (see above); False when a +wait's line never came"""
    verb, _, rest = key[1:].partition(" ")
    if verb == "type":
        subprocess.run(["xdotool", "type", "--delay", "60", rest], env=env)
        time.sleep(1)
    elif verb == "hold":
        name, _, seconds = rest.partition(" ")
        subprocess.run(["xdotool", "keydown", name], env=env)
        time.sleep(float(seconds))
        subprocess.run(["xdotool", "keyup", name], env=env)
        time.sleep(1)
    elif verb == "key":
        subprocess.run(["python3", f"{WORK}/tests/headless/key.py", "key", rest], env=env)
        time.sleep(0.3)
    elif verb == "setxkb":
        subprocess.run(["setxkbmap", "-layout", rest], env=env)
        time.sleep(1)
    elif verb == "pixel":
        tag, _, points = rest.partition(" ")
        seen, read = "no", None
        for _ in range(50):
            shot = subprocess.run(["xwd", "-root", "-silent", "-out", "/tmp/mutant.xwd"], env=env).returncode == 0
            read = subprocess.run(["python3", f"{WORK}/tests/headless/pixels.py", "/tmp/mutant.xwd", *points.split()],
                                  capture_output=True, text=True, errors="replace") if shot else None
            if read is not None and read.returncode == 0:
                seen = "yes"
                break
            time.sleep(0.2)
        SIDE.append(f"PIXEL {tag} {seen}" + ("" if read is None else " " + read.stdout.strip().replace("\n", "; ")))
    elif verb == "write":
        path, _, text = rest.partition(" ")
        with open(f"{path}.new", "w", encoding="utf-8", newline="\n") as hook:
            hook.write(text + "\n")
        os.replace(f"{path}.new", path)
    elif verb == "wait":
        end = time.time() + 30
        while time.time() < end and process.poll() is None and rest not in log_text():
            time.sleep(0.2)
        return rest in log_text()
    elif verb == "stop":
        process.send_signal(signal.SIGSTOP)
    elif verb == "cont":
        process.send_signal(signal.SIGCONT)
    elif verb == "sleep":
        time.sleep(float(rest))
    else:
        raise ValueError(f"unknown step {key!r}")
    return True


def launch(entry, env):
    """Runs the entry; returns its exit code and whether the line its keys wait for came (True with no keys)"""
    fixture = entry["fixture"]
    source = fixture if fixture.startswith("/") else f"{WORK}/tests/headless/fixtures/{fixture}.ini"
    config = f"/tmp/{entry['name']}.ini"
    shutil.copyfile(source, config)   # A settings save writes here, never into /src
    if os.path.exists(LOG):
        os.remove(LOG)
    SIDE.clear()
    keys = entry.get("keys", [])
    for key in keys:   # A hook file an earlier run of this setup wrote would be read as the launcher starts
        if key.startswith("+write "):
            hook = key[len("+write "):].partition(" ")[0]
            if os.path.exists(hook):
                os.remove(hook)
    with open(f"{OUT}/{entry['name']}.err", "w") as err:
        process = subprocess.Popen([f"{WORK}/build/streamflex", "-c", config, "-d"], env=env,
                                   stdout=subprocess.DEVNULL, stderr=err)
        end = time.time() + entry.get("seconds", 20)
        wait = entry.get("wait", "Begin program loop")
        while time.time() < end and process.poll() is None and wait not in log_text():
            time.sleep(0.2)
        pressed = not keys or wait in log_text()
        for key in keys if pressed else []:
            if process.poll() is not None:
                break
            if key.startswith("+"):
                if not step(key, process, env):
                    pressed = False
                    break
                continue
            if re.fullmatch(r"F\d+", key):
                subprocess.run(["python3", f"{WORK}/tests/headless/key.py", "key", key], env=env)
            else:
                subprocess.run(["xdotool", "key", *key.split()], env=env)
            time.sleep(1)
        if process.poll() is None:
            process.send_signal(signal.SIGCONT)   # A run left stopped would never read its TERM
        time.sleep(2)
        if process.poll() is None:
            process.terminate()
            try:
                process.wait(10)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
    shutil.copyfile(LOG, f"{OUT}/{entry['name']}.log") if os.path.exists(LOG) else open(f"{OUT}/{entry['name']}.log", "w").close()
    return process.returncode, pressed


def find_only(entries):
    """Counts each entry's find in /src; returns how many are not there exactly as often as they must be"""
    wrong = 0
    for entry in entries:
        if entry.get("find") is None:
            continue
        path = f"{SRC}/{entry['file']}"
        found = open(path, encoding="utf-8").read().count(entry["find"]) if os.path.isfile(path) else -1
        ok = found == entry.get("count", 1)
        wrong += 0 if ok else 1
        where = f"{entry['file']} is missing" if found < 0 else f"{entry['file']} holds the text {found} times"
        print(f"{'OK     ' if ok else 'WRONG  '} {entry['name']}: {where}")
    return wrong


def unit_targets(entry):
    return entry.get("targets") or " ".join(f"test_{name}" for name in re.findall(r"\w+", entry["unit"]))


def unit_run(entry, name):
    """Runs tests/unit.sh for the entry, its mutation put in unless it has none; returns its exit code and output"""
    env = dict(os.environ, UNIT_TARGETS=unit_targets(entry))
    if entry.get("find") is not None:
        env.update(MUTANT_FILE=entry["file"], MUTANT_FIND=entry["find"], MUTANT_REPLACE=entry["replace"],
                   MUTANT_COUNT=str(entry.get("count", 1)))
    run = subprocess.run(["bash", f"{SRC}/tests/unit.sh", entry["unit"]], env=env, stdout=subprocess.PIPE,
                         stderr=subprocess.STDOUT, text=True, errors="replace")
    with open(f"{OUT}/{name}.unit.log", "w", encoding="utf-8") as out:
        out.write(run.stdout)
    return run.returncode, run.stdout


def failed_tests(text):
    """ctest's count of failed tests in a unit.sh output, or None when ctest never summed up"""
    found = re.findall(r"tests passed, (\d+) tests? failed out of \d+", text)
    return int(found[-1]) if found else None


def run_units(entries, controls):
    wrong, kept = 0, {}

    def control_of(entry):
        key = (entry["unit"], unit_targets(entry))
        if key not in kept:
            kept[key] = unit_run(dict(entry, find=None), entry["name"] if entry.get("find") is None else f"control.{entry['name']}")
        return kept[key]

    for entry in entries:
        code, text = control_of(entry) if entry.get("find") is None else unit_run(entry, entry["name"])
        expects = [(rx, re.search(rx, text, re.M) is not None) for rx in entry.get("expect", [])]
        held = all(ok for _, ok in expects)
        stopped = next((line for line in STOPPED if line in text), None)
        control = None
        if controls and entry["verdict"] != "holds":
            c_code, c_text = control_of(entry)
            control = (c_code == 0 and not any(line in c_text for line in STOPPED)
                       and not any(re.search(rx, c_text, re.M) for rx, _ in expects))
        if stopped:
            ok, mark = False, "NOT-RUN"
        elif control is False:
            ok, mark = False, "CONTROL-FAILED"
        else:
            ok = (code == 0 and held) if entry["verdict"] == "holds" else (code != 0 and held)
            mark = "OK     " if ok else "WRONG  "
        wrong += 0 if ok else 1
        failed = failed_tests(text)
        tests = f"{failed} test(s) failed" if failed is not None else "ctest never ran"
        print(f"{mark} {entry['name']} ({entry['verdict']}): exit {code}, {tests}" + (f", {stopped}" if stopped else ""))
        for rx, ok in expects:
            print(f"        expect {'held  ' if ok else 'FAILED'} {rx}")
        if control is not None:
            print(f"        control {'held  ' if control else 'FAILED'} {entry['unit']} unmutated: exit 0, and none of the expectations")
    return wrong


def setup_of(entry):
    return json.dumps({key: entry.get(key) for key in SETUP}, sort_keys=True)


def judged(entry, code, pressed, text):
    """A headless run's log against the entry's expectations: (held, reached, expects, absents, exited)"""
    expects = [(rx, re.search(rx, text, re.M) is not None) for rx in entry.get("expect", [])]
    absents = [(rx, re.search(rx, text, re.M) is None) for rx in entry.get("absent", [])]
    exited = "exit" not in entry or code == entry["exit"]
    held = all(ok for _, ok in expects + absents) and exited
    reached = bool(text) and pressed and ("wait" not in entry or entry["wait"] in text)
    return held, reached, expects, absents, exited


def run_headless(entries, controls):
    tree()
    xvfb = subprocess.Popen(["Xvfb", ":99", "-screen", "0", "1920x1080x24", "-ac"],
                            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    # lavapipe's ICD, as run.sh finds it (Debian names it lvp_icd.json, Fedora lvp_icd.<arch>.json)
    icd = [path for path in ("/usr/share/vulkan/icd.d/lvp_icd.json", f"/usr/share/vulkan/icd.d/lvp_icd.{platform.machine()}.json")
           if os.path.exists(path)]
    if not icd:
        print("NO LAVAPIPE FOUND")
        sys.exit(2)
    env = dict(os.environ, DISPLAY=":99", VK_DRIVER_FILES=icd[0])
    for _ in range(100):
        if subprocess.run(["xdotool", "getdisplaygeometry"], env=env, capture_output=True).returncode == 0:
            break
        time.sleep(0.2)
    wrong, touched, kept = 0, set(), {}

    def run(entry, name):
        """Builds the entry's mutation (none when its find is None) and runs it: (code, pressed, log), or None"""
        for path in touched:   # Every file an earlier entry mutated goes back first
            shutil.copyfile(f"{SRC}/{path}", f"{WORK}/{path}")
        if entry.get("find") is not None:
            touched.add(entry["file"])
        if not mutate(entry):
            return None
        if subprocess.run(["cmake", "--build", f"{WORK}/build", "-j", str(os.cpu_count())],
                          capture_output=True).returncode != 0:
            print(f"BUILD FAILED {name}")
            return None
        code, pressed = launch(dict(entry, name=name), dict(env, **entry.get("env", {})))
        for line in SIDE:
            print(f"        {name}: {line}")
        text = open(f"{OUT}/{name}.log", encoding="utf-8", errors="replace").read()
        return code, pressed, text + "".join(line + "\n" for line in SIDE)

    def control_of(entry):
        key = setup_of(entry)
        if key not in kept:
            kept[key] = run(dict(entry, find=None), entry["name"] if entry.get("find") is None else f"control.{entry['name']}")
        return kept[key]

    for entry in entries:
        if entry["verdict"] == "caught" and not entry.get("keys") and "wait" not in entry:
            print(f"NO WITNESS {entry['name']} (caught): no keys, and no wait line to show the run reached its site")
            wrong += 1
            continue
        result = control_of(entry) if entry.get("find") is None else run(entry, entry["name"])
        if result is None:
            wrong += 1
            continue
        code, pressed, text = result
        held, reached, expects, absents, exited = judged(entry, code, pressed, text)
        control = None
        if controls and entry["verdict"] != "holds":
            ran = control_of(entry)
            if ran is None:
                control = False
            else:
                c_held, c_reached = judged(entry, *ran)[:2]
                control = c_reached and (c_held if entry["verdict"] == "caught" else not c_held)
        if not reached:
            ok, mark = False, "NOT-RUN"
        elif control is False:
            ok, mark = False, "CONTROL-FAILED"
        else:
            ok = held if entry["verdict"] in ("holds", "fires") else not held
            mark = "OK     " if ok else "WRONG  "
        wrong += 0 if ok else 1
        why = "no log" if not text else f"no line '{entry.get('wait', 'Begin program loop')}'" + ("" if pressed else ", so its keys were never pressed")
        print(f"{mark} {entry['name']} ({entry['verdict']}): exit {code}" + ("" if mark != "NOT-RUN" else f", {why}"))
        for rx, ok in expects:
            print(f"        expect {'held  ' if ok else 'FAILED'} {rx}")
        for rx, ok in absents:
            print(f"        absent {'held  ' if ok else 'FAILED'} {rx}")
        if "exit" in entry:
            print(f"        exit   {'held  ' if exited else 'FAILED'} {entry['exit']}")
        if control is not None:
            what = "held its expectations" if entry["verdict"] == "caught" else "did not hold them"
            print(f"        control {'held  ' if control else 'FAILED'} the same run unmutated {what}")
    xvfb.terminate()
    return wrong


def main():
    sys.stdout.reconfigure(line_buffering=True)   # Each verdict shows as it comes, through docker's pipe
    parser = argparse.ArgumentParser(description="Each entry's mutation, run and judged (see the docstring).")
    parser.add_argument("list", help="the JSON list of entries")
    parser.add_argument("--controls", action="store_true", help="judge each caught or fires entry against its unmutated run")
    parser.add_argument("--kind", choices=("unit", "headless"), help="run only that kind of entry")
    parser.add_argument("--find-only", action="store_true", help="count each find in /src, and run nothing")
    args = parser.parse_args()
    entries = json.load(open(args.list, encoding="utf-8"))
    if args.kind:
        entries = [entry for entry in entries if ("unit" in entry) == (args.kind == "unit")]
    if args.find_only:
        wrong = find_only(entries)
    else:
        units = [entry for entry in entries if "unit" in entry]
        shows = [entry for entry in entries if "unit" not in entry]
        wrong = run_units(units, args.controls) if units else 0
        wrong += run_headless(shows, args.controls) if shows else 0
    print(f"{len(entries)} entries, {wrong} wrong")
    sys.exit(1 if wrong else 0)


if __name__ == "__main__":
    main()
