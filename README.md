# Cadence

Cadence is a desktop app that structures your day into blocks. Each block can carry alarms, pomodoro
cycles and push-up logging, so the rhythm of work, breaks and movement is planned once and then simply
followed. It is written in C++20 and Qt 6 (QML), runs on Windows, Linux and macOS, and is free software
under the GPL-3.0.

The visual language is called Signal: a near-black canvas, one acid accent, condensed display type for
the things you glance at and a quiet grotesk for everything else.

## Status

Milestone 4: the full Signal interface. Today shows the day as stacked bars with the current block
in the accent, its pomodoro grid and the block actions; while a pomodoro runs the big timer counts
the phase and the block end becomes secondary text. Focus mode fills the screen with the running
phase. The break overlay takes the accent and logs push-ups; block start and confirmation prompts
sit on the dark canvas. The Templates screen edits the weekly template with validation from core
and applies changes to today at once. Settings persist startup, alarm and push-up preferences and
drive the runtime. Stats is a placeholder for the next milestone.

The interface speaks English, Spanish and French, following the system language unless Settings
says otherwise, and shows short passages from the Stoics and Epicurus on the running block, in
focus mode and on the overlays, with the Latin or Greek original on request. See `docs/i18n.md`
and `docs/quotes.md`.

### Running it

- The weekly template is copied to your config directory on first start
  (`templates/week.json` under the Cadence app config location) and edited from the Templates
  screen or by hand.
- Settings live next to it in `settings.json`.
- Progress is written per day under the app data location, in `progress/YYYY-MM-DD.json`,
  including every pomodoro phase, so a restart resumes the running phase where it was.
- A block skipped by mistake shows Restore in the Today list while restoring still makes
  sense: an anchored block until its end, the others until the day cutoff. An anchored block
  continues from the current time with a pomodoro plan sized for what is left; a flexible block
  goes back to the queue. The time it spent skipped never counts as work.
- A daily log of alarms, overlays and push-up sets lives next to it in `logs/YYYY-MM-DD.log`,
  kept for 14 days.
- Closing the window hides Cadence to the tray; Quit in the tray menu exits.
- `cadence --minimized` starts hidden in the tray. `--enable-autostart` and `--disable-autostart`
  register or remove launch at login and exit; the tray menu and the Settings screen toggle the
  same setting.
- On Windows, `scripts/deploy-windows.ps1` replaces the copy you run every day with the current
  release build; see `docs/deploy.md` for when it is safe and how to roll back.

### Music

Cadence can drive the Spotify desktop app through the Web API: a default playlist per activity, a
player row in focus mode and a now playing line on Today. Every user registers their own Spotify
app and pastes its Client ID in Settings; Premium is required to control playback. See
`docs/spotify.md` for the setup and the troubleshooting tool.

### Keyboard

| Key | Action |
| --- | --- |
| Space | Start, pause or resume the current block |
| F | Enter or leave focus mode |
| Esc | Leave focus mode or dismiss an overlay |
| Return | Primary action of an overlay |
| Ctrl+1 to Ctrl+4 | Today, Stats, Templates, Settings |
| Ctrl+Space, Ctrl+Right, Ctrl+Left | Play or pause, next, previous, in focus mode when connected to Spotify |

## Screenshots

Screenshots will be added once the first usable milestone lands.

## Requirements

| Tool | Version |
| --- | --- |
| CMake | 3.24 or newer |
| Ninja | any recent release |
| Qt | 6.11.2 with the Multimedia and Network Authorization modules |
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
src/core      Pure C++20 static library with no Qt dependency. Planner, pomodoro, alarms, JSON.
src/platform  Launch at login for Windows, Linux and macOS.
src/app       Qt Quick executable, QML modules, tray icon and bundled fonts.
resources     Default weekly template and the alarm sounds.
tests         Catch2 test suites for the core and platform libraries.
```

## Fonts

The UI ships with three typefaces from Google Fonts, all under the SIL Open Font License 1.1:

- Big Shoulders Display by Patric King, `src/app/fonts/BigShouldersDisplay`
- Archivo by Omnibus-Type, `src/app/fonts/Archivo`
- Noto Serif Italic by the Noto Project, `src/app/fonts/NotoSerif`, subset to Latin and Greek for
  the original line of the quotes

Each directory contains the license text that applies to it.

## License

Cadence is licensed under the GNU General Public License version 3. See `LICENSE`.
