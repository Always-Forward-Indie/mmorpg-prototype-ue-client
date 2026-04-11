# Cursor World Interaction System
**MMORPG Prototype ·  Unreal Engine 5.3/5.4**

---

## 1. Overview

The Cursor Interaction System provides WoW-style point-and-click targeting with:

| Feature | Detail |
|---|---|
| **Hover highlighting** | Floor-circle decal under the cursor (20 Hz throttled trace) |
| **Single click** | Visual select — shows target frame / nameplate, no action |
| **Double click** | Contextual action — attack / harvest / talk / pick up |
| **LMB drag** | Camera orbit without triggering a click; drag threshold configurable |
| **Cursor icon** | Changes per interactable type (native or hardware cursor) |
| **Auto-approach** | When double-click target is out of range, player walks → acts on arrival |
| **No Blueprint wiring** | All bindings set up in C++ via `HandleUIManagerInitialized` |

---

## 2. Quick Setup (3 Steps)

### Step 1 — Create the Data Asset

1. In the Content Browser, right-click → **Miscellaneous → Data Asset**.
2. Choose `WorldInteractionConfig` as the parent class.
3. Name it `DA_WorldInteraction` (or anything you like).

### Step 2 — Configure the Asset

Open `DA_WorldInteraction` and fill in:

| Section | Property | Notes |
|---|---|---|
| **Decal Material** | `DecalMaterial` | Must expose `Color` (Vector) and `Opacity` (Scalar) parameters. |
| **Decal Colors** | `DecalColors` | Map of `EInteractableType → FLinearColor`. Defaults filled in C++. |
| **Decal Sizes** | `HoverDecalSize`, `LockedDecalSize` | Half-extent of the floor circle in cm. |
| **Cursor Icons** | `InteractionCursors` | Map of type → `FCursorIconEntry`. See §4. |
| **Interaction Ranges** | `HoverTraceRange`, `InteractionRange` | Hover range 2000 cm. Action range 280 cm. |
| **Click Timing** | `DoubleClickMaxInterval`, `ClickMaxDuration`, `DragThresholdPixels` | Defaults are 0.35 s, 0.25 s, 8 px. |

### Step 3 — Assign to BP_BasicPlayer

1. Open **BP_BasicPlayer** → Defaults panel.
2. Find **CursorInteractionComponent** → **Config** → assign `DA_WorldInteraction`.

That's it. No Blueprint graph nodes needed.

---

## 3. Architecture

```
BasicPlayer
│
├─ UCursorInteractionComponent  ← brain of the whole system
│   ├─ Config (UWorldInteractionConfig)
│   ├─ TickComponent  → DoHoverTrace() at 20 Hz
│   ├─ HandleConfirmedClick()  → fires OnSingleClicked / OnDoubleClicked delegates
│   ├─ NotifyLockedTargetChanged()  ← called from SetLockedTarget / ClearLockedTarget
│   └─ SetVisualLock()  ← visual-only lock for NPC / Item / RemotePlayer
│
├─ UTargetDecalComponent  ← sits on THIS player (remote player target)
│
├─ DispatchCursorSelect()   ← bound to OnSingleClicked
└─ DispatchCursorInteract() ← bound to OnDoubleClicked

ABasicMOB       ─ UTargetDecalComponent (TargetDecal)
ABasicNPC       ─ UTargetDecalComponent (TargetDecal)
ADroppedItemActor ─ UTargetDecalComponent (TargetDecal)
ABasicPlayer    ─ UTargetDecalComponent (TargetDecal)  ← for remote player selection
```

### Component Responsibilities

| Class | File | Responsibility |
|---|---|---|
| `IWorldInteractable` | `Public/Gameplay/Interaction/IWorldInteractable.h` | Interface implemented by all interactable actors. Provides `GetInteractableType()`, `GetInteractableDisplayName()`, `CanInteract()`. |
| `UWorldInteractionConfig` | `Public/…/WorldInteractionConfig.h` | DataAsset — single place to tweak all interaction parameters: decal colors/sizes, cursor icons, timing, ranges. |
| `UTargetDecalComponent` | `Public/…/TargetDecalComponent.h` | Attaches to an actor. Lazily creates a `UDecalComponent` floor circle. Managed via `Apply(state, config, type)`. |
| `UCursorInteractionComponent` | `Public/…/CursorInteractionComponent.h` | Attached to `ABasicPlayer`. Runs the hover trace, click/double-click detection, cursor icon changes, and calls `Apply()` on the target's `UTargetDecalComponent`. |

