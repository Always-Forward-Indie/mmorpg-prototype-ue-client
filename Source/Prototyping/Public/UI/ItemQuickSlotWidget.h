#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Data/DataStructs.h"
#include "Engine/StreamableManager.h"
#include "ItemQuickSlotWidget.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnQuickSlotUsed, int32, SlotIndex);

class UInventoryManager;

UCLASS(BlueprintType, Blueprintable)
class PROTOTYPING_API UItemQuickSlotWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void Setup(UInventoryManager* InInventoryManager, int32 InSlotIndex, const FKey& InBoundKey);

	UFUNCTION(BlueprintCallable, Category = "QuickSlot")
	void AssignItem(int32 ItemId, const FString& ItemSlug, int32 Quantity, const FString& InIconPath);

	UFUNCTION(BlueprintCallable, Category = "QuickSlot")
	void ClearSlot();

	UFUNCTION(BlueprintCallable, Category = "QuickSlot")
	void UpdateQuantity(int32 NewQuantity);

	UFUNCTION(BlueprintCallable, Category = "QuickSlot")
	void UseItem();

	/** Predictive cooldown overlay (seconds). The server enforces the real
	 *  cooldown; this is display only and refreshes from use events. */
	UFUNCTION(BlueprintCallable, Category = "QuickSlot")
	void StartCooldown(float CooldownSeconds);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "QuickSlot")
	bool IsOnCooldown() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "QuickSlot")
	bool IsEmpty() const { return SlotData.itemId == 0; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "QuickSlot")
	int32 GetItemId() const { return SlotData.itemId; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "QuickSlot")
	const FItemQuickSlotData& GetSlotData() const { return SlotData; }

	UPROPERTY(BlueprintAssignable, Category = "QuickSlot")
	FOnQuickSlotUsed OnQuickSlotUsed;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget), Category = "QuickSlot")
	class UButton* ItemButton;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidget), Category = "QuickSlot")
	class UImage* ItemIcon;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "QuickSlot")
	class UTextBlock* QuantityText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "QuickSlot")
	class UTextBlock* HotkeyText;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "QuickSlot")
	class UImage* CooldownOverlay;

	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "QuickSlot")
	class UImage* EmptySlotIcon;

private:
	UFUNCTION()
	void OnSlotClicked();

	void LoadIconBySlug(const FString& Slug, int32 ExpectedItemId);
	void SetIconTexture(UTexture2D* Texture);

	UInventoryManager* InventoryManager = nullptr;
	FItemQuickSlotData SlotData;
	TSharedPtr<FStreamableHandle> StreamableHandle;

	// Predictive cooldown display (world time; server authoritative).
	double CooldownEndTime = 0.0;
	bool bCoolingDown = false;
};
