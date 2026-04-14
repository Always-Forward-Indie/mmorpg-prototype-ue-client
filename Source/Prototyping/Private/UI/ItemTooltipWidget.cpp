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

UItemTooltipWidget::UItemTooltipWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	CurrentItem = FInventoryItemStruct();
	TooltipOffset = FVector2D(10.0f, -10.0f);
	FadeInDuration = 0.2f;
	FadeOutDuration = 0.1f;
	bIsVisible = false;
	SetIsFocusable(false);
}

void UItemTooltipWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Initialize colors
	InitializeColors();
	// Hide tooltip initially
	SetVisibility(ESlateVisibility::Hidden);
	SetRenderOpacity(0.0f);

	// Subscribe to inventory updates so kill count refreshes live while the tooltip is open
	if (UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance()))
	{
		if (UInventoryManager* InvMgr = GI->GetInventoryManager())
		{
			InvMgr->OnInventoryUpdated.AddDynamic(this, &UItemTooltipWidget::OnInventoryUpdated);
		}
	}
}

void UItemTooltipWidget::OnInventoryUpdated(const FCharacterInventoryStruct& UpdatedInventory)
{
	if (CurrentItem.id <= 0 || !bIsVisible) return;

	// Find the matching item in the updated inventory and refresh kill count
	for (const FInventoryItemStruct& Item : UpdatedInventory.items)
	{
		if (Item.id == CurrentItem.id)
		{
			if (Item.killCount != CurrentItem.killCount)
			{
				CurrentItem.killCount = Item.killCount;
				UpdateKillCount();
			}
			break;
		}
	}
}

void UItemTooltipWidget::SetItemData(const FInventoryItemStruct& Item)
{
	CurrentItem = Item;

	// Resolve localised name and description from LocalizationSubsystem (keyed by slug)
	const FString& LookupSlug = CurrentItem.slug.IsEmpty() ? CurrentItem.itemSlug : CurrentItem.slug;
	if (!LookupSlug.IsEmpty())
	{
		if (UGameInstance* GI = GetGameInstance())
		{
			if (ULocalizationSubsystem* Loc = GI->GetSubsystem<ULocalizationSubsystem>())
			{
				FText LocalName = Loc->GetItemDisplayName(LookupSlug);
				if (!LocalName.IsEmpty())
					CurrentItem.name = LocalName.ToString();

				FText LocalDesc = Loc->GetItemDescription(LookupSlug);
				if (!LocalDesc.IsEmpty())
					CurrentItem.description = LocalDesc.ToString();
			}
		}
	}

	UpdateTooltipContent();
	LoadItemIcon();
}

void UItemTooltipWidget::UpdateTooltipPosition(FVector2D /*ScreenPosUnused*/)
{
	APlayerController* PC = GetOwningPlayer();
	if (!PC) return;

	// 1) ������� ������� � ����������� �������� (UE5)
	const FVector2D MousePos = UWidgetLayoutLibrary::GetMousePositionOnViewport(PC);

	// 2) �������� ����������� ����������� ������
	SetVisibility(ESlateVisibility::HitTestInvisible); // �� Collapsed
	ForceLayoutPrepass();

	FVector2D Size = GetCachedGeometry().GetLocalSize(); // ������, ��� DesiredSize
	if (Size.X <= 1.f || Size.Y <= 1.f)
		Size = GetDesiredSize();                         // fallback
	if (Size.IsZero())
		Size = FVector2D(300, 200);                      // ���������

	// 3) ������� ��������
	int32 Wpx = 0, Hpx = 0; PC->GetViewportSize(Wpx, Hpx);
	const FVector2D View(Wpx, Hpx);

	const float SafePad = 20.f;
	const FVector2D BaseOffset = TooltipOffset + FVector2D(SafePad, SafePad);

	// 4) ������� ������-�����
	FVector2D Pos = MousePos + BaseOffset;

	// 5) ������ �������������� ��� �������� - ����������� EarlyPad
	const float EarlyPad = SafePad + 50.f; // ��������� � 2.f �� 30.f ��� ����� ������� ������������
	if (Pos.X + Size.X > View.X - EarlyPad)
		Pos.X = MousePos.X - Size.X - FMath::Abs(TooltipOffset.X) - SafePad;

	if (Pos.Y + Size.Y > View.Y - EarlyPad)
		Pos.Y = MousePos.Y - Size.Y - FMath::Abs(TooltipOffset.Y) - SafePad;

	// 6) Ƹ����� ����� (������ ����������� ����)
	Pos.X = FMath::Clamp(Pos.X, 0.f, View.X - Size.X);
	Pos.Y = FMath::Clamp(Pos.Y, 0.f, View.Y - Size.Y);

	// 7) ��������� pivot � ������ �������
	SetAlignmentInViewport(FVector2D(0, 0));
	SetPositionInViewport(Pos, /*bRemoveDPIScale=*/false);
}

