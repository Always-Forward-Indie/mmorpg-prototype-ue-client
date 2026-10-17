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

void UCursorInteractionComponent::RunHoverTrace()
{
    APlayerController* PC = GetPC();
    if (!PC) return;

    // Suppress hover tracing while a UI window is consuming input.
    if (ABasicPlayer* Player = Cast<ABasicPlayer>(GetOwner()))
    {
        if (Player->IsUIBlockingInteraction())
        {
            ForceHoverClear();
            return;
        }
    }

    // Suppress hover tracing while cursor is hidden (free-look / RMB-camera mode)
    // so the player doesn't see hover decals / hints appear behind the cursor.
    if (!PC->bShowMouseCursor)
    {
        ForceHoverClear();
        return;
    }

    FVector WorldOrigin, WorldDir;
    if (!PC->DeprojectMousePositionToWorld(WorldOrigin, WorldDir)) return;

    const UWorldInteractionConfig* EffCfg = GetEffectiveConfig();
    const float Range = EffCfg ? EffCfg->HoverTraceRange : 5000.f;
    const FVector TraceEnd = WorldOrigin + WorldDir * Range;

    FCollisionQueryParams Params;
    Params.AddIgnoredActor(GetOwner());

    // Use multi-trace so an IWorldInteractable actor behind a non-interactable blocker
    // (landscape, static mesh, etc.) is still found.
    TArray<FHitResult> Hits;
    GetWorld()->LineTraceMultiByChannel(Hits, WorldOrigin, TraceEnd, ECC_Visibility, Params);

    AActor* NewActor = nullptr;
    for (const FHitResult& H : Hits)
    {
        AActor* A = H.GetActor();
        if (A && A->Implements<UWorldInteractable>())
        {
            NewActor = A;
            break;
        }
    }

    SetHoveredActor(NewActor);
}

void UCursorInteractionComponent::SetHoveredActor(AActor* NewActor)
{
    AActor* OldActor = HoveredActor.Get();
    if (OldActor == NewActor) return;

    // Remove hover decal from the previous actor (unless it is also visually locked).
    if (OldActor && OldActor != VisualLockedActor.Get())
    {
        ApplyDecal(OldActor, ETargetDecalState::Hidden, HoveredType);
    }

    HoveredActor = NewActor;
    HoveredType  = NewActor ? GetTypeFromActor(NewActor) : EInteractableType::None;

    // Apply hover decal to the new actor (unless it is already locked — locked wins).
    if (NewActor && NewActor != VisualLockedActor.Get())
    {
        ApplyDecal(NewActor, ETargetDecalState::Hover, HoveredType);
    }

    ApplyCursorIcon(HoveredType);
    OnHoverChanged.Broadcast(OldActor, NewActor, HoveredType);
}

// Static helper — no coupling to instance state.
EInteractableType UCursorInteractionComponent::GetTypeFromActor(AActor* Actor)
{
    if (!Actor) return EInteractableType::None;

    if (const ABasicMOB* Mob = Cast<ABasicMOB>(Actor))
    {
        if (!Mob->GetMOBIsDead())    return EInteractableType::MOB_Alive;
        if (Mob->CanBeHarvested())   return EInteractableType::MOB_Harvestable;
        if (Mob->HasBeenHarvested()) return EInteractableType::MOB_Harvested;
        return EInteractableType::MOB_Harvestable; // dead, state not yet synced
    }
    if (Cast<ABasicNPC>(Actor))         return EInteractableType::NPC;
    if (Cast<ADroppedItemActor>(Actor)) return EInteractableType::DroppedItem;

    // Remote player check — IsOtherClient guard avoids self-targeting.
    if (const ABasicPlayer* Player = Cast<const ABasicPlayer>(Actor))
    {
        return const_cast<ABasicPlayer*>(Player)->GetIsOtherClient()
                   ? EInteractableType::RemotePlayer
                   : EInteractableType::None;
    }

    return EInteractableType::None;
}

// ─────────────────────────────────────────────────────────────────────────────
// Cursor icon
// ─────────────────────────────────────────────────────────────────────────────

