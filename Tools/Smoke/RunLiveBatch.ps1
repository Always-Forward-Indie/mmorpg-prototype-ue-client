# Live D1 batch: all green live scenarios in one command, sequential.
# Usage: powershell -ExecutionPolicy Bypass -File Tools/Smoke/RunLiveBatch.ps1
#        .\RunLiveBatch.ps1 -Filter "skills,trade"   # subset (substring match)
#
# NOTE: -File invocation passes -Filter as one string, so values are split
# on comma/space/semicolon here (works for both -File and &-style calls).
#
# What it does per scenario: assert no stray clients -> park char via DB
# (verified by re-read) -> start client -> run world-ready actions (admin
# spawn at the PLAYER's live position / sethp / grantxp / grantlevel, char
# must be online; several actions compose with ';') -> wait LIVEBOT_RESULT
# -> record. Two-client scenarios (cross, trade) stagger by 20s.
# 614 is boosted to level 5 once (first 614 run) so stray hits stop
# one-shotting it mid-batch; documented dev-stand tweak, like gold grants.
#
# Deliberately NOT covered here (proven elsewhere, environment-blocked):
# - live_vendor_repair on 615 ends no-damaged (no damaged gear obtainable;
#   full PASS proven 18.10 on a borrowed damaged sword). Sell skipped when
#   the bag has no junk, buy PASSes.
# - Scheduling: run by hand for now (user decision, no scheduler).
#
# Requires: DEV stand on 127.0.0.1, WSL with integration-tests admin API
# (bots.admin) and psycopg. Bot creds default to the uere_01/uereb_01 pair.
param(
    [string[]]$Filter = @(),
    [string]$Login1 = "uere_01",
    [string]$Pass1 = "BotPass123",
    [string]$Login2 = "uereb_01",
    [string]$Pass2 = "BotPass123",
    [int]$StaggerSec = 20
)

$ErrorActionPreference = "Continue"
$UE = "D:\Game Dev\UE\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"
$Project = Join-Path $PSScriptRoot "..\..\Prototyping.uproject"
$Map = "/Game/Maps/MainGameContainerLevel"
$LogsDir = Join-Path $PSScriptRoot "..\..\Saved\Logs"

# --- runtime helper (generated, never committed): DB + admin ops via WSL ---
$drive = $env:TEMP.Substring(0, 1).ToLower()
$mntTemp = "/mnt/$drive" + ($env:TEMP.Substring(2) -replace "\\", "/")
$HelperPy = Join-Path $env:TEMP "livebatch_helper.py"
# Slice 2 (variant B): quest + level run on a throwaway char instead of the
# fixed 614/615 pair (one-shot quest 9000, exp_for_level cap at 10). The
# helper creates the char via the real login API with a timestamped unique
# letter-only name (Qabot XXXX — server rejects digits/underscores), the
# batch looks the id up by UNIQUE name in DB (no log-line ID parsing races),
# parks it, runs the scenario with -botchar=<id>, then deletes it via the
# real deleteCharacter API. Stale Qabot_* leftovers are cleaned first.
$HelperSrc = @'
import os
import sys
def db():
    import psycopg
    conn = psycopg.connect(host="127.0.0.1", dbname="mmo_prototype", user="postgres", password="root")
    conn.autocommit = True
    return conn
def adm():
    sys.path.insert(0, ".")
    from bots.admin import AdminClient
    return AdminClient()
def login_auth(login, password):
    sys.path.insert(0, ".")
    import os as _os
    _os.environ.setdefault("MMO_CLIENT_VERSION", "0.1.2")
    sys.path.insert(0, os.path.join(".", "proto"))
    try:
        from mmo_proto import PORTS, TARGET_HOST, MmoClient
    except ImportError:
        sys.path.insert(0, os.path.join("..", "proto"))
        from mmo_proto import PORTS, TARGET_HOST, MmoClient
    import os as _o2
    ver = _o2.environ.get("MMO_CLIENT_VERSION", "0.1.2")
    c = MmoClient(host=TARGET_HOST, port=PORTS["login"], timeout=10.0)
    c.connect()
    try:
        c.send_event("authentificationClient", {"login": login, "password": password, "clientVersion": ver})
        rsp = c.wait_for("authentificationClient", duration=10.0)
        if rsp is None or rsp.get("header", {}).get("status") != "success":
            raise RuntimeError("auth failed for %s: %s" % (login, rsp))
        h = rsp.get("header", {})
        return int(h["clientId"]), h["hash"]
    finally:
        c.close()