void UItemTooltipWidget::LoadItemIcon()
{
	if (!ItemIcon)
	{
		return;
	}

	// Get ItemManager from game instance
	UMyGameInstance* GameInstance = Cast<UMyGameInstance>(GetWorld()->GetGameInstance());
	if (!GameInstance)
	{
		UE_LOG(LogTemp, Error, TEXT("InventorySlotTooltipWidget: GameInstance not found"));
		return;
	}

	UItemManager* ItemManager = GameInstance->GetItemManager();
	if (!ItemManager)
	{
		UE_LOG(LogTemp, Error, TEXT("InventorySlotTooltipWidget: ItemManager not found"));
		return;
	}

	// Get visual data for current item using its slug
	FItemVisualData VisualData = ItemManager->GetItemVisualDataBySlug(CurrentItem.slug);

	if (!VisualData.Icon.IsNull())
	{
		// Load the icon texture asynchronously
		TSoftObjectPtr<UTexture2D> IconSoftPtr = VisualData.Icon;

		// Check if already loaded
		if (UTexture2D* LoadedTexture = IconSoftPtr.Get())
		{
			// Already loaded, set immediately
			SetIconTexture(LoadedTexture);
		}
		else
		{
			// Load asynchronously
			TWeakObjectPtr<UItemTooltipWidget> WeakThis = this;

			AsyncLoad(IconSoftPtr.ToSoftObjectPath(),
				FStreamableDelegate::CreateLambda([WeakThis, IconSoftPtr]()
					{
						if (WeakThis.IsValid())
						{
							if (UTexture2D* LoadedTexture = IconSoftPtr.Get())
							{
								WeakThis->SetIconTexture(LoadedTexture);
							}
						}
					})
			);
		}
	}
	else
	{
		// No icon found, use default or placeholder
		SetDefaultIcon();
	}

	// Show icon
	ItemIcon->SetVisibility(ESlateVisibility::Visible);

	UE_LOG(LogTemp, Warning, TEXT("InventorySlotTooltipWidget: Loading icon for item %s (ID: %d)"), *CurrentItem.name, CurrentItem.itemId);
}

void UItemTooltipWidget::SetIconTexture(UTexture2D* Texture)
{
	if (ItemIcon && Texture)
	{
		ItemIcon->SetBrushFromTexture(Texture);
		ItemIcon->SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 1.0f));
		//ItemIcon->SetColorAndOpacity(FLinearColor::White);
		ItemIcon->SetRenderOpacity(1.0f);

		UE_LOG(LogTemp, Log, TEXT("Set icon texture %s. Has alpha: %s"), *Texture->GetName(), Texture->HasAlphaChannel() ? TEXT("Yes") : TEXT("No"));
		UE_LOG(LogTemp, Warning, TEXT("InventorySlotTooltipWidget: Set icon texture for item %s"), *CurrentItem.name);
	}
}

void UItemTooltipWidget::SetDefaultIcon()
{
	if (ItemIcon)
	{
		// Set a default placeholder icon or clear the brush
		ItemIcon->SetBrushFromTexture(nullptr);
		// Or use a default placeholder texture if you have one
		// ItemIcon->SetBrushFromTexture(DefaultPlaceholderTexture);
	}
}

// Helper function for async loading
void UItemTooltipWidget::AsyncLoad(const FSoftObjectPath& Path, FStreamableDelegate Callback)
{
	if (!Path.IsValid())
	{
		if (Callback.IsBound()) { Callback.Execute(); }
		return;
	}

	if (UObject* Existing = Path.ResolveObject())
	{
		if (Callback.IsBound()) { Callback.Execute(); }
		return;
	}

	if (UAssetManager* AM = UAssetManager::GetIfInitialized())
	{
		AM->GetStreamableManager().RequestAsyncLoad(Path, MoveTemp(Callback));
	}
	else
	{
		static FStreamableManager StaticSM;
		StaticSM.RequestAsyncLoad(Path, MoveTemp(Callback));
	}
}


