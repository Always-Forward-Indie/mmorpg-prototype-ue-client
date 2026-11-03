#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "Components/ScrollBox.h"
#include "Components/Button.h"
#include "Data/DataStructs.h"
#include "UI/VendorSlotWidget.h"
#include "UI/ItemTooltipWidget.h"
#include "Gameplay/Social/PlayerInspectManager.h"
#include "PlayerInspectWidget.generated.h"

/**
 * UPlayerInspectWidget
 *
 * Read-only snapshot window for unit-menu inspect: identity header
 * (name / level / class / race) + equipped gear as icon slots with
 * tooltips + attribute totals as text rows. Never sends packets; content
 * comes only from UPlayerInspectManager. Clicks on gear do nothing
 * (display only).
 *
 * Blueprint layout (optional): create a BP subclass, bind these names,
 * assign it to UIManager.PlayerInspectWidgetClass. Bind all three or none —
 * gear rows are always built in code (uniform styling):
 *   Inspect_Title_Text  UTextBlock  "Name [Level] Class" header
 *   Inspect_Gear_Box    UScrollBox  gear slots + attribute rows go here
 *   Inspect_Close_Btn   UButton     hides the window
 *   DragHandle          UWidget     drag handle (optional — unbound = not draggable)
 * Gear slots reuse the vendor slot BP (InspectSlotClass, same asset as the
 * vendor shop uses); hover shows an item tooltip (InspectTooltipWidgetClass,
 * optional — without it the localized slot name still shows as text).
 * Placement comes from the BP root's own anchors; WindowSize/WindowPosition
 * apply to the code fallback only.
 * With no subclass assigned the whole layout is built in C++ (headless-safe).
 */
UCLASS(BlueprintType, Blueprintable)
class PROTOTYPING_API UPlayerInspectWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Inspect")
	void ShowInspect(const FPlayerInspectData& Data);

	UFUNCTION(BlueprintCallable, Category = "Inspect")
	void HideInspect();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inspect")
	bool IsInspectOpen() const;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inspect")
	FVector2D WindowSize = FVector2D(340.f, 420.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inspect")
	FVector2D WindowPosition = FVector2D(60.f, -40.f);

	// Blueprint layout slots (BindWidgetOptional — see class doc).
	UPROPERTY(meta = (BindWidgetOptional)) UTextBlock* Inspect_Title_Text = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) UScrollBox* Inspect_Gear_Box = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) UButton* Inspect_Close_Btn = nullptr;

	/**
	 * Vendor slot BP reused for gear icons (same asset as the vendor shop).
	 * Unset = gear falls back to text rows ("slot: item").
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inspect")
	TSubclassOf<UVendorSlotWidget> InspectSlotClass;

	/** Item tooltip BP for gear hover. Optional — names still show as text. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inspect")
	TSubclassOf<UItemTooltipWidget> InspectTooltipWidgetClass;

	/** Drag handle (any UWidget, typically the header). Unbound = not draggable. */
	UPROPERTY(meta = (BindWidgetOptional)) UWidget* DragHandle = nullptr;

	UFUNCTION() void HandleGearSlotHovered(int32 SlotIndex, bool bIsHovered);

protected:
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	void UpdateWindowDragPosition(const FVector2D& ScreenCursorPos);

private:
	bool bDragging = false;
	FVector2D DragOffset = FVector2D::ZeroVector;
	// Mirror of UTradeWidget: drag math runs on this tracked value, never on
	// live geometry (cached geometry is a frame stale right after content
	// rebuilds — that staleness is what made the window jump on grab).
	FVector2D CurrentViewportPosition = FVector2D::ZeroVector;
	bool bNeedPositionSync = false;
	class UScrollBox* GearBox = nullptr;
	class UTextBlock* HeaderText = nullptr;
	class UVerticalBox* RootCol = nullptr;

	// Parallel array: tooltip previews per gear slot widget index.
	TArray<FInventoryItemStruct> GearPreviews;

	UPROPERTY() UItemTooltipWidget* InspectTooltip = nullptr;

	void BuildLayout();
	void ClearGear();
	void AddGearRow(const FString& SlotName, const FString& ItemName);
	void AddGearSlotRow(const FString& SlotName, const FInventoryItemStruct& PreviewItem);
	FString LocalizeItem(const FString& Slug) const;
	void EnsureTooltip();

	// Wiring happens once (NativeConstruct may re-run on re-add to viewport).
	bool bWired = false;
};
