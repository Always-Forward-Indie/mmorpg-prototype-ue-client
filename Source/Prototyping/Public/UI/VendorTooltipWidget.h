#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/VerticalBox.h"
#include "Components/Border.h"
#include "Data/DataStructs.h"
#include "Engine/StreamableManager.h"
#include "VendorTooltipWidget.generated.h"

/**
 * UVendorTooltipWidget
 *
 * Tooltip shown when hovering over any slot in the vendor shop:
 *   - Vendor catalogue (buy tab)  — shows buy price
 *   - Player inventory (sell tab) — shows sell price
 *   - Cart entries (both tabs)    — shows price per unit and total
 *
 * Blueprint subclass must bind:
 *   TooltipBorder      UBorder
 *   ItemNameText       UTextBlock
 *   ItemTypeText       UTextBlock               (BindWidgetOptional)
 *   ItemRarityText     UTextBlock               (BindWidgetOptional)
 *   ItemDescriptionText UTextBlock              (BindWidgetOptional)
 *   QuantityText       UTextBlock               (BindWidgetOptional)
 *   PriceText          UTextBlock               price line (buy or sell)
 *   PriceLabelText     UTextBlock               (BindWidgetOptional) e.g. "Buy price:"
 *   StockText          UTextBlock               (BindWidgetOptional)
 *   ItemIcon           UImage                   (BindWidgetOptional)
 *   WeightText         UTextBlock               (BindWidgetOptional)
 *   DurabilityText     UTextBlock               (BindWidgetOptional)
 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class PROTOTYPING_API UVendorTooltipWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    /**
     * Populate the tooltip from a vendor catalogue item (buy tab).
     * Displays buy price and available stock.
     */
    UFUNCTION(BlueprintCallable, Category = "Vendor Tooltip")
    void SetDataFromShopItem(const FVendorShopItemData& Item);

    /**
     * Populate the tooltip from a player inventory item (sell tab).
     * Displays sell price and owned quantity.
     */
    UFUNCTION(BlueprintCallable, Category = "Vendor Tooltip")
    void SetDataFromInventoryItem(const FInventoryItemStruct& Item);

    /**
     * Populate the tooltip from a cart entry.
     * Displays price per unit, quantity in cart and subtotal.
     */
    UFUNCTION(BlueprintCallable, Category = "Vendor Tooltip")
    void SetDataFromCartEntry(const FVendorCartEntry& Entry);

    UFUNCTION(BlueprintCallable, Category = "Vendor Tooltip")
    void ShowTooltip();

    UFUNCTION(BlueprintCallable, Category = "Vendor Tooltip")
    void HideTooltip();

    /** Repositions the tooltip near the cursor, keeping it inside the viewport. Call every tick while visible. */
    UFUNCTION(BlueprintCallable, Category = "Vendor Tooltip")
    void UpdateTooltipPosition();

    // Visual settings
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vendor Tooltip")
    TMap<FString, FLinearColor> RarityColors;

protected:
    virtual void NativeConstruct() override;

    // ---------------------------------------------------------------------------
    // Blueprint-bound widgets
    // ---------------------------------------------------------------------------

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    UBorder* TooltipBorder = nullptr;

    UPROPERTY(meta = (BindWidget))
    UImage* Separator1;

    UPROPERTY(meta = (BindWidget))
    UImage* Separator2;

    UPROPERTY(meta = (BindWidget))
    UImage* Separator3;

    UPROPERTY(meta = (BindWidget))
    UImage* Separator4;

    UPROPERTY(meta = (BindWidget))
    UImage* Separator5;

    UPROPERTY(meta = (BindWidget))
    UImage* Separator6;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    UTextBlock* ItemNameText = nullptr;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    UTextBlock* PriceText = nullptr;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    UTextBlock* PriceLabelText = nullptr;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    UTextBlock* ItemTypeText = nullptr;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    UTextBlock* ItemRarityText = nullptr;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    UTextBlock* ItemDescriptionText = nullptr;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    UTextBlock* QuantityText = nullptr;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    UTextBlock* StockText = nullptr;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    UTextBlock* WeightText = nullptr;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    UTextBlock* DurabilityText = nullptr;

    // Equip slot line (e.g. "Main Hand (Two-Handed)")
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    UTextBlock* EquipSlotText = nullptr;

    // Level requirement line
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    UTextBlock* LevelRequirementText = nullptr;

    // Miscellaneous flags (Quest Item, Not Tradable, etc.)
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    UTextBlock* FlagsText = nullptr;

    // Item attributes (Attack, Defense …) — one TextBlock, newline-joined
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    UTextBlock* AttributesText = nullptr;

    // Use effects for consumables
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    UTextBlock* UseEffectText = nullptr;

    // Mastery slug + kill count for weapons (Item Soul)
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    UTextBlock* WeaponSoulText = nullptr;

    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    UImage* ItemIcon = nullptr;

private:
    void ApplyCommonFields(const FString& Name, const FString& TypeName,
                           const FString& RarityName, const FString& RaritySlug,
                           const FString& Description,
                           float Weight, int32 DurabilityMax, int32 DurabilityCurrent,
                           bool bIsDurable, const FString& Slug,
                           int32 LevelRequirement = 0,
                           bool bIsEquippable = false,
                           const FString& EquipSlotName = TEXT(""),
                           bool bIsTwoHanded = false,
                           bool bIsQuestItem = false,
                           bool bIsTradable = true,
                           const TMap<FString, FString>& Attributes = TMap<FString, FString>(),
                           const TArray<FItemUseEffectEntry>& UseEffects = TArray<FItemUseEffectEntry>(),
                           const FString& MasterySlug = TEXT(""),
                           int32 KillCount = 0);

    void LoadIconBySlug(const FString& Slug);
    void SetIconTexture(UTexture2D* Texture);
    void AsyncLoad(const FSoftObjectPath& AssetPath, FStreamableDelegate Callback);

    FLinearColor GetRarityColor(const FString& RaritySlug) const;

    TSharedPtr<FStreamableHandle> StreamableHandle;
};
