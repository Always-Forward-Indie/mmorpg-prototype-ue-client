#include "UI/ItemTooltipWidget.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/VerticalBox.h"
#include "Components/HorizontalBox.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Animation/WidgetAnimation.h"
#include "Engine/Engine.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/CanvasPanelSlot.h"
#include "MyGameInstance.h"
#include "Services/LocalizationSubsystem.h"

void UItemTooltipWidget::UpdateTooltipContent()
{
	UpdateItemHeader();
	UpdateItemDescription();
	UpdateItemTypeAndLevel();
	UpdateEquipInfo();
	UpdateItemDurability();
	UpdateItemWeight();
	UpdateItemAttributes();
	UpdateUseEffects();
	UpdateVendorPrices();
	UpdateKillCount();
}

void UItemTooltipWidget::UpdateItemHeader()
{
	// Update item name
	if (ItemNameText)
	{
		ItemNameText->SetText(FText::FromString(CurrentItem.name));
		//ItemNameText->SetColorAndOpacity(GetRarityColor(CurrentItem.rarity));
	}

	// Update rarity text
	if (ItemRarityText)
	{
		// Prefer raritySlug (from protocol); fall back to rarity display name
		const FString& RarityKey = CurrentItem.raritySlug.IsEmpty() ? CurrentItem.rarity : CurrentItem.raritySlug;
		FString RarityText = RarityKey.IsEmpty() ? TEXT("Common") : RarityKey;
		RarityText = RarityText.Left(1).ToUpper() + RarityText.Mid(1).ToLower();
		ItemRarityText->SetText(FText::FromString(RarityText));
		ItemRarityText->SetColorAndOpacity(GetRarityColor(RarityKey));
	}

	// Update item icon (placeholder)
	if (ItemIcon)
	{
		// TODO: Load actual item icon based on item data
		ItemIcon->SetVisibility(ESlateVisibility::Visible);
	}
}

