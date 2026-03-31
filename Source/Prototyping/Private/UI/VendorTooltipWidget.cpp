#include "UI/VendorTooltipWidget.h"
#include "Data/DataStructs.h"
#include "MyGameInstance.h"
#include "Gameplay/Items/ItemManager.h"
#include "Services/LocalizationSubsystem.h"
#include "Engine/AssetManager.h"
#include "Blueprint/WidgetLayoutLibrary.h"

// ---------------------------------------------------------------------------
// NativeConstruct
// ---------------------------------------------------------------------------

void UVendorTooltipWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // Default rarity colors (can be overridden in Blueprint)
    if (RarityColors.Num() == 0)
    {
        RarityColors.Add(TEXT("common"),    FLinearColor(0.8f, 0.8f, 0.8f));
        RarityColors.Add(TEXT("uncommon"),  FLinearColor(0.13f, 0.68f, 0.13f));
        RarityColors.Add(TEXT("rare"),      FLinearColor(0.0f, 0.44f, 0.87f));
        RarityColors.Add(TEXT("epic"),      FLinearColor(0.64f, 0.21f, 0.93f));
        RarityColors.Add(TEXT("legendary"), FLinearColor(1.0f, 0.5f, 0.0f));
    }

    SetVisibility(ESlateVisibility::Collapsed);
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

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

