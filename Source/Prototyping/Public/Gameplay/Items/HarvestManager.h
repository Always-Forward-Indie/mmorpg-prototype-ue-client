#pragma once

#include "CoreMinimal.h"
#include "Engine/World.h"
#include "Data/DataStructs.h"
#include "HarvestManager.generated.h"

// Forward declarations
class UNetworkManager;
class UMyGameInstance;
class ABasicMOB;
class UHarvestProgressWidget;
class UHarvestLootWidget;

// Delegates for harvest events
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnHarvestStarted, const FHarvestStartedStruct&, HarvestData);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnHarvestCompleted, const FHarvestCompleteStruct&, HarvestData);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnHarvestError, const FHarvestErrorStruct&, ErrorData);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnHarvestProgressUpdate, float, Progress);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLootPickupSuccess, const FCorpseLootPickupResponseStruct&, PickupData);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLootPickupError, const FCorpseLootPickupErrorStruct&, ErrorData);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLootInspectSuccess, const FCorpseLootInspectResponseStruct&, InspectData);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLootInspectError, const FCorpseLootInspectErrorStruct&, ErrorData);

UCLASS(BlueprintType)
class PROTOTYPING_API UHarvestManager : public UObject
{
	GENERATED_BODY()

public:
	UHarvestManager();

protected:
	// Called when the harvest manager is initialized
	void BeginPlay();

public:
	// Tick function - called manually via timer
	void Tick(float DeltaTime);

	// Initialize the harvest manager
	UFUNCTION(BlueprintCallable, Category = "Harvest")
	void Initialize(UNetworkManager* NetworkManager);

	// Set references
	UFUNCTION(BlueprintCallable, Category = "Harvest")
	void SetWorldContext(UWorld* World);

	UFUNCTION(BlueprintCallable, Category = "Harvest")
	void SetGameInstance(UMyGameInstance* GameInstance);

	// Get valid world context (use stored context)
	UWorld* GetValidWorld() const;

	// Subscribe to network events
	UFUNCTION(BlueprintCallable, Category = "Harvest")
	void SubscribeToNetworkManager();

	// Try to harvest the nearest dead mob
	UFUNCTION(BlueprintCallable, Category = "Harvest")
	void TryHarvestNearbyCorpse();

	// Harvest a specific corpse actor directly (cursor double-click).
	// Validates range before sending the request; does nothing if out of range.
	UFUNCTION(BlueprintCallable, Category = "Harvest")
	void TryHarvestSpecificCorpse(ABasicMOB* TargetCorpse);

	// Inspect loot on a nearby harvested corpse
	UFUNCTION(BlueprintCallable, Category = "Harvest")
	void TryInspectNearbyCorpseLoot();

	// Start harvest for a specific corpse
	UFUNCTION(BlueprintCallable, Category = "Harvest")
	void StartHarvest(int32 CorpseUID);

	// Inspect loot for a specific corpse
	UFUNCTION(BlueprintCallable, Category = "Harvest")
	void InspectCorpseLoot(int32 CorpseUID);

	// Pick up loot items
	UFUNCTION(BlueprintCallable, Category = "Harvest")
	void PickupLootItem(int32 ItemId, int32 Quantity);

	// Pick up all available loot
	UFUNCTION(BlueprintCallable, Category = "Harvest")
	void PickupAllLoot();

	// Cancel current harvest
	UFUNCTION(BlueprintCallable, Category = "Harvest")
	void CancelHarvest();

	// Check if currently harvesting
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Harvest")
	bool IsHarvesting() const { return bIsHarvesting; }

	// Get current harvest progress (0.0 to 1.0)
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Harvest")
	float GetHarvestProgress() const;

	// Get nearest harvestable corpse
	UFUNCTION(BlueprintCallable, Category = "Harvest")
	ABasicMOB* GetNearestHarvestableCorpse(float MaxDistance = 300.0f) const;

	// Get nearest already harvested corpse
	UFUNCTION(BlueprintCallable, Category = "Harvest")
	ABasicMOB* GetNearestHarvestedCorpse(float MaxDistance = 300.0f) const;

	// UI Management
	UFUNCTION(BlueprintCallable, Category = "Harvest UI")
	void SetHarvestProgressWidget(UHarvestProgressWidget* InProgressWidget);

	UFUNCTION(BlueprintCallable, Category = "Harvest UI")
	void SetHarvestLootWidget(UHarvestLootWidget* InLootWidget);

