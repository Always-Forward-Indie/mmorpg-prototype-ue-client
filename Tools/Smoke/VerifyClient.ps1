# VerifyClient: build editor target -> run MMO automation specs -> (optional) smoke clients.
# Usage: .\VerifyClient.ps1                 # build + tests
#        .\VerifyClient.ps1 -Smoke          # build + tests + 2 headless clients
#        .\VerifyClient.ps1 -SkipBuild      # tests (+ smoke) without rebuild
#        .\VerifyClient.ps1 -Sweep -Visual -Goldens  # + offline sweep + BotShot golden gate
#        .\VerifyClient.ps1 -Sweep -Visual -DevMap WorldMapV1  # run on another map
# Exit code is nonzero on any stage failure (CI-friendly; .github/workflows is empty).
param(
    [switch]$Smoke = $false,
    [switch]$SkipBuild = $false,
    [switch]$Sweep = $false,
    [switch]$Visual = $false,
    [switch]$Goldens = $false,
    [string]$TestFilter = "MMO.Protocol",
    # DevMode travel target (passed as -devmode-map=; the boot map arg is
    # overridden by the login travel without it). Default TestLevel: the
    # dedicated bot gym (lit grid floor, PlayerStart, navmesh, KillZ).
    [string]$DevMap = "TestLevel",
    # L3 tripwire (loose by design — catches renderer collapse, not noise).
    # Baseline 2026-10-15 (RTX 4090, 960x540, black viewport): ~240-300fps,
    # p95 ~4ms per sweep phase. Tighten after more runs / lit map.
    [double]$PerfFpsFloor = 30.0,
    [double]$PerfP95CeilMs = 50.0
)

$ErrorActionPreference = "Continue"
$Root = Join-Path $PSScriptRoot ".." | Join-Path -ChildPath ".." | Resolve-Path
$Project = Join-Path $Root "Prototyping.uproject"
$UBT = "D:\Game Dev\UE\UE_5.8\Engine\Build\BatchFiles\Build.bat"
$EditorCmdExe = "D:\Game Dev\UE\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
$EditorGui = "D:\Game Dev\UE\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"
# NOTE: on machines where the engine install lacks the LinuxArm64/VisionOS SDK
# descriptors, editor startup runs UBT ValidatePlatforms -AllPlatforms which
# exits 1 before tests start (UnrealEditor-Cmd only). The GUI binary tolerates
# it, so stage 2 tries -Cmd first (industry-standard headless path) and falls
# back to the GUI binary with a visible note. Fix the engine install
# (Epic Launcher 5.8 -> Verify) to return to the standard path.
$LogDir = Join-Path $Root "Saved\Logs"
$FailPatterns = @("Result=Fail", "Automation Test Failed", "Critical error:", "Assertion failed", "Fatal error", "Ensure failed")

function Fail($msg) { Write-Host "VERIFY: FAIL - $msg" -ForegroundColor Red; exit 1 }

# Preflight: BotShot checkpoints need a renderer; a headless (-nullrhi) run
# produces no screenshots, so -Goldens without -Visual must fail fast here
# instead of launching an expensive build first or silently passing.
if ($Goldens -and -not $Visual) { Fail "-Goldens requires -Visual (headless runs produce no screenshots)" }

# Stage 1: incremental editor build (only the Prototyping module recompiles + UHT).
if (-not $SkipBuild) {
    Write-Host "VERIFY: stage 1/5 - building PrototypingEditor..." -ForegroundColor Cyan
    & $UBT PrototypingEditor Win64 Development -Project="$Project" -WaitMutex
    if ($LASTEXITCODE -ne 0) { Fail "editor build exit=$LASTEXITCODE" }
    Write-Host "VERIFY: build OK" -ForegroundColor Green
}

# Stage 2: headless automation specs (no GPU; EditorContext tests need the editor).
Write-Host "VERIFY: stage 2/5 - running Automation RunTests $TestFilter ..." -ForegroundColor Cyan
$TestStamp = Get-Date
$TestArgs = @("$Project", "-nullrhi", "-unattended", "-nopause", "-NoSplash", "-NoVerifyGC",
    "-ExecCmds=Automation RunTests $TestFilter; Quit")
