# World Interactive Objects (WIO) — Setup Guide

## Overview

The WIO system allows players to interact with world objects (chests, altars, switches, loot nodes, etc.).  
It follows the same Manager + NetworkHandler pattern as NPC, MOB, and other existing systems.

---

## Architecture

```
Server (Chunk)
  └─ spawnWorldObjects / worldObjectInteractResult / worldObjectStateUpdate / worldObjectChannelCancelled
        │
        ▼
WIONetworkHandler  ──►  WorldObjectManager  ──►  WorldInteractiveObjectActor (spawned in world)
        │                       │                        │
        │                       ▼                        ▼
        │               UIManager bindings       OnProximityChanged → BasicPlayer
        │               OnInteractResult             │
        │               OnChannelCancelled           ▼
        │                                    WIOInteractionPromptWidget
        │                                    WIOChannelBarWidget
        ▼
    JSONParser (deserialization)
```

---

## Files Created

| File | Description |
|------|-------------|
| `Public/Data/WIODataStructs.h` | Enums, structs, helpers (FWorldObjectData, FWIOInteractResult, etc.) |
| `Public/Gameplay/WorldObjects/WorldObjectManager.h/.cpp` | Core manager: spawning, registry, client→server requests |
| `Public/Gameplay/WorldObjects/WIONetworkHandler.h/.cpp` | Routes chunk-server packets to WorldObjectManager |
| `Public/Gameplay/WorldObjects/WorldInteractiveObjectActor.h/.cpp` | In-world actor with sphere overlap, state machine |
| `Public/UI/WIOInteractionPromptWidget.h/.cpp` | HUD prompt when near a WIO |
| `Public/UI/WIOChannelBarWidget.h/.cpp` | Channel/cast progress bar |

## Files Modified

| File | Changes |
|------|---------|
| `Public/Utils/JSONParser.h/.cpp` | Added WIO deserialization methods |
| `Public/Data/LocalizationDataAsset.h` | Added `WorldObjectLocale` data table reference |
| `Public/Services/LocalizationSubsystem.h/.cpp` | Added WIO locale getters |
| `Public/MyGameInstance.h/.cpp` | Added WorldObjectManager + WIONetworkHandler creation/wiring |
| `Public/UI/UIManager.h/.cpp` | Added WIO widget classes, creation, delegate bindings |
| `Public/Gameplay/Players/BasicPlayer.h/.cpp` | Added WIO interaction input, proximity tracking, channel cancellation |

---

## Blueprint Setup (Step-by-Step)

### 1. DataTable: WIO Definitions (Optional)

Create a **DataTable** with Row Structure = `FWIODefinitionRow`:

| Row Name (=slug) | ActorClass | MeshOverride | MaterialOverride | InteractionIcon |
|---|---|---|---|---|
| `ancient_altar` | BP_AncientAltar (your BP actor) | SM_Altar | — | T_AltarIcon |
| `treasure_chest` | BP_TreasureChest | SM_Chest | — | T_ChestIcon |

- **Row Name** must match the server's `slug` field exactly
- **ActorClass** must be derived from `AWorldInteractiveObjectActor` (or leave empty to use default)
- This table is optional — without it, all objects use `DefaultActorClass`

### 2. DataTable: WIO Locale (Optional)

Create a **DataTable** with Row Structure = `FWIOLocaleDefinition`:

| Row Name (=nameKey) | DisplayName | Description | InteractionPrompt |
|---|---|---|---|
| `wio.ancient_altar` | Ancient Altar | A mysterious altar... | [F] Activate |
| `wio.treasure_chest` | Treasure Chest | Contains valuable loot | [F] Open |

### 3. GameInstance Blueprint (BP_GameInstance)

In your GameInstance Blueprint's Details panel:

1. **WIO Definition Table** → Assign the WIO Definitions DataTable (step 1)
2. **Default Actor Class** → Set on `WorldObjectManager` component:
   - Open BP_GameInstance → find `WorldObjectManager` property
   - Set `Default Actor Class` = your default WIO actor BP (e.g. `BP_WorldInteractiveObjectActor`)

