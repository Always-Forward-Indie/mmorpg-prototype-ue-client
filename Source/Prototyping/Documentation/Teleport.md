# Teleport / Respawn — AAA cover transition (client)

All server-authoritative jumps (`respawnResult`, `playerTeleported`, skill
teleports) run through one flow. No bare `SetActorLocation` pops.

## Flow (UTeleportManager, locally-controlled pawn)

1. **Freeze** — `ABasicPlayer::BeginTeleportCover`: cancel auto-attack /
   target / cast bar / pending interactions, flush pending combat results,
   `LockMovement()`, spring-arm camera lag OFF.
2. **Fade out** — `PlayerCameraManager::StartCameraFade` to black, 0.22s.
3. **Apply (at fade peak, invisible)** — `SnapToAuthoritativePosition`:
   validate (finite + 50km bound, `FTeleportRule`), drain velocity/input,
   `TeleportPhysics` snap, controller yaw sync, `SetCoordinates`,
   `UpdateOverlaps`. Respawn: vitals BEFORE `SetDead(false)`; death screen
   hides under black.
4. **Hold** — 0.15s, +0.35s for far teleports (>= 5000cm) so WorldPartition
   cells stream in.
5. **Reveal** — fade back in (0.30s), camera lag ON, `UnlockMovement()`,
   `teleportAck` to chunk (requestIdEcho-bound: charId + applied pos + reason).

Remote pawns snap immediately through the same `SnapToAuthoritativePosition`
(no fade — not our camera). Insane targets are rejected with error-level logs.

## Modules

- `Public/Utils/NetCore/TeleportRule.h` — pure policy (durations,
  thresholds, validation, yaw normalize). Spec: `MMO.Teleport.Rule`.
- `Public/Gameplay/Teleport/TeleportParser.h` — `respawnResult` /
  `playerTeleported` wire contract. Spec: `MMO.Teleport.Parser`.
- `UTeleportNetworkHandler` — dispatcher-registered chunk entry (owns both
  eventTypes; `UPlayerManager` keeps only the `respawnRequest` sender).
- `UTeleportManager` — flow orchestration, ack sender, DevMode entry.
- `ABasicPlayer::SnapToAuthoritativePosition` — single snap source of truth
  (also used by `devmode.teleport` + ground snap).

## Server companion (chunk-server-new)

- `teleportAck` (client -> chunk): informational confirm with
  `requestIdEcho` + applied pos. Mismatches vs server memory log error-level.
- Admin teleport: honors requested `rotZ`; seam failures log error-level.
- Skill teleports: authoritative memory + interest resubscribe like the
  respawn path (no stale-cell ghosts).

## QA

- `devmode.scenario teleport` — covered jump + `DEVMODE_BOT_RESULT` tombstone.
- `devmode.teleport X Y Z` — immediate shared-path snap + ground snap.