void UItemTooltipWidget::ShowTooltip()
{
	if (bIsVisible)
	{
		return;
	}

	bIsVisible = true;
	SetVisibility(ESlateVisibility::HitTestInvisible);

	ForceLayoutPrepass();

	// Fade in animation
	//UWidgetBlueprintLibrary::SetFocusToGameViewport();
	
	// Simple fade in (you can replace this with a proper animation blueprint)
	SetRenderOpacity(0.0f);
	
	// TODO: Implement proper fade-in animation
	// For now, just set to visible
	SetRenderOpacity(1.0f);

	UE_LOG(LogTemp, Verbose, TEXT("ItemTooltipWidget: Showing tooltip for item %s"), *CurrentItem.name);
}

void UItemTooltipWidget::HideTooltip()
{
	if (!bIsVisible)
	{
		return;
	}

	bIsVisible = false;
	
	// TODO: Implement proper fade-out animation
	// For now, just hide immediately
	SetVisibility(ESlateVisibility::Hidden);
	SetRenderOpacity(0.0f);

	UE_LOG(LogTemp, Verbose, TEXT("ItemTooltipWidget: Hiding tooltip"));
}

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

FLinearColor UItemTooltipWidget::GetRarityColor(const FString& Rarity) const
{
	if (const FLinearColor* Color = RarityColors.Find(Rarity.ToLower()))
	{
		return *Color;
	}
	
	return FLinearColor::White; // Default color
}

FLinearColor UItemTooltipWidget::GetTypeColor(const FString& Type) const
{
	if (const FLinearColor* Color = TypeColors.Find(Type.ToLower()))
	{
		return *Color;
	}
	
	return FLinearColor(0.7f, 0.7f, 0.7f, 1.0f); // Default gray
}

FLinearColor UItemTooltipWidget::GetWeightColor(float Weight) const
{
	//FColor SRGB = FColor::FromHex(TEXT("CBBDA1FF"));
	FLinearColor Col = FLinearColor(0.796079f, 0.741176f, 0.631373f, 1.0f);

	if (!bShowWeightColorCoding)
	{
		return Col; // Default light gray
	}

	if (Weight > HeavyWeightThreshold)
	{
		return FLinearColor::Red; // ������ �������� - �������
	}
	else if (Weight > MediumWeightThreshold)
	{
		return FLinearColor::Yellow; // ������� ��� - �����
	}
	else if (Weight > LightWeightThreshold)
	{
		return FLinearColor(1.0f, 0.8f, 0.4f, 1.0f); // ˸���� �������� - ���������
	}
	
	// ����� ����� �������� �������� ������
	return Col;
}

FText UItemTooltipWidget::FormatWeightText(float Weight) const
{
	if (Weight <= 0.0f)
	{
		return FText::GetEmpty();
	}

	// ��������� ������� ����� ����
	FString WeightString;
	
	// ���� ��� �������� ����� ������ (��� ������� �����)
	if (FMath::IsNearlyEqual(Weight, FMath::RoundToFloat(Weight), 0.001f))
	{
		// ���������� ��� ����� �����
		WeightString = FString::Printf(TEXT("%d"), FMath::RoundToInt(Weight));
	}
	else
	{
		// ���������� � ����� ������� ����� �������, ������� ������ ����
		WeightString = FString::Printf(TEXT("%.2f"), Weight);
		
		// ������� ������ ���� � �����
		while (WeightString.EndsWith(TEXT("0")) && WeightString.Contains(TEXT(".")))
		{
			WeightString = WeightString.LeftChop(1);
		}
		if (WeightString.EndsWith(TEXT(".")))
		{
			WeightString = WeightString.LeftChop(1);
		}
	}
	
	// ��������� ������� ��������� ���� ��������
	if (bShowWeightUnit && !WeightUnit.IsEmpty())
	{
		return FText::FromString(FString::Printf(TEXT("Weight: %s %s"), *WeightString, *WeightUnit));
	}
	else
	{
		return FText::FromString(FString::Printf(TEXT("Weight: %s"), *WeightString));
	}
}

FText UItemTooltipWidget::FormatAttributeText(const FString& AttributeName, const FString& AttributeValue) const
{
	FString FormattedName = FormatAttributeName(AttributeName);
	return FText::FromString(FString::Printf(TEXT("%s: %s"), *FormattedName, *AttributeValue));
}

