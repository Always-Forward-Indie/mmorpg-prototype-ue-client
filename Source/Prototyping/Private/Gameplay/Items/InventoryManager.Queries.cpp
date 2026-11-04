#include "Gameplay/Items/InventoryManager.h"
#include "Networking/NetworkManager.h"
#include "MyGameInstance.h"
#include "Gameplay/Items/HarvestManager.h"
#include "Utils/JSONParser.h"
#include "Utils/NetCore/InventoryQuery.h"
#include "Utils/NetCore/InventoryCapacityRule.h"
#include "Engine/World.h"
#include "UI/InventoryWidget.h"
#include "Gameplay/Equipment/EquipmentManager.h"
#include "Gameplay/Players/BasicPlayer.h"
#include "Audio/AudioManager.h"
#include "Gameplay/Items/ItemManager.h"
#include "Utils/AudioSpawnHelpers.h"
#include "CrashDiagnostics.h"
#include "Prototyping.h"

bool UInventoryManager::HasItem(int32 ItemId) const
{
	// Kind check: template identity (use/drop preconditions, harvest checks).
	return FInventoryQuery::HasItem(CurrentInventory.items, ItemId);
}

FInventoryItemStruct UInventoryManager::GetItemByInstanceId(int32 InstanceId) const
{
	// Row identity: tooltips, equip visuals, durability, kill-count.
	const int32 Idx = FInventoryQuery::IndexOfInstance(CurrentInventory.items, InstanceId);
	return Idx != INDEX_NONE ? CurrentInventory.items[Idx] : FInventoryItemStruct();
}

FInventoryItemStruct UInventoryManager::GetItemByTemplateId(int32 TemplateId) const
{
	// Kind lookup: first row of a template (sounds, icons).
	const int32 Idx = FInventoryQuery::IndexOfTemplate(CurrentInventory.items, TemplateId);
	return Idx != INDEX_NONE ? CurrentInventory.items[Idx] : FInventoryItemStruct();
}

int32 UInventoryManager::GetItemQuantity(int32 ItemId) const
{
	// Total across all rows of the template (stackables may split rows).
	return FInventoryQuery::QuantityOf(CurrentInventory.items, ItemId);
}

TArray<FInventoryItemStruct> UInventoryManager::GetItemsByType(const FString& ItemType) const
{
	// Filter lives in FInventoryQuery (pinned by MMO.Inventory.Query).
	return FInventoryQuery::ByType(CurrentInventory.items, ItemType);
}

TArray<FInventoryItemStruct> UInventoryManager::GetItemsByRarity(const FString& Rarity) const
{
	// Filter lives in FInventoryQuery (pinned by MMO.Inventory.Query).
	return FInventoryQuery::ByRarity(CurrentInventory.items, Rarity);
}

int32 UInventoryManager::GetTotalItemCount() const
{
	int32 TotalCount = 0;
	for (const FInventoryItemStruct& Item : CurrentInventory.items)
	{
		TotalCount += Item.quantity;
	}
	return TotalCount;
}

static TArray<FString>& JunkSlugsFor(UMyGameInstance* GI, int32 CharacterId)
{
	static TArray<FString> Empty;
	if (!GI || CharacterId <= 0) return Empty;
	FCharacterJunkState& State = GI->JunkSlugsByCharacter.FindOrAdd(CharacterId);
	return State.Slugs;
}

bool UInventoryManager::IsJunkSlug(const FString& Slug) const
{
	if (Slug.IsEmpty() || !gameInstance) return false;
	return JunkSlugsFor(gameInstance, CurrentInventory.characterId).Contains(Slug);
}

void UInventoryManager::ToggleJunkSlug(const FString& Slug)
{
	if (Slug.IsEmpty() || !gameInstance) return;
	TArray<FString>& Slugs = JunkSlugsFor(gameInstance, CurrentInventory.characterId);
	if (Slugs.Contains(Slug))
		Slugs.Remove(Slug);
	else
		Slugs.Add(Slug);
}

int32 UInventoryManager::GetJunkRowCount() const
{
	if (!gameInstance) return 0;
	const TArray<FString>& Slugs = JunkSlugsFor(gameInstance, CurrentInventory.characterId);
	int32 Count = 0;
	for (const FInventoryItemStruct& Item : CurrentInventory.items)
	{
		if (Item.itemId <= 0 || Item.is_equipped) continue;
		const FString ItemSlug = !Item.itemSlug.IsEmpty() ? Item.itemSlug : Item.slug;
		if (Slugs.Contains(ItemSlug)) ++Count;
	}
	return Count;
}

int32 UInventoryManager::GetMaxInventorySlots() const
{
	// Server cap wins when present; 0 = legacy/unlimited -> UI grid fallback (48).
	if (CurrentInventory.maxSlots > 0) return CurrentInventory.maxSlots;
	return 48;
}

bool UInventoryManager::CanAcceptItem(int32 TemplateId) const
{
	const bool bOwns = TemplateId > 0 && HasItem(TemplateId);
	return FInventoryCapacityRule::CanAcceptNewTemplate(
		CurrentInventory.items.Num(), GetMaxInventorySlots(), bOwns);
}

bool UInventoryManager::IsInventoryFullFor(int32 TemplateId) const
{
	return !CanAcceptItem(TemplateId);
}

int32 UInventoryManager::GetFreeSlotCount() const
{
	return FInventoryCapacityRule::FreeSlots(CurrentInventory.items.Num(), GetMaxInventorySlots());
}

void UInventoryManager::NotifyInventoryFull(const FString& ErrorCode, const FString& DisplayMessage)
{
	const FString Code = ErrorCode.IsEmpty() ? TEXT("inventory_full") : ErrorCode;
	FString Msg = DisplayMessage;
	if (Msg.IsEmpty())
	{
		Msg = TEXT("Inventory is full — free a slot first.");
	}
	UE_LOG(LogGameplay, Error, TEXT("InventoryManager: capacity refused (%s): %s"), *Code, *Msg);
	OnInventoryFull.Broadcast(Code, Msg);
}

