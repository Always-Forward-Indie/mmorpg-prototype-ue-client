<#
.SYNOPSIS
  One-command L3 contract run (dev only, never live VPS).
.DESCRIPTION
  Seeds bot accounts (Tools/Bots/seed_bots.py), exports MMO_*/MMO2_* env creds
  from Tools/Bots/bot_accounts.json, then runs pytest Tests/Contract/.
  Without this wrapper the server-backed tests SKIP (see conftest_helpers.py).
.EXAMPLE
  .\Tools\Contract\run_l3.ps1
  .\Tools\Contract\run_l3.ps1 -PytestArgs @("Tests/Contract/test_quest.py","-x","-q")
#>
param(
    [string[]]$PytestArgs = @("Tests/Contract/", "-q"),
    [int]$SeedN = 8
)

$ErrorActionPreference = "Stop"
$Repo = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
Set-Location -LiteralPath $Repo

# 0. Servers must be up (fast fail, no cryptic skips).
$need = @(
    @{ Name = "login"; Port = 27014 },
    @{ Name = "game"; Port = 27016 },
    @{ Name = "chunk"; Port = 27017 }
)
foreach ($s in $need) {
    $tcp = New-Object Net.Sockets.TcpClient
    try {
        $iar = $tcp.BeginConnect("127.0.0.1", $s.Port, $null, $null)
        if (-not $iar.AsyncWaitHandle.WaitOne(3000)) { throw "timeout" }
        $tcp.EndConnect($iar)
    } catch {
        throw "dev $($s.Name) not reachable on 127.0.0.1:$($s.Port) - bring up WSL dev servers first (see Tools/Tests/README.md)"
    } finally {
        $tcp.Close()
    }
}

# 1. Seed bot accounts (idempotent: existing logins are reused).
python Tools/Bots/seed_bots.py --n $SeedN
if ($LASTEXITCODE -ne 0) { throw "seed_bots.py failed" }

# 2. Export MMO_* (bot_01) + MMO2_* (bot_02) for the contract session.
$acc = Get-Content -LiteralPath (Join-Path $Repo "Tools/Bots/bot_accounts.json") -Raw | ConvertFrom-Json
function Get-Bot($login) {
    $b = $acc.$login
    if (-not $b) { throw "no $login in bot_accounts.json (seed -n must be >= 2)" }
    return $b
}
$b1 = Get-Bot "bot_01"
$b2 = Get-Bot "bot_02"
$env:MMO_CLIENT_ID = "$($b1.client_id)"
$env:MMO_HASH = "$($b1.hash)"
$env:MMO_CHARACTER_ID = "$($b1.character_id)"
$env:MMO2_CLIENT_ID = "$($b2.client_id)"
$env:MMO2_HASH = "$($b2.hash)"
$env:MMO2_CHARACTER_ID = "$($b2.character_id)"
Write-Host ("creds: bot_01 char={0} bot_02 char={1}" -f $env:MMO_CHARACTER_ID, $env:MMO2_CHARACTER_ID)

# 3. Run contracts.
python -m pytest @PytestArgs
exit $LASTEXITCODE
