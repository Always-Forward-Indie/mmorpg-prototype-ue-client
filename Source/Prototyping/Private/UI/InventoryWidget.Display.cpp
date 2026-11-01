#include "UI/InventoryWidget.h"
#include "Prototyping.h"
#include "UI/InventorySlotWidget.h"
#include "UI/ItemTooltipWidget.h"
#include "UI/ItemActionMenuWidget.h"
#include "UI/DropQuantityPopupWidget.h"
#include "Gameplay/Items/InventoryManager.h"
#include "Gameplay/Equipment/EquipmentManager.h"
#include "MyGameInstance.h"
#include "Gameplay/Player/PlayerStatsManager.h"
#include "Components/GridPanel.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/WrapBoxSlot.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/CanvasPanelSlot.h"

void UInventoryWidget::UpdateInventoryDisplay(const FCharacterInventoryStruct& Inventory)
{	// Preserve an open tooltip across the rebuild: rows may reorder between
	// snapshots, so re-resolve the shown item by instance id in the new
	// array; hide it when the row is gone. Without this the tooltip keeps
	// showing the pre-refresh item while the grid underneath has moved on.
	int32 OpenTooltipInstanceId = 0;
	if (ItemTooltipWidget && ItemTooltipWidget->IsVisible())
	{
		OpenTooltipInstanceId = ItemTooltipWidget->GetCurrentItem().id;
	}

	CurrentInventory = Inventory;

	// Clear all slots first so dropped items don't leave stale visuals
	for (int32 i = 0; i < InventorySlots.Num(); i++)
	{
		ClearSlot(i);
	}

	// Fill slots from the view list (search filter + sort applied);
	// items with itemId == 0 are placeholders - skip them
	const TArray<FInventoryItemStruct> DisplayItems = BuildDisplayList();
	for (int32 i = 0; i < DisplayItems.Num() && i < InventorySlots.Num(); i++)
	{
		if (DisplayItems[i].itemId > 0)
		{
			UpdateSlot(i, DisplayItems[i]);
		}
		// else: slot already cleared above
	}

	// Update inventory statistics
	UpdateInventoryStats();

	if (OpenTooltipInstanceId > 0)
	{
		bool bFound = false;
		for (int32 i = 0; i < InventorySlots.Num(); ++i)
		{
			UInventorySlotWidget* SlotWidget = GetSlotWidget(i);
			if (SlotWidget && SlotWidget->HasItem() && SlotWidget->GetItemData().id == OpenTooltipInstanceId)
			{
				FVector2D MousePosition = FVector2D::ZeroVector;
				if (GetWorld() && GetWorld()->GetFirstPlayerController())
				{
					GetWorld()->GetFirstPlayerController()->GetMousePosition(MousePosition.X, MousePosition.Y);
				}
				HoveredSlotIndex = i;
				ShowTooltip(SlotWidget->GetItemData(), MousePosition);
				bFound = true;
				break;
			}
		}
		if (!bFound)
		{
			HoveredSlotIndex = -1;
			HideTooltip();
		}
	}

	UE_LOG(LogGameplay, Verbose, TEXT("InventoryWidget: Updated display with %d items"), Inventory.items.Num());
}

void UInventoryWidget::SetInventorySize(int32 Rows, int32 Columns)
{
	InventoryRows = FMath::Max(1, Rows);
	InventoryColumns = FMath::Max(1, Columns);

	// Recreate grid with new size
	CreateInventoryGrid();

	UE_LOG(LogGameplay, Log, TEXT("InventoryWidget: Set inventory size to %dx%d"), InventoryRows, InventoryColumns);
}

void UInventoryWidget::SetInventoryVisible(bool bVisible)
{
	bIsInventoryVisible = bVisible;
	SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Hidden);

	if (!bVisible)
	{
		bDragging = false;
		HideTooltip();
		CloseContextMenu();
	}

	// �� ��������� �������� ����� - ��� ������ UIManager
	// ���������� UIManager �� ��������� ���������
	OnInventoryVisibilityChanged.Broadcast(bVisible);

	// Hide tooltip when inventory is hidden
	if (!bVisible)
	{
		HideTooltip();
	}

	UE_LOG(LogGameplay, Verbose, TEXT("InventoryWidget: Set visibility to %s"), bVisible ? TEXT("Visible") : TEXT("Hidden"));
}

void UInventoryWidget::ToggleInventory()
{
	SetInventoryVisible(!bIsInventoryVisible);
}

bool UInventoryWidget::IsInventoryVisible() const
{
	return bIsInventoryVisible;
}

