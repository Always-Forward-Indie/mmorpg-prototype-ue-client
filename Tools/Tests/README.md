# Testing runbook (dev-only; never against live VPS)

## Host stability (read this if containers/VM keep dying)
- Proven clean: no Windows reboots (event log: only manual shutdown/boot),
  no cron/apt/Task-Scheduler culprits, no Docker Desktop, no kernel OOM,
  disk/RAM fine, no WSL memory cap.
- Observed: WSL distro takes clean systemd poweroffs every ~1-40 min
  (journal boot list), dockerd cycles with it; restart policies
  (`unless-stopped` on all 4 dev services) self-heal containers — verify with
  `.\Tools\WSL\Preflight.ps1` before any run.
- NOT the cause: test traffic, rebuilds (capped `-j8`), port checks.
- Still open (needs host access): antivirus/EDR vs `vmwp.exe`, Hyper-V-Worker
  admin log, second operator/scripts issuing `wsl --shutdown`, WSL 2.7.14
  runtime bug. If flapping persists, keep one persistent `wsl` session open
  and watch `journalctl --list-boots` for new boot IDs.
- Note: `wsl --list --verbose` uptime and `uptime` inside WSL are unreliable
  here (stuck at "0 min") — trust journal boot IDs and container `STATUS` age.

## Layers
- **L1 offline:** enable `FDevModeConfig.bEnabled` on `BP_MyGameInstance`, open entry map.
  5-minute flow (`Config/DevMode/qa_presets.json`): `devmode.scenario combat` →
  `devmode.setplayerhp 1` → `devmode.setplayerhp 1000` → `devmode.reloadinventory` →
  `devmode.scenario reset` → `devmode.listmobs`. Collect `Saved/Logs/*.log` on failure.
- **L2 engine:** Session Frontend → Automation → `MMO.*` specs (Framing, NtpMath, ClientVersion).
- **L3 contracts:** `python -m pytest Tests/Contract/test_framing.py` (no server);
  server-backed need WSL dev up + creds (see below).
- **L4 bots:** `python Tools/Bots/run_swarm.py --n 8 --scenario patrol` (10 min default);
  Wave-2: `--scenario kill|harvest|death`, duo `--n 2 --scenario trade`.
  Seed first: `python Tools/Bots/seed_bots.py --n 8` (writes gitignored `bot_accounts.json`).
  `--tap` writes `swarm_<scenario>_bot_NN.jsonl` for replay.
- **L5 replay:** `python Tools/Replay/replay.py --file <jsonl>`.
- **Smoke UE:** `Tools/WSL/Preflight.ps1`, then `Tools/Smoke/SmokeClients.ps1 -n 2 -WaitSec 120`.

## First run (Windows)
```
Copy-Item server_config_dev.json server_config.json   # if missing; never live IP
.\Tools\WSL\Preflight.ps1
python -m pytest Tests/Contract/test_framing.py -v
```

## Servers (WSL Ubuntu, truth: ~/projects/mmorpg-prototype)
```
cd ~/projects/mmorpg-prototype/mmorpg-prototype-login-server && docker compose -f docker-compose.dev.yml up -d --build  # db + login first (creates mmo_network)
cd ../mmorpg-prototype-game-server && docker compose -f docker-compose.dev.yml up -d --build
cd ../mmorpg-prototype-chunk-server-new && docker compose -f docker-compose.dev.yml up -d --build
```
Build hygiene (host protection): Dockerfiles and watch scripts are capped at
`-j8` (`-j$(nproc)` pegs VmmemWSL: CPU/RAM/disk). Rebuild images only when
toolchain/Dockerfile changes; otherwise rely on in-container incremental
rebuilds. Never run image builds in parallel with test runs or the UE build.
**Never run manual `make` inside a container while watchexec watches —
parallel makes corrupt the link step (binary vanishes, restart loop).**
Serial builds only: save, wait for quiet, verify `make` up-to-date, restart.
**Fresh/recreated containers have NO build dir: `watch_and_run.sh` exits
(`binary not found`) instead of building — recover with
`docker compose -f docker-compose.dev.yml up -d --build` in the service
repo (full build ~15-25 min), never bare `docker restart` in that state.**
Logs: `Tools/WSL/Get-WslServerLogs.ps1 -Service game -Tail 200`.
Warm-up rule: after any chunk (re)start, wait for readiness instead of a fixed
sleep — measured 2026-09-15: spawn zones pushed <1s after start
(`Spawn Zone ID` in chunk log), first respawn task at +10s. Ready when the
chunk log shows `Spawn Zone ID` lines plus the first `[RESPAWN]`/`[INITIAL_SPAWN]`
summary (typically ~11s; timeout 90s). The old fixed 5-minute rule was
over-conservative (dated from cold-DB era); keep 5 min only after a full DB
wipe + content reload.

