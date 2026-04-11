// Copyright Prototyping Project. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Gameplay/Interaction/IWorldInteractable.h"
#include "TargetDecalComponent.generated.h"

class UDecalComponent;
class UMaterialInstanceDynamic;
class UWorldInteractionConfig;

/**
 * Visual target-indicator decal projected onto the floor beneath a world-interactable actor.
 *
 * === Overview ===
 * - One UDecalComponent is created lazily on the first Apply() call
 *   (deferred so the owning actor's RootComponent is valid by that point).
 * - Uses a UMaterialInstanceDynamic so Color and Opacity can be changed
 *   without creating additional material assets.
 * - UCursorInteractionComponent calls Apply() whenever hover/lock state changes;
 *   the component itself never polls or ticks.
 *
 * === Setup ===
 * Added automatically via CreateDefaultSubobject in each interactable actor's
 * C++ constructor.  No Blueprint or editor wiring is needed.
 *
 * === Material Parameters ===
 * The material pointed to by WorldInteractionConfig::DecalMaterial must expose:
 *   "Color"   (Vector3/LinearColor) – decal tint
 *   "Opacity" (Scalar, 0–1)        – decal transparency
 *
 * The Rotate animation (if desired) should be baked into the material graph.
 */
UCLASS(BlueprintType, ClassGroup = "World Interaction",
       meta = (BlueprintSpawnableComponent, DisplayName = "Target Decal"))
class PROTOTYPING_API UTargetDecalComponent : public USceneComponent
{
    GENERATED_BODY()

public:
    UTargetDecalComponent();

    virtual void BeginPlay()                                    override;
    virtual void EndPlay(const EEndPlayReason::Type Reason)     override;

    /**
     * Apply a visual state.  Safe to call every frame — skips redundant updates.
     *
     * @param NewState   Target visual state (Hidden / Hover / Locked).
     * @param Config     WorldInteractionConfig DataAsset (holds material, colors, sizes).
     * @param Type       EInteractableType used to look up the decal color from Config.
     */
    void Apply(ETargetDecalState NewState,
               UWorldInteractionConfig* Config,
               EInteractableType Type);

    /** Unconditionally hides the decal.  Useful on actor death / removal. */
    void ForceHide();

    /** Current visual state. */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Target Decal")
    ETargetDecalState GetCurrentState() const { return CurrentState; }

private:
    /** Create the UDecalComponent (called once, lazily, on the first Apply with non-Hidden state). */
    void EnsureDecalCreated(UMaterialInterface* BaseMaterial);

    /** Push Color + Opacity + Size parameters into the MID and resize the decal extent. */
    void UpdateMID(UWorldInteractionConfig* Config,
                   EInteractableType        Type,
                   ETargetDecalState        State);

    // -------------------------------------------------------------------------
    // Internal state
    // -------------------------------------------------------------------------

    /** The actual projected decal.  Null until first Apply(non-Hidden). */
    UPROPERTY()
    UDecalComponent* DecalComp = nullptr;

    /** Dynamic material instance so Color/Opacity can be changed per-frame cheaply. */
    UPROPERTY()
    UMaterialInstanceDynamic* MID = nullptr;

    /** Last applied state — used to skip redundant Apply() calls. */
    ETargetDecalState CurrentState = ETargetDecalState::Hidden;
};
