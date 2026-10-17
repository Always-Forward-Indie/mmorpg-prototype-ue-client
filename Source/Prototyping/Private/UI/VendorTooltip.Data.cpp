#include "UI/VendorTooltipWidget.h"
#include "Data/DataStructs.h"
#include "MyGameInstance.h"
#include "Gameplay/Items/ItemManager.h"
#include "Services/LocalizationSubsystem.h"
#include "Engine/AssetManager.h"
#include "Blueprint/WidgetLayoutLibrary.h"

void UVendorTooltipWidget::SetDataFromShopItem(const FVendorShopItemData& Item)
{
    ApplyCommonFields(TEXT(""), Item.itemTypeSlug, TEXT(""), Item.raritySlug,
                      TEXT(""),
                      Item.weight,
                      Item.durabilityMax, /*DurCur*/ Item.durabilityMax, // vendor items always at full dur
                      Item.isDurable,
                      Item.slug,
                      Item.levelRequirement,
                      Item.isEquippable,
                      Item.equipSlotSlug,
                      Item.isTwoHanded,
                      Item.isQuestItem,
                      Item.isTradable,
                      Item.attributes,
                      Item.useEffects,
                      Item.masterySlug,
                      Item.killCount);

    // Price: buy price
    if (PriceLabelText) { PriceLabelText->SetText(FText::FromString(TEXT("Buy:"))); PriceLabelText->SetVisibility(ESlateVisibility::Visible); }
    if (PriceText)      { PriceText->SetText(FText::FromString(FString::Printf(TEXT("%d g"), Item.priceBuy))); PriceText->SetVisibility(ESlateVisibility::Visible); }

    // Stock
    if (StockText)
    {
        if (Item.stockCurrent == -1)
        {
            StockText->SetText(FText::FromString(TEXT("In stock: \u221E")));
            StockText->SetColorAndOpacity(FLinearColor(0.6f, 0.9f, 0.6f));
        }
        else if (Item.stockCurrent == 0)
        {
            StockText->SetText(FText::FromString(TEXT("Out of stock")));
            StockText->SetColorAndOpacity(FLinearColor::Red);
        }
        else
        {
            StockText->SetText(FText::FromString(FString::Printf(TEXT("In stock: %d"), Item.stockCurrent)));
            StockText->SetColorAndOpacity(FLinearColor(0.6f, 0.9f, 0.6f));
        }
        StockText->SetVisibility(ESlateVisibility::Visible);
    }

    // Quantity not relevant for shop items
    if (QuantityText) QuantityText->SetVisibility(ESlateVisibility::Collapsed);
}

void UVendorTooltipWidget::SetDataFromInventoryItem(const FInventoryItemStruct& Item)
{
    ApplyCommonFields(TEXT(""), Item.itemTypeSlug, TEXT(""), Item.raritySlug,
                      TEXT(""),
                      Item.weight,
                      Item.durabilityMax, Item.durabilityCurrent, Item.isDurable,
                      Item.slug,
                      Item.level_requirement,
                      Item.isEquippable,
                      Item.equipSlotSlug,
                      Item.isTwoHanded,
                      Item.isQuestItem,
                      Item.isTradable,
                      Item.attributes,
                      Item.useEffects,
                      Item.masterySlug,
                      Item.killCount);

    // Price: sell price
    if (PriceLabelText) { PriceLabelText->SetText(FText::FromString(TEXT("Sell:"))); PriceLabelText->SetVisibility(ESlateVisibility::Visible); }
    if (PriceText)      { PriceText->SetText(FText::FromString(FString::Printf(TEXT("%d g"), Item.priceSell))); PriceText->SetVisibility(ESlateVisibility::Visible); }

    // Quantity owned
    if (QuantityText)
    {
        QuantityText->SetText(FText::FromString(FString::Printf(TEXT("Owned: %d"), Item.quantity)));
        QuantityText->SetVisibility(ESlateVisibility::Visible);
    }

    // Stock not relevant for inventory items
    if (StockText) StockText->SetVisibility(ESlateVisibility::Collapsed);
}

