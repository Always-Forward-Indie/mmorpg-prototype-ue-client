#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Data/DataStructs.h"
#include "BestiaryNetworkHandler.generated.h"

class UNetworkManager;
class UMyGameInstance;

// Fired when a bestiary entry response arrives from the server
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBestiaryEntryReceived, const FBestiaryEntryStruct&, Entry);

// Fired when a bestiary_tier_unlocked world_notification is received (v1.2: no MobTemplateId)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnBestiaryTierUnlocked, const FString&, MobSlug, int32, UnlockedTier, const FString&, CategorySlug);

// Fired when a getBestiaryOverview response arrives from the server
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBestiaryOverviewReceived, const TArray<FBestiaryOverviewEntryStruct>&, Entries);

// Fired on every bestiary_kill_update notification (silent, no UI)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnBestiaryKillCountUpdated, const FString&, MobSlug, int32, KillCount);

/**
* Handles getBestiaryOverview / getBestiaryEntry requests and responses,
* plus bestiary_tier_unlocked world_notification (Phase 2, §3, protocol v1.2).
*
* Usage:
*   1. Call Initialize() with valid references.
*   2. Call SubscribeToNetworkEvents() once the NetworkManager is ready.
*   3. Call RequestBestiaryOverview() to populate the bestiary mob list.
*   4. Call RequestBestiaryEntry() to query a specific mob's data.
*   5. Listen to OnBestiaryOverviewReceived / OnBestiaryEntryReceived / OnBestiaryTierUnlocked.
*
* Caching: entries are cached per session (TMap keyed by mobSlug, v1.2+).
* The cache is invalidated on bestiary_tier_unlocked for the affected mob.
*/
UCLASS(BlueprintType, Blueprintable)
class PROTOTYPING_API UBestiaryNetworkHandler : public UObject
{
    GENERATED_BODY()

public:
    UBestiaryNetworkHandler();

    UFUNCTION(BlueprintCallable, Category = "Bestiary")
    void Initialize(UMyGameInstance* InGameInstance, UNetworkManager* InNetworkManager);

    UFUNCTION(BlueprintCallable, Category = "Bestiary")
    void Shutdown();

    UFUNCTION(BlueprintCallable, Category = "Bestiary")
    void SubscribeToNetworkEvents();

    UFUNCTION(BlueprintCallable, Category = "Bestiary")
    void UnsubscribeFromNetworkEvents();

    /**
     * Send getBestiaryOverview request — returns all mobs the character has killed.
     * @param CharacterId  The owning character's ID.
     */
    UFUNCTION(BlueprintCallable, Category = "Bestiary")
    void RequestBestiaryOverview(int32 CharacterId);

    /**
     * Send getBestiaryEntry request to the chunk server (v1.2: uses mobSlug, not mobTemplateId).
     * @param CharacterId  The owning character's ID.
     * @param MobSlug      Slug identifying the mob type (e.g. "forest_wolf").
     */
    UFUNCTION(BlueprintCallable, Category = "Bestiary")
    void RequestBestiaryEntry(int32 CharacterId, const FString& MobSlug);

    /** Returns true if a cached entry exists for the given mob slug. */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Bestiary")
    bool HasCachedEntry(const FString& MobSlug) const;

    /** Returns the cached bestiary entry (empty struct if not cached). */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Bestiary")
    FBestiaryEntryStruct GetCachedEntry(const FString& MobSlug) const;

    /** Invalidates the cache for a specific mob slug. */
    UFUNCTION(BlueprintCallable, Category = "Bestiary")
    void InvalidateCacheEntry(const FString& MobSlug);

    /** Returns true if an overview arrived before any widget was bound. */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Bestiary")
    bool HasPendingOverview() const { return bHasPendingOverview; }

    /** Returns the cached pending overview and clears it. */
    UFUNCTION(BlueprintCallable, Category = "Bestiary")
    TArray<FBestiaryOverviewEntryStruct> ConsumePendingOverview();


    // Events
    UPROPERTY(BlueprintAssignable, Category = "Bestiary|Events")
    FOnBestiaryOverviewReceived OnBestiaryOverviewReceived;

    UPROPERTY(BlueprintAssignable, Category = "Bestiary|Events")
    FOnBestiaryEntryReceived OnBestiaryEntryReceived;

    UPROPERTY(BlueprintAssignable, Category = "Bestiary|Events")
    FOnBestiaryTierUnlocked OnBestiaryTierUnlocked;

    UPROPERTY(BlueprintAssignable, Category = "Bestiary|Events")
    FOnBestiaryKillCountUpdated OnBestiaryKillCountUpdated;

private:
    UFUNCTION()
    void HandleChunkServerData(const FString& ReceivedData);

    void ProcessBestiaryOverviewResponse(const FString& JsonData);
    void ProcessBestiaryEntryResponse(const FString& JsonData);
    void ProcessBestiaryTierUnlocked(const FWorldNotificationStruct& Notification);
    void ProcessBestiaryKillUpdate(const FWorldNotificationStruct& Notification);

    static FBestiaryTierStruct ParseTier(const TSharedPtr<FJsonObject>& TierObj);

    UPROPERTY()
    TObjectPtr<UMyGameInstance> GameInstance;

    UPROPERTY()
    TObjectPtr<UNetworkManager> NetworkManager;

    // Session cache: mobSlug ? entry (v1.2: slug-keyed, not template-id-keyed)
    TMap<FString, FBestiaryEntryStruct> EntryCache;

    // Pending overview: stores the last received overview before any widget bound to it.
    // Replayed in GetPendingOverview() so BestiaryWidget can fetch it on late bind.
    TArray<FBestiaryOverviewEntryStruct> PendingOverview;
    bool bHasPendingOverview = false;

    bool bIsSubscribed = false;
};