## Bot/contract creds (dev only)
One command (seeds bots, exports creds, runs the suite):
```
.\Tools\Contract\run_l3.ps1
.\Tools\Contract\run_l3.ps1 -PytestArgs @("Tests/Contract/test_quest.py", "-x", "-q")
```
Manual equivalent (seed `bot_*` accounts, or capture from a real dev client
log after login):
```
$env:MMO_CLIENT_ID="..."; $env:MMO_HASH="..."; $env:MMO_CHARACTER_ID="..."
$env:MMO2_CLIENT_ID="..."; $env:MMO2_HASH="..."; $env:MMO2_CHARACTER_ID="..."  # cross-visibility
python -m pytest Tests/Contract/ -v
```
Without creds/servers the tests SKIP (exit 0) — framing tests always run.

## Bot state rotation + resets (dev only)
Single-shot fixtures are consumed, not reset, by design — the tests SKIP
loudly instead of failing when spent:
- quest accept: non-repeatable quest; rotate with `QUEST_BOT_IDX` (default 6).
- repair flow: SQL fixture single-shot; re-apply `scenarios/repair.py`
  fixture or set `REPAIR_BOT_IDX`.
- corpse TTL test: skips past 55s age; fails instead when the kill was slow
  but under TTL — rerun (timing-flaky, see below).
Full hermetic reset (quest progress for bot_01..08 + server bounces, ~2 min):
```
.\Tools\Bots\reset_bots.ps1
```
Why the bounces: the game server caches player quests in memory (a DB wipe
alone keeps pushing stale states), the chunk caches progress per session.
Wipe order matters: DB rows first, then game, then chunk.

## Known-flaky L3 (all timing/geography, none from refactor incr 6-11)- `test_handoff.py::test_cell_enter_streams_snapshot`: the old blind +2500x
  walk from spawn crosses empty terrain (server skips empty cells by
  design) — fixed 2026-09-17 to walk through the nearest known mob cluster.
- `test_handoff.py::test_corpse_return_within_ttl`: slow kills eat the 60s
  corpse TTL (skip past 55s; hard fail just under it). Rerun on failure.
- Quest swarm vs `test_quest.py` share one non-repeatable quest: run
  `reset_bots.ps1` between them.
- No `test_reg_*` cases exist yet (tracker-bug template from AGENTS.md was
  never implemented) — recorded debt, not silently dropped.

## Load probes (dev only, never live VPS)
`Tools/Tests/auth_storm.py` (register/auth bursts) and `join_storm.py`
(concurrent `joinGameClient`, tracks `CHUNKID_0` rate). Measured 2026-09-16
(dev WSL, warm servers):
- register: 50/200/500/1000 concurrent → 100% success, ~330-500/s wall.
  Login pool (5 conns) never saturated: no timeouts, no reconnects.
- auth (8 seeded accounts round-robin): 200 concurrent → 100% success.
- game join: 200 concurrent → 200 ok, 0 `CHUNKID_0`.
Verdict: pool sizes (5/5/5) and 5s acquire timeout are plenty for dev and
small-prod auth/join bursts; re-measure before any launch with real CCU
targets. A single transient `chunkId: 0` (~2 min, self-healed, cause
unfound) was seen once — game now logs chunk register/remove/stale events
(`[ChunkManager]`), watch for them if it recurs.

