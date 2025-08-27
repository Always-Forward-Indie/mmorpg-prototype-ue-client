#include "UI/HarvestLootItemWidget.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Engine/Engine.h"
#include "Engine/AssetManager.h"
#include "MyGameInstance.h"
#include "Gameplay/Items/ItemManager.h"
#include "Data/ItemStruct.h"

void UHarvestLootItemWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Bind button event
	if (Button_Pickup)
	{
		Button_Pickup->OnClicked.AddDynamic(this, &UHarvestLootItemWidget::OnPickupButtonClicked);
	}
}

void UHarvestLootItemWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	SetSlotSize(SlotSize);
}

void UHarvestLootItemWidget::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseEnter(InGeometry, InMouseEvent);
	
	// Broadcast hover event
	OnItemHovered.Broadcast(CurrentItemData.itemId, true);
}

void UHarvestLootItemWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseLeave(InMouseEvent);
	
	// Broadcast hover event
	OnItemHovered.Broadcast(CurrentItemData.itemId, false);
}

FReply UHarvestLootItemWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// Hide tooltip on any mouse button click by broadcasting hover end
	OnItemHovered.Broadcast(CurrentItemData.itemId, false);
	
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void UHarvestLootItemWidget::SetItemData(const FHarvestItemStruct& ItemData)
{
	CurrentItemData = ItemData;
	UpdateItemDisplay();
	LoadItemIcon();
}

void UHarvestLootItemWidget::SetSlotSize(float InSize)
{
	SlotSize = InSize;
	if (SlotSizeBox)
	{
		SlotSizeBox->SetWidthOverride(SlotSize);
		SlotSizeBox->SetHeightOverride(SlotSize);
	}
}

void UHarvestLootItemWidget::OnPickupButtonClicked()
{
	// Hide tooltip when pickup button is clicked
	OnItemHovered.Broadcast(CurrentItemData.itemId, false);
	
	UE_LOG(LogTemp, Warning, TEXT("HarvestLootItemWidget: Pickup button clicked for item %d"), CurrentItemData.itemId);
	
	// Broadcast pickup request
	OnItemPickupRequested.Broadcast(CurrentItemData.itemId, CurrentItemData.quantity);
}