	UFUNCTION(BlueprintCallable, Category = "Harvest UI")
	void ShowHarvestProgress();

	UFUNCTION(BlueprintCallable, Category = "Harvest UI")
	void HideHarvestProgress();

	UFUNCTION(BlueprintCallable, Category = "Harvest UI")
	void ShowLootWindow();

	UFUNCTION(BlueprintCallable, Category = "Harvest UI")
	void HideLootWindow();

	// Event delegates
	UPROPERTY(BlueprintAssignable, Category = "Harvest Events")
	FOnHarvestStarted OnHarvestStarted;

	UPROPERTY(BlueprintAssignable, Category = "Harvest Events")
	FOnHarvestCompleted OnHarvestCompleted;

	UPROPERTY(BlueprintAssignable, Category = "Harvest Events")
	FOnHarvestError OnHarvestError;

	UPROPERTY(BlueprintAssignable, Category = "Harvest Events")
	FOnHarvestProgressUpdate OnHarvestProgressUpdate;

	UPROPERTY(BlueprintAssignable, Category = "Harvest Events")
	FOnLootPickupSuccess OnLootPickupSuccess;

	UPROPERTY(BlueprintAssignable, Category = "Harvest Events")
	FOnLootPickupError OnLootPickupError;

	UPROPERTY(BlueprintAssignable, Category = "Harvest Events")
	FOnLootInspectSuccess OnLootInspectSuccess;

	UPROPERTY(BlueprintAssignable, Category = "Harvest Events")
	FOnLootInspectError OnLootInspectError;

protected:
	// Network data processing
	UFUNCTION()
	void ProcessGameServerData(const FString& ReceivedData);

	// Send harvest start request
	void SendHarvestStartRequest(int32 CorpseUID);

	// Send harvest cancel request
	void SendHarvestCancelRequest(int32 CorpseUID);

	// Send loot pickup request
	void SendLootPickupRequest(int32 CorpseUID, const TArray<FCorpseLootPickupRequestItem>& RequestedItems);

	// Send loot inspect request
	void SendLootInspectRequest(int32 CorpseUID);

	// Update harvest progress
	void UpdateHarvestProgress(float DeltaTime);

	// Handle harvest completion
	void HandleHarvestComplete(const FHarvestCompleteStruct& HarvestData);

	// Handle loot pickup response
	void HandleLootPickupResponse(const FCorpseLootPickupResponseStruct& PickupData);

	// Handle loot inspect response
	void HandleLootInspectResponse(const FCorpseLootInspectResponseStruct& InspectData);

private:
	// References
	UPROPERTY()
	UNetworkManager* networkManager;

	UPROPERTY()
	UMyGameInstance* gameInstance;

	UPROPERTY()
	UWorld* worldContext;

	// UI References
	UPROPERTY()
	UHarvestProgressWidget* HarvestProgressWidget;

	UPROPERTY()
	UHarvestLootWidget* HarvestLootWidget;

	// Harvest state
	bool bIsHarvesting;
	int32 CurrentCorpseUID;
	float HarvestStartTime;
	float HarvestDuration;
	int64 ServerStartTime;

	// Current loot data
	TArray<FHarvestItemStruct> CurrentAvailableLoot;

	// UIDs of corpses confirmed empty by the server
	TSet<int32> KnownEmptyCorpses;

	// Settings
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings", meta = (AllowPrivateAccess = "true"))
	float MaxHarvestDistance;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings", meta = (AllowPrivateAccess = "true"))
	bool bAutoShowLootWindow;

	// Timer handle for ticking
	FTimerHandle TickTimerHandle;

	// Start/stop ticking
	void StartTicking();
	void StopTicking();

public:
	// Getters for current loot
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Harvest")
	const TArray<FHarvestItemStruct>& GetCurrentAvailableLoot() const { return CurrentAvailableLoot; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Harvest")
	int32 GetCurrentCorpseUID() const { return CurrentCorpseUID; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Harvest")
	float GetMaxHarvestDistance() const { return MaxHarvestDistance; }

	bool IsCorpseEmpty(int32 CorpseUID) const { return KnownEmptyCorpses.Contains(CorpseUID); }

	void RemoveKnownEmptyCorpse(int32 CorpseUID) { KnownEmptyCorpses.Remove(CorpseUID); }
};