void UVendorTooltipWidget::ApplyCommonFields(const FString& Name, const FString& TypeName,
const FString& RarityName, const FString& RaritySlug, const FString& Description,
float Weight, int32 DurabilityMax, int32 DurabilityCurrent,
bool bIsDurable, const FString& Slug,
int32 LevelRequirement,
bool bIsEquippable,
const FString& EquipSlotName,
bool bIsTwoHanded,
bool bIsQuestItem,
bool bIsTradable,
const TMap<FString, FString>& Attributes,
const TArray<FItemUseEffectEntry>& UseEffects,
const FString& MasterySlug,
int32 KillCount)
{
    // Resolve localised name and description via LocalizationSubsystem when a slug is available
    // Pretty-print the raw server name/slug as fallback (replace underscores, capitalise first letter)
    auto PrettyFallback = [](const FString& Raw) -> FText
    {
        if (Raw.IsEmpty()) return FText::GetEmpty();
        FString Pretty = Raw.Replace(TEXT("_"), TEXT(" "));
        if (!Pretty.IsEmpty())
            Pretty = Pretty.Left(1).ToUpper() + Pretty.Mid(1);
        return FText::FromString(Pretty);
    };

    FText LocalisedName        = PrettyFallback(Name.IsEmpty() ? Slug : Name);
    FText LocalisedDescription = FText::FromString(Description);
    if (!Slug.IsEmpty())
    {
        if (UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance()))
        {
            if (ULocalizationSubsystem* Loc = GI->GetSubsystem<ULocalizationSubsystem>())
            {
                const FText LocName = Loc->GetItemDisplayName(Slug);
                if (!LocName.IsEmpty()) LocalisedName = LocName;
                const FText LocDesc = Loc->GetItemDescription(Slug);
                if (!LocDesc.IsEmpty()) LocalisedDescription = LocDesc;
            }
        }
    }

    if (ItemNameText)
    {
        ItemNameText->SetText(LocalisedName);
        ItemNameText->SetColorAndOpacity(GetRarityColor(RaritySlug));
    }

    if (ItemTypeText)
    {
        const FText DisplayType = PrettyFallback(TypeName);
        if (!DisplayType.IsEmpty())
        {
            ItemTypeText->SetText(DisplayType);
            ItemTypeText->SetVisibility(ESlateVisibility::Visible);
        }
        else
        {
            ItemTypeText->SetVisibility(ESlateVisibility::Collapsed);
        }
    }

    if (ItemTypeText || LevelRequirementText)
    {
        const bool bHasTypeOrLevelReq = !TypeName.IsEmpty() || LevelRequirement > 1;

        if (!bHasTypeOrLevelReq)
        {
            if (Separator2) Separator2->SetVisibility(ESlateVisibility::Collapsed);
        }
        else
        {
            if (Separator2) Separator2->SetVisibility(ESlateVisibility::Visible);
        }
    }

    if (ItemRarityText)
    {
        const FString DisplayRarity = RarityName.IsEmpty() ? RaritySlug : RarityName;
        if (!DisplayRarity.IsEmpty())
        {
            ItemRarityText->SetText(FText::FromString(DisplayRarity));
            ItemRarityText->SetColorAndOpacity(GetRarityColor(RaritySlug));
            ItemRarityText->SetVisibility(ESlateVisibility::Visible);
        }
        else
        {
            ItemRarityText->SetVisibility(ESlateVisibility::Collapsed);
        }
    }

    if (ItemDescriptionText)
    {
        if (!LocalisedDescription.IsEmpty())
        {
            ItemDescriptionText->SetText(LocalisedDescription);
            ItemDescriptionText->SetVisibility(ESlateVisibility::Visible);
            if (Separator4) Separator4->SetVisibility(ESlateVisibility::Visible);
        }
        else
        {
            ItemDescriptionText->SetVisibility(ESlateVisibility::Collapsed);
            if (Separator4) Separator4->SetVisibility(ESlateVisibility::Collapsed);
        }
    }

    if (WeightText)
    {
        if (Weight > 0.f)
        {
            WeightText->SetText(FText::FromString(FString::Printf(TEXT("Weight: %.1f kg"), Weight)));
            WeightText->SetVisibility(ESlateVisibility::Visible);
            if (Separator6) Separator6->SetVisibility(ESlateVisibility::Visible);
        }
        else
        {
            WeightText->SetVisibility(ESlateVisibility::Collapsed);
            if (Separator6) Separator6->SetVisibility(ESlateVisibility::Collapsed);
        }
    }

    if (DurabilityText)
    {
        if (bIsDurable)
        {
            const float Ratio = (DurabilityMax > 0)
                ? static_cast<float>(DurabilityCurrent) / static_cast<float>(DurabilityMax)
                : 1.f;
            const FLinearColor DurColor = (Ratio <= 0.25f) ? FLinearColor::Red
                : (Ratio <= 0.5f) ? FLinearColor::Yellow
                : FLinearColor(0.6f, 0.9f, 0.6f, 1.f);
            DurabilityText->SetText(FText::FromString(
                FString::Printf(TEXT("Durability: %d / %d"), DurabilityCurrent, DurabilityMax)));
            DurabilityText->SetColorAndOpacity(DurColor);
            DurabilityText->SetVisibility(ESlateVisibility::Visible);
			if (Separator3) Separator3->SetVisibility(ESlateVisibility::Visible);
        }
        else
        {
            DurabilityText->SetVisibility(ESlateVisibility::Collapsed);
            if (Separator3) Separator3->SetVisibility(ESlateVisibility::Collapsed);
        }
    }

    // Equip slot
    if (EquipSlotText)
    {
        if (bIsEquippable && !EquipSlotName.IsEmpty())
        {
            FString SlotLabel = EquipSlotName;
            if (bIsTwoHanded) SlotLabel += TEXT(" (Two-Handed)");
            EquipSlotText->SetText(FText::FromString(SlotLabel));
            EquipSlotText->SetVisibility(ESlateVisibility::Visible);
        }
        else
        {
            EquipSlotText->SetVisibility(ESlateVisibility::Collapsed);
        }
    }

    // Level requirement
    if (LevelRequirementText)
    {
        if (LevelRequirement > 1)
        {
            LevelRequirementText->SetText(FText::FromString(
                FString::Printf(TEXT("Required Level: %d"), LevelRequirement)));
            LevelRequirementText->SetColorAndOpacity(FLinearColor(1.f, 0.85f, 0.45f));
            LevelRequirementText->SetVisibility(ESlateVisibility::Visible);
        }
        else
        {
            LevelRequirementText->SetVisibility(ESlateVisibility::Collapsed);
        }
    }

    // Flags
    if (FlagsText)
    {
        TArray<FString> Flags;
        if (bIsQuestItem)  Flags.Add(TEXT("Quest Item"));
        if (!bIsTradable)  Flags.Add(TEXT("Not Tradable"));
        if (Flags.Num() > 0)
        {
            FlagsText->SetText(FText::FromString(FString::Join(Flags, TEXT("  \u00b7  "))));
            FlagsText->SetVisibility(ESlateVisibility::Visible);
        }
        else
        {
            FlagsText->SetVisibility(ESlateVisibility::Collapsed);
        }
    }

    // Attributes
    if (AttributesText)
    {
        if (Attributes.Num() > 0)
        {
            TArray<FString> Lines;
            for (const auto& Pair : Attributes)
            {
                // Key may be a human-readable name ("Physical Attack") or a slug ("physical_attack").
                // Normalise: replace underscores then capitalise first letter only.
                FString AttrName = Pair.Key.Replace(TEXT("_"), TEXT(" "));
                if (!AttrName.IsEmpty())
                    AttrName = AttrName.Left(1).ToUpper() + AttrName.Mid(1);
                Lines.Add(FString::Printf(TEXT("%s: %s"), *AttrName, *Pair.Value));
            }
            AttributesText->SetText(FText::FromString(FString::Join(Lines, TEXT("\n"))));
            AttributesText->SetColorAndOpacity(FLinearColor(0.75f, 0.95f, 1.f));
            AttributesText->SetVisibility(ESlateVisibility::Visible);
        }
        else
        {
            AttributesText->SetVisibility(ESlateVisibility::Collapsed);
        }
    }

    // Use effects
    if (UseEffectText)
    {
        if (UseEffects.Num() > 0)
        {
            TArray<FString> EffLines;
            for (const FItemUseEffectEntry& Effect : UseEffects)
            {
                FString AttrLabel = Effect.attributeSlug.Replace(TEXT("_"), TEXT(" "));
                if (!AttrLabel.IsEmpty())
                    AttrLabel = AttrLabel.Left(1).ToUpper() + AttrLabel.Mid(1).ToLower();
                FString Line;
                if (Effect.isInstant)
                    Line = FString::Printf(TEXT("Use: Restores %.0f %s"), Effect.value, *AttrLabel);
                else if (Effect.durationSeconds > 0)
                    Line = FString::Printf(TEXT("Use: +%.0f %s for %ds"), Effect.value, *AttrLabel, Effect.durationSeconds);
                else
                    Line = FString::Printf(TEXT("Use: %.0f %s"), Effect.value, *AttrLabel);
                if (Effect.cooldownSeconds > 0)
                    Line += FString::Printf(TEXT(" (%ds cooldown)"), Effect.cooldownSeconds);
                EffLines.Add(Line);
            }
            UseEffectText->SetText(FText::FromString(FString::Join(EffLines, TEXT("\n"))));
            UseEffectText->SetColorAndOpacity(FLinearColor(0.4f, 1.f, 0.4f));
            UseEffectText->SetVisibility(ESlateVisibility::Visible);
        }
        else
        {
            UseEffectText->SetVisibility(ESlateVisibility::Collapsed);
        }
    }

    if(UseEffects.Num() == 0 && Attributes.Num() == 0)
    {
        if (Separator5) Separator5->SetVisibility(ESlateVisibility::Collapsed);
	}
    else
    {
        if (Separator5) Separator5->SetVisibility(ESlateVisibility::Visible);
    }

    // Weapon soul (mastery + kill count)
    if (WeaponSoulText)
    {
        const bool bHasMastery = !MasterySlug.IsEmpty();
        const bool bHasKills   = KillCount > 0;
        if (bHasMastery || bHasKills)
        {
            TArray<FString> Parts;
            if (bHasMastery)
            {
                FString ML = MasterySlug.Replace(TEXT("_"), TEXT(" "));
                ML = ML.Left(1).ToUpper() + ML.Mid(1).ToLower();
                Parts.Add(FString::Printf(TEXT("Mastery: %s"), *ML));
            }
            if (bHasKills)
                Parts.Add(FString::Printf(TEXT("Kills: %d"), KillCount));
            WeaponSoulText->SetText(FText::FromString(FString::Join(Parts, TEXT("  |  "))));
            WeaponSoulText->SetColorAndOpacity(FLinearColor(1.f, 0.6f, 0.1f));
            WeaponSoulText->SetVisibility(ESlateVisibility::Visible);
        }
        else
        {
            WeaponSoulText->SetVisibility(ESlateVisibility::Collapsed);
        }
    }

    LoadIconBySlug(Slug);
}