void UHarvestLootItemWidget::UpdateItemDisplay()
{
	// Update item name
	if (TextBlock_ItemName)
	{
		if (bUseCompactLayout)
		{
			TextBlock_ItemName->SetVisibility(ESlateVisibility::Collapsed);
		}

		FString ItemName = CurrentItemData.name;
		if (ItemName.IsEmpty())
		{
			ItemName = FString::Printf(TEXT("Item %d"), CurrentItemData.itemId);
		}
		TextBlock_ItemName->SetText(FText::FromString(ItemName));
		
		// Set item name color based on rarity
		FLinearColor RarityColor = GetRarityColor(CurrentItemData.rarityId);
		TextBlock_ItemName->SetColorAndOpacity(FSlateColor(RarityColor));
	}

	// Update item type (hide if compact layout)
	if (TextBlock_ItemType)
	{
		if (bUseCompactLayout)
		{
			TextBlock_ItemType->SetVisibility(ESlateVisibility::Collapsed);
		}
		else
		{
			FString TypeText = CurrentItemData.itemType;
			if (TypeText.IsEmpty())
			{
				TypeText = TEXT("Material");
			}
			TextBlock_ItemType->SetText(FText::FromString(TypeText));
			TextBlock_ItemType->SetVisibility(ESlateVisibility::Visible);
		}
	}

	// Update quantity
	if (TextBlock_Quantity)
	{
		if (CurrentItemData.quantity > 1)
		{
			FString QuantityText = FString::Printf(TEXT("x%d"), CurrentItemData.quantity);
			TextBlock_Quantity->SetText(FText::FromString(QuantityText));
			TextBlock_Quantity->SetVisibility(ESlateVisibility::Visible);
		}
		else
		{
			TextBlock_Quantity->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	// Update description (hide if compact layout)
	if (TextBlock_Description)
	{
		if (bUseCompactLayout)
		{
			TextBlock_Description->SetVisibility(ESlateVisibility::Collapsed);
		}
		else
		{
			FString Description = CurrentItemData.description;
			if (Description.IsEmpty())
			{
				Description = TEXT("A harvested item");
			}
			TextBlock_Description->SetText(FText::FromString(Description));
			TextBlock_Description->SetVisibility(ESlateVisibility::Visible);
		}
	}

	// Update rarity border color
	if (Image_RarityBorder)
	{
		FLinearColor RarityColor = GetRarityColor(CurrentItemData.rarityId);
		Image_RarityBorder->SetColorAndOpacity(RarityColor);
	}

	// Update button visibility (hide if compact layout)
	/*if (Button_Pickup)
	{
		Button_Pickup->SetVisibility(bUseCompactLayout ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}*/
}

void UHarvestLootItemWidget::LoadItemIcon()
{
	if (!Image_ItemIcon)
	{
		return;
	}

	// Get ItemManager from game instance
	UMyGameInstance* GameInstance = Cast<UMyGameInstance>(GetWorld()->GetGameInstance());
	if (!GameInstance)
	{
		UE_LOG(LogTemp, Error, TEXT("HarvestLootItemWidget: GameInstance not found"));
		SetDefaultIcon();
		return;
	}

	UItemManager* ItemManager = GameInstance->GetItemManager();
	if (!ItemManager)
	{
		UE_LOG(LogTemp, Error, TEXT("HarvestLootItemWidget: ItemManager not found"));
		SetDefaultIcon();
		return;
	}

	// Get visual data for current item using its slug
	FItemVisualData VisualData = ItemManager->GetItemVisualDataBySlug(CurrentItemData.itemSlug);

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
			TWeakObjectPtr<UHarvestLootItemWidget> WeakThis = this;

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
	Image_ItemIcon->SetVisibility(ESlateVisibility::Visible);

	UE_LOG(LogTemp, Log, TEXT("HarvestLootItemWidget: Loading icon for item %s (ID: %d)"), 
		*CurrentItemData.name, CurrentItemData.itemId);
}

void UHarvestLootItemWidget::SetIconTexture(UTexture2D* Texture)
{
	if (Image_ItemIcon && Texture)
	{
		Image_ItemIcon->SetBrushFromTexture(Texture);
		Image_ItemIcon->SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 1.0f));
		Image_ItemIcon->SetRenderOpacity(1.0f);

		UE_LOG(LogTemp, Log, TEXT("HarvestLootItemWidget: Set icon texture for item %s"), 
			*CurrentItemData.name);
	}
}

void UHarvestLootItemWidget::SetDefaultIcon()
{
	if (Image_ItemIcon)
	{
		// Set a default placeholder icon or clear the brush
		Image_ItemIcon->SetBrushFromTexture(nullptr);
		
		// You can set a default placeholder texture here if you have one
		// Image_ItemIcon->SetBrushFromTexture(DefaultPlaceholderTexture);
	}
}

void UHarvestLootItemWidget::AsyncLoad(const FSoftObjectPath& Path, FStreamableDelegate Callback)
{
	if (!Path.IsValid())
	{
		if (Callback.IsBound()) 
		{ 
			Callback.Execute(); 
		}
		return;
	}

	if (UAssetManager* AM = UAssetManager::GetIfValid())
	{
		FStreamableManager& SM = AM->GetStreamableManager();
		StreamableHandle = SM.RequestAsyncLoad(Path, MoveTemp(Callback));
	}
	else
	{
		static FStreamableManager StaticSM;
		StreamableHandle = StaticSM.RequestAsyncLoad(Path, MoveTemp(Callback));
	}
}

FLinearColor UHarvestLootItemWidget::GetRarityColor(int32 RarityId) const
{
	switch (RarityId)
	{
	case 1: // Common
		return FLinearColor::White;
	case 2: // Uncommon
		return FLinearColor::Green;
	case 3: // Rare
		return FLinearColor::Blue;
	case 4: // Epic
		return FLinearColor(0.5f, 0.0f, 0.5f, 1.0f); // Purple
	case 5: // Legendary
		return FLinearColor(1.0f, 0.5f, 0.0f, 1.0f); // Orange
	default:
		return FLinearColor::Gray;
	}
}