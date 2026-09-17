# AGENTS.md — Prototyping MMORPG Client (UE 5.8; folder name `Prototyping 5.3` is stale)

Single `Prototyping` runtime module. No Unreal replication — custom TCP + JSON over raw sockets (`Source/Prototyping/Public/Networking/NetworkManager.h`). `UMyGameInstance` (BP `BP_MyGameInstance_C`, wired in `Config/DefaultEngine.ini`) creates and owns all managers in `Init()`; systems are Manager + NetworkHandler pairs reached via `Get*Manager()`.

## Build & run

- Engine: UE 5.8 at `D:\Game Dev\UE\UE_5.8`. Both `.Target.cs` set `bOverrideBuildEnvironment=true`, `BuildSettingsVersion.Latest`. `Public/` and `Private/` mirror subdirectories; `UnrealEd` dep only when `bBuildEditor` (`Source/Prototyping/Prototyping.Build.cs`).
- Multi-client: `.\LaunchClients.ps1 -n 3` (preferred). Uses `UnrealEditor.exe -game`, no build, Live Coding OK, 960×540 grid, 1s stagger for socket init. Entry map `/Game/Maps/MainGameContainerLevel`. `LaunchClients.bat` loads a **different** map (`/Game/Maps/WorldMapV1`) — don't treat them as identical.
- `.uproject` enables only PCG + ModelingToolsEditorMode (editor-only). `Plugins/WorldMapExporter/` is editor-only screenshot tool.

## Networking & level transitions (crash-prone)

- Three persistent sockets: Login (auth) / Game (coordination) / Chunk (movement, combat, persistence). Ports 27014 / 27016 / 27017.
- `ClientVersion` (`0.1.0` SemVer on GameInstance) is sent for server compat check — bump deliberately.
- Never hold `UWorld*` across `OpenLevel`. `PreLoadMap` → `InvalidateManagerWorldContexts()` nulls them; `PostLoadMapWithWorld` → `RefreshManagerWorldContexts()` re-seeds. Follow this in any new manager.
- Loading screen (MoviePlayer, survives world teardown) hides only when: `ReadyFlags` bitmask complete (`PlayerReadyAck | UIInitialized | PlayerSpawned`) **plus** render-frame gate (12 frames editor / 15 packaged) **plus** 15s `LoadingScreenSafetyTimeout` fallback. Check `NotifyPlayerSpawned/NotifyUIInitialized/NotifyPlayerReadyAck` before touching this flow.

## Server config

- `server_config.json` is **gitignored**. Create it via `Copy-Item server_config_dev.json server_config.json` (127.0.0.1) or `server_config_live.json` (23.88.102.182). Never commit the live IP file as `server_config.json`.
- `mmo servers/` is gitignored **stale snapshot for reading only — never edit**. Server truth lives in WSL: `\\wsl.localhost\Ubuntu\home\shardanov\projects\mmorpg-prototype` (login / game / chunk-server-new repos, `docker-compose.dev.yml`, bring up login → game → chunk). Dev access from Windows is via `localhost:27014/27016/27017`. **Never run tests against live VPS.**

## Task tracker (own, no MCP — use docs link)

- Base `http://23.88.102.182:3005`, OpenAPI `/api/docs-json`. Auth: `X-API-Key` from `api_key.env` (gitignored, never commit), scoped per project.
- Tasks = `items`: `GET|POST /api/v1/projects/{projectSlug}/items`, `.../items/{sequenceNum|id}`, plus `.../comments`, `.../attachments`, `.../search`. Create requires `[title, typeId]`.
- Bug template: title + `ClientVersion` + WSL server commit + `requestId (sync_*)` + `Saved/Logs` excerpt + `Saved/Replays/*.jsonl` when from bots. Flow: search duplicates → create → attach.

## Automated tests (see Tools/Tests/README.md)

- `Tests/Contract/` (pytest, no engine): `mmo_proto.py` TCP+`\n` client, `test_framing.py` (no server), `test_conn/test_combat/test_cross_visibility/test_harvest/test_trade.py` (need WSL dev servers + `MMO_*`/`MMO2_*` creds, skip cleanly otherwise).
- `Tools/Bots/run_swarm.py --n 8 --scenario patrol|combat_sweep|chat_mesh|kill|harvest|death` + `--scenario trade` (duo pairs, even --n) + `seed_bots.py` (bot_* accounts) + `--tap` for `Tools/Replay/replay.py`.
- `Tools/WSL/Preflight.ps1` (WSL→containers→ports→dev config), `Tools/Smoke/SmokeClients.ps1 -n 2` (real UE clients + log scan).
- `Source/Prototyping/Private/Tests/` (`MMO.*` Automation specs) + `devmode.scenario combat|reset` + `Config/DevMode/qa_presets.json` for offline QA.

## DevMode (offline, no servers)

- Enable `FDevModeConfig.bEnabled` on the GameInstance Blueprint to skip login. Data: `Config/DevMode/dev_{player,inventory,mobs}.json`. Runtime commands via `UDevModeConsoleCommands`.

## Docs & logging

- System guides: `Source/Prototyping/Documentation/` (~47 files). Protocol truth: `Documentation/Server Info/API/` numbered specs.
- Log categories (`Source/Prototyping/Prototyping.h`): `LogConnection` / `LogNetPacket` / `LogPing`. Verbosity in `Config/DefaultEngine.ini [Core.Log]` (`LogConnection=Log`, `LogNetPacket=Verbose`, `LogPing=NoLogging`) — tune INI, don't recompile.

## Config quirks & ignored paths

- DX12/SM6, Lumen, VSM, Mesh Distance Fields, PSO cache, Niagara GPU — see `DefaultEngine.ini`. Don't downgrade RHI to "fix" shader issues.
- Input is Enhanced Input in Blueprints (`DefaultPlayerInputClass=/Script/EnhancedInput.EnhancedPlayerInput`); `DefaultInput.ini` is legacy axes only.
- Navmesh is `Dynamic` with agents `small` (r=60) / `big` (r=220).
- `.editorconfig` enforces `U/A/S/F/E/T/b` prefixes for C++.
- Gitignored, often absent locally: `Binaries/ Intermediate/ Saved/ DerivedDataCache/ Build/`, most `Content/` packs (meshes, audio, Polygon*, Vefects, etc.), `server_config.json`. Don't edit or recreate them; don't `git add` them.
- No CI (`.github/workflows/` empty), no lint/test runner; `Source/Prototyping/Private/Tests/NPCSystemTest.cpp` is an empty stub. Verify via editor compile + multi-client run.