FLinearColor UVendorTooltipWidget::GetRarityColor(const FString& RaritySlug) const
{
    const FString Key = RaritySlug.ToLower();
    if (const FLinearColor* Found = RarityColors.Find(Key))
        return *Found;
    return FLinearColor::White;
}

void UVendorTooltipWidget::LoadIconBySlug(const FString& Slug)
{
    if (!ItemIcon || Slug.IsEmpty()) return;

    UMyGameInstance* GI = Cast<UMyGameInstance>(GetWorld() ? GetWorld()->GetGameInstance() : nullptr);
    if (!GI) return;

    UItemManager* ItemMgr = GI->GetItemManager();
    if (!ItemMgr) return;

    FItemVisualData VisualData = ItemMgr->GetItemVisualDataBySlug(Slug);
    if (VisualData.Icon.IsNull())
    {
        ItemIcon->SetBrushFromTexture(nullptr);
        return;
    }

    if (UTexture2D* Already = VisualData.Icon.Get())
    {
        SetIconTexture(Already);
        return;
    }

    TWeakObjectPtr<UVendorTooltipWidget> WeakThis = this;
    TSoftObjectPtr<UTexture2D> SoftIcon = VisualData.Icon;
    AsyncLoad(SoftIcon.ToSoftObjectPath(),
        FStreamableDelegate::CreateLambda([WeakThis, SoftIcon]()
        {
            if (WeakThis.IsValid())
                if (UTexture2D* Tex = SoftIcon.Get())
                    WeakThis->SetIconTexture(Tex);
        })
    );
}

void UVendorTooltipWidget::SetIconTexture(UTexture2D* Texture)
{
    if (ItemIcon && Texture)
    {
        ItemIcon->SetBrushFromTexture(Texture);
        ItemIcon->SetColorAndOpacity(FLinearColor::White);
        ItemIcon->SetVisibility(ESlateVisibility::Visible);
    }
}

void UVendorTooltipWidget::AsyncLoad(const FSoftObjectPath& AssetPath, FStreamableDelegate Callback)
{
    if (AssetPath.IsValid())
    {
        StreamableHandle = UAssetManager::Get().GetStreamableManager()
            .RequestAsyncLoad(AssetPath, Callback);
    }
}
