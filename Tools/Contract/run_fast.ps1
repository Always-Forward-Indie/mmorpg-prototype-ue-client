<#
.SYNOPSIS
  Fast L3 batch (atoms, parallel-safe, teleport-based; dev only).
.DESCRIPTION
  Wraps run_l3.ps1 (seed + creds) with the fast mechanics list + JUnit XML.
  Slow chains live in run_slow.ps1. Shared-state tests never go here.
.EXAMPLE
  .\Tools\Contract\run_fast.ps1
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

$fast = @(
    "Tests/Contract/test_admin_smoke.py",
    "Tests/Contract/test_reg_learn.py",
    "Tests/Contract/test_framing.py",
    "Tests/Contract/test_conn.py",
    "Tests/Contract/test_tolerant.py",
    "Tests/Contract/test_vendor.py",
    "Tests/Contract/test_wio.py",
    "Tests/Contract/test_reg_discount.py",
    "Tests/Contract/test_reg_pvp.py",
    "Tests/Contract/test_reg_evict.py",
    "Tests/Contract/test_reg_progression.py"
)
$args = @($fast + @("--junitxml=fast.xml", "--durations=15", "-q"))
& "$PSScriptRoot\run_l3.ps1" -PytestArgs $args -SeedN $SeedN `
    -QuestBotIdx $QuestBotIdx -RepairBotIdx $RepairBotIdx -HandoffBotIdx $HandoffBotIdx
exit $LASTEXITCODE
