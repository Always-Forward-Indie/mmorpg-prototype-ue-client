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

#define LOCTEXT_NAMESPACE "InventoryWidget"

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
	if (WeightText || WeightBar || OverweightIcon)
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

		// Numbers only ("45.2/100"): captions/units are static BP texts,
		// so no hardcoded English and no locale-hostile concatenation.
		const FString MaxStr = FMath::IsNearlyEqual(MaxWeight, FMath::RoundToFloat(MaxWeight))
			? FString::Printf(TEXT("%.0f"), MaxWeight)
			: FString::Printf(TEXT("%.1f"), MaxWeight);
		const FString WeightString = FString::Printf(TEXT("%.1f/%s"), CurrentWeight, *MaxStr);

		// Encumbrance bucket lives in FInventoryWeight
		// (pinned by MMO.Inventory.Weight); colors stay here with the widget.
		const EWeightStatus WeightStatus = FInventoryWeight::StatusFor(CurrentWeight, MaxWeight);
		FLinearColor BucketColor = FLinearColor::White;
		switch (WeightStatus)
		{
		case EWeightStatus::Overweight:
			BucketColor = FLinearColor::Red;
			break;
		case EWeightStatus::Heavy:
			BucketColor = FLinearColor::Yellow;
			break;
		default:
			break;
		}

		if (WeightText)
		{
			WeightText->SetText(FText::FromString(WeightString));
			WeightText->SetColorAndOpacity(BucketColor);
		}

		// Progress bar mirrors the text bucket (same single source).
		if (WeightBar)
		{
			WeightBar->SetPercent(FInventoryWeight::Ratio(CurrentWeight, MaxWeight));
			WeightBar->SetFillColorAndOpacity(BucketColor);
		}

		// Badge is the second channel next to color (accessibility):
		// visible only at >=100%, decorative (never steals slot input).
		if (OverweightIcon)
		{
			const bool bOver = (WeightStatus == EWeightStatus::Overweight);
			if (bOver)
			{
				// Factual only: speed is server-authoritative, the client
				// applies no penalty itself (see BasicPlayer weight status).
				OverweightIcon->SetToolTipText(LOCTEXT("OverweightTip", "Overweight: carried weight exceeds the limit"));
				OverweightIcon->SetVisibility(ESlateVisibility::HitTestInvisible);
			}
			else
			{
				OverweightIcon->SetVisibility(ESlateVisibility::Collapsed);
			}
		}
	}

	// Update slots display
	if (SlotsText)
	{
		int32 UsedSlots = GetUsedSlots();
		int32 TotalSlots = GetTotalSlots();
		// Numbers only ("12/48"): the caption is a static BP text.
		FString SlotsString = FString::Printf(TEXT("%d/%d"), UsedSlots, TotalSlots);
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

#undef LOCTEXT_NAMESPACE