Measured 2026-09-17 (dev WSL, post-flap, login restarted; phase-4 gates):
- register: 50/200/500/1000 concurrent → 100% success. Latency is ~1.0s
  floor (password hash by design): p50 1.05/1.14/1.30/1.50s, p95 ≤1.89s.
- auth (seeded round-robin): 200 → 100%, 1000 → 1000/1000 (p50 1.80s).
- game join: 1000 → 1000 ok, 0 `CHUNKID_0` in chunk log.
- Steady-state RTT (kill taps, 4 bots): playerAttack p50 <1ms / p99 17ms
  (= 2026-09-16 baseline at 20 online); moveCharacter p50 <1ms / p99 48ms
  (< 63ms baseline). Server thinking time ~0ms; join flood p50 ~15.6s
  (scene-load, expected).
- Soak 62 min (patrol 15 + kill 20 + harvest 15 + death 12, ASan build):
  22/22 bot-runs green, 0 ASan/UBSan findings, no restarts.
Note 2026-09-17: a host flap wedged the login server (pool never opened,
register/auth 100% timeout) — fixed by container restart. Bots with seeded
sessions kept passing throughout (they skip login), so a green swarm does
NOT imply a healthy login path; run auth_storm as the login gate.

## Bot protocol rules (learned the hard way, mirror the real client)
- Heartbeat from the first socket: chunk kills clients idle >30s
  (`PING_TIMEOUT_SEC`). `MmoClient.start_heartbeat(10)`; never go quiet.
- `clientVersion` ONLY in login/register bodies (like `UAuthenticationManager`).
  Gameplay bodies must stay clean — extra fields are ignored, not fatal
  (verified 2026-09-16: tolerant reader, pinned by `test_tolerant.py`).
- `getSpawnZones` is a GAME-server call; on CHUNK it is only logged + ignored
  (verified 2026-09-16, no session death). The real client still sends it to
  chunk — harmless noise, but client backlog should stop sending it.
- Chunk HOST always from local config (`127.0.0.1`); only PORT from
  `chunkServerData` (game advertises Docker-internal `chunk-server`).
- Pace joins like scene loads (F2/F4 are huge floods); drain fully before
  talking. `chunk_session_as()` does all of the above; reuse it.
- Keep-alive sessions: share one session per pytest module; bots hold one
  session each. Rapid connect/disconnect churn degrades the dev chunk server
  (stale socket registry) — restart it if sessions go dry for no reason.

## Bug template (tracker: http://23.88.102.182:3005, OpenAPI /api/docs-json, X-API-Key from api_key.env)
Title + type(bug) + `ClientVersion` + WSL server commit + `requestId sync_*` +
`Saved/Logs` excerpt + replay file if from bots. Search duplicates first.
New `eventType` on server → new scenario file. Closed TODO bug → `reg_<bug>` case.

## Wave-6 additions (2026-09-17)
- **combat_storm.py** (C1): synchronized `playerAttack` bursts over N real
  Bot sessions (login→ready→spread→chase→barrier→taps). Reports outcome mix
  + RTT (~0.1s poll resolution; precise thinking-time stays with
  `latency_report.py`). Run after any combat-path change:
  `python Tools/Tests/combat_storm.py --n 4 --taps 10`.
- **run_l3 IDX rotation** (C2): `-QuestBotIdx/-RepairBotIdx/-HandoffBotIdx`
  (defaults 6/4/7, exported as env). Rotate after single-shot SKIP-consumption.
- **Corpse TTL decision** (C2): KEEP 60s (`corpse.ttl_ms`). Evidence:
  `test_corpse_return_within_ttl` green in 132s live; skip-past-55s + rerun
  policy stands. Revisit only with kill-duration distribution data.
- **Watch-ServerLogs.ps1** (C3): read-only alert scan
  (`.\Tools\WSL\Watch-ServerLogs.ps1 -Service all -Tail 1000`), exit 1 on
  FATAL (sanitizers/crashes/CHUNKID_0). Postgres operational noise
  (restarts/probes) is allow-listed. Run before/after every soak or storm.
