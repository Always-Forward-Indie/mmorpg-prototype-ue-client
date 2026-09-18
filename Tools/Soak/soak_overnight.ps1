<#
.SYNOPSIS
  Overnight soak: 8 x ~60min repeatable bot segments with gates (dev only).
.DESCRIPTION
  Chains run_swarm scenarios (patrol/kill/harvest/death/combat_sweep/chat_mesh
  — all repeatable; quest/vendor/repair/trade excluded: single-shot fixtures
  would wedge). Before each segment: Preflight (with 30-min wait-retry for
  host flaps — WSL poweroffs self-heal via restart policies). After each:
  Watch-ServerLogs gate (FATAL aborts the night with a report). Progress is
  appended to Tools/Bots/soak_overnight.log incrementally, so any
  interruption still leaves a report.
.EXAMPLE
  .\Tools\Soak\soak_overnight.ps1
  .\Tools\Soak\soak_overnight.ps1 -Segments 2 -Minutes 10  # smoke of the runner
#>
param(
    [int]$Segments = 8,
    [double]$Minutes = 60.0,
    [string[]]$Rotation = @("patrol", "kill", "harvest", "death", "combat_sweep", "chat_mesh", "patrol", "kill")
)

$ErrorActionPreference = "Stop"
$Repo = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
Set-Location -LiteralPath $Repo
$Log = Join-Path $Repo "Tools/Bots/soak_overnight.log"

function Get-LogLines($ctr) {
    # Monotonic line count per container (docker logs persist across
    # restarts). Clock-skew-proof, unlike --since.
    $n = wsl -d Ubuntu -- bash -c "docker logs $ctr 2>&1 | wc -l"
    return [int]$n
}

function Write-Soak($msg) {
    $line = "[{0}] {1}" -f (Get-Date -Format "yyyy-MM-dd HH:mm:ss"), $msg
    Write-Host $line
    Add-Content -LiteralPath $Log -Value $line
}

function Wait-Preflight($maxMin = 30) {
    $end = (Get-Date).AddMinutes($maxMin)
    while ((Get-Date) -lt $end) {
        try {
            & (Join-Path $Repo "Tools/WSL/Preflight.ps1") | Out-Null
            if ($LASTEXITCODE -eq 0) { return $true }
        } catch { }
        Write-Soak "preflight NOT READY, retry in 5 min (host flap window)"
        Start-Sleep -Seconds 300
    }
    return $false
}

Write-Soak ("soak start: {0} segments x {1} min ({2})" -f $Segments, $Minutes, ($Rotation -join ","))
$done = 0
for ($s = 0; $s -lt $Segments; $s++) {
    $scenario = $Rotation[$s % $Rotation.Count]
    Write-Soak ("--- segment {0}/{1}: {2} ---" -f ($s + 1), $Segments, $scenario)
    if (-not (Wait-Preflight)) {
        Write-Soak "ABORT: preflight never recovered"
        exit 2
    }
    $marks = @{}
    $frags = @{ login = "login-server"; game = "game-server"; chunk = "chunk-server"; db = "prototype_db" }
    foreach ($svc in $frags.Keys) {
        $frag = $frags[$svc]
        # Same fragment rule as Watch-ServerLogs.ps1; first match wins.
        $ctr = @(wsl -d Ubuntu -- bash -c "docker ps --format '{{.Names}}'" |
            Where-Object { $_ -like "*$frag*" } | Select-Object -First 1)
        if ($ctr) { $marks[$svc] = @{ Ctr = "$ctr"; Before = (Get-LogLines $ctr) } }
    }
    python Tools/Bots/run_swarm.py --n 8 --scenario $scenario --minutes $Minutes
    $code = $LASTEXITCODE
    Write-Soak ("segment {0} swarm exit={1}" -f $scenario, $code)
    if ($code -ne 0) {
        Write-Soak ("ABORT: swarm segment failed ({0})" -f $scenario)
        exit 1
    }
    # Log gate scoped to lines appended DURING this segment (marker-based:
    # clock skew makes --since unreliable, and a fixed tail reaches days
    # back into pre-existing operational noise).
    $gateFailed = $false
    foreach ($svc in $marks.Keys) {
        $after = Get-LogLines $marks[$svc].Ctr
        # Exact segment scope, no floor (a floor re-admits ancient noise).
        # Log rotation/recreate resets the counter (after < before) — then
        # fall back to a bounded tail.
        if ($after -lt $marks[$svc].Before) {
            $tail = 500
        } else {
            $tail = ($after - $marks[$svc].Before) + 20
        }
        & (Join-Path $Repo "Tools/WSL/Watch-ServerLogs.ps1") -Service $svc -Tail $tail | Out-Null
        if ($LASTEXITCODE -ne 0) { $gateFailed = $true }
    }
    if ($gateFailed) {
        Write-Soak "ABORT: log gate fired after $scenario"
        exit 1
    }
    $done++
    Write-Soak ("segment {0}/{1} green" -f ($s + 1), $Segments)
}
Write-Soak ("SOAK COMPLETE: {0}/{1} segments green" -f $done, $Segments)
exit 0
