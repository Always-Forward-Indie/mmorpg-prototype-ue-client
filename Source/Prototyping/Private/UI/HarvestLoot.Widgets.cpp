#include "UI/HarvestLootWidget.h"
#include "Components/ScrollBox.h"
#include "Components/WrapBox.h"
#include "Components/WrapBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "UI/HarvestLootItemWidget.h"
#include "UI/ItemTooltipWidget.h"
#include "Engine/Engine.h"
#include "Gameplay/Items/HarvestManager.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/CanvasPanelSlot.h"

void UHarvestLootWidget::CreateLootItemWidgets()
{
	UPanelWidget* Container = GetLootContainer();
	if (!Container || !LootItemWidgetClass)
	{
		UE_LOG(LogTemp, Error, TEXT("HarvestLootWidget: Missing required components for creating loot widgets"));
		return;
	}

	// Clear existing widgets
	Container->ClearChildren();
	LootItemWidgets.Empty();

	// Configure WrapBox if using grid layout
	if (bUseGridLayout && WrapBox_LootItems)
	{
		WrapBox_LootItems->SetInnerSlotPadding(FVector2D(SlotGap, SlotGap));
	}

	// Create widgets for each loot item
	for (const FHarvestItemStruct& LootItem : CurrentLootItems)
	{
		UHarvestLootItemWidget* ItemWidget = CreateWidget<UHarvestLootItemWidget>(GetOwningPlayer(), LootItemWidgetClass);
		if (ItemWidget)
		{
			// Set item data
			ItemWidget->SetItemData(LootItem);
			
			// Bind pickup event
			ItemWidget->OnItemPickupRequested.AddDynamic(this, &UHarvestLootWidget::OnItemPickupRequested);
			
			// Bind hover events for tooltip
			ItemWidget->OnItemHovered.AddDynamic(this, &UHarvestLootWidget::OnItemHovered);
			
			// Add to container
			if (bUseGridLayout && WrapBox_LootItems)
			{
				// Add to WrapBox with slot configuration
				if (UWrapBoxSlot* WrapSlot = WrapBox_LootItems->AddChildToWrapBox(ItemWidget))
				{
					WrapSlot->SetPadding(FMargin(SlotGap));
					WrapSlot->SetHorizontalAlignment(HAlign_Left);
					WrapSlot->SetVerticalAlignment(VAlign_Top);
					WrapSlot->SetFillEmptySpace(false);
				}
			}
			else if (ScrollBox_LootItems)
			{
				// Add to ScrollBox
				ScrollBox_LootItems->AddChild(ItemWidget);
			}
			
			LootItemWidgets.Add(ItemWidget);
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("HarvestLootWidget: Created %d loot item widgets using %s layout"), 
		LootItemWidgets.Num(), bUseGridLayout ? TEXT("Grid") : TEXT("List"));
}

void UHarvestLootWidget::UpdateItemCountDisplay()
{
	if (TextBlock_ItemCount)
	{
		FString CountText = FString::Printf(TEXT("Items: %d"), CurrentLootItems.Num());
		TextBlock_ItemCount->SetText(FText::FromString(CountText));
	}

	// Enable/disable pickup all button based on item count
	if (Button_PickupAll)
	{
		Button_PickupAll->SetIsEnabled(CurrentLootItems.Num() > 0);
	}
}