$CmdOut = & $EditorCmdExe @TestArgs 2>&1 | Out-String
if (($LASTEXITCODE -ne 0) -and ($CmdOut -match "ValidatePlatforms|Unable to find required SDK")) {
    Write-Host "VERIFY: NOTE - UnrealEditor-Cmd blocked by engine SDK validation; falling back to GUI editor binary (verify the 5.8 install to restore the standard path)" -ForegroundColor Yellow
    & $EditorGui @TestArgs 2>&1 | Out-Null
}
$NewLogs = @()
if (Test-Path -LiteralPath $LogDir) {
    $NewLogs = Get-ChildItem -LiteralPath $LogDir -Filter "*.log" |
        Where-Object { $_.LastWriteTime -ge $TestStamp } |
        Sort-Object LastWriteTime -Descending | Select-Object -First 2
}
$Hits = @()
foreach ($L in $NewLogs) {
    $Hits += Select-String -LiteralPath $L.FullName -Pattern ($FailPatterns -join "|") -ErrorAction SilentlyContinue |
        Select-Object -ExpandProperty Line
}
$Summary = @()
foreach ($L in $NewLogs) {
    $Summary += Select-String -LiteralPath $L.FullName -Pattern "tests (passed|failed)|Test Completed|Automation.*finished" -ErrorAction SilentlyContinue |
        Select-Object -Last 4 -ExpandProperty Line
}
$Summary | ForEach-Object { Write-Host $_ }
if ($Hits) { $Hits | Select-Object -First 10 | ForEach-Object { Write-Host $_ -ForegroundColor Red }; Fail "failure signatures in test logs" }
if (-not $NewLogs) { Fail "no fresh editor log found under Saved/Logs" }
Write-Host "VERIFY: tests OK" -ForegroundColor Green

