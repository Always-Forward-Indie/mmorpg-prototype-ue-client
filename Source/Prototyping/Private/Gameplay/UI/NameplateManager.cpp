// Fill out your copyright notice in the Description page of Project Settings.

#include "Gameplay/UI/NameplateManager.h"

UNameplateManager::UNameplateManager()
{
    PrimaryComponentTick.bCanEverTick = false;
}

// -----------------------------------------------------------------------
// Initialisation
// -----------------------------------------------------------------------

void UNameplateManager::SetCanvasWidget(UNameplateCanvasWidget* InCanvas)
{
    CanvasWidget = InCanvas;
}

// -----------------------------------------------------------------------
// Player
// -----------------------------------------------------------------------

void UNameplateManager::RegisterPlayer(AActor*        Actor,
                                        const FString& Name,
                                        const FString& Class,
                                        int32          Level,
                                        bool           bIsDead,
                                        float          HeadOffsetZ)
{
    if (CanvasWidget)
    {
        CanvasWidget->RegisterPlayer(Actor, Name, Class, Level, bIsDead, HeadOffsetZ);
    }
}

void UNameplateManager::UnregisterPlayer(AActor* Actor)
{
    if (CanvasWidget)
    {
        CanvasWidget->Unregister(Actor);
    }
}

void UNameplateManager::UpdatePlayerHealth(AActor* Actor, int32 CurrentHP, int32 MaxHP)
{
    if (CanvasWidget)
    {
        CanvasWidget->UpdatePlayerHealth(Actor, CurrentHP, MaxHP);
    }
}

void UNameplateManager::SetPlayerDeadState(AActor* Actor, bool bDead)
{
    if (CanvasWidget)
    {
        CanvasWidget->SetPlayerDeadState(Actor, bDead);
    }
}

// -----------------------------------------------------------------------
// NPC
// -----------------------------------------------------------------------

void UNameplateManager::RegisterNPC(AActor*               Actor,
                                     const FString&        Name,
                                     const FString&        NPCType,
                                     int32                 Level,
                                     ENPCInteractionState  InteractionState,
                                     float                 InteractRadius,
                                     float                 HeadOffsetZ)
{
    if (CanvasWidget)
    {
        CanvasWidget->RegisterNPC(Actor, Name, NPCType, Level,
                                  InteractionState, InteractRadius, HeadOffsetZ);
    }
}

void UNameplateManager::UnregisterNPC(AActor* Actor)
{
    if (CanvasWidget)
    {
        CanvasWidget->Unregister(Actor);
    }
}

// -----------------------------------------------------------------------
// Bulk
// -----------------------------------------------------------------------

void UNameplateManager::UnregisterAll()
{
    if (CanvasWidget)
    {
        CanvasWidget->UnregisterAll();
    }
}
