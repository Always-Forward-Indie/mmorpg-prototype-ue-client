#pragma once


#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Data/DataStructs.h"
#include "UI/EquipmentSlotWidget.h"
#include "UI/ItemTooltipWidget.h"
#include "UI/FocusableWindowWidget.h"
#include "EquipmentWidget.generated.h"

class UEquipmentManager;
class UInventoryManager;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEquipmentVisibilityChanged, bool, bIsVisible);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEquipSlotUnequipRequested, const FString&, SlotSlug);

/**
 * UEquipmentWidget
 *
 * Displays the 12 equipment slots as individual UEquipmentSlotWidget instances.
 * Left-click a slot ? OnEquipSlotClicked (handled externally, e.g. opens unequip confirm).
 * Right-click a slot ? immediately requests unequip via OnEquipSlotUnequipRequested.
 * Hover ? shows ItemTooltipWidget.
 *
 * Required Blueprint bindings (BindWidget):
 *   Slot_Head        UEquipmentSlotWidget
 *   Slot_Chest       UEquipmentSlotWidget
 *   Slot_Legs        UEquipmentSlotWidget
 *   Slot_Feet        UEquipmentSlotWidget
 *   Slot_Hands       UEquipmentSlotWidget
 *   Slot_Waist       UEquipmentSlotWidget
 *   Slot_Necklace    UEquipmentSlotWidget
 *   Slot_Ring1       UEquipmentSlotWidget
 *   Slot_Ring2       UEquipmentSlotWidget
 *   Slot_MainHand    UEquipmentSlotWidget
 *   Slot_OffHand     UEquipmentSlotWidget
 *   Slot_Cloak       UEquipmentSlotWidget
 *
 * Optional (BindWidgetOptional):
 *   Close_Button     UButton
 *   DragHandle       UWidget
 *   TooltipWidget    UItemTooltipWidget
 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class PROTOTYPING_API UEquipmentWidget : public UFocusableWindowWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Equipment UI")
    void BindToEquipmentManager(UEquipmentManager* InEquipmentManager, UInventoryManager* InInventoryManager);

    UFUNCTION(BlueprintCallable, Category = "Equipment UI")
    void RefreshEquipmentDisplay();

    UFUNCTION(BlueprintCallable, Category = "Equipment UI")
    void ToggleEquipment();

    // ---- Outward delegates ----
    UPROPERTY(BlueprintAssignable, Category = "Equipment UI Events")
    FOnEquipmentVisibilityChanged OnEquipmentVisibilityChanged;

    /** Fired on left-click of an occupied slot (for confirm-dialog flow) */
    UPROPERTY(BlueprintAssignable, Category = "Equipment UI Events")
    FOnEquipSlotClicked OnEquipSlotLeftClicked;

    /** Fired on right-click ? unequip without confirm */
    UPROPERTY(BlueprintAssignable, Category = "Equipment UI Events")
    FOnEquipSlotUnequipRequested OnEquipSlotUnequipRequested;

protected:
    virtual void NativeConstruct() override;
    virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

    // ---- Delegate handlers ----
    UFUNCTION() void HandleEquipmentStateChanged(const FEquipmentStateData& State);
    UFUNCTION() void HandleEquipResultReceived(const FEquipResultData& Result);
    UFUNCTION() void HandleCloseButtonClicked();
    UFUNCTION() void HandleSlotClicked(const FString& SlotSlug);
    UFUNCTION() void HandleSlotRightClicked(const FString& SlotSlug);
    UFUNCTION() void HandleSlotHovered(const FString& SlotSlug, bool bHovered);

    // ---- Required BindWidget slot widgets ----
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget)) UEquipmentSlotWidget* Slot_Head     = nullptr;
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget)) UEquipmentSlotWidget* Slot_Chest    = nullptr;
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget)) UEquipmentSlotWidget* Slot_Legs     = nullptr;
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget)) UEquipmentSlotWidget* Slot_Feet     = nullptr;
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget)) UEquipmentSlotWidget* Slot_Hands    = nullptr;
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget)) UEquipmentSlotWidget* Slot_Waist    = nullptr;
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget)) UEquipmentSlotWidget* Slot_Necklace = nullptr;
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget)) UEquipmentSlotWidget* Slot_Ring1    = nullptr;
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget)) UEquipmentSlotWidget* Slot_Ring2    = nullptr;
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget)) UEquipmentSlotWidget* Slot_MainHand = nullptr;
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget)) UEquipmentSlotWidget* Slot_OffHand  = nullptr;
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget)) UEquipmentSlotWidget* Slot_Cloak    = nullptr;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UButton*              Close_Button  = nullptr;
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UWidget*              DragHandle    = nullptr;
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UItemTooltipWidget*   EquipTooltipWidget = nullptr;

private:
    void InitializeSlots();
    void UpdateWindowDragPosition(const FVector2D& ScreenCursorPos);
    void BindSlotDelegates(UEquipmentSlotWidget* Slot);
    void ShowTooltipForSlot(const FString& SlotSlug);

    /** Returns FInventoryItemStruct for a slot if InventoryManager has it, otherwise a minimal stub */
    FInventoryItemStruct BuildTooltipItem(const FEquipmentSlotData& SlotData) const;

    static FString GetSlotDisplayName(const FString& Slug);

    UPROPERTY() UEquipmentManager* EquipmentManager = nullptr;
    UPROPERTY() UInventoryManager* InventoryManager = nullptr;

    FEquipmentStateData CachedState;

    bool      bDragging             = false;
    FVector2D DragOffset            = FVector2D::ZeroVector;
    FVector2D CurrentViewportPosition = FVector2D::ZeroVector;
};

