// Copyright Prototyping Project. All Rights Reserved.
#include "Gameplay/Interaction/TargetDecalComponent.h"
#include "Gameplay/Interaction/WorldInteractionConfig.h"
#include "Components/DecalComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"

UTargetDecalComponent::UTargetDecalComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    bAutoActivate = true;
    // Decal itself is created lazily in Apply() so the owning actor is fully
    // initialized (RootComponent valid) before we attach to it.
}

void UTargetDecalComponent::BeginPlay()
{
    Super::BeginPlay();
    // Intentionally empty — decal is constructed on demand.
}

void UTargetDecalComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    if (DecalComp)
    {
        DecalComp->DestroyComponent();
        DecalComp = nullptr;
        MID       = nullptr;
    }
    Super::EndPlay(Reason);
}

// ─────────────────────────────────────────────────────────────────────────────
// Public API
// ─────────────────────────────────────────────────────────────────────────────

void UTargetDecalComponent::Apply(ETargetDecalState    NewState,
                                  UWorldInteractionConfig* Config,
                                  EInteractableType    Type)
{
    if (!Config)
    {
        ForceHide();
        return;
    }

    if (NewState == CurrentState) return; // Nothing to do.
    CurrentState = NewState;

    if (NewState == ETargetDecalState::Hidden)
    {
        if (DecalComp) DecalComp->SetVisibility(false);
        return;
    }

    EnsureDecalCreated(Config->DecalMaterial);
    if (!DecalComp) return; // Material not assigned in Config yet — silently skip.

    UpdateMID(Config, Type, NewState);
    DecalComp->SetVisibility(true);
}

void UTargetDecalComponent::ForceHide()
{
    if (DecalComp) DecalComp->SetVisibility(false);
    CurrentState = ETargetDecalState::Hidden;
}

// ─────────────────────────────────────────────────────────────────────────────
// Internals
// ─────────────────────────────────────────────────────────────────────────────

void UTargetDecalComponent::EnsureDecalCreated(UMaterialInterface* BaseMaterial)
{
    if (DecalComp) return;         // Already created.
    if (!BaseMaterial) return;     // No material configured — bail silently.

    AActor* Owner = GetOwner();
    if (!Owner) return;

    DecalComp = NewObject<UDecalComponent>(Owner, TEXT("TargetDecalVFX"));
    DecalComp->RegisterComponent();

    // Attach to the root — place the decal center at the base of the capsule so the
    // projection volume stays below the character mesh and hits only the floor.
    DecalComp->AttachToComponent(
        Owner->GetRootComponent(),
        FAttachmentTransformRules::SnapToTargetNotIncludingScale);

    // Rotate so the decal face points straight down (-Z in decal space = -90 pitch).
    DecalComp->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f));

    // Cache the floor-level Z offset for this owner.
    // For ACharacter: bottom of the capsule (= actual ground contact point).
    // For other actors (items, etc.): root is already at floor level → 0.
    CachedFloorZ = 0.f;
    if (const ACharacter* Char = Cast<ACharacter>(Owner))
    {
        if (const UCapsuleComponent* Cap = Char->GetCapsuleComponent())
        {
            CachedFloorZ = -Cap->GetScaledCapsuleHalfHeight();
        }
    }
    // Initial position — will be corrected immediately by UpdateMID using CachedFloorZ.
    DecalComp->SetRelativeLocation(FVector(0.f, 0.f, CachedFloorZ));

    // Initial extent (overwritten by UpdateMID).
    DecalComp->DecalSize = FVector(40.f, 60.f, 60.f);
    DecalComp->SetVisibility(false);
    // bReceivesDecals was removed in UE5 — no replacement needed.

    // Build the Dynamic Material Instance so we can set Color + Opacity cheaply.
    MID = UMaterialInstanceDynamic::Create(BaseMaterial, DecalComp);
    DecalComp->SetDecalMaterial(MID);
}

void UTargetDecalComponent::UpdateMID(UWorldInteractionConfig* Config,
                                      EInteractableType        Type,
                                      ETargetDecalState        State)
{
    if (!MID || !Config) return;

    const FLinearColor Color   = Config->GetDecalColor(Type);
    const float        HalfExt = (State == ETargetDecalState::Locked)
                                     ? Config->LockedDecalSize * 0.5f
                                     : Config->HoverDecalSize  * 0.5f;
    const float        Opacity  = (State == ETargetDecalState::Locked)
                                     ? Config->LockedOpacity
                                     : Config->HoverOpacity;

    if (DecalComp)
    {
        DecalComp->DecalSize = FVector(Config->DecalDepth, HalfExt, HalfExt);

        // Shift the decal center DOWN by DecalDepth so the top of the projection
        // volume is exactly flush with the floor (CachedFloorZ) and the entire
        // volume projects only below the floor — never into the character mesh above.
        DecalComp->SetRelativeLocation(FVector(0.f, 0.f, CachedFloorZ - Config->DecalDepth));
    }

    MID->SetVectorParameterValue(FName("Color"),   Color);
    MID->SetScalarParameterValue(FName("Opacity"), Opacity);
}