def login_client(cid, hh):
    sys.path.insert(0, ".")
    import os as _os
    sys.path.insert(0, os.path.join(".", "proto"))
    try:
        from mmo_proto import PORTS, TARGET_HOST, MmoClient
    except ImportError:
        sys.path.insert(0, os.path.join("..", "proto"))
        from mmo_proto import PORTS, TARGET_HOST, MmoClient
    return MmoClient(host=TARGET_HOST, port=PORTS["login"], client_id=cid, hash_=hh, timeout=10.0)
def qabot_name():
    import random as _r
    return "Qabot " + "".join(_r.choice("ABCDEFGHIJKLMNOPQRSTUVWXYZ") for _ in range(4))
op = sys.argv[1]
if op == "setpos":
    cid, x, y, z = int(sys.argv[2]), float(sys.argv[3]), float(sys.argv[4]), float(sys.argv[5])
    cur = db().cursor(); cur.execute("UPDATE character_position SET x=%s, y=%s, z=%s WHERE character_id=%s", (x, y, z, cid))
    print("moved", cid, cur.rowcount)
elif op == "charpos":
    cid = int(sys.argv[2])
    cur = db().cursor(); cur.execute("SELECT x, y, z FROM character_position WHERE character_id=%s", (cid,))
    print("pos", cid, cur.fetchone())
elif op == "questreset":
    # Quest 9000 is one-shot server-side: a previous PASS leaves it
    # turned_in and the next run fails at accept with no-accept-choice.
    # Server resetQuest clears chunk memory AND the DB row via game
    # (a plain DB delete is invisible to the in-memory QuestStore).
    # Needs the char online (CharacterManager gate), so the batch calls
    # this after world-ready, not before start.
    cid = int(sys.argv[2])
    a = adm()
    try:
        rsp = a.call("resetQuest", characterId=cid, questSlug="dev_short_chain", timeout=15.0)
        print("QUESTRESET", rsp.get("header", {}).get("status"), str(rsp.get("body", {}))[:160])
    finally:
        a.close()
