#include "Data/EntityAudioRepository.h"
#include "Engine/DataTable.h"

void UEntityAudioRepository::Initialize(UDataTable* InTable)
{
    Table = InTable;
    bIsInitialized = true;

    if (!Table.Get())
    {
        UE_LOG(LogTemp, Warning,
            TEXT("UEntityAudioRepository::Initialize — DataTable is null. "
                 "Assign DT_EntityAudioProfiles in the GameInstance Blueprint."));
    }
    else
    {
        UE_LOG(LogTemp, Log,
            TEXT("UEntityAudioRepository: initialized with %d row(s) from '%s'."),
            Table->GetRowNames().Num(),
            *Table->GetName());
    }
}

const FEntityAudioProfile* UEntityAudioRepository::FindProfile(FName ProfileId) const
{
    if (!IsReady() || ProfileId.IsNone())
    {
        return nullptr;
    }
    return Table->FindRow<FEntityAudioProfile>(ProfileId, TEXT("EntityAudioRepository::FindProfile"));
}

bool UEntityAudioRepository::HasProfile(FName ProfileId) const
{
    return FindProfile(ProfileId) != nullptr;
}

void UEntityAudioRepository::InitializeSkillVoiceOverrides(UDataTable* InTable)
{
    SkillVoiceTable = InTable;

    if (!SkillVoiceTable.Get())
    {
        // Allowed to be null — feature is optional, log at Verbose to avoid spam.
        UE_LOG(LogTemp, Verbose,
            TEXT("UEntityAudioRepository::InitializeSkillVoiceOverrides — no table assigned. "
                 "Per-entity per-skill voice overrides are disabled."));
    }
    else
    {
        UE_LOG(LogTemp, Log,
            TEXT("UEntityAudioRepository: skill voice overrides initialized with %d row(s) from '%s'."),
            SkillVoiceTable->GetRowNames().Num(),
            *SkillVoiceTable->GetName());
    }
}

const FEntitySkillVoiceOverride* UEntityAudioRepository::FindSkillVoiceOverride(
    FName ProfileId, FName SkillSlug) const
{
    if (!SkillVoiceTable.Get() || ProfileId.IsNone() || SkillSlug.IsNone())
    {
        return nullptr;
    }
    // Composite key: "warrior_m|fireball"
    const FName CompositeKey(*FString::Printf(TEXT("%s|%s"),
        *ProfileId.ToString(), *SkillSlug.ToString()));
    return SkillVoiceTable->FindRow<FEntitySkillVoiceOverride>(
        CompositeKey, TEXT("EntityAudioRepository::FindSkillVoiceOverride"), false);
}
