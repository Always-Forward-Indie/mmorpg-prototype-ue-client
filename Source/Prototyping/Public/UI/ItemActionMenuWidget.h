#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/VerticalBox.h"
#include "Components/TextBlock.h"
#include "Data/DataStructs.h"
#include "ItemActionMenuWidget.generated.h"

// Forward declarations
class UInventoryManager;
class UEquipmentManager;
class UItemActionRowWidget;

UENUM(BlueprintType)
enum class EItemContextAction : uint8
{
	Equip    UMETA(DisplayName = "Equip"),
	Unequip  UMETA(DisplayName = "Unequip"),
	Use      UMETA(DisplayName = "Use"),
	Drop     UMETA(DisplayName = "Drop"),
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnItemContextActionSelected, EItemContextAction, Action, const FInventoryItemStruct&, Item);

/**
 * Context menu widget shown on RMB click on an inventory slot.
 * Subclass this in Blueprint, add a VerticalBox named "ActionList",
 * and assign the widget class in InventoryWidget details.
 *
 * Input handling: the menu itself captures LMB on MouseDown and fires
 * the action on MouseUp. RMB events are fully consumed so they never
 * re-trigger a new menu open while this one is visible.
 */
UCLASS()
class PROTOTYPING_API UItemActionMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UItemActionMenuWidget(const FObjectInitializer& ObjectInitializer);

	// Show the menu for a specific item at the given screen position
	UFUNCTION(BlueprintCallable, Category = "Item Context Menu")
	void ShowForItem(const FInventoryItemStruct& InItem, FVector2D ScreenPosition);

	// Hide the menu
	UFUNCTION(BlueprintCallable, Category = "Item Context Menu")
	void HideMenu();

	// Bind managers so the menu can dispatch requests directly
	UFUNCTION(BlueprintCallable, Category = "Item Context Menu")
	void SetManagers(UInventoryManager* InInventoryManager, UEquipmentManager* InEquipmentManager);

	// Execute an action for the current item
	void ExecuteAction(EItemContextAction Action);

	// Fired when the player picks an action; InventoryWidget listens to this
	UPROPERTY(BlueprintAssignable, Category = "Item Context Menu Events")
	FOnItemContextActionSelected OnActionSelected;

protected:
	virtual void NativeConstruct() override;

	// All mouse input is handled here - the menu is a single capture target
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	// Build action rows based on item flags; called inside ShowForItem
	UFUNCTION(BlueprintCallable, Category = "Item Context Menu")
	void RebuildActions();

	// Add a single labeled row
	void AddActionRow(const FText& Label, EItemContextAction Action);

protected:
	// Bind this VerticalBox in your Blueprint widget
	UPROPERTY(meta = (BindWidget))
	UVerticalBox* ActionList;

	// Widget class for each action row.  Assign a Blueprint subclass of
	// UItemActionRowWidget in your derived Blueprint to customise the
	// appearance of every row.  Leave empty to use the default C++ row.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance")
	TSubclassOf<UItemActionRowWidget> ActionRowWidgetClass;

	// Current item the menu was opened for
	UPROPERTY(BlueprintReadOnly, Category = "Item Context Menu")
	FInventoryItemStruct CurrentItem;

	UPROPERTY()
	UInventoryManager* InventoryManager = nullptr;

	UPROPERTY()
	UEquipmentManager* EquipmentManager = nullptr;

private:
	// Parallel array: action per child index in ActionList
	TArray<EItemContextAction> RowActions;

	// Index of the row the LMB was pressed on (-1 = none)
	int32 PressedRowIndex = -1;

	// Returns the ActionList child index under the given absolute screen position,
	// or -1 if no child is hit.
	int32 HitTestRowIndex(const FVector2D& AbsoluteScreenPos) const;
};
