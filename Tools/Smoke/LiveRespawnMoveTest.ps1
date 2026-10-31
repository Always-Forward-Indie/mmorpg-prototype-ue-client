# Live respawn-locomotion regression (DEV stand only, never prod).
#
# The cover-transition revive must not leave the pawn unable to translate
# (bMovementLocked stuck / MOVE_None restored over WALKING): the symptom is
# a pawn that spins in place (rotation-only packets) but never displaces.
#
# Flow: park 615 at fox arena -> live_loot_pickup hunts -> sethp 1 + seed
# foxes on the player -> death -> auto-revive -> keep hunting 90s ->
# assert 2D displacement after revive > 300cm.
param(
    [string]$Login = "uereb_01",
    [string]$Pass = "BotPass123",
    [int]$Char = 615
)

$ErrorActionPreference = "Stop"
$UE = "D:\Game Dev\UE\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"
$Project = Join-Path $PSScriptRoot "..\..\Prototyping.uproject"
$Map = "/Game/Maps/MainGameContainerLevel"
$LogsDir = Join-Path $PSScriptRoot "..\..\Saved\Logs"
$LogName = "LiveRespawnMove.log"

$drive = $env:TEMP.Substring(0, 1).ToLower()
$mntTemp = "/mnt/$drive" + ($env:TEMP.Substring(2) -replace "\\", "/")
$WslHelper = "$mntTemp/livebatch_helper.py"
if (-not (Test-Path (Join-Path $env:TEMP "livebatch_helper.py"))) {
    Write-Host "LIVERESP: missing livebatch_helper.py - run RunLiveBatch.ps1 once first" -ForegroundColor Red
    exit 1
}

function Invoke-HelperDb([string]$DbArgs) { wsl -e python3 $WslHelper $DbArgs.Split(" ") }
function Invoke-HelperAdmin([string]$AdminArgs) {
    wsl -e sh -c "cd ~/projects/mmorpg-prototype/mmorpg-prototype-integration-tests && python3 $WslHelper $AdminArgs"
}
function Wait-Log([string]$File, [string]$Pattern, [int]$TimeoutSec) {
    $t0 = Get-Date
    while (((Get-Date) - $t0).TotalSeconds -lt $TimeoutSec) {
        if (Test-Path -LiteralPath $File) {
            $h = Select-String -LiteralPath $File -Pattern $Pattern | Select-Object -First 1
            if ($h) { return $h.Line }
        }
        Start-Sleep -Seconds 2
    }
    return $null
}
function PlayerPos([string]$File, [int]$CharId) {
    $m = Select-String -LiteralPath $File -Pattern ('"id":' + $CharId) | Select-Object -Last 1
    if (-not $m) { return $null }
    $l = $m.Line; $i = $l.IndexOf("position")
    if ($i -lt 0) { return $null }
    $seg = $l.Substring($i, [Math]::Min(160, $l.Length - $i))
    $px = [regex]::Match($seg, '"x":(-?[\d.]+)').Groups[1].Value
    $py = [regex]::Match($seg, '"y":(-?[\d.]+)').Groups[1].Value
    if (-not $px -or -not $py) { return $null }
    return @([double]$px, [double]$py)
}

$fail = 0
function Fail([string]$Msg) { Write-Host "LIVERESP FAIL: $Msg" -ForegroundColor Red; $script:fail = 1 }

Get-Process UnrealEditor -ErrorAction SilentlyContinue | ForEach-Object { try { $_.Kill() } catch { } }
Start-Sleep -Seconds 3

$capFile = Join-Path $LogsDir "ChunkCaptureResp.log"
if (Test-Path -LiteralPath $capFile) { Remove-Item -LiteralPath $capFile -Force }
$capErr = "$capFile.err"
if (Test-Path -LiteralPath $capErr) { Remove-Item -LiteralPath $capErr -Force }
$cap = Start-Process -FilePath "wsl.exe" -ArgumentList "-d", "Ubuntu", "docker", "logs", "-f", "mmorpg-prototype-chunk-server-new-chunk-server-1" -RedirectStandardOutput $capFile -RedirectStandardError $capErr -PassThru

Invoke-HelperDb "setpos $Char -634 2160 300"

