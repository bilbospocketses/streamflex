#!/usr/bin/env python3
"""Prepares a Firefox profile for the probe: CPTK's user.js lines and the sideloaded XPI. Run with Firefox closed."""
import argparse
import pathlib
import shutil
import sys

MARK = "// cptk-overlay"
PREFS = [
    ("extensions.autoDisableScopes", "0"),
    ("extensions.startupScanScopes", "1"),
    ("media.autoplay.default", "0"),
]


def prepare(profile, xpi, scan_scopes=True):
    """Returns True when the XPI was copied, False when the profile already had the same file."""
    profile = pathlib.Path(profile)
    profile.mkdir(parents=True, exist_ok=True)
    userjs = profile / "user.js"
    old = userjs.read_text(encoding="utf-8").splitlines() if userjs.exists() else []
    kept = [line for line in old if not line.rstrip().endswith(MARK)]
    ours = [
        f'user_pref("{name}", {value}); {MARK}'
        for name, value in PREFS
        if scan_scopes or name != "extensions.startupScanScopes"
    ]
    userjs.write_text("\n".join(kept + ours) + "\n", encoding="utf-8")
    extensions = profile / "extensions"
    extensions.mkdir(exist_ok=True)
    dest = extensions / "overlay@cptk.app.xpi"
    data = pathlib.Path(xpi).read_bytes()
    if dest.exists() and dest.read_bytes() == data:
        return False
    shutil.copyfile(xpi, dest)
    return True


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--profile", required=True)
    ap.add_argument("--xpi", required=True)
    ap.add_argument("--no-scan-scopes", action="store_true")
    args = ap.parse_args(argv)
    print("copied" if prepare(args.profile, args.xpi, not args.no_scan_scopes) else "unchanged")
    return 0


if __name__ == "__main__":
    sys.exit(main())
