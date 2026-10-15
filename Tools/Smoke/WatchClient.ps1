# WatchClient: one watchable DevMode client + ffmpeg window recording.
# Usage: .\WatchClient.ps1 [-Scenario bot_combat_patrol] [-Fps 10] [-WaitSec 300] [-NoRecord] [-DevMap LoginLevel] [-Tombstone <regex>]
# The client window is pinned topmost for the recording (unpinned on exit);
# close/park other topmost windows (UAC prompts etc.) by hand beforehand.
# Produces Saved/WatchRuns/<stamp>_<scenario>/ : watch.mp4 + named PNGs + client log
# with the DEVMODE tombstone + WATCH_RESULT.txt. Exit nonzero on any failure.
#
# Rules: exactly ONE visual client (two renders lie/crash on one GPU — engine
# RayTracing assert; see SmokeClients.ps1). Regression checks run headless;
# watch the recording, not the live window. Offline scenarios need no servers.
param(
    [string]$Scenario = "bot_combat_patrol",
    [int]$Fps = 10,
    [int]$WaitSec = 300,
    [switch]$NoRecord = $false,
    # DevMode travel target (passed as -devmode-map=; the boot map arg is
    # overridden by the login travel without it). Default LoginLevel: lit,
    # real ground + navmesh (the default container is an empty void).
    [string]$DevMap = "LoginLevel",
    # Custom tombstone regex (default derives from $Scenario). Needed for
    # flows like soak that log their own result marker.
    [string]$Tombstone = ""
)

$ErrorActionPreference = "Continue"
$Root = Join-Path $PSScriptRoot ".." | Join-Path -ChildPath ".." | Resolve-Path
$Project = Join-Path $Root "Prototyping.uproject"
$EditorGui = "D:\Game Dev\UE\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"
$LogDir = Join-Path $Root "Saved\Logs"
$ShotsDir = Join-Path $Root "Saved\Screenshots\WindowsEditor"
$W = 960; $H = 540

function Fail($msg) {
    # Always drop the topmost pin first (a stuck on-top window otherwise
    # outlives the failed run and annoys the desktop).
    try {
        if ($Client -and $Client.MainWindowHandle -ne 0) {
            [Win32]::SetWindowPos($Client.MainWindowHandle, $HWND_NOTOPMOST, 0, 0, 0, 0, $SWP_TOPMOST_FLAGS) | Out-Null
        }
    } catch { }
    Write-Host "WATCH: FAIL - $msg" -ForegroundColor Red; exit 1
}

Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class Win32 {
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);
  [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr hWnd, IntPtr hWndInsertAfter, int X, int Y, int cx, int cy, uint uFlags);
}
"@
$HWND_TOPMOST = [IntPtr](-1)
$HWND_NOTOPMOST = [IntPtr](-2)
# Keep position/size, don't activate: the capture only needs Z-order.
$SWP_TOPMOST_FLAGS = 0x0001 -bor 0x0002 -bor 0x0010

# Preflight: never run 2 visual clients on one GPU.
$busy = Get-Process UnrealEditor -ErrorAction SilentlyContinue
if ($busy) { Fail "UnrealEditor already running (one visual client at a time)" }

# Resolve ffmpeg (PATH first, then the user-scope portable fallback).
$Ffmpeg = $null
if (-not $NoRecord) {
    $cmd = Get-Command ffmpeg -ErrorAction SilentlyContinue
    if ($cmd) { $Ffmpeg = $cmd.Source }
    else {
        $fb = Join-Path $env:LOCALAPPDATA "ffmpeg\bin\ffmpeg.exe"
        if (Test-Path -LiteralPath $fb) { $Ffmpeg = $fb }
        else {
            # winget user-scope install (Gyan.FFmpeg) lands outside PATH until
            # the shell restarts — pick it up by glob.
            $wg = Get-ChildItem "$env:LOCALAPPDATA\Microsoft\WinGet\Packages\Gyan.FFmpeg_*" -Filter ffmpeg.exe -Recurse -ErrorAction SilentlyContinue |
                Select-Object -First 1
            if ($wg) { $Ffmpeg = $wg.FullName }
        }
    }
    if (-not $Ffmpeg) { Fail "ffmpeg not found (winget install Gyan.FFmpeg, or rerun with -NoRecord)" }
}

