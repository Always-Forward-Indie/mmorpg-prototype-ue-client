<#
.SYNOPSIS
  Slow L3 batch (chains + shared-state, SERIAL; dev only).
.DESCRIPTION
  Wraps run_l3.ps1 (seed + creds) with chain tests that touch global state
  (champion counters, timed schedules, quest progress) — never parallelize
  this list. Ephemeral bots + resetWorld keep runs hermetic (no restarts).
.EXAMPLE
  .\Tools\Contract\run_slow.ps1
#>
param(
    [int]$SeedN = 8,
    [int]$QuestBotIdx = 6,
    [int]$RepairBotIdx = 4,
    [int]$HandoffBotIdx = 7
)

$ErrorActionPreference = "Stop"
$Repo = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
Set-Location -LiteralPath $Repo

$slow = @(
    "Tests/Contract/test_reg_champion.py",
    "Tests/Contract/test_reg_timed.py",
    "Tests/Contract/test_reg_turnin_short.py",
    "Tests/Contract/test_combat.py",
    "Tests/Contract/test_harvest.py",
    "Tests/Contract/test_handoff.py",
    "Tests/Contract/test_trade.py",
    "Tests/Contract/test_cross_visibility.py",
    "Tests/Contract/test_repair.py",
    "Tests/Contract/test_quest.py"
)
$args = @($slow + @("--junitxml=slow.xml", "--durations=15", "-q"))
& "$PSScriptRoot\run_l3.ps1" -PytestArgs $args -SeedN $SeedN `
    -QuestBotIdx $QuestBotIdx -RepairBotIdx $RepairBotIdx -HandoffBotIdx $HandoffBotIdx
exit $LASTEXITCODE
