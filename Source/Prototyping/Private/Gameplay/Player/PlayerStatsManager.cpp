#include "Gameplay/Player/PlayerStatsManager.h"


void UPlayerStatsManager::ApplyStatsUpdate(const FPlayerStatsUpdateStruct& InStats)
{
    if (InStats.characterId <= 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("PlayerStatsManager: Ignoring stats update with invalid characterId"));
        return;
    }

    // Partial-packet merge: the server sends focused packets (e.g. regen ticks
    // contain only health/mana, combat hits may omit experience, etc.).
    // We preserve the last-known value for any field the server left at its
    // zero-default so downstream widgets always see a coherent snapshot.

    const bool bHasExperience = (InStats.experienceCurrent > 0 || InStats.experienceNextLevel > 0
                                 || InStats.experienceLevelStart > 0);
    const bool bHasAttributes = (InStats.attributes.Num() > 0);
    const bool bHasEffects    = (InStats.activeEffects.Num() > 0);
    const bool bHasWeight     = (InStats.weightMax > 0.f);
    const bool bHasLevel      = (InStats.level > 0);

    CachedStats.characterId   = InStats.characterId;
    CachedStats.healthCurrent = InStats.healthCurrent;
    CachedStats.healthMax     = InStats.healthMax > 0 ? InStats.healthMax : CachedStats.healthMax;
    CachedStats.manaCurrent   = InStats.manaCurrent;
    CachedStats.manaMax       = InStats.manaMax > 0   ? InStats.manaMax   : CachedStats.manaMax;

    if (bHasLevel)
        CachedStats.level = InStats.level;

    if (bHasExperience)
    {
        CachedStats.experienceCurrent    = InStats.experienceCurrent;
        CachedStats.experienceLevelStart = InStats.experienceLevelStart;
        CachedStats.experienceNextLevel  = InStats.experienceNextLevel;
        CachedStats.experienceDebt       = InStats.experienceDebt;
    }

    if (bHasAttributes)
    {
        CachedStats.attributes      = InStats.attributes;
        // freeSkillPoints is only included in full stat packets (same conditions as attributes)
        CachedStats.freeSkillPoints = InStats.freeSkillPoints;

        // Passive effects (e.g. mana_shield) raise the *effective* max HP/MP above the base
        // value that arrives in the health/mana sub-objects.  Override healthMax/manaMax here
        // so the HUD bars always display the correct, effect-adjusted maximums.
        for (const FStatAttributeEntry& Attr : CachedStats.attributes)
        {
            if (Attr.slug == TEXT("max_health") && Attr.effective > 0)
                CachedStats.healthMax = Attr.effective;
            else if (Attr.slug == TEXT("max_mana") && Attr.effective > 0)
                CachedStats.manaMax = Attr.effective;
        }
    }

    // Replace effects when a full packet arrives (bHasAttributes) OR when a focused
    // effects-only packet arrives.  Using "bHasAttributes || bHasEffects" ensures that
    // an explicit empty activeEffects array (e.g. after removing the last title/buff)
    // correctly clears the cached list on the next full stats_update.
    if (bHasAttributes || bHasEffects)
        CachedStats.activeEffects = InStats.activeEffects;

    if (bHasWeight)
    {
        CachedStats.weightCurrent = InStats.weightCurrent;
        CachedStats.weightMax     = InStats.weightMax;
    }

    UE_LOG(LogTemp, Warning, TEXT("[EFFECTS] PlayerStatsManager::ApplyStatsUpdate: broadcasting charId=%d effects=%d boundListeners=%d"),
        CachedStats.characterId, CachedStats.activeEffects.Num(), OnStatsUpdated.IsBound() ? 1 : 0);
    OnStatsUpdated.Broadcast(CachedStats);

    UE_LOG(LogTemp, Log, TEXT("PlayerStatsManager: Stats applied - CharId=%d Lv=%d HP=%d/%d MP=%d/%d XP=%d[%d-%d] SP=%d Attrs=%d Effects=%d"),
        CachedStats.characterId, CachedStats.level,
        CachedStats.healthCurrent, CachedStats.healthMax,
        CachedStats.manaCurrent,   CachedStats.manaMax,
        CachedStats.experienceCurrent, CachedStats.experienceLevelStart, CachedStats.experienceNextLevel,
        CachedStats.freeSkillPoints,
        CachedStats.attributes.Num(), CachedStats.activeEffects.Num());
}


void UPlayerStatsManager::ApplyActiveEffects(const TArray<FActiveEffectEntry>& InEffects)
{
    // Replace only the activeEffects slice; vitals/attributes come from stats_update
    CachedStats.activeEffects = InEffects;
    OnActiveEffectsReceived.Broadcast(CachedStats.activeEffects);

    UE_LOG(LogTemp, Log, TEXT("PlayerStatsManager: ActiveEffects applied � %d effects"), InEffects.Num());
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
