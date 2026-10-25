#include "UI/InventoryWidget.h"
#include "Prototyping.h"
#include "UI/InventorySlotWidget.h"
#include "Utils/NetCore/InventoryWeight.h"
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

float UInventoryWidget::GetCurrentInventoryWeight() const
{
	// Summation lives in FInventoryWeight (pinned by MMO.Inventory.Weight).
	return FInventoryWeight::Total(CurrentInventory.items);
}

int32 UInventoryWidget::GetUsedSlots() const
{
	return CurrentInventory.items.Num();
}

int32 UInventoryWidget::GetFreeSlots() const
{
	return GetTotalSlots() - GetUsedSlots();
}

void UInventoryWidget::UpdateInventoryStats()
{
	// Update weight display
	if (WeightText)
	{
		float CurrentWeight = GetCurrentInventoryWeight();

		// Use server-authoritative max weight from PlayerStatsManager.
		// Falls back to the local default if the stats manager or its data is unavailable.
		float MaxWeight = MaxInventoryWeight;
		if (UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance()))
		{
			if (UPlayerStatsManager* StatsMgr = GI->GetPlayerStatsManager())
			{
				const float ServerMax = StatsMgr->GetCachedStats().weightMax;
				if (ServerMax > 0.0f)
				{
					MaxWeight = ServerMax;
				}
			}
		}

		FString WeightString = FString::Printf(TEXT("Weight: %.1f/%.1f"), CurrentWeight, MaxWeight);
		WeightText->SetText(FText::FromString(WeightString));
		
		// Encumbrance bucket lives in FInventoryWeight
		// (pinned by MMO.Inventory.Weight); colors stay here with the widget.
		switch (FInventoryWeight::StatusFor(CurrentWeight, MaxWeight))
		{
		case EWeightStatus::Overweight:
			WeightText->SetColorAndOpacity(FLinearColor::Red);
			break;
		case EWeightStatus::Heavy:
			WeightText->SetColorAndOpacity(FLinearColor::Yellow);
			break;
		default:
			WeightText->SetColorAndOpacity(FLinearColor::White);
			break;
		}
	}

	// Update slots display
	if (SlotsText)
	{
		int32 UsedSlots = GetUsedSlots();
		int32 TotalSlots = GetTotalSlots();
		FString SlotsString = FString::Printf(TEXT("Slots: %d/%d"), UsedSlots, TotalSlots);
		SlotsText->SetText(FText::FromString(SlotsString));
		
		// Optional: Change color based on slots percentage
		float SlotsPercentage = TotalSlots > 0 ? (static_cast<float>(UsedSlots) / static_cast<float>(TotalSlots)) : 0.0f;
		if (SlotsPercentage >= 1.0f)
		{
			SlotsText->SetColorAndOpacity(FLinearColor::Red);
		}
		else if (SlotsPercentage >= 0.8f)
		{
			SlotsText->SetColorAndOpacity(FLinearColor::Yellow);
		}
		else
		{
			SlotsText->SetColorAndOpacity(FLinearColor::White);
		}
	}

	// Update gold display
	if (GoldText)
	{
		GoldText->SetText(FText::FromString(FString::Printf(TEXT("%d"), CurrentInventory.gold)));
	}

	UE_LOG(LogGameplay, Verbose, TEXT("InventoryWidget: Stats updated - Weight: %.1f/%.1f, Slots: %d/%d, Gold: %d"),
		GetCurrentInventoryWeight(), MaxInventoryWeight, GetUsedSlots(), GetTotalSlots(), CurrentInventory.gold);
}

void UInventoryWidget::SetMaxInventoryWeight(float NewMaxWeight)
{
	MaxInventoryWeight = FMath::Max(0.0f, NewMaxWeight);
	UpdateInventoryStats();
	
	UE_LOG(LogGameplay, Log, TEXT("InventoryWidget: Max weight set to %.1f"), MaxInventoryWeight);
}
