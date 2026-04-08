#include "Gameplay/Player/ReputationManager.h"

void UReputationManager::ApplyReputationsState(const FPlayerReputationsState& InState)
{
    CachedCharacterId = InState.characterId;
    ReputationMap.Empty();

    for (const FReputationEntry& Entry : InState.entries)
    {
        ReputationMap.Add(Entry.factionSlug, Entry);
    }

    UE_LOG(LogTemp, Log, TEXT("ReputationManager: Loaded %d reputation entries for character %d"),
        ReputationMap.Num(), CachedCharacterId);

    OnReputationsLoaded.Broadcast(InState);
}

void UReputationManager::ApplyReputationUpdate(const FReputationUpdateData& Update)
{
    FReputationEntry& Entry = ReputationMap.FindOrAdd(Update.factionSlug);
    Entry.factionSlug = Update.factionSlug;
    Entry.value       = Update.value;
    Entry.tier        = Update.tier;

    UE_LOG(LogTemp, Log, TEXT("ReputationManager: %s -> %d (%s)"),
        *Update.factionSlug, Update.value, *Update.tier);

    OnReputationUpdated.Broadcast(Update);
}

FReputationEntry UReputationManager::GetReputation(const FString& FactionSlug) const
{
    const FReputationEntry* Found = ReputationMap.Find(FactionSlug);
    return Found ? *Found : FReputationEntry{};
}

TArray<FReputationEntry> UReputationManager::GetAllReputations() const
{
    TArray<FReputationEntry> Result;
    ReputationMap.GenerateValueArray(Result);
    return Result;
}
