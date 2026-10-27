#!/usr/bin/env python3
"""Phase 0 setup on Linux: Chrome and Edge policy files and the native-messaging host manifests. Run with sudo."""
import argparse
import json
import os
import pathlib
import posixpath
import re
import sys

CWS_UPDATE = "https://clients2.google.com/service/update2/crx"
EDGE_UPDATE = "https://edge.microsoft.com/extensionwebstorebase/v1/crx"
HOST = "app.cptk.overlay"
GECKO = "overlay-probe@cptk.app"
OURS = "cptk-overlay.json"


def bad_ids(ids):
    """What is wrong with the ids file, or None. The same rules as windows.ps1."""
    for field in ("chrome_web_store", "edge_addons"):
        if not re.fullmatch(r"[a-p]{32}", str(ids.get(field, ""))):
            return f"{field} must be 32 letters a-p, got {ids.get(field)!r}"
    # The profile scripts name the sideloaded XPI after this id, and Firefox loads it only under that name.
    if ids.get("gecko") != GECKO:
        return f"gecko must be {GECKO}, got {ids.get('gecko')!r}"
    return None


def plan(ids, host_path, root, home):
    r = pathlib.Path(root)
    origins = [f"chrome-extension://{ids['chrome_web_store']}/", f"chrome-extension://{ids['edge_addons']}/"]
    base = {"name": HOST, "description": "CPTK Overlay Probe host", "path": host_path, "type": "stdio"}
    return {
        r / "etc/opt/chrome/policies/managed" / OURS: {
            "ExtensionInstallForcelist": [f"{ids['chrome_web_store']};{CWS_UPDATE}"],
        },
        r / "etc/opt/edge/policies/managed" / OURS: {
            "ExtensionInstallForcelist": [f"{ids['edge_addons']};{EDGE_UPDATE}"],
            "StartupBoostEnabled": False,
            "BackgroundModeEnabled": False,
        },
        r / f"etc/opt/chrome/native-messaging-hosts/{HOST}.json": {**base, "allowed_origins": origins},
        r / f"etc/opt/edge/native-messaging-hosts/{HOST}.json": {**base, "allowed_origins": origins},
        pathlib.Path(home) / f".mozilla/native-messaging-hosts/{HOST}.json": {**base, "allowed_extensions": [ids["gecko"]]},
    }


def conflicts(root, ours):
    """Other managed files that also set ExtensionInstallForcelist: Chromium takes the last file's list."""
    found = []
    for folder in ("etc/opt/chrome/policies/managed", "etc/opt/edge/policies/managed"):
        path = pathlib.Path(root) / folder
        if not path.is_dir():
            continue
        for f in sorted(path.glob("*.json")):
            if f.name == ours:
                continue
            try:
                data = json.loads(f.read_text())
            except (OSError, ValueError):
                continue
            if "ExtensionInstallForcelist" in data:
                found.append(str(f))
    return found


def missing_dirs(folder):
    """The folders mkdir(parents=True) would create for folder, outermost first."""
    missing = []
    folder = pathlib.Path(folder)
    while not folder.exists():
        missing.append(folder)
        folder = folder.parent
    return list(reversed(missing))


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--ids", required=True)
    ap.add_argument("--host", required=True)
    ap.add_argument("--root", default="/")
    ap.add_argument("--home", default=None)
    ap.add_argument("--remove", action="store_true")
    args = ap.parse_args(argv)
    if not posixpath.isabs(args.host):  # a Linux path, even when the tests run on Windows
        ap.error("--host must be an absolute path")
    ids = json.loads(pathlib.Path(args.ids).read_text())
    problem = bad_ids(ids)
    if problem:
        print(f"setup: ids file {args.ids}: {problem}", file=sys.stderr)
        return 1
    owner = None
    home = args.home
    if home is None:
        sudo_user = os.environ.get("SUDO_USER")
        if sudo_user:
            import pwd

            entry = pwd.getpwnam(sudo_user)
            home, owner = entry.pw_dir, (entry.pw_uid, entry.pw_gid)
        else:
            home = os.path.expanduser("~")
    files = plan(ids, args.host, args.root, home)
    for path, content in files.items():
        if args.remove:
            if path.exists():
                path.unlink()
            continue
        made = missing_dirs(path.parent)
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(json.dumps(content, indent=2) + "\n")
        os.chmod(path, 0o644)
        if owner and str(path).startswith(str(home)):
            # Under sudo every folder mkdir made is root's; give the user back each one it made in their home.
            for folder in made:
                os.chown(folder, *owner)
            os.chown(path, *owner)
    for other in conflicts(args.root, OURS):
        print(f"setup: another policy file also sets ExtensionInstallForcelist: {other}")
    print(f"setup: {'removed' if args.remove else 'wrote'} {len(files)} files")
    return 0


if __name__ == "__main__":
    sys.exit(main())
