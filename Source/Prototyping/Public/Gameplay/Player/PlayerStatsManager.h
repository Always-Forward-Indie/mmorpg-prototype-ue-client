#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Data/DataStructs.h"
#include "PlayerStatsManager.generated.h"

// Fired whenever a fresh stats_update packet is applied
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlayerStatsUpdated, const FPlayerStatsUpdateStruct&, NewStats);

// Fired when a dedicated setPlayerActiveEffects packet arrives
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnActiveEffectsReceived, const TArray<FActiveEffectEntry>&, Effects);

/**
 * Holds the latest server-authoritative snapshot of the local player's stats.
 * Pure data-owner / event-bus — no networking, no UI logic.
 */
UCLASS(BlueprintType, Blueprintable)
class PROTOTYPING_API UPlayerStatsManager : public UObject
{
    GENERATED_BODY()

public:
    // Apply a stats snapshot received from the server
    UFUNCTION(BlueprintCallable, Category = "Player Stats")
    void ApplyStatsUpdate(const FPlayerStatsUpdateStruct& InStats);

    // Apply a dedicated setPlayerActiveEffects packet (merges into CachedStats.activeEffects)
    UFUNCTION(BlueprintCallable, Category = "Player Stats")
    void ApplyActiveEffects(const TArray<FActiveEffectEntry>& InEffects);

    // Latest cached stats (valid after the first stats_update is received)
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Player Stats")
    const FPlayerStatsUpdateStruct& GetCachedStats() const { return CachedStats; }

    // Convenience — find an attribute by slug; returns false if not found
    UFUNCTION(BlueprintCallable, Category = "Player Stats")
    bool GetAttribute(const FString& Slug, FStatAttributeEntry& OutEntry) const;

    // Fired on every successful ApplyStatsUpdate
    UPROPERTY(BlueprintAssignable, Category = "Player Stats|Events")
    FOnPlayerStatsUpdated OnStatsUpdated;

    // Fired when a setPlayerActiveEffects packet is processed
    UPROPERTY(BlueprintAssignable, Category = "Player Stats|Events")
    FOnActiveEffectsReceived OnActiveEffectsReceived;

private:
    FPlayerStatsUpdateStruct CachedStats;
};
