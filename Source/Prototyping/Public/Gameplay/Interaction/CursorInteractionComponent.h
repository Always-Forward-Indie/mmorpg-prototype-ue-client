// Copyright Prototyping Project. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Gameplay/Interaction/IWorldInteractable.h"
#include "Gameplay/Interaction/WorldInteractionConfig.h"
#include "CursorInteractionComponent.generated.h"

class ABasicPlayer;
class UTargetDecalComponent;
class UMyGameInstance;

// ─────────────────────────────────────────────────────────────────────────────
// Delegates
// ─────────────────────────────────────────────────────────────────────────────

/** Fired when a confirmed single LMB click lands on an interactable actor. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnInteractableSingleClicked,
    AActor*, Target, EInteractableType, Type);

/** Fired when a confirmed double LMB click lands on an interactable actor. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnInteractableDoubleClicked,
    AActor*, Target, EInteractableType, Type);

/**
 * Fired whenever the cursor moves onto or off an interactable actor.
 * OldTarget and/or NewTarget may be null.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnHoverChanged,
    AActor*, OldTarget, AActor*, NewTarget, EInteractableType, NewType);

// ─────────────────────────────────────────────────────────────────────────────

/**
 * UCursorInteractionComponent
 *
 * Attached to ABasicPlayer.  Owns the full cursor-based world-interaction pipeline:
 *
 *   1. Hover trace   — throttled line-trace from mouse position.  Fires OnHoverChanged
 *                      and applies Hover decal state on the hit interactable actor.
 *
 *   2. Cursor icons  — switches APlayerController::CurrentMouseCursor based on hovered type.
 *                      Supports native EMouseCursor values and hardware cursor assets.
 *
 *   3. Click detect  — LMB click vs. drag detection.  Calls HandleConfirmedClick() only
 *                      when BaSicPlayer::OnLeftMouseReleased confirms a short, stationary press.
 *
 *   4. Double click  — Two consecutive clicks on the same actor within DoubleClickMaxInterval
 *                      fire OnDoubleClicked; a lone click fires OnSingleClicked.
 *
 *   5. Decal mgmt    — Maintains Hover / Locked / Hidden states on decal components of
 *                      all visible interactable actors, respecting the priority order
 *                      Locked > Hover > Hidden.
 *
 * === Setup (one-time) ===
 *   1. Component is already added to ABasicPlayer via CreateDefaultSubobject.
 *   2. Open BP_BasicPlayer → select CursorInteractionComponent in the Components panel.
 *   3. In Details, drag DA_WorldInteractionConfig into the "Config" slot.
 *   4. Done — zero Blueprint graph nodes needed.
 *
 * === Extending ===
 *   Bind to OnSingleClicked / OnDoubleClicked / OnHoverChanged in BasicPlayer::BeginPlay
 *   or from any Blueprint inheriting the player class.
 */
UCLASS(BlueprintType, ClassGroup = "World Interaction",
       meta = (BlueprintSpawnableComponent, DisplayName = "Cursor Interaction"))
