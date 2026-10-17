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

FInventoryItemStruct UHarvestLootWidget::ConvertHarvestItemToInventoryItem(const FHarvestItemStruct& HarvestItem) const
{
	FInventoryItemStruct InventoryItem;
	
	InventoryItem.itemId = HarvestItem.itemId;
	InventoryItem.slug = HarvestItem.itemSlug;
	InventoryItem.quantity = HarvestItem.quantity;
	InventoryItem.name = HarvestItem.name;
	InventoryItem.description = HarvestItem.description;
	InventoryItem.type = HarvestItem.itemType;
	InventoryItem.weight = HarvestItem.weight;
	
	// Convert rarity from ID to string
	switch (HarvestItem.rarityId)
	{
		case 1: InventoryItem.rarity = TEXT("common"); break;
		case 2: InventoryItem.rarity = TEXT("uncommon"); break;
		case 3: InventoryItem.rarity = TEXT("rare"); break;
		case 4: InventoryItem.rarity = TEXT("epic"); break;
		case 5: InventoryItem.rarity = TEXT("legendary"); break;
		default: InventoryItem.rarity = TEXT("common"); break;
	}
	
	return InventoryItem;
}
