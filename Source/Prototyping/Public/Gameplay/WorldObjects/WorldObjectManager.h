#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Data/WIODataStructs.h"
#include "WorldObjectManager.generated.h"

// Forward declarations
class UNetworkManager;
class UMyGameInstance;
class AWorldInteractiveObjectActor;

// ─────────────────────────────────────────────────────────────────────────────
// Delegates
// ─────────────────────────────────────────────────────────────────────────────

// Fired after all WIO actors are spawned from the initial packet
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnWorldObjectsSpawned);

// Fired when a single WIO actor is spawned
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWorldObjectActorSpawned, AWorldInteractiveObjectActor*, Actor);

// Fired when an interaction result is received from the server
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWIOInteractResult, const FWIOInteractResult&, Result);

// Fired when an object state changes (broadcast)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWIOStateChanged, const FWIOStateUpdate&, StateUpdate);

// Fired when a channel is cancelled
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWIOChannelCancelled, int32, ObjectId);

// Fired when local interaction starts (request sent)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWIOInteractionStarted, int32, ObjectId);

/**
 * WorldObjectManager
 *
 * Owns WIO actor lifecycle: spawning, registry, state tracking, and client→server requests.
 * Follows the same Manager pattern as NPCManager / MOBManager / DialogueManager.
 */
UCLASS(BlueprintType, Blueprintable)
class PROTOTYPING_API UWorldObjectManager : public UObject
{
	GENERATED_BODY()

public:
	UWorldObjectManager();

	// ─── Initialization ──────────────────────────────────────────────────

	UFUNCTION(BlueprintCallable, Category = "WIO Manager")
	void Initialize(UNetworkManager* InNetworkManager, UMyGameInstance* InGameInstance);

	UFUNCTION(BlueprintCallable, Category = "WIO Manager")
	void SetWorldContext(UWorld* InWorld);

	// ─── Actor lifecycle ─────────────────────────────────────────────────

	/** Spawn actors from the spawnWorldObjects packet. Clears previous actors first. */
	UFUNCTION(BlueprintCallable, Category = "WIO Manager")
	void SpawnWorldObjects(const TArray<FWorldObjectData>& Objects);

	/** Remove all WIO actors from the world and clear the registry. */
	UFUNCTION(BlueprintCallable, Category = "WIO Manager")
	void ClearWorldState();

	// ─── Client → Server requests ────────────────────────────────────────

	/** Send worldObjectInteract to chunk server. */
	UFUNCTION(BlueprintCallable, Category = "WIO Manager")
	void RequestInteract(int32 ObjectId);

	/** Send worldObjectChannelCancel to chunk server. */
	UFUNCTION(BlueprintCallable, Category = "WIO Manager")
	void RequestCancelChannel(int32 ObjectId);

	/** Cancels the currently active channel (convenience wrapper). */
	UFUNCTION(BlueprintCallable, Category = "WIO Manager")
	void CancelActiveChannel();

	// ─── Server → Client handlers (called by WIONetworkHandler) ─────────

	void HandleSpawnWorldObjects(const TArray<FWorldObjectData>& Objects);
	void HandleInteractResult(const FWIOInteractResult& Result);
	void HandleStateUpdate(const FWIOStateUpdate& Update);
	void HandleChannelCancelled(int32 ObjectId);

	// ─── Queries ─────────────────────────────────────────────────────────

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "WIO Manager")
	AWorldInteractiveObjectActor* GetObjectActorById(int32 ObjectId) const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "WIO Manager")
	TArray<AWorldInteractiveObjectActor*> GetAllObjectActors() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "WIO Manager")
	bool IsObjectRegistered(int32 ObjectId) const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "WIO Manager")
	int32 GetObjectCount() const { return ObjectRegistry.Num(); }

	/** Returns the object data registry for lookups (e.g. for UI localization). */
	const TMap<int32, FWorldObjectData>& GetObjectDataRegistry() const { return ObjectDataRegistry; }

	/** True if we are currently channeling on any object. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "WIO Manager")
	bool IsChanneling() const { return ActiveChannelObjectId != 0; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "WIO Manager")
	int32 GetActiveChannelObjectId() const { return ActiveChannelObjectId; }

	/** Returns the channeling progress [0..1] based on local timer. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "WIO Manager")
	float GetChannelProgress() const;

	// ─── Configuration ──────────────────────────────────────────────────

	/** DataTable of FWIODefinitionRow — maps slug to actor class & visuals. */
	UFUNCTION(BlueprintCallable, Category = "WIO Manager")
	void SetDefinitionTable(UDataTable* InTable) { WIODefinitionTable = InTable; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "WIO Manager")
	UDataTable* GetDefinitionTable() const { return WIODefinitionTable; }

	/** Default actor class when no DataTable row found for a slug. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WIO Manager Settings", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<AWorldInteractiveObjectActor> DefaultActorClass;

	// ─── Events ──────────────────────────────────────────────────────────

	UPROPERTY(BlueprintAssignable, Category = "WIO Manager Events")
	FOnWorldObjectsSpawned OnWorldObjectsSpawned;

	UPROPERTY(BlueprintAssignable, Category = "WIO Manager Events")
	FOnWorldObjectActorSpawned OnWorldObjectActorSpawned;

	UPROPERTY(BlueprintAssignable, Category = "WIO Manager Events")
	FOnWIOInteractResult OnInteractResult;

	UPROPERTY(BlueprintAssignable, Category = "WIO Manager Events")
	FOnWIOStateChanged OnStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "WIO Manager Events")
	FOnWIOChannelCancelled OnChannelCancelled;

	UPROPERTY(BlueprintAssignable, Category = "WIO Manager Events")
	FOnWIOInteractionStarted OnInteractionStarted;

private:
	AWorldInteractiveObjectActor* SpawnSingleObject(const FWorldObjectData& Data);

	UPROPERTY()
	UWorld* WorldContext = nullptr;

	UPROPERTY()
	UNetworkManager* NetworkManager = nullptr;

	UPROPERTY()
	UMyGameInstance* GameInstance = nullptr;

	UPROPERTY()
	TMap<int32, TWeakObjectPtr<AWorldInteractiveObjectActor>> ObjectRegistry;

	/** Cached server data per object ID — used by UI for localization lookups. */
	TMap<int32, FWorldObjectData> ObjectDataRegistry;

	UPROPERTY()
	UDataTable* WIODefinitionTable = nullptr;

	// Channeling state
	int32   ActiveChannelObjectId = 0;
	float   ChannelDuration       = 0.f;
	double  ChannelStartTime      = 0.0;

	// Interaction cooldown (prevent double-send)
	double  LastInteractRequestTime = 0.0;
	static constexpr double InteractCooldownSec = 0.5;

	bool bIsInitialized = false;
};
