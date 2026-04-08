#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Data/DataStructs.h"
#include "TitleManager.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTitlesUpdated, const FPlayerTitlesState&, State);

/**
 * Stores the local player's earned titles and the currently equipped title.
 * Pure data-owner and event-bus — no networking, no UI.
 *
 * Populated on login or on demand via player_titles_update.
 * Keeps state live after equipTitle responses.
 */
UCLASS(BlueprintType, Blueprintable)
class PROTOTYPING_API UTitleManager : public UObject
{
    GENERATED_BODY()

public:
    /** Apply full title state from player_titles_update packet. */
    UFUNCTION(BlueprintCallable, Category = "Titles")
    void ApplyTitlesState(const FPlayerTitlesState& InState);

    /** Returns the cached state (valid after the first player_titles_update). */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Titles")
    const FPlayerTitlesState& GetCachedState() const { return CachedState; }

    /** Slug of the currently equipped title; empty string if none. */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Titles")
    FString GetEquippedTitleSlug() const { return CachedState.equippedTitleSlug; }

    /** Returns all earned titles. */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Titles")
    TArray<FTitleEntry> GetEarnedTitles() const { return CachedState.earnedTitles; }

    /** Fired whenever the state is updated (login snapshot or after equip). */
    UPROPERTY(BlueprintAssignable, Category = "Titles|Events")
    FOnTitlesUpdated OnTitlesUpdated;

private:
    FPlayerTitlesState CachedState;
};
