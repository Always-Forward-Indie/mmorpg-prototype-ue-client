# Compare-Goldens: visual regression gate for watchable client runs.
# Compares Saved/Screenshots/<name>.png against Config/DevMode/goldens/<name>.png.
# Usage: .\Compare-Goldens.ps1 [-Tolerance 12] [-MaxDiffPercent 5.0] [-ShotsDir ...] [-GoldensDir ...]
# Exit code nonzero on any mismatch or missing file (CI-friendly).
# NOTE: scenes contain dynamic elements (ambient creatures, particles), so keep
# thresholds generous; deterministic-golden mode (fixed seeds, frozen time) is future work.
param(
    [int]$Tolerance = 12,
    [double]$MaxDiffPercent = 5.0,
    [string]$ShotsDir = "",
    [string]$GoldensDir = ""
)

$ErrorActionPreference = "Continue"
$Root = Join-Path $PSScriptRoot ".." | Join-Path -ChildPath ".." | Resolve-Path
if ([string]::IsNullOrEmpty($ShotsDir)) { $ShotsDir = Join-Path $Root "Saved\Screenshots\WindowsEditor" }
if ([string]::IsNullOrEmpty($GoldensDir)) { $GoldensDir = Join-Path $Root "Config\DevMode\goldens" }

Add-Type -AssemblyName System.Drawing

function Diff-Images($shotPath, $goldenPath, $tol) {
    $a = [Drawing.Bitmap]::FromFile($shotPath)
    $b = [Drawing.Bitmap]::FromFile($goldenPath)
    try {
        if ($a.Width -ne $b.Width -or $a.Height -ne $b.Height) {
            return @{ Ok = $false; Detail = "size $($a.Width)x$($a.Height) vs $($b.Width)x$($b.Height)" }
        }
        $rect = New-Object Drawing.Rectangle(0, 0, $a.Width, $a.Height)
        $fmt = [Drawing.Imaging.PixelFormat]::Format32bppArgb
        $lockA = $a.LockBits($rect, [Drawing.Imaging.ImageLockMode]::ReadOnly, $fmt)
        $bytesA = New-Object byte[] ($lockA.Stride * $a.Height)
        [Runtime.InteropServices.Marshal]::Copy($lockA.Scan0, $bytesA, 0, $bytesA.Length)
        try { $a.UnlockBits($lockA) } catch { }
        $lockB = $b.LockBits($rect, [Drawing.Imaging.ImageLockMode]::ReadOnly, $fmt)
        $bytesB = New-Object byte[] ($lockB.Stride * $b.Height)
        [Runtime.InteropServices.Marshal]::Copy($lockB.Scan0, $bytesB, 0, $bytesB.Length)
        try { $b.UnlockBits($lockB) } catch { }
        $diff = 0
        $total = $a.Width * $a.Height
        for ($i = 0; $i -lt $bytesA.Length; $i += 4) {
            $dB = [Math]::Abs($bytesA[$i] - $bytesB[$i])
            $dG = [Math]::Abs($bytesA[$i + 1] - $bytesB[$i + 1])
            $dR = [Math]::Abs($bytesA[$i + 2] - $bytesB[$i + 2])
            if ($dR -gt $tol -or $dG -gt $tol -or $dB -gt $tol) { $diff++ }
        }
        $pct = 100.0 * $diff / $total
        return @{ Ok = ($pct -le $MaxDiffPercent); Detail = ("{0:F2}% pixels differ (budget {1:F2}%)" -f $pct, $MaxDiffPercent) }
    } finally {
        $a.Dispose(); $b.Dispose()
    }
}

if (-not (Test-Path -LiteralPath $GoldensDir)) { Write-Host "GOLDENS: FAIL - no goldens dir ($GoldensDir). Run Approve-Goldens.ps1 first." -ForegroundColor Red; exit 1 }
$Goldens = Get-ChildItem -LiteralPath $GoldensDir -Filter *.png
if (-not $Goldens) { Write-Host "GOLDENS: FAIL - no golden PNGs in $GoldensDir." -ForegroundColor Red; exit 1 }

$Failed = $false
foreach ($G in $Goldens) {
    $Shot = Join-Path $ShotsDir $G.Name
    if (-not (Test-Path -LiteralPath $Shot)) {
        Write-Host ("GOLDENS: FAIL - missing shot {0}" -f $G.Name) -ForegroundColor Red
        $Failed = $true
        continue
    }
    $R = Diff-Images $Shot $G.FullName $Tolerance
    if ($R.Ok) { Write-Host ("GOLDENS: PASS {0} ({1})" -f $G.Name, $R.Detail) -ForegroundColor Green }
    else { Write-Host ("GOLDENS: FAIL {0} ({1})" -f $G.Name, $R.Detail) -ForegroundColor Red; $Failed = $true }
}
if ($Failed) { exit 1 }
Write-Host "GOLDENS: ALL GREEN" -ForegroundColor Green
exit 0