void UItemTooltipWidget::UpdateItemDescription()
{
	if (ItemDescriptionText)
	{
		if (!CurrentItem.description.IsEmpty())
		{
			ItemDescriptionText->SetText(FText::FromString(CurrentItem.description));
			ItemDescriptionText->SetVisibility(ESlateVisibility::Visible);
		}
		else
		{
			ItemDescriptionText->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void UItemTooltipWidget::UpdateItemAttributes()
{
	if (!AttributesBox)
	{
		return;
	}

	ClearDynamicAttributes();

	// Use typed itemAttributes if available (has apply_on); fall back to legacy TMap
	if (CurrentItem.itemAttributes.Num() > 0)
	{
		for (const FItemAttributeStruct& Attr : CurrentItem.itemAttributes)
		{
			const FString& DisplayName = Attr.name.IsEmpty() ? Attr.slug : Attr.name;
			if (DisplayName.IsEmpty()) continue;

			FString ValueStr;
			if (FMath::IsNearlyEqual(Attr.value, FMath::RoundToFloat(Attr.value), 0.001f))
				ValueStr = FString::Printf(TEXT("%d"), FMath::RoundToInt(Attr.value));
			else
				ValueStr = FString::Printf(TEXT("%.1f"), Attr.value);

			UTextBlock* AttrWidget = AddAttributeTextWidget(DisplayName, ValueStr);
			if (AttrWidget && Attr.apply_on == TEXT("equip"))
			{
				AttrWidget->SetColorAndOpacity(FLinearColor(0.4f, 0.9f, 1.0f, 1.0f));
			}
		}
	}
	else
	{
		for (const auto& AttributePair : CurrentItem.attributes)
		{
			if (!AttributePair.Key.IsEmpty() && !AttributePair.Value.IsEmpty())
			{
				AddAttributeTextWidget(AttributePair.Key, AttributePair.Value);
			}
		}
	}

	const bool bHasAttribs = (CurrentItem.itemAttributes.Num() > 0 || CurrentItem.attributes.Num() > 0);
	if (!bHasAttribs)
	{
		AttributesBox->SetVisibility(ESlateVisibility::Collapsed);
		if (Separator3) Separator3->SetVisibility(ESlateVisibility::Collapsed);
	}
	else
	{
		AttributesBox->SetVisibility(ESlateVisibility::Visible);
		if (Separator3) Separator3->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}

void UItemTooltipWidget::UpdateItemTypeAndLevel()
{
	if (ItemTypeText)
	{
		// Prefer itemTypeSlug (comes from protocol item.itemTypeSlug); fall back to legacy type
		FString TypeText = CurrentItem.itemTypeSlug.IsEmpty() ? CurrentItem.type : CurrentItem.itemTypeSlug;
		if (TypeText.IsEmpty()) TypeText = TEXT("Unknown");
		TypeText = TypeText.Left(1).ToUpper() + TypeText.Mid(1).ToLower();
		ItemTypeText->SetText(FText::FromString(TypeText));
		ItemTypeText->SetColorAndOpacity(GetTypeColor(CurrentItem.itemTypeSlug.IsEmpty() ? CurrentItem.type : CurrentItem.itemTypeSlug));
	}

	if (ItemLevelText)
	{
		const int32 LvlReq = CurrentItem.levelRequirement > 0 ? CurrentItem.levelRequirement : CurrentItem.level_requirement;
		if (LvlReq >= 1)
		{
			ItemLevelText->SetText(FText::FromString(FString::Printf(TEXT("Required Level: %d"), LvlReq)));
			ItemLevelText->SetVisibility(ESlateVisibility::Visible);
		}
		else
		{
			ItemLevelText->SetVisibility(ESlateVisibility::Hidden);
		}
	}
}

void UItemTooltipWidget::UpdateItemWeight()
{
	// Update item weight
	if (WeightText)
	{
		if (CurrentItem.weight > 0.0f)
		{
			// ���������� ���������������� ������� ��������������
			FText WeightText_FormattedText = FormatWeightText(CurrentItem.weight);
			
			WeightText->SetText(WeightText_FormattedText);
			WeightText->SetVisibility(ESlateVisibility::Visible);
			
			// ��������� �������� ����������� ����
			WeightText->SetColorAndOpacity(GetWeightColor(CurrentItem.weight));
		}
		else
		{
			// �������� ������ ����, ���� ��� 0 ��� �������������
			WeightText->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void UItemTooltipWidget::UpdateItemDurability()
{
	if (!DurabilityText) return;

	if (CurrentItem.isDurable && CurrentItem.durabilityMax > 0)
	{
		const FLinearColor DurColor = CurrentItem.isDurabilityWarning
			? FLinearColor(1.0f, 0.3f, 0.1f, 1.0f)
			: FLinearColor(0.7f, 0.9f, 0.7f, 1.0f);

		DurabilityText->SetText(FText::FromString(
			FString::Printf(TEXT("Durability: %d / %d"), CurrentItem.durabilityCurrent, CurrentItem.durabilityMax)));
if (CurrentItem.durabilityMax > 0)
{
const float DurRatio = static_cast<float>(CurrentItem.durabilityCurrent) / static_cast<float>(CurrentItem.durabilityMax);
FString TierNote;
if (DurRatio < 0.25f) TierNote = TEXT(" - broken tier: -30% stats, repair soon!");
else if (DurRatio < 0.50f) TierNote = TEXT(" (-15% stats)");
else if (DurRatio < 0.75f) TierNote = TEXT(" (-5% stats)");
if (!TierNote.IsEmpty())
DurabilityText->SetText(FText::FromString(DurabilityText->GetText().ToString() + TierNote));
}
		DurabilityText->SetColorAndOpacity(DurColor);
		DurabilityText->SetVisibility(ESlateVisibility::Visible);
	}
	else
	{
		DurabilityText->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UItemTooltipWidget::UpdateEquipInfo()
{
	if (!EquipSlotText) return;

	if (CurrentItem.isEquippable && !CurrentItem.equipSlotSlug.IsEmpty())
	{
		FString SlotText = CurrentItem.equipSlotSlug.Replace(TEXT("_"), TEXT(" "));
		SlotText = SlotText.Left(1).ToUpper() + SlotText.Mid(1);
		if (CurrentItem.isTwoHanded)
			SlotText += TEXT(" (Two-Handed)");
if (CurrentItem.equipSlotSlug.Equals(TEXT("off_hand"), ESearchCase::IgnoreCase))
SlotText += TEXT(" - attacks at 50%");
if (!CurrentItem.setSlug.IsEmpty())
SlotText += FString::Printf(TEXT(" | Set: %s (bonuses apply automatically to stats)"), *CurrentItem.setSlug);
		EquipSlotText->SetText(FText::FromString(SlotText));
		EquipSlotText->SetColorAndOpacity(FLinearColor(0.9f, 0.8f, 0.5f, 1.0f));
		EquipSlotText->SetVisibility(ESlateVisibility::Visible);
	}
	else
	{
		EquipSlotText->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UItemTooltipWidget::UpdateVendorPrices()
{
	if (!VendorPriceText) return;

	if (CurrentItem.priceSell > 0 || CurrentItem.priceBuy > 0)
	{
		FString PriceStr;
		if (CurrentItem.priceBuy > 0 && CurrentItem.priceSell > 0)
			PriceStr = FString::Printf(TEXT("Buy: %d  Sell: %d"), CurrentItem.priceBuy, CurrentItem.priceSell);
		else if (CurrentItem.priceBuy > 0)
			PriceStr = FString::Printf(TEXT("Buy: %d"), CurrentItem.priceBuy);
		else
			PriceStr = FString::Printf(TEXT("Sell: %d"), CurrentItem.priceSell);

		VendorPriceText->SetText(FText::FromString(PriceStr));
		VendorPriceText->SetColorAndOpacity(FLinearColor(1.0f, 0.85f, 0.3f, 1.0f));
		VendorPriceText->SetVisibility(ESlateVisibility::Visible);
	}
	else
	{
		VendorPriceText->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UItemTooltipWidget::UpdateUseEffects()
{
	if (!UseEffectsBox) return;

	UseEffectsBox->ClearChildren();

	if (!CurrentItem.isUsable || CurrentItem.useEffects.Num() == 0)
	{
		UseEffectsBox->SetVisibility(ESlateVisibility::Collapsed);
		if (Separator4) Separator4->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	for (const FItemUseEffectEntry& Effect : CurrentItem.useEffects)
	{
		FString EffectStr;
		if (Effect.isInstant)
		{
			EffectStr = FString::Printf(TEXT("Restores %.0f %s"), Effect.value,
				Effect.attributeSlug.IsEmpty() ? TEXT("HP") : *Effect.attributeSlug.ToUpper());
		}
		else
		{
			EffectStr = FString::Printf(TEXT("%s +%.0f over %ds"),
				Effect.attributeSlug.IsEmpty() ? TEXT("Effect") : *Effect.attributeSlug.ToUpper(),
				Effect.value, Effect.durationSeconds);
		}
		if (Effect.cooldownSeconds > 0)
			EffectStr += FString::Printf(TEXT(" (CD: %ds)"), Effect.cooldownSeconds);

		UTextBlock* EffectLabel = NewObject<UTextBlock>(this);
		if (EffectLabel)
		{
			EffectLabel->SetText(FText::FromString(EffectStr));
			EffectLabel->SetColorAndOpacity(FLinearColor(0.4f, 1.0f, 0.5f, 1.0f));
			FSlateFontInfo FontInfo = EffectLabel->GetFont();
			FontInfo.Size = 12;
			EffectLabel->SetFont(FontInfo);
			UseEffectsBox->AddChild(EffectLabel);
		}
	}

	UseEffectsBox->SetVisibility(ESlateVisibility::Visible);
	if (Separator4) Separator4->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UItemTooltipWidget::UpdateKillCount()
{
	ClearKillCountWidget();

	if (KillCountWidgetClass && CurrentItem.killCount > 0)
	{
		UUserWidget* RowWidget = CreateWidget<UUserWidget>(GetOwningPlayer(), KillCountWidgetClass);
		if (RowWidget)
		{
			if (UTextBlock* RowText = Cast<UTextBlock>(RowWidget->GetWidgetFromName(TEXT("Row_Text"))))
			{
				RowText->SetText(FText::FromString(FString::Printf(TEXT("Kills: %d"), CurrentItem.killCount)));
			}
			if (MainContent)
				MainContent->AddChild(RowWidget);
			KillCountRowWidget = RowWidget;
		}
		return;
	}

	if (!KillCountText) return;

	if (CurrentItem.killCount > 0)
	{
		KillCountText->SetText(FText::FromString(FString::Printf(TEXT("Kills: %d"), CurrentItem.killCount)));
		KillCountText->SetColorAndOpacity(FLinearColor(1.0f, 0.4f, 0.2f, 1.0f));
		KillCountText->SetVisibility(ESlateVisibility::Visible);
	}
	else
	{
		KillCountText->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UItemTooltipWidget::ClearKillCountWidget()
{
	if (KillCountRowWidget)
	{
		if (MainContent)
			MainContent->RemoveChild(KillCountRowWidget);
		KillCountRowWidget = nullptr;
	}
}

