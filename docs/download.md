---
layout: default
title: Download
---
{% assign release = site.data.release %}
## License
StreamFlex is released under the [GNU General Public License v3.0](https://github.com/bilbospocketses/streamflex/blob/master/LICENSE). It started from complexlogic's original Flex Launcher (v2.2), which was released into the public domain under the Unlicense.

## Download
Binary packages are available for Windows 64 bit, Linux x86-64, and Raspberry Pi. The latest stable release is version {{ release.version }}. The Debian and Raspberry Pi packages need Debian trixie, Raspberry Pi OS trixie, Ubuntu 25.10, or newer. v0.4.1 is the last release for Debian bookworm and Ubuntu 24.04 LTS, which package no SDL3; it stays on its [release page](https://github.com/bilbospocketses/streamflex/releases/tag/v0.4.1).


<table style="border: none" width="75%">
  <tbody>
    <tr>
      <td class="download" width="170"><img src="{{ '/assets/icons/windows.svg' | relative_url }}" class="download" style="width:128px"></td>
      <td class="download">
        <h4>Windows 10/11:</h4>
        <ul><li><a href="{{ release.windows.url }}">{{ release.windows.name }}</a> (x64 package)</li>
		</ul>
      </td>
    </tr>
    <tr>
      <td class="download" width="170"><img src="{{ '/assets/icons/linux.svg' | relative_url }}" class="download" style="width:128px"></td>
      <td class="download">
        <h4>APT-based (Debian trixie, Ubuntu 25.10 or newer):</h4>
        <ul><li><a href="{{ release.deb_amd64.url }}">{{ release.deb_amd64.name }}</a> (amd64 package)</li>
        </ul>
        <h4>Pacman-based (Arch, Manjaro):</h4>
        <ul><li><a href="{{ release.arch.url }}">{{ release.arch.name }}</a> (x86-64 package)</li>
        </ul>
        {% if release.fedora %}
        <h4>Fedora 44 (Workstation, KDE Plasma):</h4>
        <ul><li><a href="{{ release.fedora.url }}">{{ release.fedora.name }}</a> (x86-64 tarball, see the <a href="https://github.com/{{ site.repository }}#fedora-44-x86-64">install steps</a>)</li>
        </ul>
        {% endif %}
      </td>
    </tr>
    <tr>
      <td class="download" width="170"><img src="{{ '/assets/icons/raspberry_pi.svg' | relative_url }}" class="download" style="width:128px"></td>
      <td class="download">
        <h4>Raspberry Pi OS trixie or newer (64 bit only)</h4>
        <ul><li><a href="{{ release.deb_arm64.url }}">{{ release.deb_arm64.name }}</a> (arm64 package)</li>
		</ul>
      </td>
    </tr>
</tbody>
</table>


## Source Code
If your platform is not listed above, you're interested in development, or you simply prefer compiling your own software, you can download source packages below. See the [compilation guide](compilation) for build instructions.

<table style="border: none" width="75%">
  <tbody>
    <tr>
      <td class="download" width="170"><img src="{{ '/assets/icons/source.svg' | relative_url }}" class="download" style="width:128px"></td>
      <td class="download">
        <h4>Source Packages:</h4>
        <ul>
          <li><a href="https://github.com/{{ site.repository }}/archive/refs/tags/{{ release.tag }}.zip">StreamFlex {{ release.tag }} Source (.zip)</a></li>
          <li><a href="https://github.com/{{ site.repository }}/archive/refs/tags/{{ release.tag }}.tar.gz">StreamFlex {{ release.tag }} Source (.tar.gz)</a></li>
		</ul>
      </td>
    </tr>
  </tbody>
</table>


