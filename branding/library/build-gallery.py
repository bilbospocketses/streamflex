"""Write the docs site's icon gallery from the shipped manifest. pages.yml runs it before Jekyll.

  python branding/library/build-gallery.py [--page docs/icons.md] [--images docs/assets/library]

Copies every icon into the site and writes one page, grouped by the manifest's groups. Both outputs are
gitignored, so the repository keeps one copy of each icon. Standard library only.
"""
import argparse
import html
import pathlib
import shutil
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import libtools  # noqa: E402

GROUPS = [
    ("video", "Subscription video"), ("free-tv", "Free and live TV"), ("servers", "Media servers and players"),
    ("music", "Music and audio"), ("games", "Games"), ("web", "Web browsers"),
    ("system", "System"), ("media", "Kinds of media"), ("general", "General"), ("devices", "Devices"),
]

HEAD = """---
layout: default
title: Icon Library
---
# Icon Library

StreamFlex ships these icons. To use one, write its name where an entry's icon path would go:

~~~ini
[Main]
Entry1=Netflix;netflix;...
Entry2=Movies;movies;:submenu Movies
~~~

A name is lowercase letters, digits and hyphens. Anything else in that field is read as the path to your own
image file, as before. See [Creating Menus](configuration#creating-menus).

<style>
.icon-grid {{ display: flex; flex-wrap: wrap; gap: 18px; margin: 12px 0 28px; }}
.icon-grid figure {{ margin: 0; width: 112px; text-align: center; }}
.icon-grid img {{ width: 96px; height: 96px; }}
.icon-grid figcaption {{ font-size: 13px; line-height: 1.3; }}
</style>

"""

BRAND_NOTE = """The service icons below are the trademarks and artwork of their owners, shown only to identify each
service. They are not covered by StreamFlex's GPL-3.0 license; see the
[brand notice](https://github.com/bilbospocketses/streamflex/blob/master/assets/icons/library/brands/NOTICE.md).

"""


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--page", default=str(libtools.ROOT / "docs" / "icons.md"))
    parser.add_argument("--images", default=str(libtools.ROOT / "docs" / "assets" / "library"))
    args = parser.parse_args()

    sections = libtools.read_sections(libtools.LIBRARY / "icons.ini")
    images = pathlib.Path(args.images)
    if images.exists():
        shutil.rmtree(images)
    parts = [HEAD.format()]
    for group, heading in GROUPS:
        members = [(name, keys) for name, keys in sections if keys.get("group") == group]
        if not members:
            continue
        if group == "video":
            parts.append(BRAND_NOTE)
        parts.append(f"## {heading}\n\n<div class=\"icon-grid\">\n")
        for name, keys in members:
            source = libtools.LIBRARY / keys["file"]
            target = images / keys["file"]
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(source, target)
            url = "{{ '/assets/library/" + keys["file"] + "' | relative_url }}"
            title = html.escape(keys["title"])
            parts.append(f'<figure><img src="{url}" alt="{title}"><figcaption><code>{name}</code><br>{title}'
                         f'</figcaption></figure>\n')
        parts.append("</div>\n\n")
    page = pathlib.Path(args.page)
    page.write_text("".join(parts), encoding="utf-8", newline="\n")
    print(f"wrote {page} with {len(sections)} icons")
    return 0


if __name__ == "__main__":
    sys.exit(main())