elif op in ("spawn", "sethp", "grantxp", "grantitem", "grantlevel", "tp", "sesscheck", "mkthrowaway", "delthrowaway", "cleanup_qabot"):
    a = adm()
    try:
        if op == "spawn":
            cid, x, y, z, n = int(sys.argv[2]), float(sys.argv[3]), float(sys.argv[4]), float(sys.argv[5]), int(sys.argv[6])
            rsp = a.spawn_mob(cid, 1, x, y, z, n, mob_template_id=4)
            print("SPAWN", rsp.get("header", {}).get("status"), str(rsp.get("body", {}))[:300])
        elif op == "sethp":
            rsp = a.set_hp(int(sys.argv[2]), int(sys.argv[3]))
            print("SETHP", rsp.get("header", {}).get("status"), str(rsp.get("body", {}))[:120])
        elif op == "grantxp":
            rsp = a.grant_xp(int(sys.argv[2]), int(sys.argv[3]))
            print("GRANTXP", rsp.get("header", {}).get("status"), str(rsp.get("body", {}))[:120])
        elif op == "grantitem":
            rsp = a.grant_item(int(sys.argv[2]), int(sys.argv[3]), int(sys.argv[4]))
            print("GRANT", rsp.get("header", {}).get("status"), str(rsp.get("body", {}))[:120])
        elif op == "grantlevel":
            rsp = a.grant_level(int(sys.argv[2]), int(sys.argv[3]))
            print("GRANTLVL", rsp.get("header", {}).get("status"), str(rsp.get("body", {}))[:160])
        elif op == "tp":
            rsp = a.teleport_to(int(sys.argv[2]), float(sys.argv[3]), float(sys.argv[4]), float(sys.argv[5]))
            print("TP", rsp.get("header", {}).get("status"), str(rsp.get("body", {}))[:160])
        elif op == "sesscheck":
            # GONE = no live server session for this char (safe to setpos +
            # login fresh). ALIVE = a stale session still holds the char's
            # server-side position and the next login would attach to it.
            try:
                rsp = a.get_state(int(sys.argv[2]))
                body = rsp.get("body", {}) if isinstance(rsp, dict) else {}
                pos = body.get("position", {}) if isinstance(body, dict) else {}
                if rsp.get("header", {}).get("status") == "success" or pos:
                    print("SESSION-ALIVE", str(pos)[:80])
                else:
                    print("SESSION-GONE")
            except Exception as e:
                print("SESSION-GONE")
        elif op == "mkthrowaway":
            # Slice 2 variant B: fresh char via the real login API.
            # Unique letter-only name (server rejects digits/underscores),
            # id resolved by UNIQUE name in DB (no log-line ID parsing).
            # Usage: mkthrowaway <login> <pass>
            login, password = sys.argv[2], sys.argv[3]
            cid, hh = login_auth(login, password)
            c = login_client(cid, hh)
            c.connect()
            try:
                name = None
                newid = 0
                for _ in range(10):
                    cand = qabot_name()
                    c.send_event("createCharacter", {"characterName": cand, "characterClass": "warrior", "characterRace": "human", "characterGender": "male"})
                    rsp = c.wait_for("createCharacter", duration=10.0)
                    if rsp is None:
                        continue
                    h = rsp.get("header", {})
                    if h.get("status") == "success":
                        name = cand
                        b = rsp.get("body", {}) or {}
                        newid = b.get("characterId") or (b.get("character") or {}).get("characterId") or 0
                        break
                    # name taken -> retry with a fresh random suffix
                    continue
                if not name:
                    print("MKCHAR FAIL no-name")
                else:
                    # Ground truth by UNIQUE name (never parse the log line).
                    cur = db().cursor()
                    cur.execute("SELECT id FROM characters WHERE name=%s AND deleted_at IS NULL", (name,))
                    row = cur.fetchone()
                    rid = int(row[0]) if row else int(newid or 0)
                    print("MKCHAR id=%d name=%s" % (rid, name))
            finally:
                c.close()
        elif op == "delthrowaway":
            # Real deleteCharacter API (same path the UE bot uses), not raw DB.
            # Usage: delthrowaway <login> <pass> <charId>
            login, password, charid = sys.argv[2], sys.argv[3], int(sys.argv[4])
            cid, hh = login_auth(login, password)
            c = login_client(cid, hh)
            c.connect()
            try:
                c.send_event("deleteCharacter", {"characterId": charid})
                rsp = c.wait_for("deleteCharacter", duration=10.0)
                st = (rsp.get("header", {}) if rsp else {}).get("status", "none")
                print("DELCHAR id=%d status=%s" % (charid, st))
            finally:
                c.close()
        elif op == "cleanup_qabot":
            # Delete stale Qabot_* leftovers from crashed runs (real API).
            # Usage: cleanup_qabot <login> <pass>
            login, password = sys.argv[2], sys.argv[3]
            cid, hh = login_auth(login, password)
            c = login_client(cid, hh)
            c.connect()
            try:
                c.send_event("getCharactersList", {})
                rsp = c.wait_for("getCharactersList", duration=10.0)
                chars = (rsp.get("body", {}) or {}).get("charactersList", []) if rsp else []
                n = 0
                for e in chars:
                    nm = e.get("characterName", "")
                    if isinstance(nm, str) and nm.startswith("Qabot "):
                        cid2 = int(e.get("characterId", 0))
                        c.send_event("deleteCharacter", {"characterId": cid2})
                        r2 = c.wait_for("deleteCharacter", duration=10.0)
                        n += 1
                print("CLEANUP deleted=%d" % n)
            finally:
                c.close()
        else:
            print("UNKNOWN-OP", op)
    finally:
        a.close()
else:
    print("UNKNOWN-OP", op)
'@
# NOTE: generated file content is written by this script at runtime.
Set-Content -LiteralPath $HelperPy -Value $HelperSrc -Encoding ASCII
$WslHelper = "$mntTemp/livebatch_helper.py"
function Invoke-Helper([string]$DbArgs, [string]$AdminArgs) {
    if ($DbArgs) {
        wsl -e python3 $WslHelper $DbArgs.Split(" ")
    } else {
        wsl -e sh -c "cd ~/projects/mmorpg-prototype/mmorpg-prototype-integration-tests && python3 $WslHelper $AdminArgs"
    }
}

$Results = @()
$Filters = @($Filter | ForEach-Object { $_ -split '[,\s;]+' } | Where-Object { $_ -ne '' })
function Wait-Log([string]$File, [string]$Pattern, [int]$TimeoutSec) {
    $t0 = Get-Date
    while (((Get-Date) - $t0).TotalSeconds -lt $TimeoutSec) {
        if (Test-Path -LiteralPath $File) {
            $h = Select-String -LiteralPath $File -Pattern $Pattern | Select-Object -First 1
            if ($h) { return $h.Line }
        }
        Start-Sleep -Seconds 4
    }
    return $null
}

