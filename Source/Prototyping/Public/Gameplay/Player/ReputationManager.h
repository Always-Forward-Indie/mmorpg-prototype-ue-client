#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Data/DataStructs.h"
#include "ReputationManager.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnReputationsLoaded,  const FPlayerReputationsState&, State);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnReputationUpdated,  const FReputationUpdateData&,   Update);

/**
 * Stores the local player's faction reputation state.
 * Pure data-owner and event-bus — no networking, no UI.
 *
 * Populated on login via player_reputations (full snapshot),
 * then kept live via reputation_update (delta on every change).
 */
UCLASS(BlueprintType, Blueprintable)
class PROTOTYPING_API UReputationManager : public UObject
{
    GENERATED_BODY()

public:
    /** Apply full snapshot from player_reputations packet. */
    UFUNCTION(BlueprintCallable, Category = "Reputation")
    void ApplyReputationsState(const FPlayerReputationsState& InState);

    /** Apply single-faction delta from reputation_update packet. */
    UFUNCTION(BlueprintCallable, Category = "Reputation")
    void ApplyReputationUpdate(const FReputationUpdateData& Update);

    /** Returns the current reputation entry for a faction (default if unknown). */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Reputation")
    FReputationEntry GetReputation(const FString& FactionSlug) const;

    /** Returns a copy of all reputation entries. */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Reputation")
    TArray<FReputationEntry> GetAllReputations() const;

    // ---- Events ----

    /** Fired once per player_reputations (full snapshot). */
    UPROPERTY(BlueprintAssignable, Category = "Reputation|Events")
    FOnReputationsLoaded OnReputationsLoaded;

    /** Fired on every reputation_update delta. */
    UPROPERTY(BlueprintAssignable, Category = "Reputation|Events")
    FOnReputationUpdated OnReputationUpdated;

private:
    int32 CachedCharacterId = 0;
    /** factionSlug → FReputationEntry */
    TMap<FString, FReputationEntry> ReputationMap;
};