- **reg_* convention** (A6): one `Tests/Contract/test_reg_<area>.py` per
  closed product decision, built on Bot/pair fixtures (walk into range —
  initiation checks range before guards; an out-of-range error proves
  nothing). Shipped: `test_reg_pvp.py` (PvP refusal, 81s live).
  Backlog (each needs live fixture design, not Blanket-added): vendor
  discount thresholds (needs rep-200 setup), learn-skill consume (trainer
  fixture), turn-in rewards (needs completable quest ~18 min), champion
  zoneId (needs threshold kills + spawn broadcast assert).
  Shipped 2026-09-18: test_reg_pvp.py (PvP refusal), test_reg_learn.py
  (insufficient_level/already_learned guards; success path blocked — max
  bot level 3 < req 5, stays unit-pinned), test_reg_discount.py (475 vs
  500 differential at sylara; vendor range uses PACKET playerPosition —
  omitting it reads (0,0,0), unlike dialogue which uses stored position),
  test_reg_turnin.py (full fox chain GREEN 22:36 after fixing dead pickup
  code + ghost hunting in quest.py — see SERVER_BUGS #9).
  Deferred with data: champion (prod threshold 100, sparse 5-8km ring,
  no invasion events, timed uninitialised — effectively unreachable;
  filed as content finding in SERVER_BUGS #8, tracker key expired).
- **UE specs** (A6): no new `MMO.*` specs shipped — engine specs require
  Session Frontend verification (CLI hangs 25+ min); unverified specs are
  worse than none. Follow-up with editor access.

## Wave-6 gates (2026-09-17, this session)
- Unit chunk 418/418, TSan fingerprint unchanged (9 Scheduler, 0 new).
- L3 targeted 18/18 (framing/tolerant/conn/combat/vendor/trade) + reg_pvp.
- auth_storm 50/50 (p50 1.06s); combat_storm live-verified; kill swarm 8/8.
- Watch-ServerLogs exit 0 post-soak (db/login noise allow-listed).

## 2026-09-18: turnin/champion/soak
- reg_turnin GREEN 22:36 (quest.py pickup-dead-code + ghost fixes).
- reg_champion farm measured: ~1 kill/45min per 4-bot farm vs threshold
  100 (sparse 5-8km ring) — effectively unreachable; recorded in
  SERVER_BUGS #8, tracker key expired (401, needs rotation).
- soak_overnight.ps1 (Tools/Soak): 8 segments x repeatable scenarios with
  preflight + marker-scoped log gates. 2026-09-18: 8/8 green (64 bot-runs,
  0 FAIL, 0 FATAL). Log: Tools/Bots/soak_overnight.log (gitignored).

## Inter-server seam contract (chunk↔game, verified 2026-09-19/20)

Traffic classes (see AGENTS.md):
- **Facts** chunk→game (~22 families, all fire-and-forget, none block):
  `savePositions/saveHpMana/savePlayTime/saveCharacterProgress/
  saveExperienceDebt/saveInventoryChange/nullifyItemOwner/
  deleteInventoryItem/transferInventoryItem/saveDurabilityChange/
  saveItemKillCount/saveEquipmentChange/saveActiveEffect/
  saveCurrencyTransaction (wired 2026-09-20, was `Unknown event type`)/
  saveSkillBarSlot/saveSkillCooldown/updatePlayerQuestProgress/
  updatePlayerFlag/savePityCounter/saveBestiaryKill/timedChampionKilled/
  saveReputation/saveMastery/savePlayerTitle/saveLearnedSkill/
  saveExperienceDebt/analyticsEvent`.
- **Boot snapshots** game→chunk (25 pushes on `chunkServerConnection`
  handshake): config, templates, vendors, zones, tables. Chunk issues no
  static pulls. **Config reload = game+chunk restart** (push only).
- **Runtime request→response** (async, never blocking): 13 join-time pulls
  (quests/flags/effects/cooldowns/inventory/pity/bestiary/rep/mastery/
  titles/emotes/attributes) + `saveLearnedSkill→setLearnedSkill` +
  `getCharacterAttributes→setCharacterAttributesRefresh`. **Rule: answers
  go on the live socket resolved at send time**
  (`ChunkManager::resolveLiveSocket`), never a captured one — writes into
  half-open stale sockets fail silently (SERVER_BUGS #11 ghost).
- **Ownership**: chunk decides in-session, game persists facts as-is
  (absolute SETs, idempotent upserts). Reputation is delta-additive
  (`add_reputation`); SP learn fact carries authoritative post-deduct value
  (`set_free_skill_points`, legacy cost-decrement fallback kept).
- **Observability**: every failure point logs error-level; Watch SEAM
  patterns are counters — any hit = investigate like FATAL (proven live:
  caught the two historical `saveCurrencyTransaction` drops).
## Test-level policy (atoms → chains → soak)
- **Level 1 — atoms** (one action each: walk, kill, pickup, dialogue…):
  state is set by admin-RPC (teleport/spawn/grant), never farmed. Fast.
- **Level 2 — chains** (quests, farm loops): composed from proven atoms +
  dev-short content. A red chain over green atoms blames the chain
  (order/state/economy), never the mechanics.
- **Level 3 — soak** (live bots, nightly): endurance, not logic.
- Spawning tests spawning, killing tests killing — never mix the two in one
  verdict. Walking on foot happens only in the locomotion test and in soak;
  everything else teleports.
- No new mechanic is accepted without its atom test.

## 2026-09-20: admin-RPC Phase A shipped (DEV only, never live VPS)
- **What**: chunk `adminCommand` (teleport/getState/grantXP/grantLevel/
  grantItem/setHP) behind 4 layers: `admin.gm_client_ids` allowlist (chunk
  has no DB — allowlist IS the role layer; bot accounts never listed) +
  `game_config admin.enabled` (default false) + `#ifdef ADMIN_RPC` (no such
  code in prod builds) + audit (served at warn `ADMIN char= op=`, rejected
  at error). Unknown `adminCommand` on prod builds → `forbidden`, session
  survives. ADMIN warn/error lines are EXPECTED on DEV (tests exercise
  rejections); Watch stays green (not FATAL patterns).
- **Files**: chunk `include/services/AdminGate.hpp` (pure gates, unit pins
  `tests/test_admin.cpp` 5/5) + `EventDispatcher::handleAdminCommand`
  (direct-response, reuses managers: teleport writes `lastValidated` with
  srvMs=0 like respawn) + `CMakeLists option(ADMIN_RPC)` (DEV
  `Dockerfile.dev`/`watch_and_run.sh` build `-DADMIN_RPC=ON`, prod
  `Dockerfile` untouched) + login `migrations/084_admin_rpc_config.sql`.
  Harness `Tools/Bots/admin.py` (separate GM session on arbitrary
  characterIds) + `Tests/Contract/test_admin_smoke.py` (teleport round-trip
  + getState + grantXP/setHP + lastValidated pin, non-GM reject + session
  survives, invalid-params matrix).
- **Setup (once per DEV db)**: apply 084 → `seed_bots.py --n 1 --prefix gm`
  → `UPDATE users SET role=1 WHERE login='gm_01'` → set
  `admin.enabled=true` + `admin.gm_client_ids=<gm clientId>` → restart
  game, then chunk (knobs push on handshake; expect `received 81` entries).
  Preflight asserts prod stays closed (flag off + zero role>=1).
- **Measured**: smoke 3/3 in 96s; `test_reg_learn` rewritten via teleport
  (no walking) 2/2 in 60s (was 3:08 success alone). Learn range checks use
  stored position — teleport works.
- **Rule**: new mechanic ships with its admin command first (atom test via
  admin-RPC). `adminKillMob` (Phase B) must NEVER pass a kill test — kills
  are proven via `playerAttack`; admin-kill is setup-only (corpse/harvest).
- **Next (Phase B)**: `adminSpawnMob/adminKillMob/adminSkipTime/
  adminResetWorld` (in-memory caches included, <10s, replaces SQL re-arm) →
  champion/timed rewrites; `brief` join only if join-share still dominates
  (measure first); hand-made parallel batches (shared-state isolation).

## 2026-09-21: admin-RPC Phase B+C shipped (DEV only)
- **Commands**: + `skipTime` (ChampionManager injected clocks + immediate
  ticks; wall-clock untouched), `spawnMob`/`killMob` (queued events:
  spawn registers + pushes a real spawn list to subscribers, kill runs the
  genuine pipeline via CombatSystem::adminKillMob), `resetWorld[champion]`
  (counters + active champions + timed re-arm + arena cull; per-character
  state solved by ephemeral bots, never scrubbed). Teleport also queued
  (full move path: validation + resubscribe WITH snapshots + evict +
  savePositions + broadcast); harness polls getState.
- **Why queued**: thin mob deltas carry no slug/name — dispatcher-side
  register left tests blind (only opportunistic respawn broadcasts
  delivered). Server-spawned mobs (timed/threshold) had the same gap:
  ChampionManager::spawnNotifyCallback now pushes a spawn list to
  subscribers (wired in EventHandler, same sender as admin spawn).
- **Rules learned hard**: (1) spawn spread on a ring (stacking breaks mob
  movement/STUCK-GUARD + poisons tracking); (2) target arena-ORIGIN foxes
  (spawnZone tag — global ghosts pull the bot across the map); (3) movement
  stays in the test zone (interest anchor follows movement — seeking global
  ghosts unsubscribes the arena); (4) harvest corpses FRESH (60s TTL):
  short bursts + immediate sweep, never 45s rounds + late harvest;
  (5) mobDeath is often culled at kill instant — harvest ALL nearby corpses,
  don't tie harvest to observed deaths; (6) skipped time contaminates
  persisted next_spawn_at (kill reports fake killedAt) — timed cycle 1 uses
  escalating skips [300, 3600, 21600]; (7) grant_level uses the SERVER exp
  table (static formula under-delivers: L8 ~= L3); (8) one AdminClient per
  thread (sockets not thread-safe); stop-event on first pass (wall = first,
  not cap).
- **Ephemeral bots** (`admin.ephemeral_bot`, idx 900+, register-only, no
  auth fallback): fresh quest/XP/inventory per run — single-shot SKIP era
  over for smoke/champion/timed/short. gm_bot stays separate (role=1).
  DEV login DB accumulates adm_* — janitor SQL deferred.
- **Content**: `dev_quest_short.sql` (quest 9000 dev_short_chain: kill 2
  arena fox -> 1 hide -> turnin, potion + 5g; giver NPC 9000, minimal
  dialogue) + `test_reg_turnin_short.py` (genuine kills via credit, genuine
  harvest attempts, grant fallback via the real onItemObtained hook).
- **Measured (DEV, 2026-09-21)**: chunk unit 453/453; fast batch 25+1skip
  (12:33, JUnit fast.xml); slow batch 15+2skip (16:52, JUnit slow.xml):
  champion 149s, timed 121-156s (2 cycles, reschedule proven, zero DB
  reads), short 301-310s, learn 60-61s, smoke 65-96s. Slow full chain stays
  nightly. Parallel batches (separate processes) are the remaining lever
  toward 8-10 min wall.
- **Wrappers**: `Tools/Contract/run_fast.ps1` (atoms) + `run_slow.ps1`
  (chains, serial). Prod stays closed (prod Dockerfile has no ADMIN_RPC,
  CMake default OFF).

## 2026-09-21 (evening): Phase C + fixes (DEV only)
- **Evict goes hermetic**: shared bot_05/06 drifted to the arena (past
  teleports persist via savePositions), so spread legs never crossed a cell
  border — 0 evictions. Now ephemeral bots + teleport to a
  boundary-straddling start (1400, 0; >=1 of 2 golden-angle legs always
  crosses past the 225u margin). 45s green.
- **Brief join** (`playerReady{brief:true}` → Phase 4 trims mob/NPC flood,
  sends one empty spawnMobsInZone to keep shape checks green; teleport
  snapshots re-add what's needed). Server: `CharacterDataStruct.briefJoin`
  parsed in the dispatcher, honored in handlePlayerReadyEvent. Harness:
  `Bot.login_join_ready(brief=True)` in teleport-flow tests (smoke/learn/
  champion/timed/short/evict); join-path tests (conn/handoff) stay full.
  Measured: ~5% (joins are protocol rounds + settles, not bytes — the big
  lever remains parallel batches, not trimming).
- **grant_level fixed**: used the static exp formula (L8 landed L3) — now
  the server table via getExperienceForLevelFromGameServer.
- **Janitor**: `Tools/Bots/janitor.sql` (adm_* + role=0 + older than 7d;
  characters CASCADE; gm_bot/bot_* never match). Verified no-op on fresh DB.
- **Final numbers**: fast 25+1skip (12:09, exit 0), slow 15+2skip (16:32,
  exit 0), chunk unit 453/453, Watch exit 0. Serial ≈ 29 min; parallel wall
  ≈ 17-20 min (batches share no fixtures: run both wrappers at once;
  measured 19:32 with contention, evict needed a load-robust retry loop —
  see below). Slow full chain (`test_reg_turnin`) stays nightly.
- **Evict goes parallel-safe**: one blind 700u leg starves under parallel
  load — now walks until eviction actually arrives (240s budget, tap
  restored via ephemeral_bot(tap=True)). Plus the deterministic
  boundary-straddling start above (shared-bot position drift was the first
  cause).

## 2026-09-20: seam rework session (tests only, no bots)
- Return channel: `ChunkManager::resolveLiveSocket` (game) + applied to
  `setLearnedSkill`, `setCharacterAttributesRefresh`, `inventoryItemIdSync`
  + error-logs. Pins: 3 new game unit tests (reconnect/fallback/null).
  Game unit 40/40.
- Twin check: attributes path symmetric (same helper), live check deferred.
- Single SP owner: chunk credits +1/level in memory (was stale till relog),
  game persists absolute SP fact (legacy decrement fallback kept);
  dialogue-learn inserts into chunk cache (was missing till relog).
  Pins: level-up SP credit x2, skill dedup, all green.
- Second echelon: reputation delta-atomic (`add_reputation`, SQL-proven
  5+3=8 with cleanup) + packet carries delta (unit pin); mastery unload
  flush, quiet (unit pin); currency wire mapped (was `Unknown event type`);
  `getSpawnZones` dead branch left alone (player path by design).
- Chunk unit 431/431 (443→475 ms). SQL idempotency probes (SP SET,
  reputation add) with cleanup. Watch SEAM section proven live (caught the
  2 historical currency drops). Live e2e (bots) deferred to Phase 6.

## 2026-09-20 (cont.): coverage sweep — WIO + progression + chaos
- WIO was dead on arrival: game pushed `setWorldObjects`, chunk had NO
  worker branch (fell into `Unknown event type`). Fixed: chunk
  `parseWorldObjectsList` + `GameServerWorker` branch (unit pin) +
  `scripts/dev_wio.sql` (9001 huge-radius examine, 9002 far tiny-radius).
  `test_wio.py` GREEN (guards + examine success + TOO_FAR). Chunk 434/434.
- Progression live: `test_reg_progression.py` (bestiary_kill_update per
  kill, GREEN). Mastery live blocked: needs EQUIPPED weapon with
  masterySlug, but `class_starter_items` is EMPTY — no character ever holds
  a weapon without vendor-buy + equip flow (no scenario equips today).
  Titles content-gated (tier 3-6 / level 10-25), quest rep (+50 merchants)
  rides the slow chain, regen stays unit-only (timing-flaky). All
  documented in the test docstring.
- Chaos: `test_reg_restart.py` (docker restart chunk mid-test → fresh login
  streams mobs again), GREEN 76s.

## 2026-09-19: Wave-6 phases 1-5 (this session)
- P0-regress: L3 30 passed + 3 skipped (16:50), soak smoke 2x10 green, Watch
  exit 0. Kill-swarm flaked first (2/8): harness bug from the evict fix —
  evicted targets always FAIL in `attack_mob` + 8 uncoordinated bots collide
  on the same mob. Fixed harness-side only (`dead_mobs` set surviving evict,
  target claims across swarm threads, 3-attempt retry w/ exclude, peer-kill
  → CONTESTED): kill stable exit 0. Commit `832e5dc`.
- Fast mechanics 1: `pytest.ini` budgets (unit s / contract ≤5 min / slow
  ≤10 min); `scripts/dev_timed.sql` (timed fires in ~3 min, not 4/6 h);
  **clock seam** in `ChampionManager` (injectable epoch/steady clocks, prod
  behavior 1-1)   + 4 pins (`TimedSpawn/TimedKillReports/Despawn/
  SurvivalEvolveViaFakeClock`): chunk unit 426/426 in 443 ms. Found+fixed on
  the way: timed reschedule type error (SERVER_BUGS #10, game `d78c874b`).
- C+A content (migration `082_champion_balance.sql`, DEV only): zones 2/6/7
  → 25 (village 100, code default 100 untouched), timed `next_spawn_at`
  seeded. Applied on dev, verified (SELECT), timed spawn live (ancient_bear
  uid 1000142), cadence restored, L3 29+4 no-regress. **Prod apply is an ops
  action** (live VPS never touched from here) — exact statement is the
  migration file; `scripts/db.sh` dump appends a NULL-reset, so a fresh
  deploy re-seeds via 082 by design.
- Live branches: `test_reg_evict.py` (mobCellLeft asserted live, 44s green —
  the earlier evictprobe 0 was probe-pattern, server emits fine);
  chance-gate live (`Threshold reached ... chance roll failed (44.4 >=
  0.0%)` x2 in chunk log, 24+ kills, 0 spawns); timed kill→DB live
  (`test_reg_timed.py` 4:17 green, `next_spawn_at = killedAt+3600`).
  Cap-gate live deferred (unit-pinned + same game_config propagation path
  as chance, which is now proven). game_config knobs need game+chunk
  restart (boot-handshake push only — a game-only restart does NOT reach
  chunk; learned the hard way).
- Fast mechanics 2 (partial): `scripts/dev_learn.sql` (Bot H → level 5/5 SP,
  repeatable) + `test_reg_learn.py::test_reg_learn_skill_success` GREEN
  3:08 (two-session persist proof). Turned up SERVER_BUGS #11 (notify lost
  + double charge, OPEN). Remaining for next session: `dev_quest_short`
  content + turnin split (spec below).
- Tracker #8/#9 skipped (X-API-Key still 401; user files manually).

## Champion balance table (dev-measured 2026-09-19; recompute, don't re-farm)
- `time_to_threshold = threshold / kill_rate(density, dps, flee)`.
- Arena (dense RECT, threshold 5): 2 bots ≈ 4 min → kill_rate ≈ 1.25/min →
  threshold 25 at arena density ≈ 20 min.
- Prod Glade (25 foxes / ~111M u²): ≈ 1 kill/45 min per 4-bot farm →
  threshold 25 ≈ 19 bot-hours; threshold 100 ≈ 75 bot-hours (still
  aspirational solo — timed is the reliable source by design).
- Timed cadence = `interval_hours`; re-arm = `killedAt + interval` (live).
- Cap 3 / chance 100 / despawn 30 min (code defaults; DB-overridable via
  `game_config`, game+chunk restart to propagate).
- Calibrate `kill_rate` from INFO threshold-window telemetry
  (`[Champion] threshold window zone=... count=4/5` in chunk log).

## dev_quest_short spec (next session; ~3-min fast chain)
- New DEV-ONLY quest `dev_short_chain` (ids 9000+): giver = new dev NPC
  near village (avoid Varan dialogue surgery); steps: kill 2x arena fox →
  collect 1x hide → turnin; rewards 1x potion + 5g.
- Needs: `quest` + `quest_step` + `dialogue_node/edge` + `quest_reward` +
  `npc` + `npc_placements` rows in a `scripts/dev_quest_short.sql`
  (idempotent, 9000+ range, by analogy with `dev_arena.sql`).
- Client: `test_reg_turnin_short.py` mirroring `quest.py` flow against the
  new slug; keep `test_reg_turnin.py` as the slow acceptance (nightly).
- Acceptance: ≤5 min green + reward deltas asserted + fixture re-runnable.