function Start-Bot([string]$Scenario, [string]$Login, [string]$Pass, [string]$LogName, [int]$BotChar = 0) {
    $log = Join-Path $LogsDir $LogName
    if (Test-Path -LiteralPath $log) { Remove-Item -LiteralPath $log -Force }
    $args = @("`"$Project`"", $Map, "-game", "-windowed", "-ResX=960", "-ResY=540",
        "-NoSplash", "-NoVerifyGC", "-bot-live=$Scenario", "-botlogin=$Login", "-botpass=$Pass",
        "-nullrhi", "-log=$LogName")
    if ($BotChar -ne 0) { $args += "-botchar=$BotChar" }
    return @{ Proc = (Start-Process -FilePath $UE -ArgumentList $args -PassThru); Log = $log }
}

function Stop-Bot($Proc, [int]$GraceSec = 30) {
    if ($Proc.HasExited) { return }
    $exited = $Proc.WaitForExit($GraceSec * 1000)
    if (-not $exited) { try { $Proc.Kill() } catch { } }
}

# Admin call with console echo (single line). Use instead of a bare
# Invoke-Helper inside functions: uncaptured wsl output would leak into
# the function's return value and pollute verdicts.
function Invoke-Admin([string]$AdminArgs) {
    $o = Invoke-Helper -AdminArgs $AdminArgs
    $t = (($o | Out-String).Trim() -replace '\s+', ' ')
    if ($t.Length -gt 180) { $t = $t.Substring(0, 180) }
    if ($t) { Write-Host $t }
}

# Spawn check shared by single and dual runs: verify the ACTUAL spawn
# from the brain's own log line against the intended park point.
# Offline DB parking is best-effort (the server's session-teardown save
# can land after our write and resurrect a stale spot — proven live 21.10).
function Confirm-Spawn($Proc, [string]$Log, [int]$CharId, $Park) {
    if (-not $Park) { return $true }
    $sp = Wait-Log $Log "DEVMODE_BOT spawn at" 120
    if ($sp -and ($sp -match "spawn at \((-?[\d.]+),\s*(-?[\d.]+)")) {
        $sdx = [double]$Matches[1] - $Park[0]; $sdy = [double]$Matches[2] - $Park[1]
        $sdist = [Math]::Sqrt($sdx * $sdx + $sdy * $sdy)
        Write-Host ("spawn check: dist {0:n0}" -f $sdist)
        return ($sdist -le 500)
    }
    Write-Host "WARNING: no spawn line seen, assuming wedged" -ForegroundColor Yellow
    return $false
}

# Player position from the server's position echoes in the client log.
# Used to seed mobs AT the player (fixed coords rot: foxes wander off and
# the player drifts while falling — stale coords miss by hundreds of uu).
function Get-PlayerPos([string]$File, [int]$CharId) {
    $m = Select-String -LiteralPath $File -Pattern ('"id":' + $CharId) | Select-Object -Last 1
    if (-not $m) { return $null }
    $l = $m.Line; $i = $l.IndexOf("position")
    if ($i -lt 0) { return $null }
    $seg = $l.Substring($i, [Math]::Min(160, $l.Length - $i))
    $px = [regex]::Match($seg, '"x":(-?[\d.]+)').Groups[1].Value
    $py = [regex]::Match($seg, '"y":(-?[\d.]+)').Groups[1].Value
    $pz = [regex]::Match($seg, '"z":(-?[\d.]+)').Groups[1].Value
    if (-not $px -or -not $py -or -not $pz) { return $null }
    return @($px, $py, $pz)
}

# char: 614 = uere_01, 615 = uereb_01. park: @(x, y, z) or $null.
# ready: admin op issued right after world-ready (char online by then).
$Scenarios = @(
    @{ Name = "live_loot_pickup";   Login = $Login2; Pass = $Pass2; Char = 615; Park = @(4700, -900, 300);   Ready = $null; Timeout = 420 },
    @{ Name = "live_harvest_run";    Login = $Login2; Pass = $Pass2; Char = 615; Park = @(4700, -900, 300);   Ready = $null; Timeout = 420 },
    @{ Name = "live_death_respawn";  Login = $Login2; Pass = $Pass2; Char = 615; Park = @(4700, -900, 300);   Ready = "sethp 615 1;spawn 3 0 0"; Timeout = 420 },
    @{ Name = "live_equip";          Login = $Login2; Pass = $Pass2; Char = 615; Park = @(700, -3200, 300);   Ready = $null; Timeout = 420 },
    @{ Name = "live_level";          Login = $Login1; Pass = $Pass1; Char = 614; Park = @(700, -3200, 300);   Ready = "GRANTXP"; Timeout = 420 },
    @{ Name = "live_titles_emotes";  Login = $Login2; Pass = $Pass2; Char = 615; Park = @(700, -3200, 300);   Ready = $null; Timeout = 420 },
    @{ Name = "live_skills";         Login = $Login2; Pass = $Pass2; Char = 615; Park = @(-634, 2160, 300);   Ready = "spawn 6"; Timeout = 480 },
    @{ Name = "live_char_create_delete"; Login = $Login1; Pass = $Pass1; Char = 614; Park = @(700, -3200, 300); Ready = "grantlevel 5"; Timeout = 420 },
    @{ Name = "live_back_to_login";  Login = $Login1; Pass = $Pass1; Char = 614; Park = @(700, -3200, 300);   Ready = $null; Timeout = 420 },
    @{ Name = "live_travel_roundtrip"; Login = $Login1; Pass = $Pass1; Char = 614; Park = @(700, -3200, 300); Ready = $null; Timeout = 540 },
    @{ Name = "live_reconnect";      Login = $Login1; Pass = $Pass1; Char = 614; Park = @(4700, -900, 300);   Ready = "spawn 2"; Timeout = 420 },
    @{ Name = "live_cross_visibility"; Login = $Login1; Pass = $Pass1; Char = 614; Park = @(700, -3200, 300); Ready = "DUAL"; Timeout = 480 },
    @{ Name = "live_patrol_kill";    Login = $Login1; Pass = $Pass1; Char = 614; Park = @(4700, -900, 300);   Ready = "spawn 3"; Timeout = 420 },
    @{ Name = "live_quest_turnin";   Login = $Login1; Pass = $Pass1; Char = 614; Park = @(700, -3200, 300);   Ready = $null; Timeout = 600 },
    @{ Name = "live_trade";          Login = $Login1; Pass = $Pass1; Char = 614; Park = @(700, -3200, 300);   Ready = "DUALGRANT"; Timeout = 480 }
)

function Invoke-ReadyActions([hashtable]$S, [string]$Log) {
    if (-not $S.Ready) { return }
    foreach ($Act in ($S.Ready -split ';')) {
        $Act = $Act.Trim()
        $Toks = @($Act -split '\s+')
        if ($Toks[0] -eq "spawn") {
            # Seed at the PLAYER (fresh position echo, post-fall), not at
            # fixed coords: the player drifts while landing and foxes
            # wander off.
            # Quest hunts only AFTER a locomotion proof + NPC walk +
            # dialogue (~1 min after world-ready): seeding at world-ready
            # goes stale (foxes wander / get farmed) and the brain chases
            # Distant-marker seeding: quest and skills hunt only AFTER a
            # long preamble (NPC walk + dialogue / waypoint + shop), so a
            # world-ready seed goes stale or loses the race (hunting starts
            # before the seed replicates and the brain commits to a distant
            # wild fox into a mesh wall — proven live 22.10). Wait for the
            # hunting marker so seeds land fresh where the hunt starts.
            # Everything else hunts within seconds of world-ready: seed at
            # once (6s settle for the landing drift).
            $SeedMarker = $null
            if ($S.Name -eq "live_quest_turnin") { $SeedMarker = "quest accepted, hunting" }
            elseif ($S.Name -eq "live_skills") { $SeedMarker = "skills learned, hunting" }
            if ($SeedMarker) {
                # Wait for the marker (budgets are ~120s), but bail early
                # if the run already finished — seeding a dead client fails
                # and wastes the retry window.
                $acc = $null; $t0 = Get-Date
                while (((Get-Date) - $t0).TotalSeconds -lt 150) {
                    if (Test-Path -LiteralPath $Log) {
                        $acc = Select-String -LiteralPath $Log -Pattern $SeedMarker | Select-Object -First 1
                        if ($acc) { break }
                        if (Select-String -LiteralPath $Log -Pattern "LIVEBOT_RESULT $($S.Name)") { break }
                    }
                    Start-Sleep -Seconds 4
                }
                if (-not $acc) {
                    if (Select-String -LiteralPath $Log -Pattern "LIVEBOT_RESULT $($S.Name)") { return }
                    Write-Host "WARNING: no hunting marker, seeding anyway" -ForegroundColor Yellow
                }
            } else {
                Start-Sleep -Seconds 6
            }
            $pp = Get-PlayerPos $Log $S.Char
            if ($pp) {
                $dx = 0; $dy = 0
                if ($Toks.Count -ge 4) { $dx = [double]$Toks[2]; $dy = [double]$Toks[3] }
                $sx = [double]$pp[0] + $dx; $sy = [double]$pp[1] + $dy
                $so = Invoke-Helper -AdminArgs "spawn $($S.Char) $sx $sy $($pp[2]) $($Toks[1])"
                $st = (($so -join "`n") -replace '\s+', ' ')
                if ($st.Length -gt 180) { $st = $st.Substring(0, 180) }
                Write-Host "seed: $st"
                # Verify the seed actually replicated to THIS client: parse
                # the returned uids and wait for the spawn packet. Admin
                # spawn on a shared stand is lossy (uid reuse, wandering
                # mobs, other bots farming) — fail loud, not silent.
                $m = [regex]::Match(($so -join "`n"), "'uids': \[([\d, ]+)\]")
                if ($m.Success) {
                    $first = ($m.Groups[1].Value -split ',')[0].Trim()
                    $seen = Wait-Log $Log $first 30
                    if (-not $seen) { Write-Host "WARNING: seeded mob uid=$first never replicated to client" -ForegroundColor Yellow }
                } else {
                    Write-Host "WARNING: spawn admin call returned no uids" -ForegroundColor Yellow
                }
            } else {
                Write-Host "no player pos echo yet, skipping seed"
            }
        } elseif ($Act -eq "GRANTXP") {
            # handled by the caller at the level baseline marker
        } elseif ($Act -like "sethp *") {
            Invoke-Admin $Act
        } elseif ($Act -like "grantlevel *") {
            Invoke-Admin "grantlevel $($S.Char) $($Act.Split(' ')[1])"
        }
    }
}

