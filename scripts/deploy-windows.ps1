<#
.SYNOPSIS
Deploys the Windows release build of Cadence to the folder it is run from.

.DESCRIPTION
Runs the release tests, records the hashes of today's progress file and of settings.json, stops
the deployed copy, moves the current target folder aside as <Target>.previous, copies the release
executable into a fresh target folder, runs windeployqt, relaunches Cadence minimized and checks
the result: exactly one process from the target, unchanged or key-diffed settings, the launch at
login registration and the lines the new copy wrote to today's log.

Nothing under the config or data folder is written. Read docs/deploy.md before running it.

.PARAMETER Target
Folder the deployed copy runs from. Default: cadence-app under the user profile.

.PARAMETER Preset
CMake preset of the release build. Default: windows-msvc-release.

.PARAMETER QtRoot
Qt kit directory holding bin\windeployqt.exe. Default: the QT_ROOT environment variable.

.PARAMETER ConfigDir
Cadence config folder (settings.json, templates). Default: Cadence\Cadence under LOCALAPPDATA.

.PARAMETER DataDir
Cadence data folder (progress, logs). Default: Cadence\Cadence under APPDATA.

.PARAMETER SkipTests
Do not run ctest on the release preset before deploying.

.PARAMETER LaunchTimeoutSec
How long to wait for exactly one process from the target after the relaunch.

.PARAMETER DryRun
Run every read-only check (the tests unless -SkipTests, the hashes, the process lookup, the Run
key, the backup path and the windeployqt command line) and print each mutating step instead of
executing it. Nothing is stopped, moved, copied or started.
#>
[CmdletBinding()]
param(
    [string]$Target = (Join-Path $env:USERPROFILE "cadence-app"),
    [string]$Preset = "windows-msvc-release",
    [string]$QtRoot = $env:QT_ROOT,
    [string]$ConfigDir = (Join-Path $env:LOCALAPPDATA "Cadence\Cadence"),
    [string]$DataDir = (Join-Path $env:APPDATA "Cadence\Cadence"),
    [switch]$SkipTests,
    [int]$LaunchTimeoutSec = 20,
    [switch]$DryRun
)

$ErrorActionPreference = "Stop"

$repo = Split-Path $PSScriptRoot -Parent
$buildDir = Join-Path $repo "build\$Preset"
$source = Join-Path $buildDir "bin\cadence.exe"
$today = (Get-Date).ToString("yyyy-MM-dd")
$progressFile = Join-Path $DataDir "progress\$today.json"
$settingsFile = Join-Path $ConfigDir "settings.json"
$logFile = Join-Path $DataDir "logs\$today.log"
$previous = "$Target.previous"
$deployedExe = Join-Path $Target "cadence.exe"
$failures = @()

function Stamp { (Get-Date).ToString("HH:mm:ss") }

# Every step that changes something goes through here, so a dry run prints it and stops there.
function Act($description, [scriptblock]$action) {
    if ($DryRun) { "would $description" } else { & $action }
}

function Hash($path) {
    if (Test-Path $path) { (Get-FileHash $path -Algorithm SHA256).Hash } else { "(absent)" }
}

# Dotted key paths of a JSON object, so a settings diff can name keys without printing values.
function Keys($obj, $prefix) {
    $out = @()
    foreach ($p in $obj.PSObject.Properties) {
        $name = if ($prefix) { "$prefix.$($p.Name)" } else { $p.Name }
        if ($p.Value -is [System.Management.Automation.PSCustomObject]) {
            $out += Keys $p.Value $name
        } else {
            $out += $name
        }
    }
    return $out
}

function SettingsKeys {
    if (Test-Path $settingsFile) { Keys (Get-Content $settingsFile -Raw | ConvertFrom-Json) "" } else { @() }
}

function TargetProcesses {
    @(Get-Process cadence -ErrorAction SilentlyContinue | Where-Object { $_.Path -like "$Target\*" })
}

