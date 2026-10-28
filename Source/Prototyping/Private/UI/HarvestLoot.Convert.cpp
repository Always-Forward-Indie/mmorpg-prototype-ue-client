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
#include "Utils/NetCore/HarvestRules.h"
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
	
	// Single source of truth for the rarity mapping (FHarvestRules).
	InventoryItem.rarity = FHarvestRules::RaritySlugForId(HarvestItem.rarityId);
	
	return InventoryItem;
}