void UInventoryWidget::CreateInventoryGrid()
{
	if (!InventoryWrap || !InventorySlotWidgetClass)
	{
		UE_LOG(LogGameplay, Error, TEXT("InventoryWidget: Cannot create wrap - missing components"));
		return;
	}

	ClearInventoryGrid();

	// ��������� WrapBox
	InventoryWrap->SetExplicitWrapSize(false); // = Use Allotted Width
	InventoryWrap->SetInnerSlotPadding(FVector2D(SlotGap, SlotGap)); // ������� ����� ��������

	const int32 TotalSlots = GetTotalSlots();
	InventorySlots.Reserve(TotalSlots);

	for (int32 Index = 0; Index < TotalSlots; ++Index)
	{
		UInventorySlotWidget* SlotWidget = CreateWidget<UInventorySlotWidget>(this, InventorySlotWidgetClass);
		if (!SlotWidget) continue;

		SlotWidget->InitializeSlot(Index);

		SlotWidget->OnSlotClicked.AddDynamic(this, &UInventoryWidget::OnSlotClicked);
		SlotWidget->OnSlotRightClicked.AddDynamic(this, &UInventoryWidget::OnSlotRightClicked);
		SlotWidget->OnSlotHovered.AddDynamic(this, &UInventoryWidget::OnSlotHovered);

		if (UWrapBoxSlot* W = InventoryWrap->AddChildToWrapBox(SlotWidget))
		{
			W->SetPadding(FMargin(SlotGap));
			W->SetHorizontalAlignment(HAlign_Left);
			W->SetVerticalAlignment(VAlign_Top);
			W->SetFillEmptySpace(false); // ����-��������� �����, �� �����������
		}

		InventorySlots.Add(SlotWidget);
	}

	UE_LOG(LogGameplay, Verbose, TEXT("InventoryWidget: Created %d inventory slots (WrapBox)"), InventorySlots.Num());

	// Update stats after creating grid
	UpdateInventoryStats();
}

void UInventoryWidget::ClearInventoryGrid()
{
	if (InventoryWrap) InventoryWrap->ClearChildren();
	InventorySlots.Empty();
}

UInventorySlotWidget* UInventoryWidget::GetSlotWidget(int32 SlotIndex) const
{
	if (SlotIndex >= 0 && SlotIndex < InventorySlots.Num())
	{
		return InventorySlots[SlotIndex];
	}
	return nullptr;
}

void UInventoryWidget::UpdateSlot(int32 SlotIndex, const FInventoryItemStruct& Item)
{
	UInventorySlotWidget* SlotWidget = GetSlotWidget(SlotIndex);
	if (SlotWidget)
	{
		SlotWidget->SetItemData(Item);
		const FString Slug = !Item.itemSlug.IsEmpty() ? Item.itemSlug : Item.slug;
		SlotWidget->SetJunked(InventoryManager && !Slug.IsEmpty() && InventoryManager->IsJunkSlug(Slug));
	}
}

void UInventoryWidget::ClearSlot(int32 SlotIndex)
{
	UInventorySlotWidget* SlotWidget = GetSlotWidget(SlotIndex);
	if (SlotWidget)
	{
		SlotWidget->ClearSlot();
	}
}

void UInventoryWidget::CycleSortMode()
{
	SortMode = static_cast<EInventorySortMode>((static_cast<uint8>(SortMode) + 1) % 4);
	UpdateSortLabel();
	UpdateInventoryDisplay(CurrentInventory);
}

void UInventoryWidget::SetSearchFilter(const FString& Filter)
{
	SearchFilter = Filter;
	UpdateInventoryDisplay(CurrentInventory);
}

TArray<FInventoryItemStruct> UInventoryWidget::BuildDisplayList() const
{
	TArray<FInventoryItemStruct> Out;
	Out.Reserve(CurrentInventory.items.Num());

	const FString Needle = SearchFilter.TrimStartAndEnd().ToLower();
	for (const FInventoryItemStruct& Item : CurrentInventory.items)
	{
		if (Item.itemId <= 0) continue;
		if (!Needle.IsEmpty())
		{
			const FString Haystack = (Item.name + TEXT(" ") + Item.slug + TEXT(" ") + Item.itemTypeSlug).ToLower();
			if (!Haystack.Contains(Needle)) continue;
		}
		Out.Add(Item);
	}

	switch (SortMode)
	{
	case EInventorySortMode::ByName:
		Out.Sort([](const FInventoryItemStruct& A, const FInventoryItemStruct& B) {
			const FString NA = !A.name.IsEmpty() ? A.name : A.slug;
			const FString NB = !B.name.IsEmpty() ? B.name : B.slug;
			return NA < NB;
		});
		break;
	case EInventorySortMode::ByRarity:
		Out.Sort([](const FInventoryItemStruct& A, const FInventoryItemStruct& B) {
			if (A.rarityId != B.rarityId) return A.rarityId > B.rarityId;
			const FString NA = !A.name.IsEmpty() ? A.name : A.slug;
			const FString NB = !B.name.IsEmpty() ? B.name : B.slug;
			return NA < NB;
		});
		break;
	case EInventorySortMode::ByType:
		Out.Sort([](const FInventoryItemStruct& A, const FInventoryItemStruct& B) {
			if (A.itemTypeSlug != B.itemTypeSlug) return A.itemTypeSlug < B.itemTypeSlug;
			return A.itemId < B.itemId;
		});
		break;
	case EInventorySortMode::Off:
	default:
		break;
	}
	return Out;
}

void UInventoryWidget::UpdateSortLabel()
{
	if (!SortButton) return;
	FString ModeName;
	switch (SortMode)
	{
	case EInventorySortMode::ByName:   ModeName = TEXT("Name"); break;
	case EInventorySortMode::ByRarity: ModeName = TEXT("Rarity"); break;
	case EInventorySortMode::ByType:   ModeName = TEXT("Type"); break;
	case EInventorySortMode::Off:
	default:                          ModeName = TEXT("Unsorted"); break;
	}
	// Tooltip needs no knowledge of the BP's inner layout (a Sort_Label
	// child would be nicer but cannot be required headless).
	SortButton->SetToolTipText(FText::FromString(FString::Printf(TEXT("Sort: %s"), *ModeName)));
}

