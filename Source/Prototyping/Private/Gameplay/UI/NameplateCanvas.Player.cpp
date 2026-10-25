// Fill out your copyright notice in the Description page of Project Settings.

#include "Gameplay/UI/NameplateCanvasWidget.h"
#include "Prototyping.h"
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

void UNameplateCanvasWidget::UpdatePlayerHealth(AActor* Actor, int32 CurrentHP, int32 MaxHP)
{
    FNameplateEntry* Entry = FindEntry(Actor);
    if (!Entry || Entry->bIsNPC)
    {
        return;
    }

    UW_PlayerNameplateWidget* Nameplate = Cast<UW_PlayerNameplateWidget>(Entry->Widget);
    if (Nameplate)
    {
        Nameplate->UpdateHealthBar(CurrentHP, MaxHP);
    }
}

void UNameplateCanvasWidget::SetPlayerDeadState(AActor* Actor, bool bDead)
{
    FNameplateEntry* Entry = FindEntry(Actor);
    if (!Entry || Entry->bIsNPC)
    {
        return;
    }

    UW_PlayerNameplateWidget* Nameplate = Cast<UW_PlayerNameplateWidget>(Entry->Widget);
    if (Nameplate)
    {
        Nameplate->SetDeadState(bDead);
    }
}

void UNameplateCanvasWidget::SetPlayerLevel(AActor* Actor, int32 NewLevel)
{
    FNameplateEntry* Entry = FindEntry(Actor);
    if (!Entry || Entry->bIsNPC) return;

    UW_PlayerNameplateWidget* Nameplate = Cast<UW_PlayerNameplateWidget>(Entry->Widget);
    if (Nameplate)
    {
        Nameplate->SetPlayerLevel(NewLevel);
    }
}

void UNameplateCanvasWidget::SetPlayerTitle(AActor* Actor, const FText& InTitle)
{
    FNameplateEntry* Entry = FindEntry(Actor);
    if (!Entry || Entry->bIsNPC) return;

    UW_PlayerNameplateWidget* Nameplate = Cast<UW_PlayerNameplateWidget>(Entry->Widget);
    if (Nameplate)
    {
        Nameplate->SetTitle(InTitle);
    }
}

void UNameplateCanvasWidget::ShowPlayerChatBubble(AActor* Actor, const FString& Text, float Duration)
{
    if (!Actor) return;

    // Only for registered remote player nameplates (not NPCs).
    FNameplateEntry* Entry = FindEntry(Actor);
    if (!Entry || Entry->bIsNPC) return;

    if (!ChatBubbleWidgetClass)
    {
        UE_LOG(LogGameplay, Warning, TEXT("NameplateCanvasWidget::ShowPlayerChatBubble - ChatBubbleWidgetClass is not set"));
        return;
    }

    // Look up or lazy-create the bubble for this actor.
    TWeakObjectPtr<AActor> WeakActor(Actor);
    UChatBubbleWidget** Found = ChatBubbles.Find(WeakActor);
    UChatBubbleWidget*  Bubble = Found ? *Found : nullptr;

    if (!Bubble)
    {
        APlayerController* PC = GetOwningPlayer();
        if (!PC) return;

        UUserWidget* RawWidget = AddWidgetToCanvas(ChatBubbleWidgetClass);
        Bubble = Cast<UChatBubbleWidget>(RawWidget);
        if (!Bubble)
        {
            UE_LOG(LogGameplay, Error, TEXT("NameplateCanvasWidget::ShowPlayerChatBubble - failed to create ChatBubbleWidget"));
            return;
        }
        ChatBubbles.Add(WeakActor, Bubble);
    }

    Bubble->Show(Text, Duration);
}