void UItemTooltipWidget::InitializeColors()
{
	// Initialize rarity colors
	RarityColors.Empty();
	RarityColors.Add(TEXT("common"), FLinearColor::White);
	RarityColors.Add(TEXT("uncommon"), FLinearColor::Green);
	RarityColors.Add(TEXT("rare"), FLinearColor::Blue);
	RarityColors.Add(TEXT("very_rare"), FLinearColor(0.3f, 0.3f, 1.0f, 1.0f));
	RarityColors.Add(TEXT("epic"), FLinearColor(0.5f, 0.0f, 1.0f, 1.0f)); // Purple
	RarityColors.Add(TEXT("legendary"), FLinearColor(1.0f, 0.5f, 0.0f, 1.0f)); // Orange
	RarityColors.Add(TEXT("mythic"), FLinearColor::Red);

	// Initialize type colors
	TypeColors.Empty();
	TypeColors.Add(TEXT("weapon"), FLinearColor(1.0f, 0.2f, 0.2f, 1.0f)); // Red
	TypeColors.Add(TEXT("armor"), FLinearColor(0.2f, 0.5f, 1.0f, 1.0f)); // Blue
	TypeColors.Add(TEXT("consumable"), FLinearColor(0.2f, 1.0f, 0.2f, 1.0f)); // Green
	TypeColors.Add(TEXT("quest"), FLinearColor(1.0f, 1.0f, 0.2f, 1.0f)); // Yellow
	TypeColors.Add(TEXT("tool"), FLinearColor(0.8f, 0.4f, 0.2f, 1.0f)); // Brown
	TypeColors.Add(TEXT("resource"), FLinearColor(0.6f, 0.6f, 0.6f, 1.0f)); // Gray
}

void UItemTooltipWidget::ClearDynamicAttributes()
{
	if (!AttributesBox)
	{
		return;
	}

	// Remove dynamically created attribute text widgets
	// Note: This is a simplified approach. In a more complex implementation,
	// you might want to track created widgets separately.
	
	TArray<UWidget*> ChildrenToRemove;
	
	for (int32 i = 0; i < AttributesBox->GetChildrenCount(); i++)
	{
		UWidget* Child = AttributesBox->GetChildAt(i);
		if (UTextBlock* TextBlock = Cast<UTextBlock>(Child))
		{
			// Check if this is a dynamically created attribute widget
			// (you could use tags or other identification methods)
			ChildrenToRemove.Add(Child);
		}
	}

	for (UWidget* Child : ChildrenToRemove)
	{
		AttributesBox->RemoveChild(Child);
	}
}

UTextBlock* UItemTooltipWidget::AddAttributeTextWidget(const FString& AttributeName, const FString& AttributeValue)
{
	if (!AttributesBox)
	{
		return nullptr;
	}

	// Create new text widget using the widget tree
	UTextBlock* AttributeTextWidget = NewObject<UTextBlock>(this);
	if (!AttributeTextWidget)
	{
		return nullptr;
	}

	// Set text content
	AttributeTextWidget->SetText(FormatAttributeText(AttributeName, AttributeValue));
	
	// Style the text (you can customize this)
	AttributeTextWidget->SetColorAndOpacity(FLinearColor(0.9f, 0.9f, 0.9f, 1.0f));

	//set font size
	FSlateFontInfo FontInfo = AttributeTextWidget->GetFont();
	FontInfo.Size = 12; // ����� ������ � pt
	AttributeTextWidget->SetFont(FontInfo);

	// Add to attributes box
	AttributesBox->AddChild(AttributeTextWidget);

	return AttributeTextWidget;
}

FString UItemTooltipWidget::FormatAttributeName(const FString& AttributeName) const
{
	FString FormattedName = AttributeName;
	
	// Replace underscores with spaces
	FormattedName = FormattedName.Replace(TEXT("_"), TEXT(" "));
	
	// Capitalize each word
	TArray<FString> Words;
	FormattedName.ParseIntoArray(Words, TEXT(" "));
	
	FormattedName.Empty();
	for (int32 i = 0; i < Words.Num(); i++)
	{
		if (!Words[i].IsEmpty())
		{
			FString Word = Words[i].ToLower();
			Word = Word.Left(1).ToUpper() + Word.Mid(1);
			
			if (i > 0)
			{
				FormattedName += TEXT(" ");
			}
			FormattedName += Word;
		}
	}
	
	return FormattedName;
}