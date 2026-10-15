# Client testing (UE side only)

> Integration harness (contracts/bots/replay/load/soak, server runbook)
> moved 2026-09-22 to the WSL repo
> `~/projects/mmorpg-prototype/mmorpg-prototype-integration-tests/`
> (runbook in its `README.md`). Do not recreate it here.

## What lives here

- **L1 offline:** enable `FDevModeConfig.bEnabled` on `BP_MyGameInstance`, open entry map.
  5-minute flow (`Config/DevMode/qa_presets.json`): `devmode.scenario combat` →
  `devmode.setplayerhp 1` → `devmode.setplayerhp 1000` → `devmode.reloadinventory` →
  `devmode.scenario reset` → `devmode.listmobs`. Collect `Saved/Logs/*.log` on failure.
- **L1 auto (golden sweep, no servers):** `Tools/Smoke/VerifyClient.ps1 -Sweep`
  launches a `-devmode=full_sweep` client (see `-devmode` / `-devmode=<scenario>`
  flags in `MyGameInstance.cpp`) that asserts reset/spawn/kill/respawn/HP
  round-trips and quits; gates on `DEVMODE_SWEEP_RESULT PASS` in the log.
- **L2 visual regression (goldens):** `Tools/Smoke/VerifyClient.ps1 -Sweep -Visual -Goldens`
  runs a visual `-devmode=bot_combat_patrol` client (`-devmode-shots` saves
  named checkpoints `DevBot_<scenario>_<step>`, deterministic names), gates on
  `DEVMODE_BOT_RESULT combat_patrol PASS`, then `Tools/Smoke/Compare-Goldens.ps1`
  gates the shots against `Config/DevMode/goldens/` (%-pixels tolerance,
  default 5%; nonzero exit on mismatch/missing). `-Goldens` requires `-Visual`
  (headless `-nullrhi` runs produce no screenshots — fail-fast usage error).
  `Tools/Smoke/Approve-Goldens.ps1` blesses new shots in one command.
  Runs target `-DevMap LoginLevel` (default: lit, real ground + navmesh; the
  default container is an empty void — bots used to skydive through every
  assert, all movement checks are 2D). A loud grounded pre-flight
  (`ExpectGround` / `ExpectGrounded`) fails void runs instead of passing them.
  Current goldens are lit world-visuals (cross-run diff 1-4% vs 5% budget —
  margin is thin: wandering wolf, foliage, particles; deterministic-golden
  mode with frozen time/seeds is future work).
  Negative control proven (tight budget fails on the 0.48% target-frame delta).
- **L5 watchable runs:** `Tools/Smoke/WatchClient.ps1 [-Scenario bot_combat_patrol] [-NoRecord] [-Tombstone <regex>]`
  runs ONE visual client (window (0,0) 960×540, rest minimized + client
  foregrounded) + ffmpeg gdigrab region recording into
  `Saved/WatchRuns/<stamp>_<scenario>/` (watch.mp4 + named PNGs + client.log +
  `WATCH_RESULT.txt` + `ffmpeg-rec.log`); nonzero exit unless the tombstone
  passes. Needs ffmpeg in PATH (winget `Gyan.FFmpeg`). Live viewing is for
  single debug runs only — regressions are reviewed from the recording.
  Movement runs per-frame (core-ticker `SteerTick`), so bot pace is
  fps-independent; a 10Hz-only input would crawl (~20 u/s at 300fps).
- **L3 perf gates:** every `full_sweep`/`soak` cycle logs
  `DEVMODE_PERF phase=<n> frames=N seconds=F fps=X p95ms=Y` per phase.
  `VerifyClient.ps1 -Sweep` gates on marker presence + a loose tripwire
  (`-PerfFpsFloor` 30, `-PerfP95CeilMs` 50; baseline 2026-10-15: ~240-330fps,
  p95 ~4ms). Shader-compile hitch context comes from `LogShaderCompilers`
  lines (info only). Tighten thresholds after more runs / lit map.