---

## 4. EInteractableType Reference

| Enum Value | Actor | Cursor | Single Click | Double Click |
|---|---|---|---|---|
| `MOB_Alive` | Living `ABasicMOB` | Sword / Attack | Select (lock) | Approach + auto-attack |
| `MOB_Harvestable` | Dead, not yet harvested | Harvest knife | Select (lock) | Approach + harvest |
| `MOB_Harvested` | Already harvested | Inspect bag | Select (lock) | Inspect loot |
| `NPC` | `ABasicNPC` | Chat bubble | Visual lock | Approach + open dialogue |
| `DroppedItem` | `ADroppedItemActor` | Hand / loot | Visual lock | Approach + pick up |
| `RemotePlayer` | Other `ABasicPlayer` | Info cursor | Visual lock | Inspect (placeholder) |
| `None` | Empty ground | Default cursor | Clear all locks | — |

---

## 5. Decal Material Setup

The system uses a single `UMaterialInstanceDynamic` per actor. Your material **must** expose:

| Parameter Name | Type | Purpose |
|---|---|---|
| `Color` | Vector (LinearColor) | Ring tint — changes per `EInteractableType` |
| `Opacity` | Scalar (float) | 0.25 on hover, 0.85 when locked |

### Minimal Material Recipe (M_TargetDecal)

1. Create a new **Decal** material (`Material Domain = Deferred Decal`, `Blend Mode = Translucent`).
2. Add a **VectorParameter** named exactly `Color`.
3. Add a **ScalarParameter** named exactly `Opacity`.
4. Connect `Color` → Emissive Color, `Opacity` → Opacity.
5. Optionally add a circular mask texture to get a clean ring.

Assign `M_TargetDecal` (or its instance) to **DecalMaterial** in `DA_WorldInteraction`.

---

## 6. Cursor Icon Configuration

`FCursorIconEntry` has two flavors:

### Native cursor (EMouseCursor values)

```cpp
FCursorIconEntry entry;
entry.CursorType = EMouseCursor::GrabHandClosed;
```

### Hardware cursor (custom bitmap)

```cpp
entry.CursorType = EMouseCursor::Custom;
entry.HardwareCursorPath = FName("UI/Cursors/AttackCursor");
entry.HotSpot = FVector2D(0.5f, 0.5f);
```

For hardware cursors, export a 32×32 PNG to `Content/UI/Cursors/` and reference its asset path (without Content/) .

---

## 7. Click FSM

```
LMB PRESSED
    │
    ├─ bLMBDragActive = false
    │   LMBPressTime = now
    │   LMBDragPixelsAccum = 0
    │
    ▼  (mouse moves while held)
LOOK()
    │
    ├─ LMBDragPixelsAccum += delta
    │
    ├─ if accum >= DragThresholdPixels
    │       bLMBDragActive = true
    │       ApplyMouseCaptureIfNoUIOpen()
    │       CursorInteractionComponent->NotifyDragStarted()
    │
    ▼
LMB RELEASED
    │
    ├─ bLMBDragActive == true
    │       → drag rotation ended; cursor restored; double-click chain reset
    │
    └─ bLMBDragActive == false
            → CursorInteractionComponent->HandleConfirmedClick()
                    │
                    ├─ double-click window open && same actor?
                    │       → OnDoubleClicked.Broadcast(target, type)
                    │               → DispatchCursorInteract()
                    │
                    └─ else
                            → OnSingleClicked.Broadcast(target, type)
                                    → DispatchCursorSelect()
```

---

## 8. EPendingInteraction — Auto-approach Flow

When `DispatchCursorInteract()` is called but the target is out of `InteractionRange`:

```
DispatchCursorInteract(Target, Type)
    │
    └─ Dist > InteractionRange
            PendingInteraction = <type>
            PendingInteractionTarget = Target
            bIsApproachingTarget = true
                    │
                    ▼  (every Tick)
            UpdateApproach()
                    │
                    ├─ moves toward PendingInteractionTarget
                    │
                    └─ Dist <= InteractionRange
                            bIsApproachingTarget = false
                            DispatchPendingInteraction()
                                    │
                                    ├─ AutoAttack → bIsAutoAttacking + DoAutoAttack()
                                    ├─ Harvest    → HarvestManager::TryHarvestSpecificCorpse()
                                    ├─ TalkNPC    → DialogueManager::OpenDialogue()
                                    └─ PickupItem → InventoryManager::PickupSpecificItem()
```

