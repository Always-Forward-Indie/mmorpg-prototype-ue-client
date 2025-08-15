// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/ItemStruct.h"
#include "Engine/DataTable.h"
#include "ItemManager.generated.h"

// Forward declarations
class ADroppedItemActor;
class UNetworkManager;
class UMyGameInstance;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnItemsDropped, const TArray<FDroppedItemStruct>&, DroppedItems);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnItemPickedUp, const FItemBaseStruct&, PickedUpItem);

/**
 * Manager class that handles all item-related functionality
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class PROTOTYPING_API UItemManager : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UItemManager();

	// Initialize the item manager
	UFUNCTION(BlueprintCallable, Category = "Item Manager")
	void Initialize(UNetworkManager* NetworkManager);

	// Set the world context
	UFUNCTION(BlueprintCallable, Category = "Item Manager")
	void SetWorldContext(UWorld* World);

	// Set game instance
	UFUNCTION(BlueprintCallable, Category = "Item Manager")
	void SetGameInstance(UMyGameInstance* GameInstance);

	// Subscribe to network manager events
	UFUNCTION(BlueprintCallable, Category = "Item Manager")
	void SubscribeToNetworkManager();

	// Process game server data
	UFUNCTION()
	void ProcessGameServerData(const FString& ReceivedData);

	// Handle dropped items
	UFUNCTION(BlueprintCallable, Category = "Item Manager")
	void HandleItemDrop(const FItemDropResponseStruct& ItemDropResponse);

	// Spawn a dropped item actor
	UFUNCTION(BlueprintCallable, Category = "Item Manager")
	ADroppedItemActor* SpawnDroppedItem(const FDroppedItemStruct& DroppedItem);

	// Check if item exists by ID
	UFUNCTION(BlueprintCallable, Category = "Item Manager")
	bool ItemExists(UWorld* World, int32 ItemUID);

	// Attempt to pick up an item
	UFUNCTION(BlueprintCallable, Category = "Item Manager")
	void SendPickUpItemRequest(int32 ItemUID);

	// Process successful item pickup
	UFUNCTION(BlueprintCallable, Category = "Item Manager")
	void ProcessItemPickup(const FItemBaseStruct& Item);

protected:
	// Called when the component is created
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// Event dispatched when items are dropped
	UPROPERTY(BlueprintAssignable, Category = "Item Manager|Events")
	FOnItemsDropped OnItemsDropped;

	// Event dispatched when an item is picked up
	UPROPERTY(BlueprintAssignable, Category = "Item Manager|Events")
	FOnItemPickedUp OnItemPickedUp;

private:
	// Reference to the network manager
	UPROPERTY()
	UNetworkManager* networkManager;

	// Reference to the game instance
	UPROPERTY()
	UMyGameInstance* gameInstance;

	// Reference to the world context
	UPROPERTY()
	UWorld* worldContext;

	// Map of currently dropped items in the world
	UPROPERTY()
	TMap<int32, ADroppedItemActor*> DroppedItemsMap;

	// Add these methods to the UItemManager class public section
	public:
		// Load and initialize the item visuals data table
		UFUNCTION(BlueprintCallable, Category = "Item Manager")
		void LoadItemVisualsDataTable(UDataTable* InItemVisualsTable);

		// Get visual data for an item by slug
		UFUNCTION(BlueprintCallable, Category = "Item Manager")
		FItemVisualData GetItemVisualDataBySlug(const FString& ItemSlug);

		// Add this to the private section
	private:
		// The data table containing item visual information
		UPROPERTY()
		UDataTable* ItemVisualsDataTable;

		// Cache for quick lookup of visual data by slug
		UPROPERTY()
		TMap<FString, FItemVisualData> ItemVisualsCache;
};