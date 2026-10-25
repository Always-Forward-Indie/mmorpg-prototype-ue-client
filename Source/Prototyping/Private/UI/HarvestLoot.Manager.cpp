#include "UI/HarvestLootWidget.h"
#include "Prototyping.h"

// S5: widget chrome gathered via LOCTEXT.
#define LOCTEXT_NAMESPACE "HarvestLoot"
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

void UHarvestLootWidget::SetHarvestManager(UHarvestManager* InHarvestManager)
{
	UnbindFromHarvestManager();
	HarvestManager = InHarvestManager;
	BindToHarvestManager();
}

void UHarvestLootWidget::BindToHarvestManager()
{
	if (HarvestManager)
	{
		HarvestManager->OnHarvestCompleted.AddDynamic(this, &UHarvestLootWidget::HandleHarvestCompleted);
		HarvestManager->OnLootPickupSuccess.AddDynamic(this, &UHarvestLootWidget::HandleLootPickupSuccess);
		HarvestManager->OnLootPickupError.AddDynamic(this, &UHarvestLootWidget::HandleLootPickupError);
	}
}

void UHarvestLootWidget::UnbindFromHarvestManager()
{
	if (HarvestManager)
	{
		HarvestManager->OnHarvestCompleted.RemoveDynamic(this, &UHarvestLootWidget::HandleHarvestCompleted);
		HarvestManager->OnLootPickupSuccess.RemoveDynamic(this, &UHarvestLootWidget::HandleLootPickupSuccess);
		HarvestManager->OnLootPickupError.RemoveDynamic(this, &UHarvestLootWidget::HandleLootPickupError);
	}
}

void UHarvestLootWidget::HandleHarvestCompleted(const FHarvestCompleteStruct& HarvestData)
{
	UE_LOG(LogGameplay, Warning, TEXT("HarvestLootWidget: Harvest completed with %d items"), 
		HarvestData.availableLoot.Num());
		
	SetLootItems(HarvestData.availableLoot);
	
	if (HarvestData.availableLoot.Num() > 0)
	{
		ShowWidget();
		UE_LOG(LogGameplay, Verbose, TEXT("HarvestLootWidget: Showing loot window"));
	}
	else
	{
		UE_LOG(LogGameplay, Warning, TEXT("HarvestLootWidget: No loot available, staying hidden"));
	}
}

void UHarvestLootWidget::HandleLootPickupSuccess(const FCorpseLootPickupResponseStruct& PickupData)
{
	SetLootItems(PickupData.remainingLoot);
	if (PickupData.remainingLoot.Num() == 0)
	{
		HideWidget();
	}
}

void UHarvestLootWidget::HandleLootPickupError(const FCorpseLootPickupErrorStruct& ErrorData)
{
	UpdateItemCountDisplay();
}

void UHarvestLootWidget::InitializeWidget()
{
	UE_LOG(LogGameplay, Verbose, TEXT("HarvestLootWidget: InitializeWidget called"));

	if (TextBlock_Title)
	{
		TextBlock_Title->SetText(LOCTEXT("HarvestLoot", "Harvest Loot"));
	}

	UpdateItemCountDisplay();
	HideWidget();
	
	UE_LOG(LogGameplay, Warning, TEXT("HarvestLootWidget: InitializeWidget completed"));
}

#undef LOCTEXT_NAMESPACE

