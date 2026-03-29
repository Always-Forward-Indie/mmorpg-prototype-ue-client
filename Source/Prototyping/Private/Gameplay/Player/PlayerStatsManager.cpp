#include "Gameplay/Player/PlayerStatsManager.h"

void UPlayerStatsManager::ApplyStatsUpdate(const FPlayerStatsUpdateStruct& InStats)
{
    if (InStats.characterId <= 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("PlayerStatsManager: Ignoring stats update with invalid characterId"));
        return;
    }

    CachedStats = InStats;
    OnStatsUpdated.Broadcast(CachedStats);

    UE_LOG(LogTemp, Log, TEXT("PlayerStatsManager: Stats applied — CharId=%d Lv=%d HP=%d/%d MP=%d/%d Attrs=%d Effects=%d"),
        CachedStats.characterId, CachedStats.level,
        CachedStats.healthCurrent, CachedStats.healthMax,
        CachedStats.manaCurrent,  CachedStats.manaMax,
        CachedStats.attributes.Num(), CachedStats.activeEffects.Num());
}

void UPlayerStatsManager::ApplyActiveEffects(const TArray<FActiveEffectEntry>& InEffects)
{
    // Replace only the activeEffects slice; vitals/attributes come from stats_update
    CachedStats.activeEffects = InEffects;
    OnActiveEffectsReceived.Broadcast(CachedStats.activeEffects);

    UE_LOG(LogTemp, Log, TEXT("PlayerStatsManager: ActiveEffects applied — %d effects"), InEffects.Num());
}


bool UPlayerStatsManager::GetAttribute(const FString& Slug, FStatAttributeEntry& OutEntry) const
{
    for (const FStatAttributeEntry& A : CachedStats.attributes)
    {
        if (A.slug.Equals(Slug, ESearchCase::IgnoreCase))
        {
            OutEntry = A;
            return true;
        }
    }
    return false;
}
