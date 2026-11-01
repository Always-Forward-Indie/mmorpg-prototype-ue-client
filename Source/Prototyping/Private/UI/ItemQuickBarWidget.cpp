#include "UI/ItemQuickBarWidget.h"
#include "UI/ItemQuickSlotWidget.h"
#include "Gameplay/Items/InventoryManager.h"
#include "Gameplay/Items/ItemManager.h"
#include "Data/ItemStruct.h"
#include "Audio/AudioManager.h"
#include "Prototyping.h"
#include "Components/HorizontalBox.h"
#include "MyGameInstance.h"

void UItemQuickBarWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

void UItemQuickBarWidget::InitQuickBar(UInventoryManager* InInventoryManager, int32 NumSlots)
{
	InventoryManager = InInventoryManager;
	if (!SlotsBox || !SlotWidgetClass)
	{
		UE_LOG(LogGameplay, Error, TEXT("ItemQuickBarWidget: cannot init — SlotsBox=%p SlotWidgetClass=%p. Bind SlotsBox in WBP and set SlotWidgetClass in BP_MyGameInstance/player interface defaults."),
			SlotsBox, SlotWidgetClass.Get());
		return;
	}

	const int32 Count = NumSlots > 0 ? NumSlots : DefaultSlotCount;
	Slots.Empty();

	// F6-F10: F1 is TargetSelf, 1-5 drive the skill bar.
	const TArray<FKey> DefaultKeys = {
		EKeys::F6, EKeys::F7, EKeys::F8, EKeys::F9, EKeys::F10
	};

	for (int32 i = 0; i < Count; ++i)
	{
		UItemQuickSlotWidget* Sl = CreateWidget<UItemQuickSlotWidget>(this, SlotWidgetClass);
		if (!Sl) continue;

		const FKey BoundKey = (i < DefaultKeys.Num()) ? DefaultKeys[i] : FKey();
		Sl->Setup(InventoryManager, i, BoundKey);
		Sl->OnQuickSlotUsed.AddDynamic(this, &UItemQuickBarWidget::OnSlotUsed);

		SlotsBox->AddChildToHorizontalBox(Sl);
		Slots.Add(Sl);
	}

	LoadSlotsState();

	if (InventoryManager)
		InventoryManager->OnItemCooldown.AddDynamic(this, &UItemQuickBarWidget::HandleItemCooldown);
}

void UItemQuickBarWidget::AssignItemToSlot(int32 SlotIndex, int32 ItemId, const FString& ItemSlug, int32 Quantity, const FString& IconPath)
{
	if (!Slots.IsValidIndex(SlotIndex)) return;
	Slots[SlotIndex]->AssignItem(ItemId, ItemSlug, Quantity, IconPath);
	SaveSlotsState();
}

void UItemQuickBarWidget::ClearSlot(int32 SlotIndex)
{
	if (!Slots.IsValidIndex(SlotIndex)) return;
	Slots[SlotIndex]->ClearSlot();
	SaveSlotsState();
}

void UItemQuickBarWidget::UseSlot(int32 SlotIndex)
{
	if (!Slots.IsValidIndex(SlotIndex)) return;
	Slots[SlotIndex]->UseItem();
}

int32 UItemQuickBarWidget::FindSlotWithItem(int32 ItemId) const
{
	if (ItemId <= 0) return INDEX_NONE;
	for (int32 i = 0; i < Slots.Num(); ++i)
	{
		if (Slots[i] && Slots[i]->GetItemId() == ItemId)
			return i;
	}
	return INDEX_NONE;
}

int32 UItemQuickBarWidget::FindFirstEmptySlot() const
{
	for (int32 i = 0; i < Slots.Num(); ++i)
	{
		if (Slots[i] && Slots[i]->IsEmpty())
			return i;
	}
	return INDEX_NONE;
}

