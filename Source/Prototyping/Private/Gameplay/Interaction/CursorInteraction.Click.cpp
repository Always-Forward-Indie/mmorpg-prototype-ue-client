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

void UCursorInteractionComponent::HandleConfirmedClick()
{
    AActor* Target = HoveredActor.Get();

    UE_LOG(LogTemp, Log, TEXT("CursorInteraction: HandleConfirmedClick — HoveredActor=%s, ListenersSingle=%d, ListenersDouble=%d"),
        Target ? *Target->GetName() : TEXT("null"),
        OnSingleClicked.IsBound() ? 1 : 0,
        OnDoubleClicked.IsBound() ? 1 : 0);

    // Click on empty world — broadcast null so BasicPlayer can clear its locked target.
    if (!Target)
    {
        OnSingleClicked.Broadcast(nullptr, EInteractableType::None);
        return;
    }

    const EInteractableType Type = HoveredType;
    const float             Now  = GetWorld()->GetTimeSeconds();

    const bool bIsDoubleClick =
        (Now - LastClickTime <= (Config ? Config->DoubleClickMaxInterval : 0.35f)) &&
        (LastClickedActor.Get() == Target);

    LastClickTime    = Now;
    LastClickedActor = Target;

    if (bIsDoubleClick)
    {
        OnDoubleClicked.Broadcast(Target, Type);
    }
    else
    {
        OnSingleClicked.Broadcast(Target, Type);
    }
}

void UCursorInteractionComponent::NotifyDragStarted()
{
    // Reset the double-click chain; drag → release must not fire a click.
    LastClickTime    = 0.f;
    LastClickedActor = nullptr;
}

void UCursorInteractionComponent::ForceHoverClear()
{
    if (HoveredActor.IsValid())
    {
        if (HoveredActor.Get() != VisualLockedActor.Get())
        {
            ApplyDecal(HoveredActor.Get(), ETargetDecalState::Hidden, HoveredType);
        }
        AActor* Old = HoveredActor.Get();
        HoveredActor = nullptr;
        HoveredType  = EInteractableType::None;
        ResetCursorIcon();
        OnHoverChanged.Broadcast(Old, nullptr, EInteractableType::None);
    }
}

void UCursorInteractionComponent::NotifyLockedTargetChanged(AActor*           OldLocked,
                                                             AActor*           NewLocked,
                                                             EInteractableType NewType)
{
    // When called from BasicPlayer, OldLocked may be stale; fall back to our own VisualLockedActor.
    AActor* EffectiveOld = OldLocked ? OldLocked : VisualLockedActor.Get();

    // Remove Locked decal from the previous actor (revert to Hover if still under cursor).
    if (EffectiveOld && EffectiveOld != NewLocked)
    {
        const bool bStillHovered = (EffectiveOld == HoveredActor.Get());
        ApplyDecal(EffectiveOld,
                   bStillHovered ? ETargetDecalState::Hover : ETargetDecalState::Hidden,
                   GetTypeFromActor(EffectiveOld));
    }

    VisualLockedActor = NewLocked;
    VisualLockedType  = NewType;

    if (NewLocked)
    {
        ApplyDecal(NewLocked, ETargetDecalState::Locked, NewType);
    }
}

void UCursorInteractionComponent::SetVisualLock(AActor* Actor, EInteractableType Type)
{
    AActor* OldLocked = VisualLockedActor.Get();

    // Clear old visual lock.
    if (OldLocked && OldLocked != Actor)
    {
        const bool bStillHovered = (OldLocked == HoveredActor.Get());
        ApplyDecal(OldLocked,
                   bStillHovered ? ETargetDecalState::Hover : ETargetDecalState::Hidden,
                   GetTypeFromActor(OldLocked));
    }

    VisualLockedActor = Actor;
    VisualLockedType  = Type;

    if (Actor)
    {
        ApplyDecal(Actor, ETargetDecalState::Locked, Type);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Hover trace
// ─────────────────────────────────────────────────────────────────────────────