Non-combat pending approach uses its own target (`PendingInteractionTarget`), completely separate from the combat locked target (`LockedTarget`). This means the player can approach an NPC even while still having a combat LockedTarget.

---

## 9. Decal State Priority

```
Hidden < Hover < Locked
```

- **Hidden** — no decal visible.
- **Hover** — cursor is over the actor, no lock.
- **Locked** — actor is the combat locked target (`LockedTarget`) OR a visual lock (NPC/Item/RemotePlayer single-click).

`CursorInteractionComponent` enforces priority: hovering over a Locked actor will not downgrade its decal to Hover.

---

## 10. API Reference

### `UCursorInteractionComponent` — called from BasicPlayer

| Method | When |
|---|---|
| `SetHoverTraceEnabled(bool)` | RMB pressed (false) / released (true) |
| `HandleConfirmedClick()` | OnLeftMouseReleased when not dragging |
| `NotifyDragStarted()` | LMB drag threshold exceeded, or after drag ends |
| `ForceHoverClear()` | When UI opens (optional — trace auto-suppresses) |
| `NotifyLockedTargetChanged(old, new, type)` | Inside `SetLockedTarget` / `ClearLockedTarget` |
| `SetVisualLock(actor, type)` | Single-click on NPC / Item / RemotePlayer (null = clear) |
| `GetHoveredActor()` | Query current hover — usable in Blueprint |
| `GetInteractionRange()` | Returns config value (280 cm default) |
| `GetConfig()` | Returns `UWorldInteractionConfig*` (may be null) |

### `UTargetDecalComponent` — called from `CursorInteractionComponent`

| Method | Purpose |
|---|---|
| `Apply(state, config, type)` | Show/hide/color decal. Config passed in each call (no stored ref). |
| `ForceHide()` | Unconditional hide (e.g. actor death). |

### `ABasicPlayer` — new dispatch methods

| Method | Purpose |
|---|---|
| `DispatchCursorSelect(Actor*, Type)` | Bound to `OnSingleClicked`. |
| `DispatchCursorInteract(Actor*, Type)` | Bound to `OnDoubleClicked`. |
| `DispatchPendingInteraction()` | Called by `UpdateApproach` on arrival. |
| `GetInteractionRange()` | Returns `CursorInteractionComponent->GetInteractionRange()`. |
| `IsUIBlockingInteraction()` | True when a UI window is open (forwarded to component). |

---

## 11. Extending the System

### Adding a new interactable type

1. Add a value to `EInteractableType` in `IWorldInteractable.h`.
2. Implement `IWorldInteractable` on the new actor class.
3. Add a `UTargetDecalComponent` to the actor's constructor.
4. Add a `DecalColors` entry in `DA_WorldInteraction`.
5. Add a `InteractionCursors` entry in `DA_WorldInteraction`.
6. Add a `case` in `ABasicPlayer::DispatchCursorSelect()` and `DispatchCursorInteract()`.
7. Add an `EPendingInteraction` value if approach logic is needed.

### Changing interaction ranges per type

Override `CanInteract()` to return false when out of a custom range, or extend `DispatchCursorInteract` to compute a per-type range.

### Disabling the system for a specific actor

Either don't implement `IWorldInteractable` (actor will be invisible to the hover trace), or override `CanInteract() const` to return false (hover/decal will still work but single/double click will do nothing).

---

## 12. File Index

| File | Purpose |
|---|---|
| `Public/Gameplay/Interaction/IWorldInteractable.h` | Interface + `EInteractableType` + `ETargetDecalState` enums |
| `Public/Gameplay/Interaction/WorldInteractionConfig.h` | DataAsset: all configuration |
| `Private/Gameplay/Interaction/WorldInteractionConfig.cpp` | Default decal colors |
| `Public/Gameplay/Interaction/TargetDecalComponent.h` | Floor-circle decal component |
| `Private/Gameplay/Interaction/TargetDecalComponent.cpp` | MID management, lazy creation |
| `Public/Gameplay/Interaction/CursorInteractionComponent.h` | Main brain component header |
| `Private/Gameplay/Interaction/CursorInteractionComponent.cpp` | Hover trace, click detection, cursor icons, decal dispatch |
