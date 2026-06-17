#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Data/DataStructs.h"
#include "Utils/JSONParser.h"
#include "Networking/NetworkManager.h"
#include "Gameplay/NPCs/BasicNPC.h"
#include "Kismet/GameplayStatics.h"
#include "NPCManager.generated.h"

// Forward declarations
class UMyGameInstance;

// Delegates for NPC events
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNPCSpawned, ABasicNPC*, SpawnedNPC);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNPCRemoved, int32, NPCId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNPCDataReceived, const FNPCSpawnDataStruct&, NPCData);

/**
 * Manager class responsible for handling NPC spawning, management, and network events
 * Follows SOLID principles and integrates with existing architecture
 */
UCLASS(BlueprintType, Blueprintable)
class PROTOTYPING_API UNPCManager : public UObject
{
	GENERATED_BODY()

public:
	UNPCManager(const FObjectInitializer& ObjectInitializer);

	// Initialization
	UFUNCTION(BlueprintCallable, Category = "NPC Manager")
	void Initialize(UNetworkManager* NetworkManager);

	UFUNCTION(BlueprintCallable, Category = "NPC Manager")
	void SetWorldContext(UWorld* World);

	UFUNCTION(BlueprintCallable, Category = "NPC Manager")
	void SetGameInstance(UMyGameInstance* GameInstance);

	// Network subscription
	UFUNCTION(BlueprintCallable, Category = "NPC Manager")
	void SubscribeToNetworkManager();

	// NPC management
	UFUNCTION(BlueprintCallable, Category = "NPC Manager")
	void SpawnNPC(const FNPCStruct& NPCData);

	UFUNCTION(BlueprintCallable, Category = "NPC Manager")
	void SpawnNPCs(const TArray<FNPCStruct>& NPCsData);

	UFUNCTION(BlueprintCallable, Category = "NPC Manager")
	void RemoveNPC(int32 NPCId);

	UFUNCTION(BlueprintCallable, Category = "NPC Manager")
	void RemoveAllNPCs();

	// NPC queries
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "NPC Manager")
	ABasicNPC* GetNPCById(int32 NPCId) const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "NPC Manager")
	TArray<ABasicNPC*> GetAllNPCs() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "NPC Manager")
	TArray<ABasicNPC*> GetNPCsByType(const FString& NPCType) const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "NPC Manager")
	TArray<ABasicNPC*> GetInteractableNPCs() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "NPC Manager")
	int32 GetNPCCount() const { return SpawnedNPCs.Num(); }

	// Utility methods
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "NPC Manager")
	bool NPCExists(int32 NPCId) const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "NPC Manager")
	ABasicNPC* GetNearestNPC(const FVector& Location, float MaxDistance = 1000.0f) const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "NPC Manager")
	TArray<ABasicNPC*> GetNPCsInRadius(const FVector& Location, float Radius) const;

	// Network request methods
	UFUNCTION(BlueprintCallable, Category = "NPC Manager")
	void RequestNPCData(const FClientDataStruct& ClientData);

	// Configuration access
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "NPC Manager")
	UDataTable* GetNPCDefinitionTable() const { return NPCDefinitionTable; }

	UFUNCTION(BlueprintCallable, Category = "NPC Manager")
	void SetNPCDefinitionTable(UDataTable* InNPCDefinitionTable) { NPCDefinitionTable = InNPCDefinitionTable; }

	// Events
	UPROPERTY(BlueprintAssignable, Category = "NPC Manager Events")
	FOnNPCSpawned OnNPCSpawned;

	UPROPERTY(BlueprintAssignable, Category = "NPC Manager Events")
	FOnNPCRemoved OnNPCRemoved;

	UPROPERTY(BlueprintAssignable, Category = "NPC Manager Events")
	FOnNPCDataReceived OnNPCDataReceived;

protected:
	// Network event handler
	UFUNCTION()
	void ProcessGameServerData(const FString& ReceivedData);

	// Internal spawn logic
	ABasicNPC* CreateNPCActor(const FNPCStruct& NPCData);

	// Validation
	bool ValidateNPCData(const FNPCStruct& NPCData) const;

	// Cleanup
	void CleanupInvalidNPCs();

private:
	// Core references
	UPROPERTY()
	UWorld* worldContext;

	UPROPERTY()
	UNetworkManager* networkManager;

	UPROPERTY()
	UMyGameInstance* gameInstance;

	// NPC storage
	UPROPERTY()
	TMap<int32, ABasicNPC*> SpawnedNPCs;

	// Configuration
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC Manager Settings", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<ABasicNPC> DefaultNPCClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC Manager Settings", meta = (AllowPrivateAccess = "true"))
	class UDataTable* NPCDefinitionTable;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC Manager Settings", meta = (AllowPrivateAccess = "true"))
	float DefaultSpawnHeight = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC Manager Settings", meta = (AllowPrivateAccess = "true"))
	bool bAutoCleanupInvalidNPCs = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC Manager Settings", meta = (AllowPrivateAccess = "true"))
	float CleanupInterval = 30.0f;

	TArray<FNPCStruct> PendingNPCSpawns;
	void FlushPendingSpawns();

	bool bIsInitialized = false;
	FTimerHandle CleanupTimerHandle;
};