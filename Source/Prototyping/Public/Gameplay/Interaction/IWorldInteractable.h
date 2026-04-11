// Copyright Prototyping Project. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "IWorldInteractable.generated.h"

/**
 * Interactable object type.
 * Drives cursor icon selection, decal color, and interaction dispatch inside
 * UCursorInteractionComponent and ABasicPlayer::DispatchCursorInteract().
 */
UENUM(BlueprintType)
enum class EInteractableType : uint8
{
    None             UMETA(DisplayName = "None"),
    MOB_Alive        UMETA(DisplayName = "MOB – Alive"),
    MOB_Harvestable  UMETA(DisplayName = "MOB – Harvestable Corpse"),
    MOB_Harvested    UMETA(DisplayName = "MOB – Already Harvested"),
    NPC              UMETA(DisplayName = "NPC"),
    DroppedItem      UMETA(DisplayName = "Dropped Item"),
    RemotePlayer     UMETA(DisplayName = "Remote Player"),
};

/**
 * Visual state of the target indicator projected on the floor.
 * Priority: Locked > Hover > Hidden.
 * CursorInteractionComponent always applies the highest-priority applicable state.
 */
UENUM(BlueprintType)
enum class ETargetDecalState : uint8
{
    Hidden  UMETA(DisplayName = "Hidden"),
    Hover   UMETA(DisplayName = "Hover"),
    Locked  UMETA(DisplayName = "Locked Target"),
};

UINTERFACE(MinimalAPI, Blueprintable)
class UWorldInteractable : public UInterface
{
    GENERATED_BODY()
};

/**
 * Interface implemented by every world object the cursor can hover and interact with.
 *
 * Implementing classes: ABasicMOB, ABasicNPC, ADroppedItemActor, ABasicPlayer (remote).
 *
 * Design contract:
 *  - GetInteractableType()    → type tag for cursor icons and dispatch routing
 *  - GetInteractableDisplayName() → text shown in tooltip / target frame
 *  - CanInteract()            → whether a double-click action is currently meaningful
 *
 * Decal rendering is intentionally NOT part of this interface.
 * UCursorInteractionComponent locates UTargetDecalComponent via FindComponentByClass
 * and calls Apply() directly, keeping the interface lean.
 */
class PROTOTYPING_API IWorldInteractable
{
    GENERATED_BODY()

public:
    /** Returns the type tag used for cursor icon selection and interaction dispatch. */
    virtual EInteractableType GetInteractableType() const = 0;

    /**
     * Display name shown in hover tooltip and any target frame widget.
     * Override to provide localized text.
     */
    virtual FText GetInteractableDisplayName() const = 0;

    /**
     * Whether a double-click (action) interaction is currently meaningful.
     * Single-click (select / visual lock) is always allowed if the interface is present.
     * Default: true.
     */
    virtual bool CanInteract() const { return true; }
};
