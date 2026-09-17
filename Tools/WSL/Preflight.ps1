# Preflight for dev-server tests (Windows -> WSL).
# Checks: WSL dist up, dev containers up, TCP ports reachable from Windows,
# server_config.json present. Exit 0 = ready, 1 = not ready.
param(
    [string]$Distro = "Ubuntu",
    [string]$ServersRoot = "projects/mmorpg-prototype"
)

$ErrorActionPreference = "Continue"
$Failed = $false

function Fail([string]$Msg) {
    Write-Host "PREFLIGHT FAIL: $Msg" -ForegroundColor Red
    $script:Failed = $true
}

# 1. WSL distro responsive (wsl --list parsing is encoding-fragile; use exit code)
wsl -d $Distro -- echo wsl-up 2>$null | Out-Null
if ($LASTEXITCODE -ne 0) {
    Fail "WSL distro $Distro not responding"
} else { Write-Host "WSL $Distro running" -ForegroundColor Green }

# 2. Containers (login creates mmo_network + db; game/chunk join it)
$Ps = wsl -d $Distro -- bash -c "docker ps --format '{{.Names}}'" 2>$null
Write-Host "Running containers:" -ForegroundColor Cyan
Write-Host ($Ps -join "`n")
foreach ($Need in @("mmorpg_prototype_db", "login-server", "game-server", "chunk-server")) {
    if (($Ps -join "`n") -notmatch $Need) {
        Fail "container '$Need' not running (bring up: login -> game -> chunk, docker-compose.dev.yml in ~/$ServersRoot)"
    }
}

# 2b. Duplicate mmo_network splits Docker DNS (chunk/game resolve fails with
# "Host not found" fatal). Exactly one network named mmo_network must exist.
$Nets = wsl -d $Distro -- bash -c "docker network ls --format '{{.Name}}'" 2>$null
$Dupes = @($Nets | Where-Object { $_ -match "mmo_network" })
if ($Dupes.Count -ne 1) {
    Fail ("mmo_network split: found [{0}] (want exactly [mmo_network]). Fix: stop all, docker network rm <dup>, bring up login -> game -> chunk" -f ($Dupes -join ", "))
} else { Write-Host "mmo_network OK (single)" -ForegroundColor Green }

# 3. Ports reachable from Windows (WSL2 localhost forwarding)
foreach ($Port in @(27014, 27016, 27017)) {
    $R = Test-NetConnection -ComputerName 127.0.0.1 -Port $Port -WarningAction SilentlyContinue
    if ($R.TcpTestSucceeded) { Write-Host "port ${Port}: open" -ForegroundColor Green }
    else { Fail "port $Port closed on 127.0.0.1 (is the matching server container up?)" }
}

# 4. server_config.json present (gitignored, dev variant expected for tests)
$Cfg = Join-Path $PSScriptRoot "..\..\server_config.json"
if (-not (Test-Path -LiteralPath $Cfg)) { Fail "server_config.json missing (run: Copy-Item server_config_dev.json server_config.json)" }
else {
    $Txt = Get-Content -LiteralPath $Cfg -Raw
    if ($Txt -match "23\.88\.102\.182") { Fail "server_config.json points at LIVE - tests must run against dev (127.0.0.1)" }
    else { Write-Host "server_config.json OK (dev)" -ForegroundColor Green }
}

if ($Failed) { Write-Host "PREFLIGHT: NOT READY" -ForegroundColor Red; exit 1 }
Write-Host "PREFLIGHT: READY" -ForegroundColor Green; exit 0
