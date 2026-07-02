// Fill out your copyright notice in the Description page of Project Settings.

#include "Gameplay/UI/PlayerNameplateComponent.h"
#include "Gameplay/UI/NameplateManager.h"
#include "UI/UIManager.h"
#include "Gameplay/Players/BasicPlayer.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "TimerManager.h"

UPlayerNameplateComponent::UPlayerNameplateComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UPlayerNameplateComponent::BeginPlay()
{
    Super::BeginPlay();

    if (bInitialised && !bIsLocalPlayer)
    {
        TryRegister();
    }
}

void UPlayerNameplateComponent::TryRegister()
{
    if (bRegistered || !bInitialised || bIsLocalPlayer)
    {
        return;
    }

    UNameplateManager* Mgr = ResolveNameplateManager();
    if (!Mgr)
    {
        // NameplateManager not ready yet � retry in 0.5s
        if (UWorld* World = GetWorld())
        {
            World->GetTimerManager().SetTimer(
                RetryTimerHandle, this, &UPlayerNameplateComponent::TryRegister, 0.5f, false);
        }
        return;
    }

    Mgr->RegisterPlayer(GetOwner(),
                        CachedCharData.characterName,
                        CachedCharData.characterClass,
                        CachedCharData.characterLevel,
                        CachedCharData.bIsDead,
                        HeadOffsetZ);
    bRegistered = true;

    // Apply any title that arrived before registration completed.
    if (!PendingTitle.IsEmpty())
    {
        Mgr->SetPlayerTitle(GetOwner(), PendingTitle);
    }
}

void UPlayerNameplateComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(RetryTimerHandle);
    }
    if (UNameplateManager* Mgr = ResolveNameplateManager())
    {
        Mgr->UnregisterPlayer(GetOwner());
    }
    Super::EndPlay(EndPlayReason);
}

void UPlayerNameplateComponent::InitialiseFromCharacterData(const FCharacterDataStruct& CharData,
                                                              bool bIsLocal,
                                                              bool bForceRefresh)
{
    if (bInitialised && !bForceRefresh)
    {
        return;
    }

    CachedCharData  = CharData;
    bIsLocalPlayer  = bIsLocal;
    bInitialised    = true;

    if (bIsLocalPlayer)
    {
        return;
    }

    if (bForceRefresh)
    {
        bRegistered = false;
        if (UNameplateManager* Mgr = ResolveNameplateManager())
        {
            Mgr->UnregisterPlayer(GetOwner());
        }
    }

    TryRegister();
}

void UPlayerNameplateComponent::UpdateHealth(int32 CurrentHP, int32 MaxHP)
{
    if (UNameplateManager* Mgr = ResolveNameplateManager())
    {
        Mgr->UpdatePlayerHealth(GetOwner(), CurrentHP, MaxHP);
    }
}

void UPlayerNameplateComponent::SetDeadState(bool bNewDead)
{
    CachedCharData.bIsDead = bNewDead;
    if (UNameplateManager* Mgr = ResolveNameplateManager())
    {
        Mgr->SetPlayerDeadState(GetOwner(), bNewDead);
    }
}

void UPlayerNameplateComponent::UpdateLevel(int32 NewLevel)
{
    CachedCharData.characterLevel = NewLevel;
    if (UNameplateManager* Mgr = ResolveNameplateManager())
    {
        Mgr->SetPlayerLevel(GetOwner(), NewLevel);
    }
}

void UPlayerNameplateComponent::UpdateTitle(const FText& InTitle)
{
    PendingTitle = InTitle;  // Always cache so post-registration replay works.
    if (UNameplateManager* Mgr = ResolveNameplateManager())
    {
        Mgr->SetPlayerTitle(GetOwner(), InTitle);
    }
}

void UPlayerNameplateComponent::ShowChatBubble(const FString& Text, float Duration)
{
    if (UNameplateManager* Mgr = ResolveNameplateManager())
    {
        Mgr->ShowPlayerChatBubble(GetOwner(), Text, Duration);
    }
}

UNameplateManager* UPlayerNameplateComponent::ResolveNameplateManager() const
{
    APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
    if (!PC)
    {
        return nullptr;
    }

    // UIManager is a component on ABasicPlayer (Pawn), not on PlayerController
    if (ABasicPlayer* Player = Cast<ABasicPlayer>(PC->GetPawn()))
    {
        if (UUIManager* UI = Player->GetUIManager())
        {
            return UI->GetNameplateManager();
        }
    }
    return nullptr;
}