function Format-Result($Line) {
    if ($Line -and $Line -match "LIVEBOT_RESULT \S+ (PASS|FAIL).*reason=(\S+)") { return "$($Matches[1])/$($Matches[2])" }
    return "NO-RESULT"
}

# Slice 2 (variant B): throwaway char lifecycle for one-shot content.
# Creates via the real login API (letter-only Qabot XXXX), resolves the id
# by UNIQUE name in DB (no log-line ID parsing), parks/plays/deletes.
# Stale Qabot leftovers from crashed runs are cleaned first (same API).
function New-ThrowawayChar([string]$Login, [string]$Pass) {
    $null = Invoke-Helper -AdminArgs "cleanup_qabot $Login $Pass"
    $mk = Invoke-Helper -AdminArgs "mkthrowaway $Login $Pass"
    $t = (($mk -join "`n") -replace '\s+', ' ')
    Write-Host "throwaway: $t"
    $m = [regex]::Match(($mk -join "`n"), "MKCHAR id=(\d+) name=([A-Za-z ']+)")
    if (-not $m.Success) { return $null }
    return @{ Id = [int]$m.Groups[1].Value; Name = $m.Groups[2].Value.Trim() }
}

function Remove-ThrowawayChar([string]$Login, [string]$Pass, [int]$CharId) {
    if ($CharId -le 0) { return }
    $dr = Invoke-Helper -AdminArgs "delthrowaway $Login $Pass $CharId"
    Write-Host "throwaway delete: $dr"
}