# Stage 3 (optional): offline golden sweep — DevMode client runs full_sweep and quits itself.
if ($Sweep) {
    Write-Host "VERIFY: stage 3/5 - DevMode full_sweep..." -ForegroundColor Cyan
    $SweepStamp = Get-Date
    # Visual runs render the game (watchable window + DevSweep_*.png shots via
    # -devmode-shots); headless runs skip rendering. Never run 2+ visual clients
    # (engine RayTracing assert on one GPU — see SmokeClients.ps1).
    # NOTE 2026-10-12: on this box even ONE rendered client dies in
    # RayTracingGeometryManager.cpp:988 under the Parsec virtual display
    # (plain client, no devmode — control run). Use Visual only on a machine
    # with a working display path; automation stays headless.
    $SweepArgs = @("`"$Project`"", "/Game/Maps/$DevMap", "-game", "-windowed", "-ResX=960", "-ResY=540", "-NoSplash", "-NoVerifyGC", "-devmode=full_sweep", "-devmode-map=$DevMap", "-log=SweepVerify.log")
    if ($Visual) { $SweepArgs += "-devmode-shots" } else { $SweepArgs += "-nullrhi" }
    $SweepProc = Start-Process -FilePath $EditorGui -ArgumentList $SweepArgs -PassThru
    if (-not $SweepProc.WaitForExit(300000)) {
        try { $SweepProc.Kill() } catch { }
        Fail "sweep client did not quit within 300s"
    }
    $SweepLogs = @()
    if (Test-Path -LiteralPath $LogDir) {
        $SweepLogs = Get-ChildItem -LiteralPath $LogDir -Filter "*.log" |
            Where-Object { $_.LastWriteTime -ge $SweepStamp } |
            Sort-Object LastWriteTime -Descending | Select-Object -First 1
    }
    $SweepResult = @()
    foreach ($L in $SweepLogs) {
        $SweepResult += Select-String -LiteralPath $L.FullName -Pattern "DEVMODE_SWEEP_RESULT (PASS|FAIL)" -ErrorAction SilentlyContinue |
            Select-Object -ExpandProperty Line
    }
    $SweepResult | ForEach-Object { Write-Host $_ }
    if (-not $SweepResult) { Fail "no DEVMODE_SWEEP_RESULT tombstone in sweep log" }
    if ($SweepResult -match "DEVMODE_SWEEP_RESULT FAIL") { Fail "full_sweep reported failures" }
    # L3 soft gate (presence only — thresholds come after baseline calibration).
    $PerfMarks = @()
    foreach ($L in $SweepLogs) {
        $PerfMarks += Select-String -LiteralPath $L.FullName -Pattern "DEVMODE_PERF phase=" -ErrorAction SilentlyContinue |
            Select-Object -ExpandProperty Line
    }
    $PerfMarks | ForEach-Object { Write-Host $_ }
    if (-not $PerfMarks) { Fail "no DEVMODE_PERF markers in sweep log" }
    # L3 tripwire on values (loose defaults — see param block for baseline).
    $PerfBad = @()
    foreach ($M in $PerfMarks) {
        if ($M -match "fps=([0-9.]+) p95ms=([0-9.]+)") {
            $Fps = [double]$Matches[1]; $P95 = [double]$Matches[2]
            if (($Fps -gt 0) -and (($Fps -lt $PerfFpsFloor) -or ($P95 -gt $PerfP95CeilMs))) {
                $PerfBad += $M
            }
        }
    }
    if ($PerfBad) { $PerfBad | ForEach-Object { Write-Host "PERF: $_" -ForegroundColor Red }; Fail "perf tripwire: fps<$PerfFpsFloor or p95>$PerfP95CeilMs ms" }
    # Cold-cache shader-compile hitches surface here as LogShaderCompilers
    # activity inside the sweep window (info only — no gate yet).
    foreach ($L in $SweepLogs) {
        $ShaderLines = Select-String -LiteralPath $L.FullName -Pattern "LogShaderCompilers.*(Compiling|Compiled|shaders left)" -ErrorAction SilentlyContinue |
            Select-Object -Last 3 -ExpandProperty Line
    }
    $ShaderLines | ForEach-Object { Write-Host "SHADER: $_" }
    Write-Host "VERIFY: sweep OK" -ForegroundColor Green
}
# Stage 4 (optional): L2 golden gate — visual bot_combat_patrol run saves
# deterministic BotShot checkpoints (DevBot_*.png via -devmode-shots),
# then Compare-Goldens.ps1 gates them against Config/DevMode/goldens.
# NOTE: this is a separate run from full_sweep (stage 3 produces DevSweep_*
# shots, a different family). Standalone use: -Visual -Goldens.
if ($Goldens) {
    Write-Host "VERIFY: stage 4/5 - golden screenshots (bot_combat_patrol)..." -ForegroundColor Cyan
    $GoldenStamp = Get-Date
    # Same launch pattern as the sweep stage; the bot brain quits itself
    # (bDevModeAutoQuit) after the DEVMODE_BOT_RESULT tombstone.
    $GoldenArgs = @("`"$Project`"", "/Game/Maps/$DevMap", "-game", "-windowed", "-ResX=960", "-ResY=540", "-NoSplash", "-NoVerifyGC", "-devmode=bot_combat_patrol", "-devmode-map=$DevMap", "-devmode-shots", "-log=GoldenVerify.log")
    $GoldenProc = Start-Process -FilePath $EditorGui -ArgumentList $GoldenArgs -PassThru
    if (-not $GoldenProc.WaitForExit(300000)) {
        try { $GoldenProc.Kill() } catch { }
        Fail "golden client did not quit within 300s"
    }
    $GoldenLogs = @()
    if (Test-Path -LiteralPath $LogDir) {
        $GoldenLogs = Get-ChildItem -LiteralPath $LogDir -Filter "*.log" |
            Where-Object { $_.LastWriteTime -ge $GoldenStamp } |
            Sort-Object LastWriteTime -Descending | Select-Object -First 1
    }
    $GoldenResult = @()
    foreach ($L in $GoldenLogs) {
        $GoldenResult += Select-String -LiteralPath $L.FullName -Pattern "DEVMODE_BOT_RESULT combat_patrol (PASS|FAIL)" -ErrorAction SilentlyContinue |
            Select-Object -ExpandProperty Line
    }
    $GoldenResult | ForEach-Object { Write-Host $_ }
    if (-not $GoldenResult) { Fail "no DEVMODE_BOT_RESULT tombstone in golden log" }
    if ($GoldenResult -match "DEVMODE_BOT_RESULT combat_patrol FAIL") { Fail "bot_combat_patrol reported failures" }
    Write-Host "VERIFY: golden run OK, comparing screenshots..." -ForegroundColor Cyan
    & (Join-Path $PSScriptRoot "Compare-Goldens.ps1")
    if ($LASTEXITCODE -ne 0) { Fail "golden screenshots differ (review the shots, then run Approve-Goldens.ps1 to bless)" }
    Write-Host "VERIFY: goldens OK" -ForegroundColor Green
}
if ($Smoke) {
    Write-Host "VERIFY: stage 5/5 - smoke clients..." -ForegroundColor Cyan
    & (Join-Path $PSScriptRoot "SmokeClients.ps1") -n 2 -WaitSec 150
    if ($LASTEXITCODE -ne 0) { Fail "smoke clients reported crashes" }
    Write-Host "VERIFY: smoke OK" -ForegroundColor Green
}

Write-Host "VERIFY: ALL GREEN" -ForegroundColor Green
exit 0
