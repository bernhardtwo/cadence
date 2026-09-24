# Deploying on Windows

The copy of Cadence you run every day lives in its own folder, outside the build tree, and is
registered to launch at login from there with `--minimized`. `scripts/deploy-windows.ps1`
replaces that copy with the current release build. It is meant for a single machine: the build
and the deployed copy are on the same computer.

## When it is safe

The script terminates the running copy, so it must only run between blocks: nothing Active, and
no block starting within the next ten minutes. Progress is written on every action, so a
termination loses nothing, but a running pomodoro would resume where it was only after the
relaunch, and a block start alarm that fires during the deploy is lost. Check the Today screen or
today's progress file first. A cooperative quit command is planned; see the "Clean shutdown from
outside" entry in `follow-ups.md`.

Deploy only a commit whose release build passed its tests. The script runs them itself unless
told not to.

## Running it

From a shell where `QT_ROOT` points at the Qt kit (the folder holding `bin\windeployqt.exe`) and
the release preset has been built:

```powershell
cmake --build --preset windows-msvc-release
powershell -ExecutionPolicy Bypass -File scripts\deploy-windows.ps1
```

The defaults deploy `build\windows-msvc-release\bin\cadence.exe` into `cadence-app` under your
user profile and read the config and data folders Cadence uses on Windows (`Cadence\Cadence`
under `%LOCALAPPDATA%` and `%APPDATA%`). Every path is a parameter:

| Parameter | Default | Meaning |
| --- | --- | --- |
| `-Target` | `%USERPROFILE%\cadence-app` | Folder the deployed copy runs from |
| `-Preset` | `windows-msvc-release` | Build preset to deploy and test |
| `-QtRoot` | `%QT_ROOT%` | Qt kit with `windeployqt` |
| `-ConfigDir` | `%LOCALAPPDATA%\Cadence\Cadence` | Where `settings.json` lives |
| `-DataDir` | `%APPDATA%\Cadence\Cadence` | Where `progress\` and `logs\` live |
| `-SkipTests` | off | Do not run `ctest` on the preset first |
| `-LaunchTimeoutSec` | 20 | Wait for the relaunched process |

The script never writes under the config or data folder and refuses a target that overlaps
either of them.

## What it does and what each check means

1. **Checks.** The release executable and `windeployqt` exist and the target is a safe folder.
2. **Release tests.** `ctest --preset <Preset>`; a failure stops the deploy before anything is
   touched. `-SkipTests` opts out, for a build whose tests you just watched pass.
3. **Before.** SHA-256 of today's progress file and of `settings.json`, the dotted key paths of
   `settings.json` and the number of lines in today's log. These are the baseline for the checks
   after the relaunch.
4. **Stop.** Every `cadence.exe` running from the target is terminated (no quit command exists
   yet). The script waits until none is left.
5. **Back up and deploy.** The whole target folder is moved to `<Target>.previous`, replacing an
   older backup, and a fresh target receives the executable and the Qt runtime from `windeployqt`
   with the same flags as the post-build step of the normal build. Translations, quotes and the
   bundled fonts are compiled into the executable, so nothing else is copied. The copied
   executable must hash the same as the build.
6. **Relaunch.** `cadence.exe --minimized` from the target. The script then polls, for up to
   `-LaunchTimeoutSec`, until exactly one process runs from the target and has done so for three
   consecutive seconds. A second process would mean the single instance handover failed; zero
   means the new copy exited at startup (look at the log lines below). Other `cadence.exe`
   processes, such as a debug test session, are listed but do not count.
7. **After.** The two hashes again. The progress file must be unchanged unless a block was
   running, which the safe window rules out. `settings.json` normally stays unchanged; when a new
   version adds settings it is rewritten on the first start, and the script prints only the keys
   that were added or removed, never the values.
8. **Run key.** `HKCU\Software\Microsoft\Windows\CurrentVersion\Run\Cadence` must be
   `"<Target>\cadence.exe" --minimized`. The registration is written by Cadence itself, from
   Settings or the tray menu, so a mismatch means launch at login points at another copy.
9. **Log.** The lines today's log gained since the deploy started. The new copy writes
   `start version <version>` first; its absence fails the run.

The script exits with code 1 when any check after the relaunch fails, and lists them. A failure
before the relaunch throws and stops the run at that step.

## Rolling back

The previous copy is intact in `<Target>.previous`. To go back, stop the new copy and swap the
folders:

```powershell
Get-Process cadence | Where-Object { $_.Path -like "$env:USERPROFILE\cadence-app\*" } | Stop-Process -Force
Move-Item "$env:USERPROFILE\cadence-app" "$env:USERPROFILE\cadence-app.broken"
Move-Item "$env:USERPROFILE\cadence-app.previous" "$env:USERPROFILE\cadence-app"
Start-Process "$env:USERPROFILE\cadence-app\cadence.exe" -ArgumentList "--minimized"
```

The Run key keeps pointing at the target folder, so launch at login needs no change. Remove the
`.broken` folder once the cause is understood. Settings and progress are not part of the deploy
and are not rolled back; a newer version that added settings keys leaves them in
`settings.json`, and older versions ignore keys they do not know.