# One live client run: start -> world-ready gate -> ready actions ->
# tombstone wait -> stop. Returns the formatted result.
function Invoke-SingleRun([hashtable]$S, [string]$Login, [string]$Pass, [string]$LogName) {
    if ($S.Name -eq "live_quest_turnin") {
        # Quest accept is gated on server-side quest state, which lives in
        # the char's session (stale sessions outlive clients — server #119).
        # A lingering session from the previous run still holds turned_in
        # and the fresh accept is offered locked. Wait (bounded) for it.
        $t0 = Get-Date
        while (((Get-Date) - $t0).TotalSeconds -lt 240) {
            $sr = Invoke-Helper -AdminArgs "sesscheck $($S.Char)"
            if (($sr -join "`n") -match "SESSION-GONE") { break }
            Start-Sleep -Seconds 10
        }
    }
    for ($a = 1; $a -le 2; $a++) {
    if ($S.Park) {
        $null = Invoke-Helper -DbArgs "setpos $($S.Char) $($S.Park[0]) $($S.Park[1]) $($S.Park[2])"
    }
    $b = Start-Bot $S.Name $Login $Pass $(if ($a -eq 1) { $LogName } else { $LogName -replace '\.log$', '_retry2.log' }) $(if ($S.BotChar) { $S.BotChar } else { 0 })
    $wr = Wait-Log $b.Log "LIVEBOT world ready" 240
    if (-not $wr) { Stop-Bot $b.Proc 5; return "FAIL/no-world-ready" }
    if ($a -eq 1 -and $S.Name -eq "live_quest_turnin" -and -not $S.BotChar) {
        # Server-side reset (memory + DB). Char is online by now, which
        # the admin gate requires (CharacterManager must know the char).
        # Skipped for throwaway chars (fresh quest state, nothing to reset).
        Invoke-Admin "questreset $($S.Char)"
    }
    # Spawn check: offline DB parking is best-effort (the server's session-
    # teardown save can land after our write and resurrect a stale spot —
    # proven live 21.10). Verify the ACTUAL spawn from the brain's own log
    # line; on mismatch kill + re-park + restart once (the teardown save is
    # a one-time event, so the second write sticks).
    $sp = Wait-Log $b.Log "DEVMODE_BOT spawn at" 120
    if ($S.Park -and $sp -and ($sp -match "spawn at \((-?[\d.]+),\s*(-?[\d.]+)")) {
        $sdx = [double]$Matches[1] - $S.Park[0]; $sdy = [double]$Matches[2] - $S.Park[1]
        $sdist = [Math]::Sqrt($sdx * $sdx + $sdy * $sdy)
        Write-Host ("spawn check: dist {0:n0}" -f $sdist)
        if ($sdist -gt 500) {
            Write-Host "stale spawn (attempt $a), re-parking..." -ForegroundColor Yellow
            Stop-Bot $b.Proc 5
            Start-Sleep -Seconds 10
            continue
        }
    }
    Invoke-ReadyActions $S $b.Log
    if ($S.Name -eq "live_level") {
        $bl = Wait-Log $b.Log "level baseline" 300
        if ($bl) { Invoke-Admin "grantxp $($S.Char) 20000" }
    }
    $r = Wait-Log $b.Log "LIVEBOT_RESULT $($S.Name)" $S.Timeout
    Stop-Bot $b.Proc
    $suffix = if ($a -eq 2) { " (respawn)" } else { "" }
    return (Format-Result $r) + $suffix
    }
    return "FAIL/spawn-mismatch"
}

