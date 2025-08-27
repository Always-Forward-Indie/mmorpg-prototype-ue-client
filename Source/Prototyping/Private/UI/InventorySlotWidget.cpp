#include "UI/InventorySlotWidget.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Engine/Engine.h"
#include "MyGameInstance.h"

UInventorySlotWidget::UInventorySlotWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SlotIndex = -1;
	bIsHighlighted = false;
	bIsSelected = false;
	bIsHovered = false;
	bIsOccupied = false;
	CurrentItem = FInventoryItemStruct();

	// Initialize default colors
	DefaultBorderColor = FLinearColor(0.1f, 0.1f, 0.1f, 1.0f);
	HighlightBorderColor = FLinearColor(1.0f, 1.0f, 0.0f, 1.0f);
	SelectedBorderColor = FLinearColor(0.0f, 1.0f, 0.0f, 1.0f);
	HoveredBorderColor = FLinearColor(0.5f, 0.5f, 1.0f, 1.0f);
}

void UInventorySlotWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Initialize rarity colors
	InitializeRarityColors();

	// Bind button events
	if (SlotButton)
	{
		SlotButton->OnClicked.AddDynamic(this, &UInventorySlotWidget::OnSlotButtonClicked);
		SlotButton->OnHovered.AddDynamic(this, &UInventorySlotWidget::OnSlotButtonHovered);
		SlotButton->OnUnhovered.AddDynamic(this, &UInventorySlotWidget::OnSlotButtonUnhovered);
	}

	// Initialize visual appearance
	UpdateSlotAppearance();
}

void UInventorySlotWidget::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseEnter(InGeometry, InMouseEvent);
	
	bIsHovered = true;
	UpdateSlotAppearance();
	OnSlotHovered.Broadcast(SlotIndex, true);
}

void UInventorySlotWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseLeave(InMouseEvent);
	
	bIsHovered = false;
	UpdateSlotAppearance();
	OnSlotHovered.Broadcast(SlotIndex, false);
}

FReply UInventorySlotWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// Hide tooltip on any mouse button click
	OnSlotHovered.Broadcast(SlotIndex, false);
	
	if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		OnSlotRightClicked.Broadcast(SlotIndex);
		return FReply::Handled();
	}

	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void UInventorySlotWidget::InitializeSlot(int32 InSlotIndex)
{
	SlotIndex = InSlotIndex;
	ClearSlot();
	
	UE_LOG(LogTemp, Verbose, TEXT("InventorySlotWidget: Initialized slot %d"), SlotIndex);
}

void UInventorySlotWidget::SetItemData(const FInventoryItemStruct& Item)
{
	CurrentItem = Item;

	if (CurrentItem.itemId > 0)
	{
		bIsOccupied = true;
	}
	
	// Update visual components
	LoadItemIcon();
	UpdateQuantityText();
	UpdateSlotAppearance();

	UE_LOG(LogTemp, Verbose, TEXT("InventorySlotWidget: Set item data for slot %d - %s"), SlotIndex, *Item.name);
}

void UInventorySlotWidget::ClearSlot()
{
	CurrentItem = FInventoryItemStruct();
	
	// Clear visual components
	if (ItemIcon)
	{
		ItemIcon->SetBrushFromTexture(nullptr);
		ItemIcon->SetVisibility(ESlateVisibility::Hidden);
	}
	
	if (QuantityText)
	{
		QuantityText->SetText(FText::GetEmpty());
		QuantityText->SetVisibility(ESlateVisibility::Hidden);
	}
	
	UpdateSlotAppearance();
}

void UInventorySlotWidget::SetSlotHighlight(bool bHighlighted)
{
	bIsHighlighted = bHighlighted;
	UpdateSlotAppearance();
}

void UInventorySlotWidget::SetSlotSelected(bool bSelected)
{
	bIsSelected = bSelected;
	UpdateSlotAppearance();
}

void UInventorySlotWidget::UpdateSlotAppearance()
{
	if (!SlotBorder && !RarityBorder)
	{
		return;
	}

	// Determine border color based on state
	FLinearColor BorderColor = DefaultBorderColor;

	if (bIsOccupied)
	{
		ImgOccupied->SetVisibility(ESlateVisibility::Visible);
		ImgBase->SetVisibility(ESlateVisibility::Hidden);
	}
	else if (!bIsOccupied)
	{
		ImgOccupied->SetVisibility(ESlateVisibility::Hidden);
	}
	
	if (bIsSelected && bIsOccupied)
	{
		BorderColor = SelectedBorderColor;
		ImgSelected->SetVisibility(ESlateVisibility::Visible);
		//ImgBase->SetColorAndOpacity(HasItem()
		//	? FLinearColor(1.06f, 1.06f, 1.06f, 1.f)   // +6% €ркости
		//	: FLinearColor(1.f, 1.f, 1.f, 1.f));
	}
	else if (!bIsSelected)
	{
		ImgSelected->SetVisibility(ESlateVisibility::Hidden);
	}


	if (bIsHighlighted && bIsOccupied)
	{
		BorderColor = HighlightBorderColor;
	}


	if (bIsHovered && bIsOccupied)
	{
		BorderColor = HoveredBorderColor;
		ImgStroke->SetVisibility(ESlateVisibility::Visible);
	}
	else if (!bIsHovered)
	{
		ImgStroke->SetVisibility(ESlateVisibility::Hidden);
	}

	// Apply border color
	//if (SlotBorder)
	//{
	//	SlotBorder->SetBrushColor(BorderColor);
	//}

	// Set rarity border color if item exists
	//if (RarityBorder && HasItem())
	//{
	//	FLinearColor RarityColor = GetRarityColor(CurrentItem.rarity);
	//	RarityBorder->SetBrushColor(RarityColor);
	//	RarityBorder->SetVisibility(ESlateVisibility::Visible);
	//}
	//else if (RarityBorder)
	//{
	//	RarityBorder->SetVisibility(ESlateVisibility::Hidden);
	//}
}

