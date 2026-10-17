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

void UHarvestLootWidget::SetLootItems(const TArray<FHarvestItemStruct>& LootItems)
{
	CurrentLootItems = LootItems;
	CreateLootItemWidgets();
	UpdateItemCountDisplay();

	UE_LOG(LogTemp, Warning, TEXT("HarvestLootWidget: Set %d loot items"), LootItems.Num());
}

void UHarvestLootWidget::ClearLootItems()
{
	CurrentLootItems.Empty();
	
	UPanelWidget* Container = GetLootContainer();
	if (Container)
	{
		Container->ClearChildren();
	}
	
	LootItemWidgets.Empty();
	UpdateItemCountDisplay();
}

void UHarvestLootWidget::ShowWidget()
{
	if (!bIsVisible && CurrentLootItems.Num() > 0)
	{
		bIsVisible = true;
		SetVisibility(ESlateVisibility::Visible);
		
		// �� ��������� �������� ����� - ��� ������ UIManager
		// ���������� UIManager �� ��������� ���������
		OnHarvestLootVisibilityChanged.Broadcast(true);
		
		UE_LOG(LogTemp, Warning, TEXT("HarvestLootWidget: Widget shown - Items: %d"), CurrentLootItems.Num());
	}
}

void UHarvestLootWidget::HideWidget()
{
	bIsVisible = false;
	bDragging = false;
	SetVisibility(ESlateVisibility::Hidden);
	
	// Hide tooltip when widget is hidden
	HideTooltip();
	
	// �� ��������� �������� ����� - ��� ������ UIManager
	// ���������� UIManager �� ��������� ���������
	OnHarvestLootVisibilityChanged.Broadcast(false);
	
	UE_LOG(LogTemp, Warning, TEXT("HarvestLootWidget: Widget hidden"));
}

void UHarvestLootWidget::UpdateRemainingLoot(const TArray<FHarvestItemStruct>& RemainingLoot)
{
	SetLootItems(RemainingLoot);
	
	if (RemainingLoot.Num() == 0)
	{
		HideWidget();
	}
}

void UHarvestLootWidget::OnPickupAllClicked()
{
	UE_LOG(LogTemp, Warning, TEXT("HarvestLootWidget: Pickup all clicked"));

	if (HarvestManager)
	{
		HarvestManager->PickupAllLoot();
	}
}

void UHarvestLootWidget::OnCloseClicked()
{
	UE_LOG(LogTemp, Warning, TEXT("HarvestLootWidget: Close clicked"));
	HideWidget();
}

void UHarvestLootWidget::OnItemPickupRequested(int32 ItemId, int32 Quantity)
{
	// Hide tooltip when item pickup is requested
	HideTooltip();
	
	UE_LOG(LogTemp, Warning, TEXT("HarvestLootWidget: Item pickup requested - ID: %d, Quantity: %d"), ItemId, Quantity);

	if (HarvestManager)
	{
		HarvestManager->PickupLootItem(ItemId, Quantity);
	}
}

void UHarvestLootWidget::OnItemHovered(int32 ItemId, bool bIsHovered)
{
	if (bIsHovered)
	{
		HoveredItemId = ItemId;
		
		// Find the item and show tooltip
		for (const FHarvestItemStruct& Item : CurrentLootItems)
		{
			if (Item.itemId == ItemId)
			{
				FVector2D MousePosition = FVector2D::ZeroVector;
				if (GetWorld() && GetWorld()->GetFirstPlayerController())
				{
					GetWorld()->GetFirstPlayerController()->GetMousePosition(MousePosition.X, MousePosition.Y);
				}
				ShowTooltip(Item, MousePosition);
				break;
			}
		}
	}
	else
	{
		if (HoveredItemId == ItemId)
		{
			HoveredItemId = -1;
			HideTooltip();
		}
	}
}

void UHarvestLootWidget::ShowTooltip(const FHarvestItemStruct& Item, FVector2D Position)
{
	if (ItemTooltipWidget)
	{
		// Convert harvest item to inventory item for tooltip
		FInventoryItemStruct InventoryItem = ConvertHarvestItemToInventoryItem(Item);
		
		ItemTooltipWidget->SetItemData(InventoryItem);
		ItemTooltipWidget->UpdateTooltipPosition(Position);
		ItemTooltipWidget->ShowTooltip();
	}
}

void UHarvestLootWidget::HideTooltip()
{
	if (ItemTooltipWidget)
	{
		ItemTooltipWidget->HideTooltip();
	}
}

UPanelWidget* UHarvestLootWidget::GetLootContainer() const
{
	// Use WrapBox for grid layout if enabled and available, otherwise use ScrollBox
	if (bUseGridLayout && WrapBox_LootItems)
	{
		return WrapBox_LootItems;
	}
	else if (ScrollBox_LootItems)
	{
		return ScrollBox_LootItems;
	}
	
	return nullptr;
}

