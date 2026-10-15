# Approve-Goldens: bless current watchable-run screenshots as new goldens.
# Usage: .\Approve-Goldens.ps1 [-ShotsDir ...] [-GoldensDir ...] [-Filter "DevBot_*"]
# Copies matching PNGs from Saved/Screenshots into Config/DevMode/goldens.
# Review the shots (open them!) BEFORE approving — approving a broken frame
# bakes the bug into the gate.
param(
    [string]$ShotsDir = "",
    [string]$GoldensDir = "",
    [string]$Filter = "DevBot_*.png"
)

$ErrorActionPreference = "Stop"
$Root = Join-Path $PSScriptRoot ".." | Join-Path -ChildPath ".." | Resolve-Path
if ([string]::IsNullOrEmpty($ShotsDir)) { $ShotsDir = Join-Path $Root "Saved\Screenshots\WindowsEditor" }
if ([string]::IsNullOrEmpty($GoldensDir)) { $GoldensDir = Join-Path $Root "Config\DevMode\goldens" }

if (-not (Test-Path -LiteralPath $ShotsDir)) { throw "shots dir missing: $ShotsDir (run a -devmode-shots client first)" }
New-Item -ItemType Directory -Force -Path $GoldensDir | Out-Null
$Shots = Get-ChildItem -LiteralPath $ShotsDir -Filter $Filter | Sort-Object LastWriteTime -Descending
if (-not $Shots) { throw "no shots matching '$Filter' in $ShotsDir" }

# Keep newest per file name (engine may version shots across runs).
$Seen = @{}
foreach ($S in $Shots) {
    if ($Seen.ContainsKey($S.Name)) { continue }
    $Seen[$S.Name] = $true
    Copy-Item -LiteralPath $S.FullName -Destination (Join-Path $GoldensDir $S.Name) -Force
    Write-Host ("APPROVED {0} ({1:0}KB)" -f $S.Name, ($S.Length / 1KB))
}
Write-Host ("Goldens dir: {0} ({1} file(s))" -f $GoldensDir, $Seen.Count)
