#include "Gameplay/Player/MasteryManager.h"

void UMasteryManager::ApplyMasteriesState(const FPlayerMasteriesState& InState)
{
    CachedCharacterId = InState.characterId;
    MasteryValues.Empty();

    for (const FMasteryEntry& Entry : InState.entries)
    {
        MasteryValues.Add(Entry.masterySlug, Entry.value);
    }

    UE_LOG(LogTemp, Log, TEXT("MasteryManager: Loaded %d mastery entries for character %d"),
        MasteryValues.Num(), CachedCharacterId);

    OnMasteriesLoaded.Broadcast(InState);
}

void UMasteryManager::ApplyMasteryUpdate(const FMasteryUpdateData& Update)
{
    MasteryValues.Add(Update.masterySlug, Update.value);

    UE_LOG(LogTemp, Log, TEXT("MasteryManager: %s -> %.1f"),
        *Update.masterySlug, Update.value);

    OnMasteryUpdated.Broadcast(Update);
}

float UMasteryManager::GetMasteryValue(const FString& MasterySlug) const
{
    const float* Found = MasteryValues.Find(MasterySlug);
    return Found ? *Found : 0.0f;
}

TArray<FMasteryEntry> UMasteryManager::GetAllMasteries() const
{
    TArray<FMasteryEntry> Result;
    Result.Reserve(MasteryValues.Num());
    for (const auto& Pair : MasteryValues)
    {
        FMasteryEntry Entry;
        Entry.masterySlug = Pair.Key;
        Entry.value       = Pair.Value;
        Result.Add(Entry);
    }
    return Result;
}
