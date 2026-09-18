<#
.SYNOPSIS
  Reset swarm-bot state for hermetic quest/contract runs (dev only).
.DESCRIPTION
  The fox quest is NOT repeatable and bot quest rows persist in Postgres AND
  in the game-server in-memory cache. Stale rows (turned_in, or mid-farm
  from an earlier run) wedge both the quest swarm and test_quest.py accept
  case. This script deletes player_quest rows for the seeded bot characters,
  then bounces game-server (drops its quest cache) and chunk-server (fresh
  static push + respawn), waiting for readiness between steps.
  Scoped to bot_accounts.json character IDs only - never touches real players.
.EXAMPLE
  .\Tools\Bots\reset_bots.ps1
#>
$ErrorActionPreference = "Stop"
$Repo = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
Set-Location -LiteralPath $Repo

$accPath = Join-Path $Repo "Tools/Bots/bot_accounts.json"
if (-not (Test-Path -LiteralPath $accPath)) {
    throw "no bot_accounts.json - run Tools/Bots/seed_bots.py first"
}
$acc = Get-Content -LiteralPath $accPath -Raw | ConvertFrom-Json
$charIds = @()
for ($i = 1; $i -le 8; $i++) {
    $login = "bot_{0:00}" -f $i
    $b = $acc.$login
    if ($b -and [int]$b.character_id -gt 0) { $charIds += [int]$b.character_id }
}
if ($charIds.Count -eq 0) { throw "no bot_01..bot_08 characters in bot_accounts.json" }
$inList = ($charIds | ForEach-Object { "$_" }) -join ","

Write-Host ("wiping player_quest for bot chars: {0}" -f $inList)
# Pass the ID list via a temp file (wsl.exe mangles inline SQL quoting).
$idsFile = Join-Path $env:TEMP "bot_reset_ids.txt"
Set-Content -LiteralPath $idsFile -Value $inList -NoNewline
$idsWsl = ($idsFile -replace '^C:', '/mnt/c') -replace '\\', '/'
wsl -d Ubuntu -- /home/shardanov/db_wipe_quest.sh $idsWsl
if ($LASTEXITCODE -ne 0) { throw "quest wipe failed" }

Write-Host "bouncing game-server (drops in-memory quest cache)..."
wsl -d Ubuntu -- bash -lc "docker restart mmorpg-prototype-game-server-game-server-1"
Start-Sleep -Seconds 30

Write-Host "bouncing chunk-server (fresh static push)..."
wsl -d Ubuntu -- bash -lc "docker restart mmorpg-prototype-chunk-server-new-chunk-server-1"
Start-Sleep -Seconds 45

$ready = 0
for ($i = 1; $i -le 9; $i++) {
    Start-Sleep -Seconds 10
    # Warm-up rule (Tools/Tests/README.md): the static push completes in
    # seconds; the "Active clients" heartbeat every 10s proves the sim loop
    # is alive. NOTE: no --since filter: container/host clock skew makes
    # time-windowed log reads unreliable. --tail is skew-proof.
    $log = wsl -d Ubuntu -- bash -lc "docker logs --tail 60 mmorpg-prototype-chunk-server-new-chunk-server-1 2>&1"
    $zones = @($log | Select-String -Pattern "Spawn Zone ID").Count
    $heartbeat = @($log | Select-String -Pattern "Active clients").Count
    if ($zones -gt 0 -or $heartbeat -gt 0) { $ready = 1; break }
}
if ($ready -lt 1) { throw "chunk-server does not look ready after bounce (no spawn zones / heartbeat)" }
Write-Host "reset complete: quest rows wiped, game+chunk bounced and simulating."