void UInventorySlotWidget::LoadItemIcon()
{
	if (!ItemIcon || !HasItem())
	{
		return;
	}

	// Get ItemManager from game instance
	UMyGameInstance* GameInstance = Cast<UMyGameInstance>(GetWorld()->GetGameInstance());
	if (!GameInstance)
	{
		UE_LOG(LogTemp, Error, TEXT("InventorySlotWidget: GameInstance not found"));
		return;
	}

	UItemManager* ItemManager = GameInstance->GetItemManager();
	if (!ItemManager)
	{
		UE_LOG(LogTemp, Error, TEXT("InventorySlotWidget: ItemManager not found"));
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
			TWeakObjectPtr<UInventorySlotWidget> WeakThis = this;

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

	UE_LOG(LogTemp, Warning, TEXT("InventorySlotWidget: Loading icon for item %s (ID: %d)"), *CurrentItem.name, CurrentItem.itemId);
}

void UInventorySlotWidget::SetIconTexture(UTexture2D* Texture)
{
	if (ItemIcon && Texture)
	{
		ItemIcon->SetBrushFromTexture(Texture);
		ItemIcon->SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 1.0f));
		//ItemIcon->SetColorAndOpacity(FLinearColor::White);
		ItemIcon->SetRenderOpacity(1.0f);

		UE_LOG(LogTemp, Log, TEXT("Set icon texture %s. Has alpha: %s"), *Texture->GetName(), Texture->HasAlphaChannel() ? TEXT("Yes") : TEXT("No"));
		UE_LOG(LogTemp, Warning, TEXT("InventorySlotWidget: Set icon texture for item %s"), *CurrentItem.name);
	}
}

void UInventorySlotWidget::SetDefaultIcon()
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
void UInventorySlotWidget::AsyncLoad(const FSoftObjectPath& AssetPath, FStreamableDelegate Callback)
{
	if (AssetPath.IsValid())
	{
		UAssetManager& AssetManager = UAssetManager::Get();
		TSharedPtr<FStreamableHandle> Handle = AssetManager.GetStreamableManager().RequestAsyncLoad(
			AssetPath,
			Callback
		);

		// Store the handle if you need to cancel loading later
		StreamableHandle = Handle;
	}
}

void UInventorySlotWidget::UpdateQuantityText()
{
	if (!QuantityText)
	{
		return;
	}

	if (HasItem() && CurrentItem.quantity > 1)
	{
		QuantityText->SetText(FText::AsNumber(CurrentItem.quantity));
		QuantityText->SetVisibility(ESlateVisibility::Visible);
	}
	else
	{
		QuantityText->SetText(FText::GetEmpty());
		QuantityText->SetVisibility(ESlateVisibility::Hidden);
	}
}

FLinearColor UInventorySlotWidget::GetRarityColor(const FString& Rarity) const
{
	if (const FLinearColor* Color = RarityColors.Find(Rarity))
	{
		return *Color;
	}
	
	// Default color for unknown rarity
	return FLinearColor::White;
}

void UInventorySlotWidget::InitializeRarityColors()
{
	RarityColors.Empty();
	
	// Standard rarity colors
	RarityColors.Add(TEXT("common"), FLinearColor::White);
	RarityColors.Add(TEXT("uncommon"), FLinearColor::Green);
	RarityColors.Add(TEXT("rare"), FLinearColor::Blue);
	RarityColors.Add(TEXT("epic"), FLinearColor(0.5f, 0.0f, 1.0f, 1.0f)); // Purple
	RarityColors.Add(TEXT("legendary"), FLinearColor(1.0f, 0.5f, 0.0f, 1.0f)); // Orange
	RarityColors.Add(TEXT("mythic"), FLinearColor::Red);
	
	// Additional rarities (case-insensitive alternatives)
	RarityColors.Add(TEXT("normal"), FLinearColor::White);
	RarityColors.Add(TEXT("magic"), FLinearColor::Blue);
	RarityColors.Add(TEXT("unique"), FLinearColor(1.0f, 1.0f, 0.0f, 1.0f)); // Yellow
}

void UInventorySlotWidget::OnSlotButtonClicked()
{
	OnSlotClicked.Broadcast(SlotIndex);
}

void UInventorySlotWidget::OnSlotButtonHovered()
{

}

void UInventorySlotWidget::OnSlotButtonUnhovered()
{

}

void UInventorySlotWidget::SetSlotSide(float InSide)
{
	SlotSide = InSide;
	if (SlotSizeBox)
	{
		SlotSizeBox->SetWidthOverride(SlotSide);
		SlotSizeBox->SetHeightOverride(SlotSide);
	}
}

void UInventorySlotWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	SetSlotSide(SlotSide);
}