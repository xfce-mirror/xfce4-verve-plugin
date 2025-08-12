[![License](https://img.shields.io/badge/License-GPL%20v2-blue.svg)](https://gitlab.xfce.org/panel-plugins/xfce4-verve-plugin/-/blob/master/COPYING)

# xfce4-verve-plugin

The Verve panel plugin is a comfortable command line plugin for the Xfce
panel.

----

### Homepage

[Xfce4-verve-plugin documentation](https://docs.xfce.org/panel-plugins/xfce4-verve-plugin)

### Changelog

See [NEWS](https://gitlab.xfce.org/panel-plugins/xfce4-verve-plugin/-/blob/master/NEWS) for details on changes and fixes made in the current release.

### Source Code Repository

[Xfce4-verve-plugin source code](https://gitlab.xfce.org/panel-plugins/xfce4-verve-plugin)

### Download a Release Tarball

[Xfce4-verve-plugin archive](https://archive.xfce.org/src/panel-plugins/xfce4-verve-plugin)
    or
[Xfce4-verve-plugin tags](https://gitlab.xfce.org/panel-plugins/xfce4-verve-plugin/-/tags)

### Installation

From source code repository: 

    % cd xfce4-verve-plugin
    % meson setup build
    % meson compile -C build
    % meson install -C build

From release tarball:

    % tar xf xfce4-verve-plugin-<version>.tar.xz
    % cd xfce4-verve-plugin-<version>
    % meson setup build
    % meson compile -C build
    % meson install -C build

Note: If the output says "required file ./ltmain.sh not found", run libtoolize and then run the same autogen.sh command again.

### Uninstallation

    % ninja uninstall -C build

### Reporting Bugs

Visit the [reporting bugs](https://docs.xfce.org/panel-plugins/xfce4-verve-plugin/bugs) page to view currently open bug reports and instructions on reporting new bugs or submitting bugfixes.

