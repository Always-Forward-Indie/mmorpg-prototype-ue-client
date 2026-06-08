# AGENTS.md — Prototyping MMORPG Client (UE 5.7)

## Architecture

- **NOT Unreal replication.** All networking is custom TCP with JSON-encoded packets over raw sockets. Three persistent connections: **Login Server** (auth), **Game Server** (coordination), **Chunk Server** (movement/combat/persistence). See `Source/Prototyping/Public/Networking/NetworkManager.h`.
- **Service locator pattern.** `UMyGameInstance` (BP subclass `BP_MyGameInstance_C` wired in `DefaultEngine.ini`) creates and owns all ~30 manager objects in `Init()`. Every system retrieves its manager from the GameInstance. Each system follows **Manager + NetworkHandler** pairs.
- **Level entry point** is `/Game/Maps/MainGameContainerLevel`. Level transitions use a 4-phase loading screen gate with ready-flags bitmask before hiding.
- **Single runtime module**: `Prototyping`. Module source in `Source/Prototyping/`. `Public/` and `Private/` mirror each other's subdirectory structure.

## Build & Run

- **Engine**: UE 5.7 at `D:\Game Dev\UE\UE_5.7`. Both `.Target.cs` files set `bOverrideBuildEnvironment = true` and `BuildSettingsVersion.Latest`.
- **Build module**: `Prototyping` (Game + Editor targets). Editor target pulls `UnrealEd` conditionally.
- **Multiplayer testing**: `LaunchClients.ps1 -n 3` or `LaunchClients.bat 3` — launches standalone clients via `UnrealEditor.exe -game` (no build needed). Defaults to 2 clients, 960×540, map `/Game/Maps/MainGameContainerLevel` (PS1) or `/Game/Maps/WorldMapV1` (BAT). Hardcoded UE path in both scripts.
- **Server config** in `server_config.json` — three localhost ports (27014, 27016, 27017). The `mmo servers/` directory contains separate server repos.
- **`.uproject`** enables PCG and ModelingToolsEditorMode plugins. Custom plugin `WorldMapExporter/` is editor-only (map screenshot capture tool).

## Dev Mode (offline testing)

- Configured via `FDevModeConfig` on the GameInstance Blueprint. Set `bEnabled = true` to skip login and load directly to a level.
- JSON configs in `Config/DevMode/` — `dev_player.json` (stats), `dev_inventory.json` (starter items), `dev_mobs.json` (test mobs).
- Console commands registered at runtime via `UDevModeConsoleCommands`.

## Documentation

- **C++ dev guides**: `Source/Prototyping/Documentation/` — 47 markdown files covering individual systems (combat, skills, UI, drag-drop, time sync, nameplates, etc.).
- **Server protocol docs**: `Documentation/Server Info/API/` — numbered protocol specs (00–11) for client-server communication.

## Logging

Three project-wide log categories defined in `Source/Prototyping/Prototyping.h`:
- `LogConnection` — login, world load, disconnect flow
- `LogNetPacket` — raw send/receive packet dumps
- `LogPing` — ping / time-sync noise

Verbosity per-category configured in `Config/DefaultEngine.ini` `[Core.Log]` without recompilation.

## Rendering & Config

- DX12, SM6, Lumen GI, Virtual Shadow Maps, Mesh Distance Fields. PSO caching enabled. Niagara GPU particles enabled. See `Config/DefaultEngine.ini`.
- Enhanced Input system (input actions in Blueprints, `DefaultPlayerInputClass=/Script/EnhancedInput.EnhancedPlayerInput`).
- Dynamic nav mesh with two agent types: "small" (radius 60) and "big" (radius 220).
- Naming conventions enforced via `.editorconfig` — UE standard `U`/`A`/`F`/`E`/`T`/`S`/`b` prefixes.
