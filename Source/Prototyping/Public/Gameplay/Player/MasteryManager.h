#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Data/DataStructs.h"
#include "MasteryManager.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMasteriesLoaded,   const FPlayerMasteriesState&, State);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMasteryUpdated,    const FMasteryUpdateData&,    Update);

/**
 * Stores the local player's weapon/skill mastery state.
 * Pure data-owner and event-bus — no networking, no UI.
 *
 * Populated on login via player_masteries (full snapshot),
 * then kept live via mastery_update (delta per flush / tier crossing).
 */
UCLASS(BlueprintType, Blueprintable)
class PROTOTYPING_API UMasteryManager : public UObject
{
    GENERATED_BODY()

public:
    /** Apply full snapshot from player_masteries packet. */
    UFUNCTION(BlueprintCallable, Category = "Mastery")
    void ApplyMasteriesState(const FPlayerMasteriesState& InState);

    /** Apply single-slot delta from mastery_update packet. */
    UFUNCTION(BlueprintCallable, Category = "Mastery")
    void ApplyMasteryUpdate(const FMasteryUpdateData& Update);

    /** Returns the current value for the given mastery slug (0.0 if unknown). */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Mastery")
    float GetMasteryValue(const FString& MasterySlug) const;

    /** Returns a copy of all entries. */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Mastery")
    TArray<FMasteryEntry> GetAllMasteries() const;

    // ---- Events ----

    /** Fired once per player_masteries (full snapshot). */
    UPROPERTY(BlueprintAssignable, Category = "Mastery|Events")
    FOnMasteriesLoaded OnMasteriesLoaded;

    /** Fired on every mastery_update delta. */
    UPROPERTY(BlueprintAssignable, Category = "Mastery|Events")
    FOnMasteryUpdated OnMasteryUpdated;

private:
    /** characterId → mastery slug → value */
    int32 CachedCharacterId = 0;
    TMap<FString, float> MasteryValues;
};
