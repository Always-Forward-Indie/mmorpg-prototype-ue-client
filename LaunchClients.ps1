# ============================================================
#  LaunchClients.ps1
#  Launches N standalone game clients using UnrealEditor.exe.
#
#  NO BUILD REQUIRED — uses the editor binaries directly, just
#  like PIE but each client is a fully isolated process.
#  Hot-reload / Live Coding still works between launches.
#
#  Usage (from project root, or right-click ? Run with PowerShell):
#    .\LaunchClients.ps1           # 2 clients
#    .\LaunchClients.ps1 -n 3      # N clients
#    .\LaunchClients.ps1 -nolog    # suppress per-client log window
# ============================================================
param(
    [int]   $n     = 2,
    [switch]$nolog = $false
)

$UE      = "D:\Game Dev\UE\UE_5.7\Engine\Binaries\Win64\UnrealEditor.exe"
$Project = Join-Path $PSScriptRoot "Prototyping.uproject"
# Start on the login/container map — same entry point as normal play
$Map     = "/Game/Maps/MainGameContainerLevel"

# 2?2 window grid, each 960?540
$W = 960; $H = 540
$Slots = @(
    @{ X=0;    Y=0   },
    @{ X=960;  Y=0   },
    @{ X=0;    Y=560 },
    @{ X=960;  Y=560 }
)

Write-Host "Launching $n standalone client(s) — no build needed" -ForegroundColor Cyan
Write-Host "Map: $Map`n"

for ($i = 0; $i -lt $n; $i++) {
    $slot = $Slots[$i % $Slots.Count]

    $args = @(
        "`"$Project`"",
        $Map,
        "-game",          # standalone, not PIE
        "-windowed",
        "-ResX=$W", "-ResY=$H",
        "-WinX=$($slot.X)", "-WinY=$($slot.Y)",
        "-NoSplash",
        "-NoVerifyGC"
    )

    if (-not $nolog) { $args += "-log" }

    Write-Host ("  Client {0}: pos=({1},{2})" -f ($i+1), $slot.X, $slot.Y)
    Start-Process -FilePath $UE -ArgumentList $args
    Start-Sleep -Milliseconds 1000   # stagger so sockets initialise cleanly
}

Write-Host "`nAll clients launched." -ForegroundColor Green
