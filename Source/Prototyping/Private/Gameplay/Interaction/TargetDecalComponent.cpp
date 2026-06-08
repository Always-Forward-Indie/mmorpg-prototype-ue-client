// Copyright Prototyping Project. All Rights Reserved.
#include "Gameplay/Interaction/TargetDecalComponent.h"
#include "Gameplay/Interaction/WorldInteractionConfig.h"
#include "Components/DecalComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"

static constexpr float FloorInterpSpeed = 15.f;

UTargetDecalComponent::UTargetDecalComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 0.05f;
    bAutoActivate = true;
}

void UTargetDecalComponent::BeginPlay()
{
    Super::BeginPlay();
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

void UTargetDecalComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                          FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!DecalComp || !DecalComp->IsVisible() || !KnownConfig)
    {
        return;
    }

    UpdateFloorTrace();
    CachedFloorZ = FMath::FInterpTo(CachedFloorZ, TargetFloorZ, DeltaTime, FloorInterpSpeed);

    const float DecalCenterZ = CachedFloorZ - KnownConfig->DecalDepth * 0.5f;
    DecalComp->SetRelativeLocation(FVector(0.f, 0.f, DecalCenterZ));
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

    if (NewState == CurrentState) return;
    CurrentState = NewState;

    if (NewState == ETargetDecalState::Hidden)
    {
        if (DecalComp) DecalComp->SetVisibility(false);
        return;
    }

    EnsureDecalCreated(Config->DecalMaterial);
    if (!DecalComp) return;

    KnownConfig = Config;
    UpdateFloorTrace();
    CachedFloorZ = TargetFloorZ;
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
    if (DecalComp) return;
    if (!BaseMaterial) return;

    AActor* Owner = GetOwner();
    if (!Owner) return;

    DecalComp = NewObject<UDecalComponent>(Owner, TEXT("TargetDecalVFX"));
    DecalComp->RegisterComponent();

    DecalComp->AttachToComponent(
        Owner->GetRootComponent(),
        FAttachmentTransformRules::SnapToTargetNotIncludingScale);

    DecalComp->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f));

    TargetFloorZ = 0.f;
    if (const ACharacter* Char = Cast<ACharacter>(Owner))
    {
        if (const UCapsuleComponent* Cap = Char->GetCapsuleComponent())
        {
            TargetFloorZ = -Cap->GetScaledCapsuleHalfHeight();
        }
    }
    CachedFloorZ = TargetFloorZ;

    DecalComp->SetRelativeLocation(FVector(0.f, 0.f, TargetFloorZ));
    DecalComp->DecalSize = FVector(40.f, 60.f, 60.f);
    DecalComp->SetVisibility(false);

    MID = UMaterialInstanceDynamic::Create(BaseMaterial, DecalComp);
    DecalComp->SetDecalMaterial(MID);
}

void UTargetDecalComponent::UpdateFloorTrace()
{
    AActor* Owner = GetOwner();
    if (!Owner) return;

    UWorld* World = GetWorld();
    if (!World) return;

    const FVector RootLoc = Owner->GetActorLocation();
    const FVector Start   = RootLoc + FVector(0.f, 0.f, 200.f);
    const FVector End     = RootLoc - FVector(0.f, 0.f, 600.f);

    FCollisionQueryParams Params;
    Params.bTraceComplex = true;
    Params.AddIgnoredActor(Owner);

    for (TActorIterator<APawn> It(World); It; ++It)
    {
        Params.AddIgnoredActor(*It);
    }

    FHitResult Hit;
    if (World->LineTraceSingleByChannel(Hit, Start, End, ECC_WorldStatic, Params))
    {
        TargetFloorZ = Hit.ImpactPoint.Z - RootLoc.Z;
    }
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
    const float        Opacity = (State == ETargetDecalState::Locked)
                                     ? Config->LockedOpacity
                                     : Config->HoverOpacity;

    if (DecalComp)
    {
        DecalComp->DecalSize = FVector(Config->DecalDepth, HalfExt, HalfExt);

        const float DecalCenterZ = CachedFloorZ - Config->DecalDepth * 0.5f;
        DecalComp->SetRelativeLocation(FVector(0.f, 0.f, DecalCenterZ));
    }

    MID->SetVectorParameterValue(FName("Color"),   Color);
    MID->SetScalarParameterValue(FName("Opacity"), Opacity);
}