$log = Join-Path $LogsDir $LogName
if (Test-Path -LiteralPath $log) { Remove-Item -LiteralPath $log -Force }
$args = @("`"$Project`"", $Map, "-game", "-windowed", "-ResX=960", "-ResY=540",
    "-NoSplash", "-NoVerifyGC", "-bot-live=live_skills", "-botlogin=$Login", "-botpass=$Pass",
    "-nullrhi", "-log=$LogName", "-botchar=$Char")
$proc = Start-Process -FilePath $UE -ArgumentList $args -PassThru

$sp = $null
if (-not $fail) {
    # Seed patrol targets at JOIN (not spawn): the brain must find mobs the
    # instant it starts, or a target-less patrol PASSes in seconds and quits
    # before any later seed lands (unknown_character race).
    $jn = Wait-Log $log "joinGameCharacter OK" 180
    if (-not $jn) { Fail "no join in 180s" }
    else { Invoke-HelperAdmin "spawn $Char -634 2160 300 6" }
}
$sp = Wait-Log $log "DEVMODE_BOT spawn at" 180
if (-not $fail) {
    if (-not $sp) { Fail "no spawn line in 180s" }
    else { Write-Host "LIVERESP: $sp" }
}

# Skills targets (batch Ready "spawn 6"): the hunt starts after a long
# waypoint+shop preamble, so join-time seeds are scenery for our kill
# rounds; the brain cannot finish for minutes.
if (-not $fail) {
    Start-Sleep -Seconds 8
    $pp0 = PlayerPos $log $Char
    if ($pp0) { Write-Host ("LIVERESP: settled at ({0:n0},{1:n0})" -f $pp0[0], $pp0[1]) }
    Start-Sleep -Seconds 20
}

# Kill rounds: sethp 1 + seed AGGRESSIVE wolves (template 6, aggro 700)
# on the player, repeat until dead. Foxes (template 4) are passive and the
# bot one-shots stragglers; loot bots also finish in seconds, so strike
# immediately after spawn (batch death flow order: sethp, then seed).
$rev = $null
if (-not $fail) {
    $KillPy = Join-Path $env:TEMP "live_kill.py"
    Set-Content -LiteralPath $KillPy -Value @'
import sys
sys.path.insert(0, "/home/shardanov/projects/mmorpg-prototype/mmorpg-prototype-integration-tests")
from bots.admin import AdminClient
cid, x, y, z = int(sys.argv[1]), float(sys.argv[2]), float(sys.argv[3]), float(sys.argv[4])
a = AdminClient()
try:
    rsp = a.call("setHP", characterId=cid, value=1, timeout=10.0)
    print("SETHP", rsp.get("header", {}).get("status"), flush=True)
    # Single call (fast): 3 wolves stacked on the player. The spread variant
    # costs 8 sequential round-trips the loot brain uses to finish and quit.
    rsp = a.spawn_mob(cid, 1, x, y, z, 3, mob_template_id=6)
    print("SPAWNWOLF", rsp.get("header", {}).get("status"), str(rsp.get("body", {}))[:160], flush=True)
finally:
    a.close()
'@ -Encoding ASCII
    $WslKill = "$mntTemp/live_kill.py"
    $t0 = Get-Date
    $round = 0
    while (((Get-Date) - $t0).TotalSeconds -lt 150) {
        $rev = Select-String -LiteralPath $log -Pattern "has been revived" | Select-Object -First 1
        if ($rev) { break }
        if (Select-String -LiteralPath $log -Pattern "LIVEBOT_RESULT live_skills") { Fail "bot finished before death"; break }
        $round++
        if ($round -gt 4) { break }
        $pp = PlayerPos $log $Char
        if ($pp) {
            Write-Host ("LIVERESP: kill round $round at ({0:n0},{1:n0})" -f $pp[0], $pp[1])
            wsl -e python3 $WslKill $Char $($pp[0]) $($pp[1]) 300
        }
        Start-Sleep -Seconds 20
    }
    if (-not $fail -and -not $rev) { Fail "no revive in 150s" }
    elseif ($rev) { Write-Host "LIVERESP: $($rev.Line.Substring(0, [Math]::Min(150, $rev.Line.Length)))" }
}

# Sample post-revive displacement for 90s (hunt must resume: >300cm).
if (-not $fail) {
    Start-Sleep -Seconds 10
    $p0 = PlayerPos $log $Char
    Start-Sleep -Seconds 80
    $p1 = PlayerPos $log $Char
    if (-not $p0 -or -not $p1) { Fail "no position samples" }
    else {
        $dx = $p1[0] - $p0[0]; $dy = $p1[1] - $p0[1]
        $dist = [Math]::Sqrt($dx * $dx + $dy * $dy)
        Write-Host ("LIVERESP: post-revive displacement {0:n0}cm ({1:n0},{2:n0}) -> ({3:n0},{4:n0})" -f $dist, $p0[0], $p0[1], $p1[0], $p1[1])
        if ($dist -lt 300.0) { Fail "pawn did not translate after revive (stuck locomotion)" }
    }
}

if (-not $proc.HasExited) { try { $proc.Kill() } catch { } }
try { $cap.Kill() } catch { }

if ($fail) { Write-Host "LIVERESP: FAIL" -ForegroundColor Red; exit 1 }
Write-Host "LIVERESP: PASS" -ForegroundColor Green
