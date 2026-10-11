#!/usr/bin/env python3
"""The headless mutant driver (plan Task 1.3). For each entry of a JSON list it puts one mutation into
a copy of the source, builds the launcher with the harness's test hooks (no sanitizers, for speed),
runs it on a fixture under Xvfb with -d, presses the entry's keys a second apart, ends it (TERM, then
KILL 10 s later), and judges its log. It runs in the streamflex-test image: /src the repository
(read-only; the lists are in /src/tests/mutants), /out the logs and the verdicts.
    python3 /src/tests/mutants/mutants.py /src/tests/mutants/<list>.json

verdict "holds": an unmutated control, whose expectations all hold. "fires": a failure forced, whose
failure branch's expectations all hold. "caught": the old form put back, whose expectations (the
check's) must NOT all hold: the check catches the mutant. A run that never got far enough to be
judged (no log at all, or keys to press and the line they wait for never came) is NOT-RUN, which is
wrong whatever its verdict: a mutant that stops the launcher before its check can look is not caught.
Each verdict is followed by every expectation, and whether it held."""
import json
import os
import platform
import re
import shutil
import subprocess
import sys
import time

SRC, WORK, OUT = "/src", "/work", "/out"
LOG = os.path.expanduser("~/.local/share/streamflex/streamflex.log")


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


def launch(entry, env):
    """Runs the entry; returns its exit code and whether the line its keys wait for came (True with no keys)"""
    fixture = entry["fixture"]
    source = fixture if fixture.startswith("/") else f"{WORK}/tests/headless/fixtures/{fixture}.ini"
    config = f"/tmp/{entry['name']}.ini"
    shutil.copyfile(source, config)   # A settings save writes here, never into /src
    if os.path.exists(LOG):
        os.remove(LOG)
    keys = entry.get("keys", [])
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
            if re.fullmatch(r"F\d+", key):
                subprocess.run(["python3", f"{WORK}/tests/headless/key.py", "key", key], env=env)
            else:
                subprocess.run(["xdotool", "key", key], env=env)
            time.sleep(1)
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


def main():
    sys.stdout.reconfigure(line_buffering=True)   # Each verdict shows as it comes, through docker's pipe
    entries = json.load(open(sys.argv[1], encoding="utf-8"))
    tree()
    xvfb = subprocess.Popen(["Xvfb", ":99", "-screen", "0", "1920x1080x24", "-ac"],
                            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    # lavapipe's ICD, as run.sh finds it (Debian names it lvp_icd.json, Fedora lvp_icd.<arch>.json),
    # and run.sh's stand-in for the display's VSync, which lavapipe's swapchain does not honor, so
    # frames come at the display's pace as in every harness run
    icd = [path for path in ("/usr/share/vulkan/icd.d/lvp_icd.json", f"/usr/share/vulkan/icd.d/lvp_icd.{platform.machine()}.json")
           if os.path.exists(path)]
    if not icd:
        print("NO LAVAPIPE FOUND")
        sys.exit(2)
    env = dict(os.environ, DISPLAY=":99", VK_DRIVER_FILES=icd[0], STREAMFLEX_TEST_SIMULATE_VSYNC="1")
    for _ in range(100):
        if subprocess.run(["xdotool", "getdisplaygeometry"], env=env, capture_output=True).returncode == 0:
            break
        time.sleep(0.2)
    wrong = 0
    touched = set()
    for entry in entries:
        for path in touched:   # Every file an earlier entry mutated goes back first
            shutil.copyfile(f"{SRC}/{path}", f"{WORK}/{path}")
        if entry.get("find") is not None:
            touched.add(entry["file"])
        if not mutate(entry):
            wrong += 1
            continue
        if subprocess.run(["cmake", "--build", f"{WORK}/build", "-j", str(os.cpu_count())],
                          capture_output=True).returncode != 0:
            print(f"BUILD FAILED {entry['name']}")
            wrong += 1
            continue
        code, pressed = launch(entry, dict(env, **entry.get("env", {})))
        text = open(f"{OUT}/{entry['name']}.log", encoding="utf-8", errors="replace").read()
        expects = [(rx, re.search(rx, text, re.M) is not None) for rx in entry.get("expect", [])]
        absents = [(rx, re.search(rx, text, re.M) is None) for rx in entry.get("absent", [])]
        exited = "exit" not in entry or code == entry["exit"]
        held = all(ok for _, ok in expects + absents) and exited
        if not text or not pressed:
            ok, mark = False, "NOT-RUN"
        else:
            ok = held if entry["verdict"] in ("holds", "fires") else not held
            mark = "OK     " if ok else "WRONG  "
        wrong += 0 if ok else 1
        why = "no log" if not text else "its keys never pressed: no line '" + entry.get("wait", "Begin program loop") + "'"
        print(f"{mark} {entry['name']} ({entry['verdict']}): exit {code}" + ("" if mark != "NOT-RUN" else f", {why}"))
        for rx, ok in expects:
            print(f"        expect {'held  ' if ok else 'FAILED'} {rx}")
        for rx, ok in absents:
            print(f"        absent {'held  ' if ok else 'FAILED'} {rx}")
        if "exit" in entry:
            print(f"        exit   {'held  ' if exited else 'FAILED'} {entry['exit']}")
    xvfb.terminate()
    print(f"{len(entries)} entries, {wrong} wrong")
    sys.exit(1 if wrong else 0)


if __name__ == "__main__":
    main()