### 4. Localization Data Asset

Open your **LocalizationDataAsset** (the one assigned in `LocalizationSubsystem`):

- **World Object Locale** → Assign the WIO Locale DataTable (step 2)

### 5. UIManager Blueprint (on BasicPlayer)

In your Player Blueprint's UIManager component:

1. **WIO Interaction Prompt Widget Class** → Create a Widget Blueprint (WBP) based on `WIOInteractionPromptWidget`:
   - Add a TextBlock or custom layout
   - Implement `OnPromptUpdated` event to update your visuals
   - Widget receives: `ObjectData`, `DisplayName`, `PromptText`

2. **WIO Channel Bar Widget Class** → Create a WBP based on `WIOChannelBarWidget`:
   - **Required**: Add `ProgressBar` named exactly `ChannelProgressBar`
   - **Optional**: Add `TextBlock` named `ChannelNameText` (object name)
   - **Optional**: Add `TextBlock` named `ChannelTimeText` (countdown)
   - Implement `OnChannelStarted`, `OnChannelCompleted`, `OnChannelCancelled` for animations

### 6. Default WIO Actor Blueprint

Create a Blueprint derived from `AWorldInteractiveObjectActor`:

1. **Static Mesh** → Set a default mesh (e.g. a glowing orb, cube, etc.)
2. Override BP events for visual feedback:
   - `OnStateChanged(NewState, OldState)` — change material, play VFX
   - `OnInteractionSuccess(Result)` — play success VFX, sound
   - `OnInteractionFailed(ErrorCode)` — play fail sound
   - `OnRespawned()` — play respawn VFX

---

## Input Binding

The system reuses the existing **InteractAction** (F key by default):

- **F key** near a WIO → sends `worldObjectInteract` to server
- **F key** while channeling → cancels the channel
- **Escape** while channeling → cancels the channel
- **Any movement (WASD)** while channeling → cancels the channel
- **Walking out of interaction range** → cancels channel + hides prompt

No additional Input Action or Input Mapping Context changes are needed.

---

## Server Integration

The system expects these server packets:

### Server → Client
| Event Type | Description |
|---|---|
| `spawnWorldObjects` | Initial spawn of all WIO objects (body.worldObjects array) |
| `worldObjectInteractResult` | Result of an interaction (success/fail, loot, channel start) |
| `worldObjectStateUpdate` | Object state change (active/depleted/disabled) |
| `worldObjectChannelCancelled` | Server confirms channel cancellation |

### Client → Server
| Event Type | Description |
|---|---|
| `worldObjectInteract` | Player wants to interact (body: characterId, objectId) |
| `worldObjectChannelCancel` | Player cancels a channel (body: characterId, objectId) |

---

## Interaction Flow

1. Player enters WIO actor's `InteractionSphere` → prompt widget appears
2. Player presses F → `WorldObjectManager::RequestInteract(objectId)` → packet sent
3. Server responds with `worldObjectInteractResult`:
   - **Examine** → immediate success, shows dialogue/text
   - **Search** → immediate success, loot items array
   - **Channeled** → success with `channelTimeSec` > 0, channel bar appears
   - **channeled_complete** → channel finished, success
4. Object state may change to `Depleted` → actor updates visuals
5. After `respawnSec`, object returns to `Active` via `worldObjectStateUpdate`

---

## Troubleshooting

- **No prompt appears**: Check that `WIOInteractionPromptWidgetClass` is set in UIManager
- **F key doesn't work**: Ensure `InteractAction` Input Action is assigned in BasicPlayer BP
- **Objects don't spawn**: Check `WorldObjectManager` logs, verify `DefaultActorClass` is set
- **No localization**: Check that `WorldObjectLocale` DataTable is assigned in LocalizationDataAsset
- **Channel bar missing**: Check that `WIOChannelBarWidgetClass` is set, and WBP has `ChannelProgressBar` ProgressBar
