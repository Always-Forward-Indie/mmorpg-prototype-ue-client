#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Data/DataStructs.h"
#include "EmoteManager.generated.h"

/**
 * Broadcast when the local player's unlocked emote list arrives from the server.
 * UI subscribes to this to rebuild the emote grid.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlayerEmotesLoaded, const FPlayerEmotesState&, State);

/**
 * Broadcast when any character in the zone plays an emote (emoteAction packet).
 * PlayerManager / BasicPlayer instances subscribe to route playback to the
 * correct UEmoteComponent.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnEmoteActionReceived,
    int32,          CharacterId,
    const FString&, EmoteSlug,
    const FString&, AnimationName);

/**
 * Runtime data-owner and event-bus for the emote system.
 * Pure state — no networking, no UI, no montage logic.
 *
 * Lifecycle:
 *   1. LoadDefinitions()     called by EmoteNetworkHandler when setEmoteDefinitionsData arrives.
 *   2. ApplyPlayerEmotes()   called by EmoteNetworkHandler when player_emotes arrives.
 *   3. DispatchEmoteAction() called by EmoteNetworkHandler when emoteAction arrives.
 */
UCLASS(BlueprintType, Blueprintable)
class PROTOTYPING_API UEmoteManager : public UObject
{
    GENERATED_BODY()

public:
    // ── Global catalog ──────────────────────────────────────────────────────

    /** Replace the global emote catalog (called when SET_EMOTE_DEFINITIONS data is loaded). */
    UFUNCTION(BlueprintCallable, Category = "Emotes")
    void LoadDefinitions(const TArray<FEmoteDefinitionData>& InDefinitions);

    /** Returns all known emote definitions, sorted by sortOrder ascending. */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Emotes")
    TArray<FEmoteDefinitionData> GetAllDefinitions() const;

    /** Find a definition by slug. Returns false when the slug is not in the catalog. */
    UFUNCTION(BlueprintCallable, Category = "Emotes")
    bool GetDefinitionBySlug(const FString& Slug, FEmoteDefinitionData& OutDef) const;

    // ── Per-player state ────────────────────────────────────────────────────

    /** Store the local player's unlocked emote list and fire OnPlayerEmotesLoaded. */
    UFUNCTION(BlueprintCallable, Category = "Emotes")
    void ApplyPlayerEmotes(const FPlayerEmotesState& InState);

    /** Returns the last-received player emotes state (valid after first player_emotes packet). */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Emotes")
    const FPlayerEmotesState& GetPlayerEmotesState() const { return CachedPlayerEmotes; }

    /**
     * Returns only the emotes for the given category slug ("general", "social", "dance").
     * Pass an empty string to get all unlocked emotes.
     */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Emotes")
    TArray<FEmoteDefinitionData> GetPlayerEmotesByCategory(const FString& Category) const;

    // ── Broadcast ───────────────────────────────────────────────────────────

    /**
     * Fire OnEmoteActionReceived — called by EmoteNetworkHandler when emoteAction arrives.
     * Not exposed to Blueprint (internal routing only).
     */
    void DispatchEmoteAction(int32 CharacterId, const FString& Slug, const FString& AnimationName);

    // ── Events ──────────────────────────────────────────────────────────────

    /** Fired when the local player's emote list is loaded or refreshed. */
    UPROPERTY(BlueprintAssignable, Category = "Emotes|Events")
    FOnPlayerEmotesLoaded OnPlayerEmotesLoaded;

    /**
     * Fired for every emoteAction broadcast from the server.
     * Listeners (PlayerManager, local player) must route to the correct UEmoteComponent.
     */
    UPROPERTY(BlueprintAssignable, Category = "Emotes|Events")
    FOnEmoteActionReceived OnEmoteActionReceived;

private:
    /** slug → definition (global catalog). */
    TMap<FString, FEmoteDefinitionData> Definitions;

    /** Last state received via player_emotes. */
    FPlayerEmotesState CachedPlayerEmotes;
};
