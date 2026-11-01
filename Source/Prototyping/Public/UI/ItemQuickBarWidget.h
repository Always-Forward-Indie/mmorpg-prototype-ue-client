#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Data/DataStructs.h"
#include "ItemQuickBarWidget.generated.h"

class UItemQuickSlotWidget;
class UInventoryManager;
class UMyGameInstance;

UCLASS(BlueprintType, Blueprintable)
class PROTOTYPING_API UItemQuickBarWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "QuickBar")
	void InitQuickBar(UInventoryManager* InInventoryManager, int32 NumSlots = 5);

	UFUNCTION(BlueprintCallable, Category = "QuickBar")
	void AssignItemToSlot(int32 SlotIndex, int32 ItemId, const FString& ItemSlug, int32 Quantity, const FString& IconPath);

	UFUNCTION(BlueprintCallable, Category = "QuickBar")
	void ClearSlot(int32 SlotIndex);

	UFUNCTION(BlueprintCallable, Category = "QuickBar")
	void UseSlot(int32 SlotIndex);

	/** Toggle a usable item on the bar: remove when present, else occupy
	 *  the first empty slot (no-op + error sound when full). */
	UFUNCTION(BlueprintCallable, Category = "QuickBar")
	void ToggleQuickbarItem(const FInventoryItemStruct& Item);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "QuickBar")
	bool HasItem(int32 ItemId) const { return FindSlotWithItem(ItemId) != INDEX_NONE; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "QuickBar")
	int32 FindSlotWithItem(int32 ItemId) const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "QuickBar")
	int32 FindFirstEmptySlot() const;

	UFUNCTION(BlueprintCallable, Category = "QuickBar")
	void RefreshFromInventory();

	UFUNCTION(BlueprintCallable, Category = "QuickBar")
	void HandleInventoryUpdated(const FCharacterInventoryStruct& Inventory);

	UFUNCTION()
	void HandleItemCooldown(int32 ItemId, int32 CooldownSeconds);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "QuickBar")
	int32 GetSlotCount() const { return Slots.Num(); }

protected:
	virtual void NativeConstruct() override;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget), Category = "QuickBar")
	class UHorizontalBox* SlotsBox;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QuickBar")
	TSubclassOf<UItemQuickSlotWidget> SlotWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QuickBar")
	int32 DefaultSlotCount = 5;

private:
	UFUNCTION()
	void OnSlotUsed(int32 SlotIndex);

	UPROPERTY()
	TArray<UItemQuickSlotWidget*> Slots;

	UPROPERTY()
	UInventoryManager* InventoryManager = nullptr;

	void SaveSlotsState();
	void LoadSlotsState();
};