void UItemQuickBarWidget::ToggleQuickbarItem(const FInventoryItemStruct& Item)
{
	if (Item.itemId <= 0 || !InventoryManager) return;

	const int32 Existing = FindSlotWithItem(Item.itemId);
	if (Existing != INDEX_NONE)
	{
		ClearSlot(Existing);
		return;
	}

	const int32 Empty = FindFirstEmptySlot();
	if (Empty == INDEX_NONE)
	{
		UE_LOG(LogGameplay, Warning, TEXT("ItemQuickBarWidget: bar is full — clear a slot (RMB) first"));
		if (UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance()))
		{
			if (GI->AudioManager)
				GI->AudioManager->PlayUISound(EUISoundEvent::ErrorSound);
		}
		return;
	}

	// Resolve the catalog slug (rows carry both; either may be populated).
	FString Slug = !Item.slug.IsEmpty() ? Item.slug : Item.itemSlug;
	FString IconPath;
	if (UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance()))
	{
		if (UItemManager* ItemMgr = GI->GetItemManager())
		{
			const FItemVisualData Vis = ItemMgr->GetItemVisualDataBySlug(Slug);
			if (!Vis.Icon.IsNull())
				IconPath = Vis.Icon.ToSoftObjectPath().ToString();
		}
	}
	AssignItemToSlot(Empty, Item.itemId, Slug, Item.quantity, IconPath);
}

void UItemQuickBarWidget::RefreshFromInventory()
{
	if (!InventoryManager) return;

	const FCharacterInventoryStruct& Inventory = InventoryManager->GetInventory();

	for (int32 i = 0; i < Slots.Num(); ++i)
	{
		const int32 ItemId = Slots[i]->GetItemId();
		if (ItemId <= 0) continue;

		const FInventoryItemStruct* Found = nullptr;
		for (const FInventoryItemStruct& InvItem : Inventory.items)
		{
			if (InvItem.itemId == ItemId)
			{
				Found = &InvItem;
				break;
			}
		}

		if (Found && Found->quantity > 0)
		{
			Slots[i]->UpdateQuantity(Found->quantity);
		}
		else
		{
			Slots[i]->ClearSlot();
		}
	}
}

void UItemQuickBarWidget::HandleInventoryUpdated(const FCharacterInventoryStruct& Inventory)
{
	RefreshFromInventory();
}

void UItemQuickBarWidget::HandleItemCooldown(int32 ItemId, int32 CooldownSeconds)
{
	if (ItemId <= 0 || CooldownSeconds <= 0) return;
	const int32 SlotIndex = FindSlotWithItem(ItemId);
	if (Slots.IsValidIndex(SlotIndex))
		Slots[SlotIndex]->StartCooldown(static_cast<float>(CooldownSeconds));
}

void UItemQuickBarWidget::OnSlotUsed(int32 SlotIndex)
{
	RefreshFromInventory();
}

void UItemQuickBarWidget::SaveSlotsState()
{
	if (!InventoryManager) return;

	UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance());
	if (!GI) return;

	// Per-character persistence: shared TArray would leak one character's
	// bar into another's after relog / character switch.
	const int32 CharId = InventoryManager->GetInventory().characterId;
	FCharacterQuickBarState SlotsData;
	for (const UItemQuickSlotWidget* Sl : Slots)
	{
		if (Sl && !Sl->IsEmpty())
		{
			SlotsData.Slots.Add(Sl->GetSlotData());
		}
	}
	if (CharId > 0)
		GI->QuickBarSlotsByCharacter.Add(CharId, MoveTemp(SlotsData));
}

void UItemQuickBarWidget::LoadSlotsState()
{
	UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance());
	if (!GI || !InventoryManager) return;

	const int32 CharId = InventoryManager->GetInventory().characterId;
	const FCharacterQuickBarState* SavedState = (CharId > 0)
		? GI->QuickBarSlotsByCharacter.Find(CharId)
		: nullptr;
	if (!SavedState) return;

	for (const FItemQuickSlotData& SavedSlot : SavedState->Slots)
	{
		if (Slots.IsValidIndex(SavedSlot.slotIndex))
		{
			Slots[SavedSlot.slotIndex]->AssignItem(
				SavedSlot.itemId, SavedSlot.itemSlug,
				SavedSlot.quantity, SavedSlot.iconPath);
		}
	}
}
