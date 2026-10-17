// Fill out your copyright notice in the Description page of Project Settings.

#include "Gameplay/UI/NameplateCanvasWidget.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/SlateBlueprintLibrary.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"

// -----------------------------------------------------------------------
// UUserWidget overrides
// -----------------------------------------------------------------------

void UNameplateCanvasWidget::NativeConstruct()
{
    Super::NativeConstruct();
}

void UNameplateCanvasWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    if (!NameplateCanvas)
    {
        return;
    }

    APlayerController* PC = GetOwningPlayer();
    if (!PC)
    {
        return;
    }

    APawn* Pawn = PC->GetPawn();
    // On the login/character-select screen there is no pawn — the player is
    // viewing through a CameraActor.  Fall back to the camera position so
    // distance-based visibility culling works correctly for preview characters.
    FVector PawnLocation;
    if (Pawn)
    {
        PawnLocation = Pawn->GetActorLocation();
    }
    else if (PC->PlayerCameraManager)
    {
        PawnLocation = PC->PlayerCameraManager->GetCameraLocation();
    }
    else
    {
        PawnLocation = FVector::ZeroVector;
    }

    for (FNameplateEntry& Entry : Entries)
    {
        if (!Entry.Actor.IsValid())
        {
            // Actor was destroyed - hide widget, will be cleaned up next unregister call
            if (Entry.Widget)
            {
                Entry.Widget->SetVisibility(ESlateVisibility::Collapsed);
            }
            continue;
        }

        TickEntry(Entry, PC, PawnLocation, InDeltaTime);
    }

    // ---- Chat-bubble tick --------------------------------------------------
    // Same projection pipeline as nameplates but at an extra Z offset so the
    // bubble appears above the nameplate.  Opacity is driven by distance so the
    // bubble fades in/out smoothly as the local player approaches / retreats,
    // and also fades in on message arrival and out on timer expiry.
    for (auto It = ChatBubbles.CreateIterator(); It; ++It)
    {
        TWeakObjectPtr<AActor> WeakActor = It.Key();
        AActor* BubbleActor = WeakActor.Get();
        UChatBubbleWidget* Bubble = It.Value();

        if (!IsValid(BubbleActor) || !Bubble)
        {
            if (Bubble) Bubble->RemoveFromParent();
            ChatBubbleOpacity.Remove(WeakActor);
            It.RemoveCurrent();
            continue;
        }

        float& CurrentOpacity = ChatBubbleOpacity.FindOrAdd(WeakActor);

        // Distance-based target opacity: fade to 0 beyond ChatBubbleMaxVisibleDistance.
        const float DistToBubble = FVector::Dist(PawnLocation, BubbleActor->GetActorLocation());
        float TargetOpacity = 0.0f;
        if (Bubble->IsShowing() && DistToBubble <= ChatBubbleMaxVisibleDistance)
        {
            const float FadeStart = ChatBubbleMaxVisibleDistance - FMath::Max(ChatBubbleFadeDistance, 1.f);
            if (DistToBubble <= FadeStart)
            {
                TargetOpacity = 1.0f;
            }
            else
            {
                TargetOpacity = FMath::Clamp(
                    1.0f - (DistToBubble - FadeStart) / FMath::Max(ChatBubbleFadeDistance, 1.f),
                    0.0f, 1.0f);
            }
        }

        // World-space anchor: capsule top + extra offset above the nameplate.
        float CapsuleHalf = 0.f;
        if (const ACharacter* Char = Cast<ACharacter>(BubbleActor))
            if (const UCapsuleComponent* Cap = Char->GetCapsuleComponent())
                CapsuleHalf = Cap->GetScaledCapsuleHalfHeight();

        const FVector BubbleWorld = BubbleActor->GetActorLocation()
                                  + FVector(0.f, 0.f, CapsuleHalf + ChatBubbleHeadOffsetZ);

        // Camera dot-product guard: snap opacity to 0 instantly when behind camera.
        if (APlayerCameraManager* CamMgr = PC->PlayerCameraManager)
        {
            const FVector CamFwd = CamMgr->GetActorForwardVector();
            const FVector ToPoint = BubbleWorld - CamMgr->GetCameraLocation();
            if (FVector::DotProduct(ToPoint, CamFwd) <= 0.f)
            {
                CurrentOpacity = 0.f;
                Bubble->SetRenderOpacity(0.f);
                Bubble->SetVisibility(ESlateVisibility::Collapsed);
                if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(Bubble->Slot))
                    S->SetPosition(FVector2D(-9999.f, -9999.f));
                continue;
            }
        }

        // Smooth fade toward target (skip interp when already at 0 and target is 0).
        if (CurrentOpacity < 0.01f && TargetOpacity <= 0.f)
            CurrentOpacity = 0.f;
        else
            CurrentOpacity = ChatBubbleFadeSpeed > 0.f
                ? FMath::FInterpConstantTo(CurrentOpacity, TargetOpacity, InDeltaTime, ChatBubbleFadeSpeed)
                : TargetOpacity;

        const bool bShouldBeVisible = CurrentOpacity > 0.01f;

        if (!bShouldBeVisible)
        {
            Bubble->SetVisibility(ESlateVisibility::Collapsed);
            if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(Bubble->Slot))
                S->SetPosition(FVector2D(-9999.f, -9999.f));
            continue;
        }

        FVector2D ScreenPosPx;
        if (!PC->ProjectWorldLocationToScreen(BubbleWorld, ScreenPosPx, true))
        {
            // Off-screen: snap so the bubble doesn’t linger on a stale position.
            CurrentOpacity = 0.f;
            Bubble->SetVisibility(ESlateVisibility::Collapsed);
            if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(Bubble->Slot))
                S->SetPosition(FVector2D(-9999.f, -9999.f));
            continue;
        }

        const float DPIScale = UWidgetLayoutLibrary::GetViewportScale(this);
        const FVector2D ScreenPos = ScreenPosPx / FMath::Max(DPIScale, 0.01f);

        Bubble->SetRenderOpacity(CurrentOpacity);
        if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(Bubble->Slot))
            S->SetPosition(ScreenPos);
        if (Bubble->GetVisibility() == ESlateVisibility::Collapsed)
            Bubble->SetVisibility(ESlateVisibility::HitTestInvisible);
    }

    // ---- NPC Speech-bubble tick -------------------------------------------
    // Identical distance-fade logic as player chat bubbles.
    for (auto It = NPCSpeechBubbles.CreateIterator(); It; ++It)
    {
        TWeakObjectPtr<AActor> WeakActor = It.Key();
        AActor* BubbleActor = WeakActor.Get();
        UChatBubbleWidget* Bubble = It.Value();

        if (!IsValid(BubbleActor) || !Bubble)
        {
            if (Bubble) Bubble->RemoveFromParent();
            NPCSpeechBubbleOpacity.Remove(WeakActor);
            NPCSpeechBubbleExpiry.Remove(WeakActor);
            It.RemoveCurrent();
            continue;
        }

        float& CurrentOpacity = NPCSpeechBubbleOpacity.FindOrAdd(WeakActor);

        // Distance-based target opacity.
        const float DistToBubble = FVector::Dist(PawnLocation, BubbleActor->GetActorLocation());
        float TargetOpacity = 0.0f;
        if (Bubble->IsShowing() && DistToBubble <= ChatBubbleMaxVisibleDistance)
        {
            const float FadeStart = ChatBubbleMaxVisibleDistance - FMath::Max(ChatBubbleFadeDistance, 1.f);
            if (DistToBubble <= FadeStart)
            {
                TargetOpacity = 1.0f;
            }
            else
            {
                TargetOpacity = FMath::Clamp(
                    1.0f - (DistToBubble - FadeStart) / FMath::Max(ChatBubbleFadeDistance, 1.f),
                    0.0f, 1.0f);
            }
        }

        float CapsuleHalf = 0.f;
        if (const ACharacter* Char = Cast<ACharacter>(BubbleActor))
            if (const UCapsuleComponent* Cap = Char->GetCapsuleComponent())
                CapsuleHalf = Cap->GetScaledCapsuleHalfHeight();

        const FVector BubbleWorld = BubbleActor->GetActorLocation()
                                  + FVector(0.f, 0.f, CapsuleHalf + NPCSpeechBubbleHeadOffsetZ);

        // Camera dot-product guard: snap opacity to 0 instantly when behind camera.
        if (APlayerCameraManager* CamMgr = PC->PlayerCameraManager)
        {
            const FVector CamFwd = CamMgr->GetActorForwardVector();
            const FVector ToPoint = BubbleWorld - CamMgr->GetCameraLocation();
            if (FVector::DotProduct(ToPoint, CamFwd) <= 0.f)
            {
                CurrentOpacity = 0.f;
                Bubble->SetRenderOpacity(0.f);
                Bubble->SetVisibility(ESlateVisibility::Collapsed);
                if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(Bubble->Slot))
                    S->SetPosition(FVector2D(-9999.f, -9999.f));
                continue;
            }
        }

        // Smooth fade.
        if (CurrentOpacity < 0.01f && TargetOpacity <= 0.f)
            CurrentOpacity = 0.f;
        else
            CurrentOpacity = ChatBubbleFadeSpeed > 0.f
                ? FMath::FInterpConstantTo(CurrentOpacity, TargetOpacity, InDeltaTime, ChatBubbleFadeSpeed)
                : TargetOpacity;

        const bool bShouldBeVisible = CurrentOpacity > 0.01f;

        if (!bShouldBeVisible)
        {
            Bubble->SetVisibility(ESlateVisibility::Collapsed);
            if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(Bubble->Slot))
                S->SetPosition(FVector2D(-9999.f, -9999.f));
            continue;
        }

        FVector2D ScreenPosPx;
        if (!PC->ProjectWorldLocationToScreen(BubbleWorld, ScreenPosPx, true))
        {
            CurrentOpacity = 0.f;
            Bubble->SetVisibility(ESlateVisibility::Collapsed);
            if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(Bubble->Slot))
                S->SetPosition(FVector2D(-9999.f, -9999.f));
            continue;
        }

        const float DPIScale = UWidgetLayoutLibrary::GetViewportScale(this);
        const FVector2D ScreenPos = ScreenPosPx / FMath::Max(DPIScale, 0.01f);

        Bubble->SetRenderOpacity(CurrentOpacity);
        if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(Bubble->Slot))
            S->SetPosition(ScreenPos);
        if (Bubble->GetVisibility() == ESlateVisibility::Collapsed)
            Bubble->SetVisibility(ESlateVisibility::HitTestInvisible);
    }
}

// -----------------------------------------------------------------------
// Registration API
// -----------------------------------------------------------------------

