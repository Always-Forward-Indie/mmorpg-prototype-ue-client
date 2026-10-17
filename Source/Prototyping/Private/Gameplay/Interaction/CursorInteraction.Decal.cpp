// Copyright Prototyping Project. All Rights Reserved.
#include "Gameplay/Interaction/CursorInteractionComponent.h"
#include "Gameplay/Interaction/WorldInteractionConfig.h"
#include "Gameplay/Interaction/TargetDecalComponent.h"
#include "Gameplay/Mobs/BasicMOB.h"
#include "Gameplay/NPCs/BasicNPC.h"
#include "Gameplay/Items/DroppedItemActor.h"
#include "Gameplay/Players/BasicPlayer.h"
#include "MyGameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Texture2D.h"
#include "GenericPlatform/ICursor.h"
#include "CrashDiagnostics.h"
#include "Framework/Application/SlateApplication.h"

void UCursorInteractionComponent::ApplyDecal(AActor* Actor,
                                              ETargetDecalState State,
                                              EInteractableType Type) const
{
    if (!Actor) return;
    UWorldInteractionConfig* Cfg = GetEffectiveConfig();
    if (!Cfg) return;
    if (UTargetDecalComponent* Decal = Actor->FindComponentByClass<UTargetDecalComponent>())
    {
        Decal->Apply(State, Cfg, Type);
    }
}

ETargetDecalState UCursorInteractionComponent::ResolveDecalState(AActor* Actor) const
{
    if (!Actor) return ETargetDecalState::Hidden;
    if (Actor == VisualLockedActor.Get()) return ETargetDecalState::Locked;
    if (Actor == HoveredActor.Get())      return ETargetDecalState::Hover;
    return ETargetDecalState::Hidden;
}

APlayerController* UCursorInteractionComponent::GetPC() const
{
    if (const APawn* Pawn = Cast<APawn>(GetOwner()))
    {
        return Cast<APlayerController>(Pawn->GetController());
    }
    return nullptr;
}

UWorldInteractionConfig* UCursorInteractionComponent::GetEffectiveConfig() const
{
    if (Config) return Config;

    // Fall back to GameInstance-level config (set once, survives level transitions).
    if (const UWorld* W = GetWorld())
    {
        if (const UMyGameInstance* GI = Cast<UMyGameInstance>(UGameplayStatics::GetGameInstance(W)))
        {
            return GI->WorldInteractionConfig;
        }
    }
    return nullptr;
}
