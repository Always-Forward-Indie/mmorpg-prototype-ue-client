// Fill out your copyright notice in the Description page of Project Settings.


#include "Gameplay/UI/NPCNameplateComponent.h"
#include "Gameplay/UI/NameplateManager.h"
#include "UI/UIManager.h"
#include "Gameplay/Players/BasicPlayer.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Services/LocalizationSubsystem.h"
#include "MyGameInstance.h"

UNPCNameplateComponent::UNPCNameplateComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UNPCNameplateComponent::BeginPlay()
{
    Super::BeginPlay();

    if (UWorld* W = GetWorld())
    {
        if (UMyGameInstance* GI = Cast<UMyGameInstance>(W->GetGameInstance()))
        {
            if (ULocalizationSubsystem* LocSys = GI->GetSubsystem<ULocalizationSubsystem>())
            {
                LocSys->OnLocaleChanged.AddDynamic(this, &UNPCNameplateComponent::HandleLocaleChanged);
            }
        }
    }

    // If data was set before BeginPlay (via SetNPCData), try to register now.
    // If NameplateManager is not ready yet, schedule a retry.
    if (bInitialised)
    {
        TryRegister();
    }
}

void UNPCNameplateComponent::TryRegister()
{
    if (bRegistered || !bInitialised)
    {
        return;
    }

    UNameplateManager* Mgr = ResolveNameplateManager();
    if (!Mgr)
    {
        // NameplateManager not ready yet — retry in 0.5s
        if (UWorld* World = GetWorld())
        {
            World->GetTimerManager().SetTimer(
                RetryTimerHandle, this, &UNPCNameplateComponent::TryRegister, 0.5f, false);
        }
        return;
    }

    const float RadiusToUse = (InteractRadius > 0.f)
        ? InteractRadius
        : static_cast<float>(CachedNPCData.radius > 0 ? CachedNPCData.radius : 300);

    const ENPCInteractionState State = CachedNPCData.ComputeInteractionState();

    // Resolve display name via localization subsystem (slug-based), fall back to slug string.
    FString DisplayName = CachedNPCData.slug;
    if (UWorld* W = GetWorld())
    {
        if (UMyGameInstance* GI = Cast<UMyGameInstance>(W->GetGameInstance()))
        {
            if (ULocalizationSubsystem* Loc = GI->GetSubsystem<ULocalizationSubsystem>())
            {
                FText Localised = Loc->GetNPCDisplayName(CachedNPCData.slug);
                if (!Localised.IsEmpty())
                    DisplayName = Localised.ToString();
            }
        }
    }

    Mgr->RegisterNPC(GetOwner(), DisplayName, CachedNPCData.npcType,
                     CachedNPCData.level, State, RadiusToUse, HeadOffsetZ);
    bRegistered = true;
}

void UNPCNameplateComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(RetryTimerHandle);
        if (UMyGameInstance* GI = Cast<UMyGameInstance>(World->GetGameInstance()))
        {
            if (ULocalizationSubsystem* LocSys = GI->GetSubsystem<ULocalizationSubsystem>())
            {
                LocSys->OnLocaleChanged.RemoveDynamic(this, &UNPCNameplateComponent::HandleLocaleChanged);
            }
        }
    }
    if (UNameplateManager* Mgr = ResolveNameplateManager())
    {
        Mgr->UnregisterNPC(GetOwner());
    }
    Super::EndPlay(EndPlayReason);
}

void UNPCNameplateComponent::InitialiseFromNPCData(const FNPCStruct& NPCData, bool bForceRefresh)
{
    if (bInitialised && !bForceRefresh)
    {
        return;
    }

    CachedNPCData = NPCData;
    bInitialised  = true;

    if (bForceRefresh)
    {
        bRegistered = false;
        if (UNameplateManager* Mgr = ResolveNameplateManager())
        {
            Mgr->UnregisterNPC(GetOwner());
        }
    }

    // Try to register immediately; if UIManager not ready yet, TryRegister schedules a retry.
    TryRegister();
}

UNameplateManager* UNPCNameplateComponent::ResolveNameplateManager() const
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

void UNPCNameplateComponent::HandleLocaleChanged(const FString& NewLocale)
{
    if (bInitialised)
    {
        InitialiseFromNPCData(CachedNPCData, true);
    }
}

