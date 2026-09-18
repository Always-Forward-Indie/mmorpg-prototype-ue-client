# Server bugs found by bot/contract testing (dev env, 2026-09-12..13)

## 0. Blind tradeAccept fabricated sessions — FIXED {#trade-invite}
- `handleTradeAcceptEvent` created a session WITHOUT checking for a pending
  invite: a blind accept built `trade_{a}_{b}_{ts}` from thin air, and the real
  request then failed with `already_in_trade` (caught via replay forensics).
- Fix (chunk): `TradeSessionManager` tracks pending invites (TTL 60s, same as
  sessions, swept by the existing 30s scheduler task); accept requires a live
  invite else `no_pending_invite`; decline consumes; request records.
  Verified: `test_trade_accept_without_invite_rejected` + live duo trade +
  replay.
- Spec `06` updated (flow + error codes).

Repro: `Tests/Contract/` + `Tools/Bots/` against WSL dev servers
(`docker-compose.dev.yml`, login→game→chunk-server-new). Evidence = chunk/game
container logs + packet taps (`run_swarm.py --tap`).

## 8. Threshold champions unreachable in prod + interest ghosts (2026-09-18)
- **Threshold 100 unreachable**: all prod zones (`village/fields/ruins/forest`)
  have `champion_threshold_kills=100`, but the Fox Glade spawn zone is an
  ANNULUS (inner R 5608, outer R 8172 around (-434,-973)) holding 25 foxes
  over ~111M u². Measured bot kill rate in the ring: ~1 kill/45 min per
  4-bot farm (foxes flee faster than bots close). 100 same-(zone,template)
  kills is effectively unreachable; no invasion events configured
  (`has_invasion_wave=false` everywhere); timed templates (4h/6h) have NULL
  `next_spawn_at` (skipped). Threshold logic itself is unit-pinned
  (`test_champion.cpp`); the LIVE path has never fired. Content-balance
  decision needed (lower thresholds? denser spawns? invasion events?).
- **Interest ghosts**: culled mobs freeze client-side with no eviction —
  47 tracked foxes vs 25 spawned. Bots chased ghosts (walk 30s + attack 45s
  per ghost, all `out_of_range`). Fixed bot-side (fresh-only ≤30s hunting +
  navigate-by-ghosts `_seek`, walk-to-corpse before query, restored
  inspect+pickup dead code in quest.py which froze `have` at its seeded 1).
  UE client likely affected the same way (stale nameplates/targets) —
  needs client-side verification.
- Tracker filing blocked 2026-09-18: `X-API-Key` returns 401
  (requestId ba3b1cd7) — needs key rotation, then file both items.

## 9. Quest farm stall root-caused: dead pickup code, not loot RNG (2026-09-18)
- `test_reg_turnin` failed 3x identically (`have` frozen at exactly 1 over
  16-19 kills). Loot path proven healthy the whole time (server-wide hide
  rate 48% vs configured 45%; focused probe 2/5).
- Root cause (test design, NOT product): `quest.py::_harvest_corpse` had
  its inspect+pickup block stranded after a `return` (dead code) —
  harvest completion only GENERATES loot (`addedToInventory=false`), so
  `have` could never advance past its seeded 1. Restored via
  `_pickup_corpse_loot` (inspect + pickup-all). reg_turnin GREEN 22:36.
- Same class of ghost bug as #8 (now fixed in quest.py +
  champion_farm.py): culled mobs freeze client-side, bots chased ghosts.
  Fix: fresh-only (≤30s) hunting + `_seek` navigation-by-ghosts +
  walk-to-corpse before query.

## 1. `getSpawnZones` unknown on chunk — client send removed, server path SAFE
- Chunk logs `Unknown event type: getSpawnZones` and ignores it (log-only,
  verified by bisect: clean `getSpawnZones` does NOT break sessions).
- The client send was dead code anyway (`SendGetConnectedPlayersRequest` is
  never called; server pushes Phase 4 after ready) → removed in
  `PlayerManager.cpp`. No server change needed.

## 2. Extra body fields are SAFE (bisect cleared this) {#strict-body}
- Re-bisect on a healthy server: `getConnectedCharacters` with extra
  `clientVersion`/`junkField` → answered normally, session lives. The earlier
  deaths blamed on fields were the poisoned-server window (stale socket
  registry, see #7). Bots still mirror the real client (version only in
  login/register) as hygiene.

## 3. Two-session chat/binding decay under join churn — CLOSED 2026-09-13 {#chat-cross}
- Shipped earlier: stale-disconnect guard (`ClientEventHandler`, skips removal
  when the registered socket is live), read-only `getClientsList()` (was
  purging live sessions), PING grace for not-yet-ready clients.