- **L4 soak (offline):** `-devmode=soak [-soak-iters=N] [-soak-period=F]`
  loops sweep churn cycles; each logs `DEVMODE_SOAK iter=N memMB=X peakMB=Y`
  (watch `memMB` growth across iters), finish logs `DEVMODE_SOAK_RESULT`.
  `N=0` runs until killed (overnight: no `-devmode-shots`, scan the log for
  crash tombstones + flat memory in the morning). Short verify:
  `WatchClient.ps1 -Scenario soak -NoRecord -Tombstone "DEVMODE_SOAK_RESULT (PASS|FAIL)"`
  with `-soak-iters=3`. Live soak + reconnects + AFK disconnect: dev servers,
  separate runbook (integration-tests repo).
- **L1 action bot (no servers):** `-devmode=bot_combat_patrol` runs
  `UDevModeBotBrain`: drives the REAL player (movement via AddMovementInput,
  lock via SetLockedTarget, attack loop via OnAttackInput) to patrol and kill
  2 seeded mobs; mob HP is server-authoritative so the killing blow is an
  honest stub via the setmobhp path (logged `BOT_HIT_STUB`); gates on
  `DEVMODE_BOT_RESULT combat_patrol PASS`. Every step has a timeout plus a
  120s global watchdog — never hangs CI.
- **L1 Phase A bots (no servers):** `-devmode=bot_loot_pickup` (kill 1 → walk
  to corpse → stub drop via the real `SpawnDroppedItem` path, logged
  `BOT_LOOT_STUB` → resolve via real inventory/pickup calls → assert inv +1;
  gates on `DEVMODE_BOT_RESULT loot_pickup PASS`) and
  `-devmode=bot_death_respawn` (waypoint → stub death via the real
  `SetDead(true)` path, logged `BOT_DEATH_STUB` → assert dead + death screen
  → stub revive via the real `SetDead(false)` path, logged `BOT_RESPAWN_STUB`
  → assert alive + HP + spawn point; gates on
  `DEVMODE_BOT_RESULT death_respawn PASS`). Manual variants:
  `devmode.bot <combat_patrol|loot_pickup|death_respawn|harvest_run>` (stay in world).
- **L1 Phase B bots:** `-devmode=bot_harvest_run` (offline: kill → stub
  harvest via the real `SetHarvested` path, logged `BOT_HARVEST_STUB` →
  stub loot → inv +1; gates on `DEVMODE_BOT_RESULT harvest_run PASS`).
  Live (dev servers): `-bot-live=live_harvest_run` (real kill → real harvest
  progress → inspect → pickup-all → inv growth) and
  `-bot-live=live_vendor_repair` (walk to Varan NPC 1 → open shop → sell 1
  junk stack, gold+ → open repair shop → repair damaged durable,
  durability=max gold-; gates on `LIVEBOT_RESULT ... PASS`). Vendor needs a
  stocked bot (junk stack + damaged durable; `bot_01` qualifies) and an
  admin teleport near the NPC as setup (server validates shop range from
  its stored position). Gather-NODE variant blocked: no gather nodes on dev
  (only examine stones 9001/9002) — needs server dev content + restart.
- **L1 Phase C bots (live only):** `live_cross_visibility` now talks after
  meeting (plan Phase C): both clients send one `local` message and assert
  the peer's arrival in their log + the peer actor bubble
  (`LIVEBOT_RESULT ... PASS ... reason=chat-done`, both sides). Needs the
  pair on open ground (local radius is 50u server-side; the old rendezvous
  sat over the void — default is now the Varan courtyard). Quest:
  `-bot-live=live_quest_turnin` (dialogue with NPC 9000 → accept → 2 fox
  kills + harvest + loot each corpse → turn-in dialogue → assert +5g and
  item 46; single-shot quest, needs a fresh bot + admin setup: teleport
  near the NPC + spawn 2 foxes via `spawn_mob mob_template_id=4`).
- **L1 Phase D bots (all live only):** `-bot-live=live_skills` (bot_08,
  boosted by `dev_learn.sql`: trainer shop at Edrik NPC 3 → learn
  power_slash + battle_cry → cast damage skill, assert mob HP drop +
  cooldown + mana → cast self buff, assert `battle_cry_phys_atk` in
  activeEffects), `-bot-live=live_equip` (equip main_hand: slot + visual
  mesh asserts → unequip → restore pre-equipped state),
  `-bot-live=live_level` (XP granted externally mid-run via admin
  `grant_xp`; polls ExperienceManager progression),
  `-bot-live=live_titles_emotes` (equip earned `first_hunter` → play first
  owned emote from the server snapshot on own actor).
