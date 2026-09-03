# QScintilla (embedded code editor)

This project links a **static** QScintilla 2.14.1 build for Qt 5.14.2.

## Layout

- `QScintilla_src-2.14.1/` — upstream source (GPL-3.0 / commercial dual license)
- `qscintilla-install/` — headers + `lib/qscintilla2_qt5.lib` used by CMake
- `build_qscintilla.bat` — rebuild script

## Rebuild

```bat
third_party\build_qscintilla.bat
```

Requires Visual Studio 2022 x64 tools and Qt at `D:\Qt\5.14.2\msvc2017_64`.