- Root cause of the `xfail` turned out to be a **test artifact**, not a server
  drop: the module-scoped `pair` fixture never drained setup backlog, so the
  `A -> B` test's `wait_for("chatMessage")` returned test-1's self-echo marker
  still sitting in B's socket buffer and failed the marker assert. Verified by
  probe (`Tools/Bots` sessions + drain + marker predicate): `A->B` and `B->A`
  both deliver, self-echo intact on both.
- Test fix (`Tests/Contract/test_cross_visibility.py`): drain both sessions in
  the fixture, `_wait_marker()` with marker predicate, `xfail` removed, plus a
  new symmetric `test_zone_chat_b_to_a`. Verified 3x green + full contract
  suite 19/19 + `chat_mesh` swarm 8/8.
- Server hardening shipped in the same pass (WSL chunk-server-new, genuine
  hazard classes found by audit, behavior-preserving):
  `ClientManager` generation-stamped registry (`SocketEntry{sock,gen}`,
  `setClientSocket` returns gen, `removeClientData(id, expectedGen=0)`,
  O(1) `socket->client` reverse index with forward-entry confirmation);
  `NetworkManager` write queues validate owner identity (raw `socket*` address
  reuse after free can no longer inherit a stale queue); broadcast uses
  `getActiveSnapshots()`; `[SESSDBG]` temp logs removed.
- Lesson: cross-session tests must drain shared fixtures and match markers,
  never bare `wait_for(eventType)`.

## 4. `chunkIp` advertised Docker-internal hostname — FIXED {#chunk-ip}
- `joinGameClient` (game) returned `chunkIp: "chunk-server"`. Game now prefers
  `CHUNK_PUBLIC_HOST` env for the client response (dev `.env` = 127.0.0.1);
  server-to-server still uses `CHUNK_SERVER_HOST`. UE ignores the field (uses
  `server_config.json`); bots use returned port + local host like UE.

## 5. ClientVersion drift — FIXED (client bumped) {#version-drift}
- Client bumped 0.1.0 → 0.1.2 (`MyGameInstance.h`), inside dev login range
  0.1.2–0.1.4. Bump deliberately on future protocol changes.

## 6. PING_TIMEOUT (30s) vs long scene loads {#ping-timeout}
- Chunk force-disconnects clients idle >30s (`ChunkServer PING_TIMEOUT_SEC`).
  Join→F2→F4 with big world-states takes ~60s; clients MUST heartbeat from the
  first socket (real `PingManager` does; bots do via `MmoClient.start_heartbeat`).
  Consider first-ping grace (e.g. 90s) for slow loaders.

## Total join outage (game returned chunkId 0) — FIXED 2026-09-15
- Symptom: game `joinGameClient` returned `chunkId 0 / port 0` for ALL bots
  (0/200), while chunk↔game link looked alive.
- Root cause: game `ChunkManager` had the same stale-disconnect class as P0:
  link flap → reconnect re-registers chunk 1 on a new socket → the STALE
  disconnect event (old socket) arrives later → `removeChunkServerDataBySocket`
  erases the fresh entry. Permanent outage (re-registration only happens at
  chunk boot). Proven by unit test + live recovery via chunk restart.