void UVendorTooltipWidget::SetDataFromCartEntry(const FVendorCartEntry& Entry)
{
    if (ItemNameText)
    {
        FText EntryName = FText::FromString(Entry.slug);
        if (!Entry.slug.IsEmpty())
        {
            if (UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance()))
            {
                if (ULocalizationSubsystem* Loc = GI->GetSubsystem<ULocalizationSubsystem>())
                {
                    const FText Loc_Name = Loc->GetItemDisplayName(Entry.slug);
                    if (!Loc_Name.IsEmpty()) EntryName = Loc_Name;
                }
            }
        }
        ItemNameText->SetText(EntryName);
    }

    if (ItemTypeText)    ItemTypeText->SetVisibility(ESlateVisibility::Collapsed);
    if (ItemRarityText)  ItemRarityText->SetVisibility(ESlateVisibility::Collapsed);
    if (ItemDescriptionText) ItemDescriptionText->SetVisibility(ESlateVisibility::Collapsed);
    if (WeightText)      WeightText->SetVisibility(ESlateVisibility::Collapsed);
    if (DurabilityText)  DurabilityText->SetVisibility(ESlateVisibility::Collapsed);
    if (StockText)       StockText->SetVisibility(ESlateVisibility::Collapsed);

    // Price per unit
    if (PriceLabelText) { PriceLabelText->SetText(FText::FromString(TEXT("Price / pc:"))); PriceLabelText->SetVisibility(ESlateVisibility::Visible); }
    if (PriceText)      { PriceText->SetText(FText::FromString(FString::Printf(TEXT("%d g  (total: %d g)"), Entry.pricePerUnit, Entry.pricePerUnit * Entry.quantity))); PriceText->SetVisibility(ESlateVisibility::Visible); }

    // Quantity in cart
    if (QuantityText)
    {
        QuantityText->SetText(FText::FromString(FString::Printf(TEXT("In cart: %d / %d"), Entry.quantity, Entry.maxQuantity)));
        QuantityText->SetVisibility(ESlateVisibility::Visible);
    }

    LoadIconBySlug(Entry.slug);
}

void UVendorTooltipWidget::ShowTooltip()
{
    SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UVendorTooltipWidget::HideTooltip()
{
    SetVisibility(ESlateVisibility::Collapsed);
}

void UVendorTooltipWidget::UpdateTooltipPosition()
{
    APlayerController* PC = GetOwningPlayer();
    if (!PC) return;

    const FVector2D MousePos = UWidgetLayoutLibrary::GetMousePositionOnViewport(PC);

    ForceLayoutPrepass();
    FVector2D Size = GetCachedGeometry().GetLocalSize();
    if (Size.X <= 1.f || Size.Y <= 1.f)
        Size = GetDesiredSize();
    if (Size.IsZero())
        Size = FVector2D(300.f, 200.f);

    int32 Wpx = 0, Hpx = 0;
    PC->GetViewportSize(Wpx, Hpx);
    const FVector2D View(Wpx, Hpx);

    const FVector2D Offset(20.f, 20.f);
    FVector2D Pos = MousePos + Offset;

    if (Pos.X + Size.X > View.X - 10.f)
        Pos.X = MousePos.X - Size.X - Offset.X;
    if (Pos.Y + Size.Y > View.Y - 10.f)
        Pos.Y = MousePos.Y - Size.Y - Offset.Y;

    Pos.X = FMath::Clamp(Pos.X, 0.f, View.X - Size.X);
    Pos.Y = FMath::Clamp(Pos.Y, 0.f, View.Y - Size.Y);

    SetAlignmentInViewport(FVector2D(0.f, 0.f));
    SetPositionInViewport(Pos, false);
}

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