# Tombstone: bot brains log DEVMODE_BOT_RESULT <brain> (bot_ prefix stripped),
# full_sweep logs DEVMODE_SWEEP_RESULT. Override with -Tombstone for flows
# with their own marker (e.g. soak -> "DEVMODE_SOAK_RESULT (PASS|FAIL)").
$TombPattern = "DEVMODE_SWEEP_RESULT (PASS|FAIL)"
if ($Scenario.StartsWith("bot_")) { $TombPattern = "DEVMODE_BOT_RESULT $($Scenario.Substring(4)) (PASS|FAIL)" }
if (-not [string]::IsNullOrEmpty($Tombstone)) { $TombPattern = $Tombstone }

$Stamp = Get-Date -Format "yyyyMMdd-HHmmss"
$RunDir = Join-Path $Root "Saved\WatchRuns\$($Stamp)_$Scenario"
New-Item -ItemType Directory -Force -Path $RunDir | Out-Null
Write-Host "WATCH: run dir $RunDir" -ForegroundColor Cyan

# Window pinned to (0,0) so gdigrab captures a fixed region; everything else
# is minimized and the client pulled to front before recording (region capture
# sees the screen, not the window — overlaps would bake into watch.mp4).
$QProject = '"' + $Project + '"'
$ClientArgs = @($QProject, "/Game/Maps/$DevMap", "-game", "-windowed", "-ResX=$W", "-ResY=$H",
    "-WinX=0", "-WinY=0", "-NoSplash", "-NoVerifyGC", "-devmode=$Scenario", "-devmode-map=$DevMap", "-devmode-shots", "-log=WatchClient.log")
$LogStamp = Get-Date
Write-Host "WATCH: launching visual client ($Scenario)..." -ForegroundColor Cyan
$Client = Start-Process -FilePath $EditorGui -ArgumentList $ClientArgs -PassThru

$Rec = $null
$RecErr = Join-Path $RunDir "ffmpeg-rec.log"
if (-not $NoRecord) {
    # Give the window a moment to appear at (0,0), then pull it to the front
    # so the fixed-region capture holds the client. Close/park overlapping
    # windows by hand before recording (gdigrab sees the screen, not the window).
    Start-Sleep -Seconds 8
    if (-not $Client.HasExited) {
        # Pin the client above all other windows for the recording (region
        # capture sees the screen, not the window). Best-effort: unpinned
        # again in Fail() and at the end.
        try {
            $Client.Refresh()
            $Hwnd = $Client.MainWindowHandle
            if ([string]::IsNullOrEmpty("$Hwnd") -or "$Hwnd" -eq "0") { Write-Host "WATCH: NOTE - no main window yet, skipping topmost pin" -ForegroundColor Yellow }
            else {
                [Win32]::ShowWindow($Hwnd, 9) | Out-Null
                [Win32]::SetWindowPos($Hwnd, $HWND_TOPMOST, 0, 0, 0, 0, $SWP_TOPMOST_FLAGS) | Out-Null
            }
        } catch { }
        Start-Sleep -Seconds 2
        $Mp4 = Join-Path $RunDir "watch.mp4"
        $psi = New-Object System.Diagnostics.ProcessStartInfo
        $psi.FileName = $Ffmpeg
        $psi.Arguments = "-y -v error -f gdigrab -framerate $Fps -offset_x 0 -offset_y 0 -video_size ${W}x${H} -i desktop -preset veryfast -pix_fmt yuv420p ""$Mp4"""
        $psi.UseShellExecute = $false
        $psi.RedirectStandardInput = $true
        $psi.RedirectStandardError = $true
        $psi.CreateNoWindow = $true
        $Rec = [System.Diagnostics.Process]::Start($psi)
        # Fail fast if the capture dies on startup (wrong display, bad region).
        Start-Sleep -Seconds 5
        if ($Rec.HasExited) {
            $Rec.StandardError.ReadToEnd() | Out-File -LiteralPath $RecErr -Encoding utf8
            Fail "ffmpeg exited on startup (see ffmpeg-rec.log)"
        }
        Write-Host "WATCH: recording ${W}x${H}@${Fps}fps -> watch.mp4" -ForegroundColor Cyan
    }
}

