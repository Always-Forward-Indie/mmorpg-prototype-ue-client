<#
.SYNOPSIS
  Log alert scan for WSL dev servers (read-only, never live VPS).
.DESCRIPTION
  Tails `docker logs` for login/game/chunk/db and scans for fatal patterns
  (crashes, sanitizers, join outages, stale-registry symptoms). Prints
  matches with service prefix + summary, exits 1 when any FATAL pattern
  hits (CI/soak gate friendly), 0 otherwise.
.EXAMPLE
  .\Tools\WSL\Watch-ServerLogs.ps1
  .\Tools\WSL\Watch-ServerLogs.ps1 -Service chunk -Tail 500
  .\Tools\WSL\Watch-ServerLogs.ps1 -Service all -Tail 2000 -FailOnFatal:$false
#>
param(
    [ValidateSet("login", "game", "chunk", "db", "all")]
    [string]$Service = "all",
    [int]$Tail = 1000,
    [bool]$FailOnFatal = $true
)

$ErrorActionPreference = "Stop"

# Container name fragments (match `docker ps` names).
$targets = @{
    login = @("login-server")
    game  = @("game-server")
    chunk = @("chunk-server")
    db    = @("prototype_db", "_db")
}
$names = if ($Service -eq "all") { $targets.Values | ForEach-Object { $_ } } else { $targets[$Service] }

# FATAL = gate-breaking (crash / sanitizer / outage). WARN = symptoms worth a look.
$fatalPatterns = @(
    "FATAL", "AddressSanitizer", "ThreadSanitizer", "UndefinedBehaviorSanitizer",
    "SEGV", "Segmentation fault", "Assertion.*failed", "purecall",
    "CHUNKID_0", "chunkId: 0", "chunkId 0",
    "Out of memory", "Killed process", "SIGKILL"
)
$warnPatterns = @(
    "stale", "STALE", "already_in_trade", "no_pending_invite",
    "PING_TIMEOUT", "disconnect", "reconnect", "backoff",
    "queue.*overflow", "dropped", "writeQueue", "gcWriteQueues",
    "MOVE_VALIDATE", "Target is dead", "PvP is not available"
)
# SEAM = inter-server contract breaches (chunk<->game). Any hit here means a
# cross-server fact/response was lost: investigate like a FATAL (the next
# "ghost" will surface exactly in these lines). Counts are the seam counters.
$seamPatterns = @(
    "no live chunk socket", "socket not available", "Unknown event type",
    "unexpected data type", "invalid params", "parse error"
)

function Get-MatchingContainer($fragment) {
    $hit = wsl -d Ubuntu -- bash -c "docker ps --format '{{.Names}}'" | Where-Object { $_ -like "*$fragment*" } | Select-Object -First 1
    return $hit
}

$exitCode = 0
# Postgres operational noise that is NOT an app crash (restarts, probes).
$dbBenign = @("administrator command", "starting up", "does not exist", "checkpoint", "autovacuum")
# Global false-positive guard: libraries that log the word FATAL for
# explicitly non-fatal conditions (e.g. login's "[[Error (not fatal)]]",
# postgres "starting up" quoted inside the login/game retry loop — the
# retry itself is the recovery, logged separately on persistent failure).
$globalBenign = @("not fatal", "is starting up")
$seen = @{}
foreach ($frag in $names) {
    $ctr = Get-MatchingContainer $frag
    if (-not $ctr) { Write-Host "[$frag] container not running, skipped"; continue }
    if ($seen.ContainsKey($ctr)) { continue }  # two fragments, one container
    $seen[$ctr] = $true
    $log = wsl -d Ubuntu -- bash -c "docker logs --tail $Tail $ctr 2>&1"
    $log = @($log | Where-Object { $line = $_; -not ($globalBenign | Where-Object { $line -like "*$_*" }) })
    if ($frag -like "*db*") { $log = @($log | Where-Object { $line = $_; -not ($dbBenign | Where-Object { $line -like "*$_*" }) }) }
    $fatals = @($log | Select-String -Pattern $fatalPatterns)
    $warns = @($log | Select-String -Pattern $warnPatterns | Select-Object -Last 20)
    $seams = @($log | Select-String -Pattern $seamPatterns)
    foreach ($m in $fatals) { Write-Host "[$ctr] FATAL: $($m.Line)" }
    if ($warns.Count -gt 0) {
        Write-Host "[$ctr] warn/hint lines (last $($warns.Count)):"
        foreach ($m in $warns) { Write-Host "[$ctr]   $($m.Line)" }
    }
    if ($seams.Count -gt 0) {
        Write-Host "[$ctr] SEAM contract breaches (last $($seams.Count), investigate like FATAL):"
        foreach ($m in ($seams | Select-Object -Last 10)) { Write-Host "[$ctr]   $($m.Line)" }
    }
    if ($warns.Count -gt 0) {
        Write-Host "[$ctr] warn/hint lines (last $($warns.Count)):"
        foreach ($m in $warns) { Write-Host "[$ctr]   $($m.Line)" }
    }
    if ($fatals.Count -gt 0) {
        Write-Host "[$ctr] $($fatals.Count) FATAL pattern hits in last $Tail lines"
        if ($FailOnFatal) { $exitCode = 1 }
    } else {
        Write-Host "[$ctr] clean ($Tail lines scanned)"
    }
}
exit $exitCode
