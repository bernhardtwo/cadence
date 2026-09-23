# Cadence

Cadence is a desktop app that structures your day into blocks. Each block can carry alarms, pomodoro
cycles and push-up logging, so the rhythm of work, breaks and movement is planned once and then simply
followed. It is written in C++20 and Qt 6 (QML), runs on Windows, Linux and macOS, and is free software
under the GPL-3.0.

The visual language is called Signal: a near-black canvas, one acid accent, condensed display type for
the things you glance at and a quiet grotesk for everything else.

## Status

Milestone 1: repository scaffold, build system, continuous integration and a minimal running window.
Scheduling, alarms, the tray icon and music integration are not implemented yet.

## Screenshots

Screenshots will be added once the first usable milestone lands.

## Requirements

| Tool | Version |
| --- | --- |
| CMake | 3.24 or newer |
| Ninja | any recent release |
| Qt | 6.11.2 with the Multimedia module |
| Compiler | MSVC 2022 or newer, GCC 12 or newer, Clang 15 or newer, AppleClang 15 or newer |

Point CMake at your Qt installation either with `CMAKE_PREFIX_PATH` or by setting the `QT_ROOT`
environment variable to the kit directory (the one containing `bin`, `lib` and `qml`).

## Building

All platforms use the presets declared in `CMakePresets.json`. Every preset has a `-debug` and a
`-release` variant. The commands below use release; swap the suffix for a debug build.

### Windows (MSVC)

Open a "x64 Native Tools Command Prompt" or run `vcvars64.bat` first so `cl` is on the PATH.

```bat
set QT_ROOT=C:\Qt\6.11.2\msvc2022_64
cmake --preset windows-msvc-release
cmake --build --preset windows-msvc-release
ctest --preset windows-msvc-release
```

The build runs `windeployqt` after linking, so `build\windows-msvc-release\bin\cadence.exe` starts by
double-click. Pass `-DCADENCE_DEPLOY_QT_RUNTIME=OFF` at configure time to skip that step.

### Linux

Install a compiler, Ninja and the OpenGL and xkbcommon development packages of your distribution
(on Debian and Ubuntu: `build-essential ninja-build libgl1-mesa-dev libxkbcommon-dev`), then:

```sh
export QT_ROOT=$HOME/Qt/6.11.2/gcc_64
cmake --preset linux-release
cmake --build --preset linux-release
ctest --preset linux-release
./build/linux-release/bin/cadence
```

### macOS

Install the Xcode command line tools and Ninja (`brew install ninja`), then:

```sh
export QT_ROOT=$HOME/Qt/6.11.2/macos
cmake --preset macos-release
cmake --build --preset macos-release
ctest --preset macos-release
open build/macos-release/bin/cadence.app
```

## Project layout

```
src/core    Pure C++20 static library with no Qt dependency. Domain logic lives here.
src/app     Qt Quick executable, QML modules and bundled fonts.
tests       Catch2 test suites for the core library.
```

## Fonts

The UI ships with two typefaces from Google Fonts, both under the SIL Open Font License 1.1:

- Big Shoulders Display by Patric King, `src/app/fonts/BigShouldersDisplay`
- Archivo by Omnibus-Type, `src/app/fonts/Archivo`

Each directory contains the license text that applies to it.

## License

Cadence is licensed under the GNU General Public License version 3. See `LICENSE`.