if (-not $Client.WaitForExit($WaitSec * 1000)) {
    try { $Client.Kill() } catch { }
    try { if ($Rec -and -not $Rec.HasExited) { $Rec.Kill() } } catch { }
    Fail "client did not quit within ${WaitSec}s"
}
# Graceful 'q' so the mp4 finalizes (moov atom); Kill() would leave it unplayable.
try {
    if ($Rec -and -not $Rec.HasExited) {
        $Rec.StandardInput.WriteLine("q")
        if (-not $Rec.WaitForExit(20000)) { $Rec.Kill() }
    }
} catch { }
try {
    if ($Rec) { $Rec.StandardError.ReadToEnd() | Out-File -LiteralPath $RecErr -Encoding utf8 }
} catch { }
if ((-not $NoRecord) -and -not (Test-Path -LiteralPath (Join-Path $RunDir "watch.mp4"))) {
    Fail "no watch.mp4 captured (see ffmpeg-rec.log)"
}
Write-Host "WATCH: client exited, collecting..." -ForegroundColor Cyan

$NewLogs = @()
if (Test-Path -LiteralPath $LogDir) {
    $NewLogs = Get-ChildItem -LiteralPath $LogDir -Filter "*.log" |
        Where-Object { $_.LastWriteTime -ge $LogStamp } |
        Sort-Object LastWriteTime -Descending | Select-Object -First 1
}
$Result = @()
foreach ($L in $NewLogs) {
    Copy-Item -LiteralPath $L.FullName -Destination (Join-Path $RunDir "client.log") -Force
    $Result += Select-String -LiteralPath $L.FullName -Pattern $TombPattern -ErrorAction SilentlyContinue |
        Select-Object -ExpandProperty Line
}
$Result | ForEach-Object { Write-Host $_ }

$Shots = @()
if (Test-Path -LiteralPath $ShotsDir) {
    $Shots = Get-ChildItem -LiteralPath $ShotsDir -Filter "Dev*.png" |
        Where-Object { $_.LastWriteTime -ge $LogStamp }
    foreach ($S in $Shots) { Copy-Item -LiteralPath $S.FullName -Destination $RunDir -Force }
}
Write-Host "WATCH: collected $($Shots.Count) shot(s)" -ForegroundColor Cyan

$Verdict = "FAIL"
# NOTE: -match is case-INsensitive in PowerShell, and the PASS tombstone
# contains "failures=0" — a plain "FAIL" pattern false-positives on it.
# Match the uppercase standalone token instead (case-sensitive).
if ($Result -and -not ($Result -cmatch " FAIL")) { $Verdict = "PASS" }
"$Verdict $Scenario" | Out-File -LiteralPath (Join-Path $RunDir "WATCH_RESULT.txt") -Encoding utf8
$Result | Out-File -LiteralPath (Join-Path $RunDir "WATCH_RESULT.txt") -Append -Encoding utf8
if ($Verdict -eq "FAIL") { Fail "tombstone missing or FAIL (see $RunDir)" }
try {
    if ($Client -and $Client.MainWindowHandle -ne 0) {
        [Win32]::SetWindowPos($Client.MainWindowHandle, $HWND_NOTOPMOST, 0, 0, 0, 0, $SWP_TOPMOST_FLAGS) | Out-Null
    }
} catch { }
Write-Host "WATCH: ALL GREEN ($RunDir)" -ForegroundColor Green
exit 0