foreach ($S in $Scenarios) {
    if ($Filters.Count -gt 0 -and -not ($Filters | Where-Object { $S.Name -like "*$_*" })) { continue }
    Write-Host "=== $($S.Name) ===" -ForegroundColor Cyan
    # Slice 2 state (single-path throwaway); defined here so the shared
    # revive/cleanup tail below sees it on DUAL paths too ($null-safe).
    $ThrowawayId = 0
    $OrigChar = $null

    $strays = @(Get-Process | Where-Object { $_.Path -like "*UnrealEditor*" })
    if ($strays.Count -gt 0) { Write-Host "stray clients running, aborting batch" -ForegroundColor Red; exit 1 }

    if ($S.Park) {
        $null = Invoke-Helper -DbArgs "setpos $($S.Char) $($S.Park[0]) $($S.Park[1]) $($S.Park[2])"
        $pos = Invoke-Helper -DbArgs "charpos $($S.Char)"
        Write-Host "parked: $pos"
    }

    if ($S.Ready -eq "DUAL" -or $S.Ready -eq "DUALGRANT") {
        # Two-client scenarios share the rendezvous. Spawn is verified per
        # client from its own log; on mismatch kill + re-park + restart once
        # (same teardown-race handling as single runs).
        $log1 = "Batch_$($S.Name).log"
        $b1 = $null; $pok1 = $false
        for ($a = 1; $a -le 2; $a++) {
            if ($S.Park) { $null = Invoke-Helper -DbArgs "setpos $($S.Char) $($S.Park[0]) $($S.Park[1]) $($S.Park[2])" }
            $b1 = Start-Bot $S.Name $S.Login $S.Pass $(if ($a -eq 1) { $log1 } else { $log1 -replace '\.log$', '_retry2.log' })
            $wr = Wait-Log $b1.Log "LIVEBOT world ready" 240
            if (-not $wr) { Stop-Bot $b1.Proc 5; break }
            $pok1 = Confirm-Spawn $b1.Proc $b1.Log $S.Char $S.Park
            if ($pok1) { break }
            Write-Host "P1 stale spawn (attempt $a), re-parking..." -ForegroundColor Yellow
            Stop-Bot $b1.Proc 5
            Start-Sleep -Seconds 10
        }
        if (-not $pok1 -and -not $wr) { $o1 = "FAIL/no-world-ready"; $o2 = ""
        } elseif (-not $pok1) { Stop-Bot $b1.Proc 5; $o1 = "FAIL/spawn-mismatch"; $o2 = ""
        } else {
            if ($S.Ready -eq "DUALGRANT") { Invoke-Admin "grantitem 614 16 10" }
            Start-Sleep -Seconds $StaggerSec
            $log2 = "Batch_$($S.Name)_p2.log"
            $b2 = $null; $pok2 = $false
            for ($a = 1; $a -le 2; $a++) {
                $null = Invoke-Helper -DbArgs "setpos 615 700 -3200 300"
                $b2 = Start-Bot $S.Name $Login2 $Pass2 $(if ($a -eq 1) { $log2 } else { $log2 -replace '\.log$', '_retry2.log' })
                $wr2 = Wait-Log $b2.Log "LIVEBOT world ready" 240
                if ($wr2) { $pok2 = Confirm-Spawn $b2.Proc $b2.Log 615 @(700, -3200, 300) }
                if ($pok2 -or -not $wr2) { break }
                Write-Host "P2 stale spawn (attempt $a), restarting..." -ForegroundColor Yellow
                Stop-Bot $b2.Proc 5
                Start-Sleep -Seconds 10
            }
            $r1 = Wait-Log $b1.Log "LIVEBOT_RESULT $($S.Name)" $S.Timeout
            $r2 = $null
            # P2 starts a full stagger + load after P1 and its legs run on
            # their own budgets (trade confirm alone is 120s) — give it a
            # full window from P1's finish instead of cutting a legit wait.
            if ($pok2) { $r2 = Wait-Log $b2.Log "LIVEBOT_RESULT $($S.Name)" 300 }
            Stop-Bot $b1.Proc; if ($b2 -and -not $b2.HasExited) { Stop-Bot $b2.Proc }
            $o1 = Format-Result $r1; $o2 = if ($pok2) { Format-Result $r2 } else { "FAIL/spawn-mismatch" }
        }
    } else {
        $log1 = "Batch_$($S.Name).log"
        # Slice 2 variant B: quest + level run on a throwaway char (fresh
        # quest state, no exp cap) instead of fixed 614. Create -> park ->
        # run with -botchar=<id from DB> -> delete via real API.
        $ThrowawayId = 0
        $OrigChar = $S.Char
        if ($S.Name -eq "live_quest_turnin" -or $S.Name -eq "live_level") {
            $tw = New-ThrowawayChar $S.Login $S.Pass
            if (-not $tw) {
                $o1 = "FAIL/throwaway-create"; $o2 = ""
            } else {
                $ThrowawayId = $tw.Id
                $S.Char = $ThrowawayId
                $S.BotChar = $ThrowawayId
                Write-Host "throwaway char $($tw.Name) id=$ThrowawayId for $($S.Name)"
                $o1 = Invoke-SingleRun $S $S.Login $S.Pass $log1
                $o2 = ""
            }
        } else {
            $o1 = Invoke-SingleRun $S $S.Login $S.Pass $log1
            $o2 = ""
        }
    }
    # Revive-and-retry: a logged-in corpse (stale death from a previous run)
    # fails instantly with player-died. Run the death scenario once as the
    # documented revive path, then re-run the scenario a single time.
    if ($o1 -like "FAIL/player-died*" -and $S.Name -ne "live_death_respawn") {
        Write-Host "player-died at start, reviving via death scenario..." -ForegroundColor Yellow
        $rec = @{ Name = "live_death_respawn"; Char = $S.Char; Park = @(4700, -900, 300); Timeout = 420 }
        if ($ThrowawayId -gt 0) { $rec.BotChar = $ThrowawayId }
        $ro = Invoke-SingleRun $rec $S.Login $S.Pass "Batch_recover_$($S.Name).log"
        Write-Host "recovery: $ro"
        if ($ro -like "PASS*") {
            $o1 = (Invoke-SingleRun $S $S.Login $S.Pass "Batch_$($S.Name)_retry.log") + " (retry)"
        }
    }
    if ($ThrowawayId -gt 0) {
        Remove-ThrowawayChar $S.Login $S.Pass $ThrowawayId
        $S.Char = $OrigChar
        $S.Remove("BotChar")
    }
    $row = [pscustomobject]@{ Scenario = $S.Name; P1 = $o1; P2 = $o2 }
    $Results += $row
    Write-Host "$($S.Name): $($row.P1) $($row.P2)"
}

Write-Host "`n=== BATCH SUMMARY ===" -ForegroundColor Cyan
$Results | Format-Table -AutoSize | Out-String | Write-Host
$bad = @($Results | Where-Object { $_.P1 -notlike "PASS*" -or ($_.P2 -ne "" -and $_.P2 -notlike "PASS*") })
if ($bad.Count -gt 0) { Write-Host "BATCH: FAIL ($($bad.Count) red)" -ForegroundColor Red; exit 1 }
Write-Host "BATCH: ALL GREEN" -ForegroundColor Green; exit 0
