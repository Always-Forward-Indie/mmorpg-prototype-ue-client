#include "Gameplay/Items/HarvestManager.h"
#include "Prototyping.h"
#include "Networking/NetworkManager.h"
#include "MyGameInstance.h"
#include "Utils/JSONParser.h"
#include "Engine/World.h"
#include "Gameplay/Mobs/BasicMOB.h"
#include "Gameplay/Players/BasicPlayer.h"
#include "Gameplay/Players/PlayerAnimInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"
#include "UI/HarvestProgressWidget.h"
#include "UI/HarvestLootWidget.h"
#include "TimerManager.h"
#include "CrashDiagnostics.h"

void UHarvestManager::SetHarvestProgressWidget(UHarvestProgressWidget* InProgressWidget)
{
	HarvestProgressWidget = InProgressWidget;
	if (HarvestProgressWidget)
	{
		// Set the harvest manager reference in the widget
		HarvestProgressWidget->SetHarvestManager(this);
		UE_LOG(LogGameplay, Warning, TEXT("HarvestManager: Harvest progress widget set and connected"));
	}
}

void UHarvestManager::SetHarvestLootWidget(UHarvestLootWidget* InLootWidget)
{
	HarvestLootWidget = InLootWidget;
	if (HarvestLootWidget)
	{
		// Set the harvest manager reference in the widget
		HarvestLootWidget->SetHarvestManager(this);
		UE_LOG(LogGameplay, Warning, TEXT("HarvestManager: Harvest loot widget set and connected"));
	}
}

void UHarvestManager::ShowHarvestProgress()
{
	if (HarvestProgressWidget)
	{
		HarvestProgressWidget->ShowWidget();
		UE_LOG(LogGameplay, Warning, TEXT("HarvestManager: Showing harvest progress"));
	}
	else
	{
		UE_LOG(LogGameplay, Error, TEXT("HarvestManager: Cannot show harvest progress - widget is null"));
	}
}

void UHarvestManager::HideHarvestProgress()
{
	if (HarvestProgressWidget)
	{
		HarvestProgressWidget->HideWidget();
		UE_LOG(LogGameplay, Warning, TEXT("HarvestManager: Hiding harvest progress"));
	}
	else
	{
		UE_LOG(LogGameplay, Error, TEXT("HarvestManager: Cannot hide harvest progress - widget is null"));
	}
}

void UHarvestManager::ShowLootWindow()
{
	if (HarvestLootWidget)
	{
		HarvestLootWidget->SetLootItems(CurrentAvailableLoot);
		HarvestLootWidget->ShowWidget();
		UE_LOG(LogGameplay, Warning, TEXT("HarvestManager: Showing loot window"));
	}
	else
	{
		UE_LOG(LogGameplay, Error, TEXT("HarvestManager: Cannot show loot window - widget is null"));
	}
}

void UHarvestManager::HideLootWindow()
{
	if (HarvestLootWidget)
	{
		HarvestLootWidget->HideWidget();
		UE_LOG(LogGameplay, Warning, TEXT("HarvestManager: Hiding loot window"));
	}
	else
	{
		UE_LOG(LogGameplay, Error, TEXT("HarvestManager: Cannot hide loot window - widget is null"));
	}
}

