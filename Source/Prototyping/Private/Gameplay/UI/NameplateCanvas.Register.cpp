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

void UNameplateCanvasWidget::RegisterPlayer(AActor*        Actor,
                                             const FString& Name,
                                             const FString& Class,
                                             int32          Level,
                                             bool           bIsDead,
                                             float          HeadOffsetZ)
{
    UE_LOG(LogGameplay, Warning, TEXT("NameplateCanvasWidget::RegisterPlayer called for actor '%s' (name='%s')"), 
        Actor ? *Actor->GetName() : TEXT("NULL"), *Name);

    if (!Actor || FindEntry(Actor))
    {
        UE_LOG(LogGameplay, Warning, TEXT("NameplateCanvasWidget::RegisterPlayer - actor is null or already registered"));
        return;
    }

    if (!PlayerNameplateWidgetClass)
    {
        UE_LOG(LogGameplay, Warning, TEXT("NameplateCanvasWidget: PlayerNameplateWidgetClass is not set"));
        return;
    }

    UUserWidget* RawWidget = AddWidgetToCanvas(PlayerNameplateWidgetClass);
    if (!RawWidget)
    {
        UE_LOG(LogGameplay, Error, TEXT("NameplateCanvasWidget::RegisterPlayer - AddWidgetToCanvas returned null"));
        return;
    }

    UW_PlayerNameplateWidget* Nameplate = Cast<UW_PlayerNameplateWidget>(RawWidget);
    if (Nameplate)
    {
        Nameplate->SetPlayerInfo(Name, Class, Level, bIsDead);
        // Explicitly hide the title row — title arrives later via SetPlayerTitle.
        // Prevents an empty text slot from showing when the player has no title.
        Nameplate->SetTitle(FText::GetEmpty());
        UE_LOG(LogGameplay, Warning, TEXT("NameplateCanvasWidget::RegisterPlayer - successfully created and initialized nameplate for '%s'"), *Name);
    }

    FNameplateEntry& Entry  = Entries.AddDefaulted_GetRef();
    Entry.Actor             = Actor;
    Entry.Widget            = RawWidget;
    Entry.HeadOffsetZ       = (HeadOffsetZ > 0.f) ? HeadOffsetZ : DefaultPlayerHeadOffsetZ;
    Entry.bIsNPC            = false;
    Entry.bIsLocalPlayer    = false;
}

void UNameplateCanvasWidget::RegisterNPC(AActor*               Actor,
                                          const FString&        Name,
                                          const FString&        NPCType,
                                          int32                 Level,
                                          ENPCInteractionState  InteractionState,
                                          float                 InteractRadius,
                                          float                 HeadOffsetZ)
{
    UE_LOG(LogGameplay, Warning, TEXT("NameplateCanvasWidget::RegisterNPC called for actor '%s' (name='%s', type='%s')"), 
        Actor ? *Actor->GetName() : TEXT("NULL"), *Name, *NPCType);

    if (!Actor || FindEntry(Actor))
    {
        UE_LOG(LogGameplay, Warning, TEXT("NameplateCanvasWidget::RegisterNPC - actor is null or already registered"));
        return;
    }

    if (!NPCNameplateWidgetClass)
    {
        UE_LOG(LogGameplay, Warning, TEXT("NameplateCanvasWidget: NPCNameplateWidgetClass is not set"));
        return;
    }

    UUserWidget* RawWidget = AddWidgetToCanvas(NPCNameplateWidgetClass);
    if (!RawWidget)
    {
        UE_LOG(LogGameplay, Error, TEXT("NameplateCanvasWidget::RegisterNPC - AddWidgetToCanvas returned null"));
        return;
    }

    UW_NPCNameplateWidget* Nameplate = Cast<UW_NPCNameplateWidget>(RawWidget);
    if (Nameplate)
    {
        Nameplate->SetNPCInfo(Name, NPCType, Level, InteractionState);
        UE_LOG(LogGameplay, Warning, TEXT("NameplateCanvasWidget::RegisterNPC - successfully created and initialized nameplate for '%s'"), *Name);
    }

    FNameplateEntry& Entry  = Entries.AddDefaulted_GetRef();
    Entry.Actor             = Actor;
    Entry.Widget            = RawWidget;
    Entry.HeadOffsetZ       = (HeadOffsetZ > 0.f) ? HeadOffsetZ : DefaultNPCHeadOffsetZ;
    Entry.InteractRadius    = InteractRadius;
    Entry.bIsNPC            = true;
    Entry.bIsLocalPlayer    = false;
}

void UNameplateCanvasWidget::Unregister(AActor* Actor)
{
    // Remove chat bubble for this actor
    TWeakObjectPtr<AActor> WeakActor(Actor);
    if (UChatBubbleWidget** BubblePtr = ChatBubbles.Find(WeakActor))
    {
        if (*BubblePtr) (*BubblePtr)->RemoveFromParent();
        ChatBubbles.Remove(WeakActor);
        ChatBubbleOpacity.Remove(WeakActor);
    }

    // Remove NPC speech bubble for this actor
    if (UChatBubbleWidget** NPCBubblePtr = NPCSpeechBubbles.Find(WeakActor))
    {
        if (*NPCBubblePtr) (*NPCBubblePtr)->RemoveFromParent();
        NPCSpeechBubbles.Remove(WeakActor);
        NPCSpeechBubbleOpacity.Remove(WeakActor);
    }
    NPCSpeechBubbleExpiry.Remove(WeakActor);

    for (int32 i = Entries.Num() - 1; i >= 0; --i)
    {
        if (Entries[i].Actor == Actor)
        {
            if (Entries[i].Widget && NameplateCanvas)
            {
                Entries[i].Widget->RemoveFromParent();
            }
            Entries.RemoveAtSwap(i);
            return;
        }
    }
}

void UNameplateCanvasWidget::UnregisterAll()
{
    for (FNameplateEntry& Entry : Entries)
    {
        if (Entry.Widget)
        {
            Entry.Widget->RemoveFromParent();
        }
    }
    Entries.Reset();

    for (auto& Pair : ChatBubbles)
    {
        if (Pair.Value) Pair.Value->RemoveFromParent();
    }
    ChatBubbles.Empty();
    ChatBubbleOpacity.Empty();

    for (auto& Pair : NPCSpeechBubbles)
    {
        if (Pair.Value) Pair.Value->RemoveFromParent();
    }
    NPCSpeechBubbles.Empty();
    NPCSpeechBubbleOpacity.Empty();
    NPCSpeechBubbleExpiry.Empty();
}

// -----------------------------------------------------------------------
// Live update API
// -----------------------------------------------------------------------

