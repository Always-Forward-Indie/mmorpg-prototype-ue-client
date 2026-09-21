# ADMIN-RPC PLAN: instant test-state setup (teleport/spawn/grant/read)

## Status 2026-09-21 (Phase B+C shipped, proof live)
- Measured (DEV): smoke 3/3 ~65-96s; learn 2/2 ~60s (was 3:08); timed
  ~121-156s, 2 cycles, reschedule proven, zero DB reads (was 4:17 + manual
  re-arm); champion ~130-149s (was ~15 min farm); short chain ~301-310s
  (new; slow chain stays nightly). Chunk unit 453/453. Fast batch 25+1skip
  (12:33), slow batch 15+2skip (16:52), Watch exit 0, ADMIN audit verified.
- New since Phase A: skipTime/grantLevel-fix/spawnMob/killMob/resetWorld
  (queued events with snapshot reuse), ephemeral bots (no more single-shot
  SKIP), dev_short_chain content, run_fast/run_slow + JUnit. Hard rules:
  ring-spread spawns, arena-origin targeting, zone-local movement, fresh
  corpses (60s TTL), harvest-all sweep (mobDeath culled), escalating timed
  skips (time-travel debt), server-table grantLevel, one AdminClient/thread.
- Remaining lever: parallel batch processes (shared-state isolation) toward
  8-10 min wall; adm_* janitor SQL; brief-join only if measured join share
  dominates.

## Status 2026-09-21 evening (Phase C done, both batches green twice)
- Fast 25+1skip (12:09, exit 0), slow 15+2skip (16:32, exit 0); unit
  453/453; Watch exit 0. Measured realities: brief-join ≈ 5% (joins are
  rounds+settles, not bytes); parallel wall ≈ slow batch ≈ 17 min (no
  shared fixtures between batches). grant_level now uses the server table.
  Evict hermetic (boundary-straddling start). Janitor SQL shipped.
  Slow full chain stays nightly; soak stays nightly.
- Shipped: teleport/getState/grantXP/grantLevel/grantItem/setHP + gates +
  harness + smoke + learn rewrite. Measured: smoke 3/3 in 96s, learn 2/2
  in 60s (was 3:08 success alone). Chunk unit 446/446 (5 new AdminGate
  pins), Watch exit 0, ADMIN audit verified (served→warn, rejected→error).
- Deviations from §2 (deliberate, recorded): role layer = `admin.gm_client_ids`
  allowlist, not `users.is_gm` column (chunk has no DB; `users.role>=1` stays
  source of truth, allowlist enforced in chunk; A2 may push role via
  handshake) — no schema change needed. Handler = direct-response in
  `EventDispatcher` (like `handleGetCharacterExperience`), not a queued
  `AdminEventHandler` — fewer moving parts, same manager reuse; event-queue
  variant only if dispatcher-thread safety ever demands it. `resetWorld`
  deferred to Phase B (needs in-memory clearing, not a stub).
- Next: Phase B (spawn/kill/skipTime/resetWorld + champion/timed rewrites).

## 1. Why (numbers)

Tests currently spend time acquiring state, not asserting it:

| Test today | Time | Walking/waiting share | With admin-RPC |
|---|---|---|---|
| Champion farm ~15 min | 30km hike + 5-8 fox farm | Target ~3 min |
| Timed ~8 min + arena prep | hike + window wait | ~2 min |
| Quest 22 min | 3 hikes + 45% loot lottery | ~5 min (dev_short + teleported legs) |
| Learn 3 min | walk to trainer | ~1 min |
| Full L3 ~20-25 min | sum of the above | **~8-10 min** |

Rule after rollout: **locomotion is paid for exactly once** (dedicated
locomotion test); everything else teleports.

## 2. What we build

### 2.1. Server commands (chunk; game only where its data lives). Extended set:

