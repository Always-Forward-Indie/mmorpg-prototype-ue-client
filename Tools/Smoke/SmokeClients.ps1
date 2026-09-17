# Smoke: N real UE clients, wait, scan logs, kill. Wraps LaunchClients.ps1 pattern.
# Usage: .\SmokeClients.ps1 -n 2 -WaitSec 150            # headless (default)
#        .\SmokeClients.ps1 -n 2 -WaitSec 150 -Visual     # with rendering
#
# Headless (-nullrhi) is the default: two rendered -game instances on one GPU
# crash in RayTracingGeometryManager (engine assert, not a game bug). Visual
# mode is for manual eyeballing only — run ONE client at a time.
param(
    [int]$n = 2,
    [int]$WaitSec = 150,
    [switch]$NoLog = $false,
    [switch]$Visual = $false
)

$ErrorActionPreference = "Continue"
$UE = "D:\Game Dev\UE\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"
$Project = Join-Path $PSScriptRoot "..\..\Prototyping.uproject"
$Map = "/Game/Maps/MainGameContainerLevel"
$W = 960; $H = 540
$Slots = @(@{ X = 0; Y = 0 }, @{ X = 960; Y = 0 }, @{ X = 0; Y = 560 }, @{ X = 960; Y = 560 })

$LogsDir = Join-Path $PSScriptRoot "..\..\Saved\Logs"
$StartTime = Get-Date

# Case-insensitive crash scan. NOTE: engine writes "Critical error:" and
# "Assertion failed" (NOT "CRITICAL"/"Fatal error") — match those too.
$CrashPattern = "critical error|assertion failed|fatal|ensure failed|exception_access|unhandled exception|gpu.*removed"
$InfoPattern = "ReadyFlags|RemoveLoadingScreen|LogConnection.*connected"

$Procs = @()
for ($i = 0; $i -lt $n; $i++) {
    $slot = $Slots[$i % $Slots.Count]
    $args = @("`"$Project`"", $Map, "-game", "-windowed", "-ResX=$W", "-ResY=$H",
        "-WinX=$($slot.X)", "-WinY=$($slot.Y)", "-NoSplash", "-NoVerifyGC")
    if (-not $Visual) { $args += "-nullrhi" }
    if (-not $NoLog) { $args += "-log" }
    $Procs += Start-Process -FilePath $UE -ArgumentList $args -PassThru
    Start-Sleep -Milliseconds 1000
}
if ($Visual -and $n -gt 1) {
    Write-Host "WARNING: multiple visual clients risk the engine RayTracing assert; prefer -n 1" -ForegroundColor Yellow
}
Write-Host "Launched $n client(s), waiting $WaitSec s..." -ForegroundColor Cyan
Start-Sleep -Seconds $WaitSec

$Failed = $false
$NewLogs = @()
if (Test-Path -LiteralPath $LogsDir) {
    # Match by write time (UE reuses file names across runs, rotating old ones).
    $NewLogs = Get-ChildItem -LiteralPath $LogsDir -Filter "*.log" |
        Where-Object { $_.LastWriteTime -ge $StartTime } |
        Sort-Object LastWriteTime -Descending
    if (-not $NewLogs) {
        $NewLogs = Get-ChildItem -LiteralPath $LogsDir -Filter "*.log" |
            Sort-Object LastWriteTime -Descending | Select-Object -First $n
    }
}
foreach ($L in $NewLogs) {
    $Hits = Select-String -LiteralPath $L.FullName -Pattern $CrashPattern -ErrorAction SilentlyContinue |
        Select-Object -Last 5
    $Info = Select-String -LiteralPath $L.FullName -Pattern $InfoPattern -ErrorAction SilentlyContinue |
        Select-Object -Last 3
    Write-Host "--- $($L.Name) ---" -ForegroundColor Cyan
    $Hits | ForEach-Object { Write-Host $_.Line }
    $Info | ForEach-Object { Write-Host $_.Line }
    if ($Hits) { $Failed = $true }
}

foreach ($P in $Procs) {
    try { if (-not $P.HasExited) { $P.Kill() } } catch { }
}
if ($Failed) { Write-Host "SMOKE: FAIL (crash signatures in logs)" -ForegroundColor Red; exit 1 }
Write-Host "SMOKE: DONE (no crash signatures)" -ForegroundColor Green; exit 0