- Fix: generation stamps in game `ChunkManager` (same pattern as chunk P0:
  per-registration gen, removal only on match, stale reverse entries dropped,
  old-socket mappings cleaned on re-add) + chunk re-asserts the
  `chunkServerConnection` handshake every 60s (idempotent heartbeat;
  destructor/reconnect cancel the chain so timers can't pile up or hang join).
- Verified: `tests/test_chunk_manager.cpp` ALL OK (game repo `tests/`,
  same no-gtest pattern); live join returns chunk 1; repeat 200/200 green.
- Lesson: every async registry with reconnects needs generation stamps —
  now covered on chunk sessions, chunk write-queues, and game chunks.

## SEGV under 200-churn — FIXED 2026-09-15 (write-queue lifecycle)
- Symptom: ASan SEGV (null+0xa3) in `epoll start_op` under `doNextWrite`,
  200-join storm, ~192 sessions. First 200-run passed by luck (race).
- Root cause: erase-then-recreate of per-socket write queues. Teardown
  (`removeActiveSession`/`removeWriteQueue`) erased the map entry while a
  straggler send recreated it for the same live socket → two strands →
  concurrent `async_write` on one socket (UB). My P0 in-place reset had the
  same flaw class (foreign-thread mutation of live queue state).
- Fix (chunk + login): same live object always reuses its queue; stale
  mappings are REPLACED (never mutated); nothing is erased on teardown —
  reclamation only via `gcWriteQueues()` (owner-expired, provably no pending
  strand work since every posted lambda holds the socket). GC runs on the
  existing periodic cadence (chunk `cleanupInactiveSessions`, login 60s
  cleanup branch). Lock-order note kept nesting-free.
- Verified: 200/200 patrol green (x3 on warm world), zero ASan, zero drops,
  registry reaped to 0. Interest counters at 200: 123719 mob updates built
  → 17991 events pushed (**85% culled**), 15057 resubscribes, no flap storm.
- Cold-world caveat: 200-storms within ~3 min of a chunk restart fail
  spuriously (spawn data still arriving) — warm-up rule in README.
- Enter-snapshots are rate-limited per client (2s, `interest.snapshots` +
  `interest.snapshot_cooldown_ms`, counts in Interest line); snapshot
  content verified by `test_handoff.py` (mob + corpse return paths).
- Process lesson: never run manual `make` inside the container while
  watchexec watches — parallel makes corrupt the link (binary vanishes).
  Serial builds only (wait for quiet), verify `make` up-to-date + restart.

## Load & soak 2026-09-14 (dev stand, cap 200 — no 2000 per scope)
- Seed: `seed_bots.py --n 200` (fixed name pool for n>8: letters, no digits —
  login rejects digit names with `ERR_CHAR_NAME_INVALID`).
- 20/20 `chat_mesh` OK; chunk ~11% CPU, queues 0, no drops/overflows.
- 200/200 `patrol` OK (1s stagger); no `dropped`/overflow logs; registry
  reaps cleanly (`Active clients: 0` after).
- S1: 8 bots `patrol --minutes 30` → 8/8 OK, RSS 1004→1017MB over 31 min
  (flat, no leak slope). Post-200 RSS retains ~1GB at 0 clients — allocator
  retention in Debug builds, not session leak (registry verified empty);
  revisit on S2-nightly if slope appears.
- Spatial v1 (distance culling 4000u) was built, then REVERTED same day:
  culled mobs froze in client state (spawn lists only at F4), hunters chased
  stale points (`kill` regressed, proven by revert → green). Proper v2 needs
  per-client subscriptions (zone enter → full sync + thin updates).

## P2 R-bugs — FIXED 2026-09-14 (WSL game/login/chunk, verified by restart)
- Game `EventQueue` had no stop-predicate: `GameServer::~GameServer` joined
  event threads blocked forever in `popBatch()` → container hung to SIGKILL.
  Ported the chunk fix (`stopped_` + `notify_all`, `pop/popBatch` return
  false) and wired `eventQueue{Game,Chunk,Ping}.stop()` into
  `GameServer::stop()`. Verified: `docker restart` game stops in ~1s,
  chunk reconnects in ~5s (`Loaded 58 items from Game Server`).
- Login `EventQueue` same class of bug (worse: destructor joined without any
  `stop()`; main loop only escaped via 60s `tryPopBatch` timeout). Added
  `stopped_` to `pop/popBatch/tryPopBatch` + `stop()`, destructor calls
  `stop()` first. Verified: login restarts instantly, fresh auth works.
- Login `sendResponse` had no strand (concurrent `async_write` on N
  io_context threads = UB). Ported chunk's per-socket strand write queue
  (single queue, no critical/bulk split — login traffic is thin) with the
  same owner-identity guard against raw-pointer address reuse; queues freed
  on all three disconnect branches.
- Scheduler fragility (chunk+game): bare `t.func()` could kill the scheduler
  thread (PING-reaper, cleanups, mob ticks). Wrapped in try/catch with
  stderr log; `GameServer/LoginServer::processBatch` now catch pool
  full/stopped throws per event (drop + log, never kill the loop).

## 7. Dev compose fixes applied (kept, needed to boot at all)
- `docker-compose.dev.yml` had no `env_file:` → containers never read `.env`
  (DB_HOST defaulted to 127.0.0.1 → fatal). Added `env_file: [.env]` +
  overrides: login/game `DB_HOST=db`, game `CHUNK_SERVER_HOST=chunk-server`,
  chunk `GAME_SERVER_HOST=game-server`.
- Added `{}` `config.json` stubs (required by `COPY` in `Dockerfile.dev`;
  servers are env-configured, file unused).
- `.env` hostnames fixed the same way (were `127.0.0.1`, invalid in Docker net).
