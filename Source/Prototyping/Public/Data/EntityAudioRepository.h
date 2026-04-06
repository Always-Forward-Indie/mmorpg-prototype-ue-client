#pragma once
#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Engine/DataTable.h"
#include "Data/DataStructs.h"
#include "EntityAudioRepository.generated.h"

/**
 * Repository for FEntityAudioProfile rows loaded from DT_EntityAudioProfiles.
 *
 * Assign the DataTable in the GameInstance Blueprint ("Entity Audio Profiles Table")
 * and then call FindProfile() passing any FName row key, e.g. "warrior_m", "wolf",
 * "goblin_shaman".  Returns nullptr when the profile is not found so callers can
 * fall back gracefully.
 *
 * Follows the same pattern as USkillDefinitionRepository (no runtime caching needed
 * — FindRow is O(1) via the internal TMap that UDataTable maintains itself).
 */
UCLASS(BlueprintType)
class PROTOTYPING_API UEntityAudioRepository : public UObject
{
    GENERATED_BODY()

public:
    /** Bind to a DataTable asset. Call once during GameInstance::Init. */
    UFUNCTION(BlueprintCallable, Category = "Entity Audio Repository")
    void Initialize(UDataTable* InTable);

    /**
     * Bind the per-entity per-skill voice override table (DT_EntitySkillVoiceOverrides).
     * Optional — call in GameInstance::Init after Initialize().
     * Row key format: "{audioProfileId}|{skillSlug}"  e.g. "warrior_m|fireball"
     */
    UFUNCTION(BlueprintCallable, Category = "Entity Audio Repository")
    void InitializeSkillVoiceOverrides(UDataTable* InTable);

    /** Returns the audio profile for the given RowName, or nullptr if not found. */
    const FEntityAudioProfile* FindProfile(FName ProfileId) const;

    /**
     * Returns the per-entity per-skill voice override row, or nullptr if not found.
     * Builds composite key "{ProfileId}|{SkillSlug}" internally.
     * Returns nullptr when the table was not assigned or the row does not exist —
     * callers fall through to the generic VoiceCastStart[] / VoiceCastRelease[] pool.
     */
    const FEntitySkillVoiceOverride* FindSkillVoiceOverride(FName ProfileId, FName SkillSlug) const;

    /** True when ProfileId exists as a row in the bound table. */
    UFUNCTION(BlueprintCallable, Category = "Entity Audio Repository")
    bool HasProfile(FName ProfileId) const;

    /** True when Initialize() has been called with a valid table. */
    UFUNCTION(BlueprintCallable, Category = "Entity Audio Repository")
    bool IsReady() const { return bIsInitialized && Table.Get() != nullptr; }

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Repository")
    TObjectPtr<UDataTable> Table;

    /** Optional: DT_EntitySkillVoiceOverrides — per-entity per-skill voice pools. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Repository")
    TObjectPtr<UDataTable> SkillVoiceTable;

private:
    bool bIsInitialized = false;
};