| Command | Params | Effect (must reuse existing managers, never duplicate logic) |
|---|---|---|
| `adminTeleport` | characterId, x, y, z | Server-side position set incl. lastValidated write (else the next legal step trips teleport-cheat detection) |
| `adminSpawnMob` | templateId/mobSlug, x, y, z, count | Spawn via SpawnZoneManager logic, returns uids |
| `adminKillMob` | uid or radius | Kill through the real combat pipeline (drop/quest credit must behave genuinely) |
| `adminGrantLevel` / `adminGrantXP` | characterId, level/xp | Via `grantExperience` (SP/stats/titles accrue normally) |
| `adminGrantItem` / `adminGrantGold` | characterId, itemId/slug, qty | Via InventoryManager (quest hooks, weight — normally) |
| `adminSetHP` / `adminSetMana` | characterId, value | Regen/threshold cases |
| `adminSkipTime` | seconds | Advance time where clocks are injected (ChampionManager seam ready); wall-clock spots get TODOs, not hacks |
| `adminGetState` | characterId | READ: position/HP/SP/skills/inventory/quest steps/flags — for asserts without log parsing |
| `adminResetWorld` | scope | Reset test garbage (9000+ dev zones/fixture states) between runs |

### 2.2. Protection (defense in depth, industry practice — all layers, always)
1. **Role**: only accounts with GM flag (`users.is_gm`, default false)
   are served; bot accounts have no flag (separate `gm_bot` seed).
2. **Config**: `game_config admin.enabled=false` by default; enabled on
   DEV by hand only.
3. **Compile**: whole handler behind `#ifdef ADMIN_RPC` (no such code in
   prod builds physically).
4. **Audit**: every command logs at warn (`ADMIN char= op=`) — visible in
   Watch; on prod (layers 1–3 off) any attempt is an error.
No single layer suffices. Prod deploy: flag off + non-DEV build + zero
GM accounts. Preflight asserts the last one.

### 2.3. Harness (`Tools/Bots/admin.py`)
Thin wrapper over `MmoClient` reusing heartbeat/join logic: `teleport_to()`,
`spawn_mob()`, `kill_mob()`, `grant_level()`, `grant_item/gold()`,
`set_hp()`, `skip_time()`, `get_state()`, `reset_world()`. No walking.

## 3. Phases

### Phase A. RPC core (~3 h)
- A1. Chunk: `handleAdminCommandEvent` + subcommands above (reuse
  managers!). GM-flag check + config gate + `#ifdef` + audit log.
- A2. `admin.py` harness + 2 proof tests: `test_admin_smoke.py`
  (teleport round-trip + getState assert) and rewritten `test_reg_learn`
  via teleport (3 min -> ~1 min).
- Accept: builds (DEV and prod profile — latter has no such code), unit
  on parsing/gates, both proof tests green, Watch clean (ADMIN lines
  visible).

### Phase B. Time & world (~2 h)
- B1. `adminSkipTime` (injected clocks only).
- B2. `adminResetWorld` (9000+ zones + fixture states).
- B3. Rewrite `test_reg_champion` (arena fox spawn + teleport, target
  ~3 min) and `test_reg_timed` (time-skip instead of window wait, ~2 min).
- Accept: both green in under 5 min combined.

### Phase C. Pipeline (~2 h)
- C1. Split L3 into parallel batches by historic runtimes (Riot-style
  load balancer; start with a hand-made 2–3 batch list).
- C2. JUnit-XML from pytest (`--junitxml`) + log summary (CI groundwork;
  CI farm itself is a separate project).
- C3. Docs: runbook section (fast vs full tests), refreshed budgets
  (fact ≤2 min, slow ≤10 min), AGENTS.md rule ("new concession mechanic
  ships with its admin command first").
- Accept: L3 ~8–10 min wall-time, readable report.

## 4. Estimates, risks, out of scope
~7 h + live runs. Risks: (1) duplicating manager logic in handlers instead
of reusing it — forbidden by rule in A1 (bug trap #1 of such systems);
(2) teleport breaking movement invariants (lastValidated!) — fixed by
writing validation state, pinned by test; (3) GM flag leaking to prod —
4 layers + preflight assert.
Out of scope: Gauntlet/UE side (waits for editor), CI farm (separate
project), ML bots (Riot-scale, unneeded).

## 5. Industry mapping (why this shape)
Riot: RPC on client+server, farm with runtime balancing. Epic: tiered
budgets (smoke ≤1s), multi-process orchestrator. Common formula: state by
command, asserts by read, locomotion tested once, long stability at night.
