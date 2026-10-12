# Third-party components

* PySide6 / Qt: LGPLv3, GPLv3 or commercial terms, depending on modules. https://www.qt.io/licensing/
* python-vlc: LGPLv2.1 or later. https://pypi.org/project/python-vlc/
* VLC / libVLC: libVLC LGPLv2.1 or later; VLC and various plugins have GPL and other licenses. Preserve all bundled COPYING, AUTHORS and plugin notices. Sources: https://www.videolan.org/vlc/download-sources.html

Only Qt Widgets/Core/Gui are used by the application. Distribution must satisfy the licenses of the actual bundled binaries, including source/relinking obligations where applicable. This project does not include Microsoft artwork or branding.

The distribution includes the Qt LGPLv3/GPLv3 texts and Qt exception in `licenses/`, the python-vlc LGPLv2.1 text, and the VLC installation’s own notices. Qt shared libraries remain replaceable in the `_internal/PySide6` directory. Qt 6.10.2 sources: https://github.com/qt/qtbase/tree/v6.10.2 and https://download.qt.io/archive/qt/6.10/6.10.2/submodules/ . PySide6 6.10.2 sources: https://code.qt.io/cgit/pyside/pyside-setup.git/tag/?h=v6.10.2 .