- **L1 Phase E:** `bot_periphery` (offline: locale RU/EN toggle + restore,
  camera zoom min/max/clamp through the real input path + boom converge;
  music leg FAILs loudly until designer BP content exists — `ZoneMusicMap`
  is empty and nothing calls `PlayMusicForZone`),
  `-bot-live=live_char_create_delete` (list → create `Test V..Z` → verify
  in list → delete → verify gone; name rotation on taken),
  `-bot-live=live_back_to_login` (return to login, assert map from the
  PostLoadMap callback + core-ticker watchdog, tombstone + quit).
  Out of bot scope: chunk transitions (dev runs a single chunk — N/A),
  AFK disconnect (deferred to the night soak).
- **L1 live bot (dev servers):** `-bot-live=<scenario> -botlogin=<user>
  -botpass=<pass>` drives the REAL online flow (auth → chars → join → spawn)
  via `ULiveBotDriver`, then the same brain in live mode: no spawn teleport,
  no hit stub (real server damage, 120s kill budget), swept direct movement
  (`bDirectMove`, honest pace from MaxWalkSpeed — live CharacterMovement input
  barely converts, cause still open, see changelog); gates on
  `LIVEBOT_RESULT ... PASS`. Scenarios: `live_patrol_kill` (2 real kills),
  `live_loot_pickup` (1 real kill → harvest corpse → inspect → pickup-all →
  inv growth), `live_death_respawn` (one-hit pull → real server damage →
  death screen → real respawn request → alive + HP),
  `live_cross_visibility` (2 headless clients walk to a rendezvous and assert
  each other's remote actor via `SpawnedPlayers`). Live-run hygiene: a stale
  server-side death from a killed run is recovered automatically
  (`RecoverRevive`); a character wedged into geometry needs one admin
  teleport (`bots/admin.py` `teleport_to`, gm_01) — the brain reports
  `waypoint-timeout` with the blocking actor in `BOT_STUCK` lines. Note the
  chunk validates movement server-side (`positionCorrection`): teleports and
  off-pace moves are reverted, so live legs never teleport (offline only).
  Cross-world safe:
  core-ticker polling, PostLoadMap-tracked world (never `GetWorld()` across
  travel), GC-rooted driver/brain, per-state + 300/360s watchdogs.
  Manual variant: `devmode.expect <hp|mobs|inv|gold> <eq|ne|gt|lt|ge|le> <Value>`,
  plus `devmode.setmobhp`, `devmode.killmob`, `devmode.clearcorpses`.
  Watchable variant: `-devmode-map=LoginLevel -devmode-shots` (+ `VerifyClient -Visual`;
  needs a working display; shots land in `Saved/Screenshots/`).
- **L2 engine:** Session Frontend → Automation → `MMO.*` specs
  (`Source/Prototyping/Private/Tests/`: Framing, NtpMath, ClientVersion).
- **Smoke UE:** `Tools/Smoke/SmokeClients.ps1 -n 2 -WaitSec 120`
  (real `-game` clients, headless by default, log scan for crash signatures).

## Notes

- Entry map `/Game/Maps/MainGameContainerLevel`
  (`LaunchClients.bat` uses `/Game/Maps/WorldMapV1` — not identical).
- Log categories (`Source/Prototyping/Prototyping.h`): `LogConnection` /
  `LogNetPacket` / `LogPing`; verbosity in `Config/DefaultEngine.ini [Core.Log]`.
- Never hold `UWorld*` across `OpenLevel` (see AGENTS.md Pre/PostLoadMap flow).
- Loading screen: MoviePlayer covers travel and auto-exits at load end (it must
  never wait for manual stop — that traps the game thread inside LoadMap until
  first click). Readiness is covered by a plain-UMG overlay, hidden on full
  `ReadyFlags` (`PlayerReadyAck | UIInitialized | PlayerSpawned`) + frame gate;
  two unconditional wall-clock safeties (8s core-ticker watchdog, 15s safety)
  force removal even if flags/gates stall (e.g. unfocused window). Window
  foreground is re-asserted around OpenLevel + focus-keeper ticker while the
  screen is up. Travel is measured (`Travel START/DONE` with wall-clock + fps).
