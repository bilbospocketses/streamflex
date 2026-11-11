#!/usr/bin/env python3
"""Phase 0 setup on Linux: the Firefox native-messaging host manifest. Linux is Firefox only, so no browser policy is
written. Run it as the desktop user; under sudo, without --home, it writes into the invoking user's home and gives them
the files."""
import argparse
import json
import os
import pathlib
import posixpath
import sys

HOST = "app.cptk.overlay"
GECKO = "overlay-probe@cptk.app"


def bad_ids(ids):
    """What is wrong with the ids file for Linux, or None. Only the Gecko id is used here: the Chrome Web Store and
    Edge Add-ons ids in the same file are Windows-only."""
    # The profile scripts name the sideloaded XPI after this id, and Firefox loads it only under that name.
    if ids.get("gecko") != GECKO:
        return f"gecko must be {GECKO}, got {ids.get('gecko')!r}"
    return None


def plan(ids, host_path, home):
    manifest = {"name": HOST, "description": "CPTK Overlay Probe host", "path": host_path, "type": "stdio",
                "allowed_extensions": [ids["gecko"]]}
    return {pathlib.Path(home) / f".mozilla/native-messaging-hosts/{HOST}.json": manifest}


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
    for path, content in plan(ids, args.host, home).items():
        if args.remove:
            if path.exists():
                path.unlink()
                print(f"setup: removed {path}")
            else:
                print(f"setup: nothing to remove at {path}")
            continue
        made = missing_dirs(path.parent)
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(json.dumps(content, indent=2) + "\n")
        os.chmod(path, 0o644)
        if owner:
            # Under sudo every folder mkdir made is root's; give the user back each one it made in their home.
            for folder in made:
                os.chown(folder, *owner)
            os.chown(path, *owner)
        print(f"setup: wrote {path}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
