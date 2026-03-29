#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/FocusableWindowWidget.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/ScrollBox.h"
#include "Data/DataStructs.h"
#include "RepairShopWidget.generated.h"

class URepairManager;
class URepairItemRowBinding;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRepairShopVisibilityChanged, bool, bIsVisible);

/**
 * RepairShopWidget
 *
 * Shows the list of equipped durable items with their durability and repair cost.
 * Provides per-item repair and "Repair All" button.
 *
 * Blueprint subclass must bind:
 *   Repair_Items_Box  UScrollBox  — item rows added here
 *   Total_Cost_Text   UTextBlock  — total repair cost summary
 *   Repair_All_Btn    UButton     — repair all button
 *   Status_Text       UTextBlock  (BindWidgetOptional)
 *   Close_Button      UButton     (BindWidgetOptional)
 *
 * Each row widget (RepairRowClass) needs:
 *   Row_Name_Text     UTextBlock
 *   Row_Durability_Text UTextBlock
 *   Row_Cost_Text     UTextBlock
 *   Row_Repair_Btn    UButton
 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class PROTOTYPING_API URepairShopWidget : public UFocusableWindowWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Repair UI")
    void BindToRepairManager(URepairManager* InRepairManager);

    UFUNCTION(BlueprintCallable, Category = "Repair UI")
    void RefreshDisplay();

    UFUNCTION(BlueprintCallable, Category = "Repair UI")
    void OpenShop();

    UFUNCTION(BlueprintCallable, Category = "Repair UI")
    void CloseShop();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Repair UI")
    TSubclassOf<UUserWidget> RepairRowClass;

    UPROPERTY(BlueprintAssignable, Category = "Repair UI Events")
    FOnRepairShopVisibilityChanged OnRepairShopVisibilityChanged;

    // Called by row binding helpers
    void DispatchRepairItem(int32 InventoryItemId);

protected:
    virtual void NativeConstruct() override;
    virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    void UpdateWindowDragPosition(const FVector2D& ScreenCursorPos);

    UFUNCTION() void HandleRepairShopOpened(const FRepairShopData& ShopData);
    UFUNCTION() void HandleRepairItemResult(const FRepairItemResultData& Result);
    UFUNCTION() void HandleRepairAllResult(const FRepairAllResultData& Result);
    UFUNCTION() void HandleRepairAllClicked();
    UFUNCTION() void HandleCloseButtonClicked();

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))         UScrollBox* Repair_Items_Box = nullptr;
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))         UTextBlock* Total_Cost_Text  = nullptr;
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))         UButton*    Repair_All_Btn   = nullptr;
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UTextBlock* Status_Text      = nullptr;
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UButton*    Close_Button     = nullptr;
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional)) UWidget*    DragHandle       = nullptr;

private:
    UPROPERTY() URepairManager* RepairManager = nullptr;
    UPROPERTY() TArray<URepairItemRowBinding*> RowBindings;

    FRepairShopData CachedShop;

    void ShowStatus(const FString& Msg);

    bool      bDragging = false;
    FVector2D DragOffset = FVector2D::ZeroVector;
    FVector2D CurrentViewportPosition = FVector2D::ZeroVector;
};

// ---------------------------------------------------------------------------
// Per-row binding helper
// ---------------------------------------------------------------------------

UCLASS()
class PROTOTYPING_API URepairItemRowBinding : public UObject
{
    GENERATED_BODY()

public:
    void Setup(URepairShopWidget* InWidget, int32 InInventoryItemId);

    UFUNCTION()
    void HandleClicked();

private:
    UPROPERTY() URepairShopWidget* Widget = nullptr;
    int32 InventoryItemId = 0;
};
