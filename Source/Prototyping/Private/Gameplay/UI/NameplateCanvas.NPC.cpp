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

void UNameplateCanvasWidget::ShowNPCSpeechBubble(AActor* Actor, const FText& Text, float Duration)
{
    if (!Actor) return;

    // Determine which widget class to use (NPC variant or shared fallback)
    TSubclassOf<UChatBubbleWidget> BubbleClass = NPCSpeechBubbleWidgetClass
        ? NPCSpeechBubbleWidgetClass : ChatBubbleWidgetClass;

    if (!BubbleClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("NameplateCanvasWidget::ShowNPCSpeechBubble - no bubble widget class set"));
        return;
    }

    TWeakObjectPtr<AActor> WeakActor(Actor);
    UChatBubbleWidget** Found = NPCSpeechBubbles.Find(WeakActor);
    UChatBubbleWidget*  Bubble = Found ? *Found : nullptr;

    if (!Bubble)
    {
        UUserWidget* RawWidget = AddWidgetToCanvas(BubbleClass);
        Bubble = Cast<UChatBubbleWidget>(RawWidget);
        if (!Bubble)
        {
            UE_LOG(LogTemp, Error, TEXT("NameplateCanvasWidget::ShowNPCSpeechBubble - failed to create bubble widget"));
            return;
        }
        NPCSpeechBubbles.Add(WeakActor, Bubble);
    }

    Bubble->Show(Text.ToString(), Duration);

    // Record expiry time so NativeTick can position the bubble even while it is visible
    if (UWorld* World = GetWorld())
    {
        NPCSpeechBubbleExpiry.Add(WeakActor, World->GetTimeSeconds() + Duration);
    }
}

void UNameplateCanvasWidget::SetNPCInteractionState(AActor* Actor, ENPCInteractionState NewState)
{
    FNameplateEntry* Entry = FindEntry(Actor);
    if (!Entry || !Entry->bIsNPC)
    {
        return;
    }

    UW_NPCNameplateWidget* Nameplate = Cast<UW_NPCNameplateWidget>(Entry->Widget);
    if (Nameplate)
    {
        // Re-use SetNPCInfo with cached name/type is verbose; a dedicated method is cleaner.
        // For now drive only the indicator icons via a targeted call if the widget exposes it,
        // otherwise accept that a full refresh requires re-registering with bForceRefresh.
        (void)NewState; // placeholder until W_NPCNameplateWidget exposes SetInteractionState
    }
}

// -----------------------------------------------------------------------
// Internal helpers
// -----------------------------------------------------------------------

FNameplateEntry* UNameplateCanvasWidget::FindEntry(AActor* Actor)
{
    for (FNameplateEntry& Entry : Entries)
    {
        if (Entry.Actor == Actor)
        {
            return &Entry;
        }
    }
    return nullptr;
}

UUserWidget* UNameplateCanvasWidget::AddWidgetToCanvas(TSubclassOf<UUserWidget> WidgetClass)
{
    if (!NameplateCanvas || !WidgetClass)
    {
        return nullptr;
    }

    APlayerController* PC = GetOwningPlayer();
    if (!PC)
    {
        UE_LOG(LogTemp, Warning, TEXT("NameplateCanvasWidget::AddWidgetToCanvas � no owning PlayerController"));
        return nullptr;
    }

    UUserWidget* Widget = CreateWidget<UUserWidget>(PC, WidgetClass);
    if (!Widget)
    {
        UE_LOG(LogTemp, Warning, TEXT("NameplateCanvasWidget::AddWidgetToCanvas � CreateWidget returned null for class %s"),
            *WidgetClass->GetName());
        return nullptr;
    }

    UCanvasPanelSlot* NewWidgetSlot = NameplateCanvas->AddChildToCanvas(Widget);
    if (NewWidgetSlot)
    {
        NewWidgetSlot->SetAutoSize(true);
        NewWidgetSlot->SetAlignment(FVector2D(0.5f, 1.0f));
        NewWidgetSlot->SetPosition(FVector2D(-9999.f, -9999.f));
    }

    Widget->SetVisibility(ESlateVisibility::Collapsed);
    return Widget;
}

