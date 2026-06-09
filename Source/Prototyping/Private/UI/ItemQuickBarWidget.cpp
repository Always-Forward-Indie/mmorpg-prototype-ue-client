#include "UI/ItemQuickBarWidget.h"
#include "UI/ItemQuickSlotWidget.h"
#include "Gameplay/Items/InventoryManager.h"
#include "Components/HorizontalBox.h"
#include "MyGameInstance.h"

void UItemQuickBarWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

void UItemQuickBarWidget::InitQuickBar(UInventoryManager* InInventoryManager, int32 NumSlots)
{
	InventoryManager = InInventoryManager;
	if (!SlotsBox || !SlotWidgetClass) return;

	const int32 Count = NumSlots > 0 ? NumSlots : DefaultSlotCount;
	Slots.Empty();

	const TArray<FKey> DefaultKeys = {
		EKeys::F1, EKeys::F2, EKeys::F3, EKeys::F4, EKeys::F5
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

void UItemQuickBarWidget::OnSlotUsed(int32 SlotIndex)
{
	RefreshFromInventory();
}

void UItemQuickBarWidget::SaveSlotsState()
{
	if (!InventoryManager) return;

	UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance());
	if (!GI) return;

	// Store current quickbar state on the GameInstance for persistence across level loads
	GI->QuickBarSlots.Empty();
	for (const UItemQuickSlotWidget* Sl : Slots)
	{
		if (Sl && !Sl->IsEmpty())
		{
			GI->QuickBarSlots.Add(Sl->GetSlotData());
		}
	}
}

void UItemQuickBarWidget::LoadSlotsState()
{
	UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance());
	if (!GI) return;

	for (const FItemQuickSlotData& SavedSlot : GI->QuickBarSlots)
	{
		if (Slots.IsValidIndex(SavedSlot.slotIndex))
		{
			Slots[SavedSlot.slotIndex]->AssignItem(
				SavedSlot.itemId, SavedSlot.itemSlug,
				SavedSlot.quantity, SavedSlot.iconPath);
		}
	}
}