function Under($path, $folder) {
    $p = [System.IO.Path]::GetFullPath($path).TrimEnd('\') + '\'
    $f = [System.IO.Path]::GetFullPath($folder).TrimEnd('\') + '\'
    return $p.StartsWith($f, [System.StringComparison]::OrdinalIgnoreCase)
}

"== $(Stamp) checks =="
if (-not (Test-Path $source)) { throw "release build not found: $source (build the $Preset preset first)" }
if (-not $QtRoot) { throw "QtRoot is empty: set QT_ROOT or pass -QtRoot" }
$windeployqt = Join-Path $QtRoot "bin\windeployqt.exe"
if (-not (Test-Path $windeployqt)) { throw "windeployqt not found: $windeployqt" }
# The target is replaced wholesale, so it must never contain or sit inside the user's data.
$targetFull = [System.IO.Path]::GetFullPath($Target).TrimEnd('\')
if ($targetFull -eq [System.IO.Path]::GetPathRoot($targetFull).TrimEnd('\')) { throw "Target must not be a drive root" }
foreach ($folder in @($ConfigDir, $DataDir)) {
    if ((Under $folder $Target) -or (Under $Target $folder)) { throw "Target $Target overlaps $folder" }
}
"source  $source"
"target  $Target"
"backup  $previous"
if ($DryRun) { "mode    dry run: nothing is stopped, moved, copied or started" }

if ($SkipTests) {
    "tests   skipped (-SkipTests)"
} else {
    "== $(Stamp) release tests =="
    Push-Location $repo
    try {
        & ctest --preset $Preset --output-on-failure
        if ($LASTEXITCODE -ne 0) { throw "release tests failed (ctest exit code $LASTEXITCODE)" }
    } finally {
        Pop-Location
    }
}

"== $(Stamp) before =="
$progressBefore = Hash $progressFile
$settingsBefore = Hash $settingsFile
"progress $progressFile : $progressBefore"
"settings $settingsFile : $settingsBefore"
$keysBefore = SettingsKeys
$logLinesBefore = if (Test-Path $logFile) { @(Get-Content $logFile).Count } else { 0 }

"== $(Stamp) stop the deployed copy =="
# There is no quit command on the single instance socket yet (docs/follow-ups.md), so the process
# is terminated. Progress is persisted on every action, so nothing is lost.
foreach ($p in TargetProcesses) {
    Act "stop pid $($p.Id) started $($p.StartTime)" {
        "stopping pid $($p.Id) started $($p.StartTime)"
        Stop-Process -Id $p.Id -Force
    }
}
if (-not $DryRun) {
    $deadline = (Get-Date).AddSeconds(10)
    while ((TargetProcesses).Count -gt 0 -and (Get-Date) -lt $deadline) { Start-Sleep -Milliseconds 250 }
    if ((TargetProcesses).Count -gt 0) { throw "the deployed copy is still running" }
}

"== $(Stamp) back up and deploy =="
if (Test-Path $Target) {
    if (Test-Path $previous) {
        Act "remove the older backup $previous" {
            "removing the older backup $previous"
            Remove-Item -Recurse -Force $previous
        }
    }
    Act "move $Target to $previous" {
        Move-Item $Target $previous
        "moved $Target to $previous"
    }
}
Act "create $Target" { New-Item -ItemType Directory -Path $Target | Out-Null }
Act "copy $source to $deployedExe" { Copy-Item $source $deployedExe }
$build = Get-Item $source
"build   cadence.exe $($build.Length) bytes, built $($build.LastWriteTime), sha256 $(Hash $source)"
if (-not $DryRun -and (Hash $source) -ne (Hash $deployedExe)) {
    throw "copied executable differs from the build"
}
# Same flags as the post-build step in src/app/CMakeLists.txt. Translations, quotes and fonts are
# compiled into the executable, so windeployqt brings everything else.
$deployArgs = @("--qmldir", (Join-Path $repo "src\app\qml"), "--no-translations", "--no-system-d3d-compiler", "--no-opengl-sw", $deployedExe)
Act "run: `"$windeployqt`" $($deployArgs -join ' ')" {
    & $windeployqt @deployArgs | Select-Object -Last 1
    if ($LASTEXITCODE -ne 0) { throw "windeployqt failed (exit code $LASTEXITCODE)" }
    $exe = Get-Item $deployedExe
    "deployed cadence.exe $($exe.Length) bytes, built $($exe.LastWriteTime)"
}

"== $(Stamp) relaunch =="
Act "start: `"$deployedExe`" --minimized (working directory $Target)" {
    Start-Process -FilePath $deployedExe -ArgumentList "--minimized" -WorkingDirectory $Target | Out-Null
}
# One process must appear and stay alone: a second one would hand over to the first and exit.
$deadline = (Get-Date).AddSeconds($LaunchTimeoutSec)
$stable = 0
$count = (TargetProcesses).Count
while (-not $DryRun -and (Get-Date) -lt $deadline) {
    Start-Sleep -Seconds 1
    $count = (TargetProcesses).Count
    if ($count -eq 1) { $stable += 1 } else { $stable = 0 }
    if ($stable -ge 3) { break }
}
foreach ($p in TargetProcesses) { "cadence pid $($p.Id) path $($p.Path) started $($p.StartTime)" }
if ($DryRun) {
    "processes from the target now: $count (the relaunch check needs a real run)"
} elseif ($stable -ge 3) {
    "processes from the target: 1 (stable for 3 s)"
} else {
    $failures += "expected exactly one process from the target after $LaunchTimeoutSec s, saw $count"
}
$others = @(Get-Process cadence -ErrorAction SilentlyContinue | Where-Object { $_.Path -notlike "$Target\*" })
if ($others.Count -gt 0) { "other cadence processes (not from the target): $($others.Count)" }

"== $(Stamp) after =="
$progressAfter = Hash $progressFile
$settingsAfter = Hash $settingsFile
"progress after : $progressAfter  $(if ($progressAfter -eq $progressBefore) { 'UNCHANGED' } else { 'CHANGED' })"
"settings after : $settingsAfter  $(if ($settingsAfter -eq $settingsBefore) { 'UNCHANGED' } else { 'CHANGED' })"
if ($settingsAfter -ne $settingsBefore) {
    $keysAfter = SettingsKeys
    "settings keys added  : $(($keysAfter | Where-Object { $keysBefore -notcontains $_ }) -join ', ')"
    "settings keys removed: $(($keysBefore | Where-Object { $keysAfter -notcontains $_ }) -join ', ')"
}
$runKey = (Get-ItemProperty 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Run' -ErrorAction SilentlyContinue).Cadence
$expectedRun = "`"$deployedExe`" --minimized"
"run key: $runKey"
if ($runKey -ne $expectedRun) { $failures += "Run key 'Cadence' is not $expectedRun" }

"== $(Stamp) log lines written since the deploy started ($logFile) =="
if (Test-Path $logFile) {
    $lines = @(Get-Content $logFile | Select-Object -Skip $logLinesBefore)
    if ($lines.Count -eq 0) { "(none yet)" } else { $lines }
    if (-not $DryRun -and -not ($lines -match "^\S+\s+start version ")) { $failures += "no 'start version' line in today's log after the relaunch" }
} else {
    $failures += "no log file for today: $logFile"
}

if ($failures.Count -gt 0) {
    ""
    "DEPLOY CHECKS FAILED:"
    $failures | ForEach-Object { " - $_" }
    exit 1
}
"== $(Stamp) $(if ($DryRun) { 'dry run done: nothing was changed' } else { 'done' }) =="
