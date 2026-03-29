#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Components/ProgressBar.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Data/DataStructs.h"
#include "EquipmentSlotWidget.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEquipSlotClicked,        const FString&, SlotSlug);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEquipSlotRightClicked,   const FString&, SlotSlug);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEquipSlotHovered,       const FString&, SlotSlug, bool, bIsHovered);

/**
 * UEquipmentSlotWidget
 *
 * Represents a single equipment slot (head, chest, main_hand, …).
 * Displays item icon, durability bar, "blocked" overlay for two-handed weapons
 * and fires click / hover delegates consumed by UEquipmentWidget.
 *
 * Required Blueprint bindings (BindWidget):
 *   SlotButton        UButton
 *   ItemIcon          UImage
 *   SlotNameText      UTextBlock   — static label ("Head", "Main Hand", …)
 *   DurabilityBar     UProgressBar
 *
 * Optional (BindWidgetOptional):
 *   BlockedOverlay    UImage       — shown when blockedByTwoHanded
 *   RarityBorder      UBorder
 *   ImgOccupied       UImage
 *   ImgEmpty          UImage
 *   ImgStroke         UImage       — hover stroke
 */
UCLASS()
class PROTOTYPING_API UEquipmentSlotWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UEquipmentSlotWidget(const FObjectInitializer& ObjectInitializer);

    /** Called by EquipmentWidget once after creation to set the slug ("main_hand", "head", …) */
    UFUNCTION(BlueprintCallable, Category = "Equipment Slot")
    void InitializeSlot(const FString& InSlotSlug, const FString& InSlotDisplayName);

    /** Push server-authoritative slot data into the widget */
    UFUNCTION(BlueprintCallable, Category = "Equipment Slot")
    void SetSlotData(const FEquipmentSlotData& SlotData);

    /** Load the icon from ItemManager using the item slug stored in SlotData */
    UFUNCTION(BlueprintCallable, Category = "Equipment Slot")
    void RefreshIcon();

    /** Clear to empty state (no item) */
    UFUNCTION(BlueprintCallable, Category = "Equipment Slot")
    void ClearSlot();

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Equipment Slot")
    bool IsOccupied() const { return CachedSlotData.bIsOccupied; }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Equipment Slot")
    FString GetSlotSlug() const { return SlotSlug; }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Equipment Slot")
    FEquipmentSlotData GetSlotData() const { return CachedSlotData; }

    // ---- Delegates ----
    UPROPERTY(BlueprintAssignable, Category = "Equipment Slot Events")
    FOnEquipSlotClicked      OnEquipSlotClicked;

    UPROPERTY(BlueprintAssignable, Category = "Equipment Slot Events")
    FOnEquipSlotRightClicked OnEquipSlotRightClicked;

    UPROPERTY(BlueprintAssignable, Category = "Equipment Slot Events")
    FOnEquipSlotHovered      OnEquipSlotHovered;

protected:
    virtual void NativeConstruct() override;
    virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;
    virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

    void UpdateVisuals();
    void UpdateDurabilityBar();

    // ---- Required BindWidget ----
    UPROPERTY(meta = (BindWidget))
    UButton* SlotButton = nullptr;

    UPROPERTY(meta = (BindWidget))
    UImage* ItemIcon = nullptr;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* SlotNameText = nullptr;

    UPROPERTY(meta = (BindWidget))
    UProgressBar* DurabilityBar = nullptr;

    // ---- Optional BindWidget ----
    UPROPERTY(meta = (BindWidgetOptional))
    UImage* BlockedOverlay = nullptr;

    UPROPERTY(meta = (BindWidgetOptional))
    UBorder* RarityBorder = nullptr;

    UPROPERTY(meta = (BindWidgetOptional))
    UImage* ImgOccupied = nullptr;

    UPROPERTY(meta = (BindWidgetOptional))
    UImage* ImgEmpty = nullptr;

    UPROPERTY(meta = (BindWidgetOptional))
    UImage* ImgStroke = nullptr;

    // ---- State ----
    UPROPERTY(BlueprintReadOnly, Category = "Equipment Slot")
    FEquipmentSlotData CachedSlotData;

    UPROPERTY(BlueprintReadOnly, Category = "Equipment Slot")
    FString SlotSlug = "";

    UPROPERTY(BlueprintReadOnly, Category = "Equipment Slot")
    FString SlotDisplayName = "";

    // Durability warning threshold (0..1)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment Slot")
    float DurabilityWarningThreshold = 0.25f;

    // Colors
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment Slot")
    FLinearColor ColorEmpty = FLinearColor(0.3f, 0.3f, 0.3f, 1.f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment Slot")
    FLinearColor ColorOccupied = FLinearColor::White;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment Slot")
    FLinearColor ColorDurabilityOk = FLinearColor(0.2f, 0.8f, 0.2f, 1.f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment Slot")
    FLinearColor ColorDurabilityWarn = FLinearColor(1.f, 0.55f, 0.f, 1.f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment Slot")
    FLinearColor ColorDurabilityLow = FLinearColor(0.9f, 0.1f, 0.1f, 1.f);

    bool bIsHovered = false;

private:
    UFUNCTION() void HandleSlotButtonClicked();
    UFUNCTION() void HandleSlotButtonHovered();
    UFUNCTION() void HandleSlotButtonUnhovered();

    void SetIconTexture(UTexture2D* Texture);
    void SetDefaultIcon();
    void AsyncLoad(const FSoftObjectPath& Path, FStreamableDelegate Callback);

    TSharedPtr<FStreamableHandle> StreamableHandle;
};
