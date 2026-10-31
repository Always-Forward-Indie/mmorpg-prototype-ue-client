# Live teleport end-to-end (DEV stand only, never prod).
#
# Parks char 615 (uereb_01) at the vendor, starts one live UE bot, then
# admin-teleports it across the map (cross-cell far teleport) and asserts:
#  - server: teleport applied (admin poll) + [TeleportAck] confirmed, no divergence
#  - client: TeleportManager cover line for charId=615, no error/crash signatures
#
# Requires: DEV stand on 127.0.0.1, WSL integration-tests admin API.
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
$LogName = "LiveTeleport.log"

$drive = $env:TEMP.Substring(0, 1).ToLower()
$mntTemp = "/mnt/$drive" + ($env:TEMP.Substring(2) -replace "\\", "/")
$WslHelper = "$mntTemp/livebatch_helper.py"
if (-not (Test-Path (Join-Path $env:TEMP "livebatch_helper.py"))) {
    Write-Host "LIVETP: missing livebatch_helper.py - run RunLiveBatch.ps1 once first (it generates the helper)" -ForegroundColor Red
    exit 1
}

function Invoke-HelperAdmin([string]$AdminArgs) {
    wsl -e sh -c "cd ~/projects/mmorpg-prototype/mmorpg-prototype-integration-tests && python3 $WslHelper $AdminArgs"
}
function Invoke-HelperDb([string]$DbArgs) {
    wsl -e python3 $WslHelper $DbArgs.Split(" ")
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

$fail = 0
function Fail([string]$Msg) { Write-Host "LIVETP FAIL: $Msg" -ForegroundColor Red; $script:fail = 1 }

# 0. no stray clients (a stale session would attach the login to old state)
Get-Process UnrealEditor -ErrorAction SilentlyContinue | ForEach-Object { try { $_.Kill() } catch { } }
Start-Sleep -Seconds 3

# 0b. live-capture the chunk log (docker rotation eats lines in minutes —
# grep the capture file, never `docker logs` after the fact)
$capFile = Join-Path $LogsDir "ChunkCapture.log"
if (Test-Path -LiteralPath $capFile) { Remove-Item -LiteralPath $capFile -Force }
$capErr = "$capFile.err"
if (Test-Path -LiteralPath $capErr) { Remove-Item -LiteralPath $capErr -Force }
$cap = Start-Process -FilePath "wsl.exe" -ArgumentList "-d", "Ubuntu", "docker", "logs", "-f", "mmorpg-prototype-chunk-server-new-chunk-server-1" -RedirectStandardOutput $capFile -RedirectStandardError $capErr -PassThru

# 1. park at skill trainer (-634,2160,300) for live_skills (long preamble:
#    waypoint walk + shop before any hunt, so the bot is guaranteed online
#    when the admin teleport fires — instant scenarios like live_equip quit
#    in ~1s and race the teleport)
Invoke-HelperDb "setpos $Char -634 2160 300"

# 2. start one live bot (live_skills runs minutes; we kill it after asserts)
$log = Join-Path $LogsDir $LogName
if (Test-Path -LiteralPath $log) { Remove-Item -LiteralPath $log -Force }
$args = @("`"$Project`"", $Map, "-game", "-windowed", "-ResX=960", "-ResY=540",
    "-NoSplash", "-NoVerifyGC", "-bot-live=live_skills", "-botlogin=$Login", "-botpass=$Pass",
    "-nullrhi", "-log=$LogName", "-botchar=$Char")
$proc = Start-Process -FilePath $UE -ArgumentList $args -PassThru

$sp = Wait-Log $log "DEVMODE_BOT spawn at" 180
if (-not $sp) { Fail "no spawn line in 180s" }
else { Write-Host "LIVETP: $sp" }

# 2b. seed hunt targets (foxes, template 4) near the trainer, like the batch Ready
if (-not $fail) {
    Write-Host "LIVETP: seeding hunt targets near trainer"
    Invoke-HelperAdmin "spawn $Char -634 2160 300 6"
    Write-Host "LIVETP: waiting out the waypoint+shop preamble"
    Start-Sleep -Seconds 25
}

# 3. cross-cell far teleport via admin-RPC (fox arena 4738,-925).
#    Tolerant poll: the bot keeps walking after the snap, so getState is
#    only informational (arrival within 600cm at any poll inside 20s).
#    The binding asserts are the client cover line + server [TeleportAck].
if (-not $fail) {
    Write-Host "LIVETP: admin teleport $Char -> (4738,-925,300)"
    $TpPy = Join-Path $env:TEMP "live_tp_check.py"
    Set-Content -LiteralPath $TpPy -Value @'
import sys, time
sys.path.insert(0, "/home/shardanov/projects/mmorpg-prototype/mmorpg-prototype-integration-tests")
from bots.admin import AdminClient
cid, x, y, z = int(sys.argv[1]), float(sys.argv[2]), float(sys.argv[3]), float(sys.argv[4])
a = AdminClient()
try:
    rsp = a.call("teleport", characterId=cid, x=x, y=y, z=z, timeout=15.0)
    assert rsp["header"].get("status") == "success", rsp
    print("TP-OK", flush=True)
    import math
    for _ in range(40):
        try:
            st = a.get_state(cid, timeout=5.0)
            pos = (st.get("body", {}) or {}).get("position", {}) or {}
            dx = pos.get("x", 0.0) - x; dy = pos.get("y", 0.0) - y
            if math.sqrt(dx * dx + dy * dy) < 600.0:
                print("TP-APPLIED pos=(%.0f,%.0f)" % (pos.get("x", 0), pos.get("y", 0)), flush=True)
                break
        except Exception as e:
            print("TP-POLL %s" % e, flush=True)
        time.sleep(0.5)
    else:
        print("TP-DRIFTED (teleport did not stick)", flush=True)
        sys.exit(1)
finally:
    a.close()
'@ -Encoding ASCII
    $WslTp = "$mntTemp/live_tp_check.py"
    wsl -e python3 $WslTp $Char 4738 -925 300
    if ($LASTEXITCODE -ne 0) { Fail "teleport did not stick (see TP-DRIFTED)" }
    Start-Sleep -Seconds 10
}

# 4. client asserts
$cover = Select-String -LiteralPath $log -Pattern "TeleportManager: charId=$Char teleport to" | Select-Object -Last 1
if ($cover) { Write-Host "LIVETP: $($cover.Line)" } else { Fail "no TeleportManager cover line for charId=$Char" }
$errs = Select-String -LiteralPath $log -Pattern 'Critical error|Fatal error|Ensure failed|Automation Test Failed' -ErrorAction SilentlyContinue
if ($errs) { Fail "client error signatures:"; $errs | Select-Object -First 5 | ForEach-Object { Write-Host $_.Line } }

# 5. server asserts from the live capture (immune to docker log rotation).
#    NOTE: [TeleportAck] success-confirm is INFO and LOG_LEVEL_EVENTS=warn
#    suppresses it by design (errors are the signal) — assert the ABSENCE
#    of failure lines instead: no divergence, no unknown-event.
$srv = Select-String -LiteralPath $capFile -Pattern 'TeleportAck|Unknown event type' -ErrorAction SilentlyContinue
Write-Host "LIVETP server ack lines:"; $srv | Select-Object -Last 5 | ForEach-Object { Write-Output $_.Line }
$srvText = ($srv | ForEach-Object { $_.Line }) -join "`n"
if ($srvText -match "divergence") { Fail "teleportAck divergence reported" }
if ($srvText -match "Unknown event type") { Fail "server does not know teleportAck" }

# 5b. client asserts, post-snap: reveal + ack-send happened, and no
#     positionCorrection was APPLIED after the cover (server grace means
#     none should arrive; the manager guard would log "ignoring" instead).
$reveal = Select-String -LiteralPath $log -Pattern "TeleportManager: reveal charId=$Char" | Select-Object -Last 1
if ($reveal) { Write-Host "LIVETP: $($reveal.Line)" } else { Fail "no TeleportManager reveal line for charId=$Char" }
$ackSent = Select-String -LiteralPath $log -Pattern "TeleportManager: teleportAck sent charId=$Char" | Select-Object -Last 1
if ($ackSent) { Write-Host "LIVETP: $($ackSent.Line)" } else { Fail "teleportAck was never sent" }
$appliedCorr = $null
if ($cover) {
    $appliedCorr = Select-String -LiteralPath $log -Pattern "positionCorrection: clientId=" -ErrorAction SilentlyContinue | Where-Object { $_.LineNumber -gt $cover.LineNumber } | Select-Object -Last 1
}
if ($appliedCorr) { Fail "a positionCorrection was applied after the cover: $($appliedCorr.Line)" }

# 6. cleanup
if (-not $proc.HasExited) { try { $proc.Kill() } catch { } }
try { $cap.Kill() } catch { }

if ($fail) { Write-Host "LIVETP: FAIL" -ForegroundColor Red; exit 1 }
Write-Host "LIVETP: PASS" -ForegroundColor Green
