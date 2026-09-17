# Tail dev-server logs from WSL Docker (read-only).
# Usage: .\Get-WslServerLogs.ps1 -Service game -Tail 200
param(
    [ValidateSet("login", "game", "chunk", "db", "all")]
    [string]$Service = "game",
    [int]$Tail = 200,
    [string]$Distro = "Ubuntu"
)

# Compose generates long names; match by substring (transport-safe, no pipes here).
$Map = @{
    login = "mmorpg-prototype-login-server-login-server-1";
    game  = "mmorpg-prototype-game-server-game-server-1";
    chunk = "mmorpg-prototype-chunk-server-new-chunk-server-1";
    db    = "mmorpg_prototype_db"
}

if ($Service -eq "all") {
    foreach ($K in @("login", "game", "chunk")) {
        Write-Host ("===== {0} =====" -f $K) -ForegroundColor Cyan
        wsl -d $Distro -- docker logs $Map[$K] --tail $Tail
    }
} else {
    wsl -d $Distro -- docker logs $Map[$Service] --tail $Tail
}
