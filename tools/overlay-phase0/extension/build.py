#!/usr/bin/env python3
"""Builds CPTK Overlay 0.0.1 (the Phase 0 probe) for Chromium (Chrome, Edge) and Firefox."""
import json
import pathlib
import shutil
import struct
import zipfile
import zlib

HERE = pathlib.Path(__file__).resolve().parent
VERSION = "0.0.1"

BASE = {
    "manifest_version": 3,
    "name": "CPTK Overlay",
    "version": VERSION,
    "description": (
        "Drives streaming sites with a TV remote inside CPTK's own browser profiles. "
        "It does nothing anywhere else."
    ),
    "permissions": ["nativeMessaging", "storage"],
    "host_permissions": ["<all_urls>"],
    "content_scripts": [{"matches": ["<all_urls>"], "js": ["content.js"], "run_at": "document_start"}],
    "icons": {"128": "icon128.png"},
}

FLAVORS = {
    "chromium": {"background": {"service_worker": "background.js"}},
    "firefox": {
        "background": {"scripts": ["background.js"]},
        "browser_specific_settings": {
            "gecko": {
                "id": "overlay@cptk.app",
                "strict_min_version": "140.0",
                "data_collection_permissions": {"required": ["none"]},
            }
        },
    },
}


def icon_png(size=128, rgb=(0x1F, 0x6F, 0xB2)):
    """A plain square icon, written without any imaging library."""
    row = b"\x00" + bytes(rgb) * size
    raw = row * size

    def chunk(kind, data):
        body = kind + data
        return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body) & 0xFFFFFFFF)

    header = struct.pack(">IIBBBBB", size, size, 8, 2, 0, 0, 0)
    return b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header) + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b"")


def build():
    dist = HERE / "dist"
    for flavor, extra in FLAVORS.items():
        out = dist / flavor
        if out.exists():
            shutil.rmtree(out)
        out.mkdir(parents=True)
        for name in ("content.js", "background.js"):
            shutil.copyfile(HERE / "src" / name, out / name)
        (out / "icon128.png").write_bytes(icon_png())
        manifest = {**BASE, **extra}
        (out / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
        archive = dist / f"cptk-overlay-{flavor}-{VERSION}.zip"
        with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED) as z:
            for path in sorted(out.iterdir()):
                z.write(path, path.name)
        print(f"built {archive}")


if __name__ == "__main__":
    build()
