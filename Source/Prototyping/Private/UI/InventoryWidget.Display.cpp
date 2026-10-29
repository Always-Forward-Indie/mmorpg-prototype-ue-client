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
{
	// Preserve an open tooltip across the rebuild: rows may reorder between
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

	// Fill slots with items; items with itemId == 0 are placeholders - skip them
	for (int32 i = 0; i < Inventory.items.Num() && i < InventorySlots.Num(); i++)
	{
		if (Inventory.items[i].itemId > 0)
		{
			UpdateSlot(i, Inventory.items[i]);
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