class PROTOTYPING_API UCursorInteractionComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UCursorInteractionComponent();

    virtual void BeginPlay()                                                       override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
                               FActorComponentTickFunction* ThisTickFunction)      override;

    // ─────────────────────────────────────────────────────────────────────────
    // Config — assign DA_WorldInteractionConfig here in BP_BasicPlayer defaults
    // ─────────────────────────────────────────────────────────────────────────

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "World Interaction|Config")
    UWorldInteractionConfig* Config = nullptr;

    // ─────────────────────────────────────────────────────────────────────────
    // Public API — called by ABasicPlayer
    // ─────────────────────────────────────────────────────────────────────────

    /**
     * Enable / disable the hover line-trace.
     * Called with false by BasicPlayer when RMB is held (camera drag).
     * Re-enabled when RMB is released.
     */
    UFUNCTION(BlueprintCallable, Category = "World Interaction")
    void SetHoverTraceEnabled(bool bEnabled);

    /**
     * Process a confirmed LMB click (short stationary press detected by BasicPlayer).
     * Updates double-click tracking and broadcasts OnSingleClicked or OnDoubleClicked.
     */
    void HandleConfirmedClick();

    /**
     * Call when BasicPlayer detects that LMB has moved beyond the drag threshold.
     * Resets the double-click chain so drag → release never fires a click.
     */
    void NotifyDragStarted();

    /** Clear hover state (called when UI opens, mouse leaves viewport, etc.). */
    void ForceHoverClear();

    /**
     * Notify that the combat locked target changed.
     * Applies Locked decal on the new actor, removes it from the old one.
     *
     * @param OldLocked  Previous locked actor (may be null).
     * @param NewLocked  Newly locked actor   (may be null if cleared).
     * @param NewType    EInteractableType of NewLocked.
     */
    void NotifyLockedTargetChanged(AActor* OldLocked, AActor* NewLocked,
                                   EInteractableType NewType);

    /**
     * Set a visual-only lock (non-combat: NPC, item, remote player single-click).
     * Does not affect LockedTarget in BasicPlayer.
     * Calling with null clears the visual lock.
     */
    void SetVisualLock(AActor* Actor, EInteractableType Type);

    // ─────────────────────────────────────────────────────────────────────────
    // Queries
    // ─────────────────────────────────────────────────────────────────────────

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "World Interaction")
    AActor* GetHoveredActor() const { return HoveredActor.Get(); }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "World Interaction")
    EInteractableType GetHoveredType() const { return HoveredType; }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "World Interaction")
    AActor* GetVisualLockedActor() const { return VisualLockedActor.Get(); }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "World Interaction")
    float GetInteractionRange() const
    {
        const UWorldInteractionConfig* Cfg = GetEffectiveConfig();
        return Cfg ? Cfg->InteractionRange : 280.f;
    }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "World Interaction")
    float GetItemPickupRange() const
    {
        const UWorldInteractionConfig* Cfg = GetEffectiveConfig();
        return Cfg ? Cfg->ItemPickupRange : 180.f;
    }

    /** Returns the config asset (may be null if not assigned in BP defaults). */
    UWorldInteractionConfig* GetConfig() const { return Config; }

    /**
     * Returns the active config: own Config property if set, otherwise
     * UMyGameInstance::WorldInteractionConfig.  May return null if neither is assigned.
     */
    UWorldInteractionConfig* GetEffectiveConfig() const;

    /**
     * Reads a UTexture2D from an FCursorIconEntry and creates a native hardware
     * cursor handle via ICursor::CreateCursorFromRGBABuffer.
     *
     * Requirements for the source texture:
     *   CompressionSettings = UserInterface2D   (uncompressed BGRA)
     *   MipGenSettings      = NoMipmaps
     *   Never Stream        = true   (Texture → Level Of Detail panel)
     *   Dimensions          ≤ 64×64  (32×32 recommended for normal DPI)
     *
     * Returns nullptr when the texture is null, not CPU-readable, compressed,
     * or the platform does not support pixel-based cursor creation.
     *
     * Declared public static so UMyGameInstance can call it during Init()
     * to preload handles before the first level is loaded.
     */
    static void* BuildCursorHandle(const FCursorIconEntry& Entry);

    // ─────────────────────────────────────────────────────────────────────────
    // Events
    // ─────────────────────────────────────────────────────────────────────────

    UPROPERTY(BlueprintAssignable, Category = "World Interaction|Events")
    FOnInteractableSingleClicked OnSingleClicked;

    UPROPERTY(BlueprintAssignable, Category = "World Interaction|Events")
    FOnInteractableDoubleClicked OnDoubleClicked;

    UPROPERTY(BlueprintAssignable, Category = "World Interaction|Events")
    FOnHoverChanged OnHoverChanged;

private:
    // ─────────────────────────────────────────────────────────────────────────
    // Hover trace
    // ─────────────────────────────────────────────────────────────────────────

    void  RunHoverTrace();
    void  SetHoveredActor(AActor* NewActor);
    static EInteractableType GetTypeFromActor(AActor* Actor);

    bool  bHoverTraceEnabled = true;
    float HoverAccumulator   = 0.f;

    TWeakObjectPtr<AActor> HoveredActor;
    EInteractableType      HoveredType = EInteractableType::None;

    // ─────────────────────────────────────────────────────────────────────────
    // Visual lock tracking (decal state management)
    // ─────────────────────────────────────────────────────────────────────────

    TWeakObjectPtr<AActor> VisualLockedActor;
    EInteractableType      VisualLockedType = EInteractableType::None;

    // ─────────────────────────────────────────────────────────────────────────
    // Cursor icon
    // ─────────────────────────────────────────────────────────────────────────

    void ApplyCursorIcon(EInteractableType Type);
    void ResetCursorIcon();

    /**
     * Pre-loads every cursor texture defined in Config into platform cursor handles
     * (e.g. HCURSOR on Windows).  Called once in BeginPlay.
     * Handles are kept alive for the lifetime of this component.
     */
    void PreloadCursors();

private:

    /** Per-type platform cursor handles built in PreloadCursors().  void* = HCURSOR etc. */
    TMap<EInteractableType, void*> PreloadedCursorHandles;

    /** Handle for the default cursor (no interactable hovered). */
    void* PreloadedDefaultCursorHandle = nullptr;

    EInteractableType LastAppliedCursorType = EInteractableType::None;
    bool bCursorIconDirty = false;

    /**
     * True once we have successfully set PC->CurrentMouseCursor for the first time.
     * ResetCursorIcon() returns early when GetPC() is null (e.g. possession not yet
     * replicated on client in a client-server game).  TickComponent retries every
     * frame until a valid PC is available.
     */
    bool bCursorInitialized = false;

    // ─────────────────────────────────────────────────────────────────────────
    // Click / double-click
    // ─────────────────────────────────────────────────────────────────────────

    float                  LastClickTime  = 0.f;
    TWeakObjectPtr<AActor> LastClickedActor;

    // ─────────────────────────────────────────────────────────────────────────
    // Helpers
    // ─────────────────────────────────────────────────────────────────────────

    // ─────────────────────────────────────────────────────────────────────────
    // Private helpers
    // ─────────────────────────────────────────────────────────────────────────

    void              ApplyDecal(AActor* Actor, ETargetDecalState State, EInteractableType Type) const;
    APlayerController* GetPC() const;
    ETargetDecalState ResolveDecalState(AActor* Actor) const;
};
