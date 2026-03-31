#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/DataStructs.h"
#include "Data/ItemStruct.h"
#include "Utils/JSONParser.h"
#include "Gameplay/Items/DroppedItemActor.h"
#include "Kismet/GameplayStatics.h"
#include "InventoryManager.generated.h"

// Forward declarations
class UNetworkManager;
class UMyGameInstance;

// Delegates for inventory events
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInventoryUpdated, const FCharacterInventoryStruct&, Inventory);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnItemAdded, const FInventoryItemStruct&, Item, int32, NewQuantity);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnItemRemoved, const FInventoryItemStruct&, Item, int32, RemainingQuantity);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInventoryLoadComplete, bool, bSuccess);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnItemPickupAttempted, const FItemBaseStruct&, Item);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnNoItemsInRange);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class PROTOTYPING_API UInventoryManager : public UActorComponent
{
	GENERATED_BODY()

public:
	UInventoryManager();

	// Initialize the inventory manager
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void Initialize(UNetworkManager* NetworkManager);

	// Set world context and game instance
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void SetWorldContext(UWorld* World);

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void SetGameInstance(UMyGameInstance* GameInstance);

	// Subscribe to network manager events
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void SubscribeToNetworkManager();

	// Request inventory data from server
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void RequestInventoryData(int32 CharacterId);

	// Get current inventory
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory")
	FCharacterInventoryStruct GetInventory() const { return CurrentInventory; }

	// Check if item exists in inventory
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory")
	bool HasItem(int32 ItemId) const;

	// Get item by ID
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory")
	FInventoryItemStruct GetItemById(int32 ItemId) const;

	// Get item quantity
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory")
	int32 GetItemQuantity(int32 ItemId) const;

	// Get all items of specific type
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory")
	TArray<FInventoryItemStruct> GetItemsByType(const FString& ItemType) const;

	// Get all items of specific rarity
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory")
	TArray<FInventoryItemStruct> GetItemsByRarity(const FString& Rarity) const;

	// Get total number of items in inventory
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory")
	int32 GetTotalItemCount() const;

	// Check if inventory is loaded
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory")
	bool IsInventoryLoaded() const { return bInventoryLoaded; }

	// Use item (consume, equip, etc.)
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void UseItem(int32 ItemId, int32 Quantity = 1);

	// Drop item
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void DropItem(int32 ItemId, int32 Quantity = 1);

	// Events
	UPROPERTY(BlueprintAssignable, Category = "Inventory Events")
	FOnInventoryUpdated OnInventoryUpdated;

	UPROPERTY(BlueprintAssignable, Category = "Inventory Events")
	FOnItemAdded OnItemAdded;

	UPROPERTY(BlueprintAssignable, Category = "Inventory Events")
	FOnItemRemoved OnItemRemoved;

	UPROPERTY(BlueprintAssignable, Category = "Inventory Events")
	FOnInventoryLoadComplete OnInventoryLoadComplete;

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void PickupNearbyItem();

	// Get the nearest dropped item within specified distance (returns null if none found)
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory")
	ADroppedItemActor* GetNearestDroppedItem(float MaxDistance = 500.0f) const;

	// Get all dropped items within range, sorted by distance
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory")
	TArray<ADroppedItemActor*> GetAllDroppedItemsInRange(float MaxDistance = 500.0f) const;

	// Add these new events near the existing event declarations
	UPROPERTY(BlueprintAssignable, Category = "Inventory Events")
	FOnItemPickupAttempted OnItemPickupAttempted;

	UPROPERTY(BlueprintAssignable, Category = "Inventory Events")
	FOnNoItemsInRange OnNoItemsInRange;

public:
	// Add these new methods after the existing public methods

	// UI Integration methods
	UFUNCTION(BlueprintCallable, Category = "Inventory UI")
	void SetInventoryUIWidget(class UInventoryWidget* InventoryUIWidget);

	UFUNCTION(BlueprintCallable, Category = "Inventory UI")
	void ShowInventoryUI();

	UFUNCTION(BlueprintCallable, Category = "Inventory UI")
	void HideInventoryUI();

	UFUNCTION(BlueprintCallable, Category = "Inventory UI")
	void ToggleInventoryUI();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory UI")
	bool IsInventoryUIVisible() const;

	// Handle UI events
	UFUNCTION(BlueprintCallable, Category = "Inventory UI")
	void HandleSlotClicked(int32 SlotIndex, const FInventoryItemStruct& Item);

	UFUNCTION(BlueprintCallable, Category = "Inventory UI")
	void HandleSlotRightClicked(int32 SlotIndex, const FInventoryItemStruct& Item);

protected:
	// Add after existing protected methods

	// UI Widget reference
	UPROPERTY()
	class UInventoryWidget* InventoryUIWidget;

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	// Network callback function
	UFUNCTION()
	void ProcessGameServerData(const FString& ReceivedData);

	// Process inventory data from server
	void ProcessInventoryData(const FString& JsonData);
	void ProcessInventoryUpdate(const FString& JsonData);

	// Local inventory management
	void UpdateLocalInventory(const FCharacterInventoryStruct& NewInventory);
	void AddItemToInventory(const FInventoryItemStruct& Item);
	void RemoveItemFromInventory(int32 ItemId, int32 Quantity);

public:
	// Add item to local inventory (public utility function)
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void AddItemToLocalInventory(const FInventoryItemStruct& Item);

	// Send requests to server
	void SendUseItemRequest(int32 ItemId, int32 Quantity);
	void SendDropItemRequest(int32 ItemId, int32 Quantity);

	// Set the character ID this manager owns.
	// Called before SubscribeToNetworkManager so packets are filtered correctly.
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void SetOwnerCharacterId(int32 InCharacterId) { OwnerCharacterId = InCharacterId; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory")
	int32 GetOwnerCharacterId() const { return OwnerCharacterId; }

	// References
	UPROPERTY()
	UNetworkManager* networkManager;

	UPROPERTY()
	UMyGameInstance* gameInstance;

	UPROPERTY()
	UWorld* worldContext;

	// Current inventory data
	UPROPERTY()
	FCharacterInventoryStruct CurrentInventory;

	// Flag to track if inventory has been loaded
	bool bInventoryLoaded;

private:
	// Character ID this manager belongs to.
	// Packets whose characterId doesn't match are silently ignored.
	int32 OwnerCharacterId = 0;
};