#include "Gameplay/Items/HarvestManager.h"
#include "Prototyping.h"
#include "Networking/NetworkManager.h"
#include "MyGameInstance.h"
#include "Gameplay/Items/InventoryManager.h"
#include "Utils/JSONParser.h"
#include "Engine/World.h"
#include "Gameplay/Mobs/BasicMOB.h"
#include "Gameplay/Mobs/MOBManager.h"
#include "Gameplay/Players/BasicPlayer.h"
#include "Gameplay/Players/PlayerAnimInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"
#include "UI/HarvestProgressWidget.h"
#include "UI/HarvestLootWidget.h"
#include "TimerManager.h"
#include "CrashDiagnostics.h"

void UHarvestManager::UpdateHarvestProgress(float DeltaTime)
{
	if (!bIsHarvesting)
	{
		UE_LOG(LogGameplay, Warning, TEXT("HarvestManager::UpdateHarvestProgress - Not harvesting, returning early"));
		return;
	}

	// Calculate current progress
	float Progress = GetHarvestProgress();
	
	// Always broadcast progress updates (let the widget decide if it needs to update UI)
	OnHarvestProgressUpdate.Broadcast(Progress);
	
	// Log progress changes more frequently for debugging
	static float LastLoggedProgress = -1.0f;
	if (FMath::Abs(Progress - LastLoggedProgress) > 0.05f) // Log every 5% change
	{
		UE_LOG(LogGameplay, Warning, TEXT("HarvestManager::UpdateHarvestProgress - Broadcasting progress %.1f%%"), 
			Progress * 100.0f);
		LastLoggedProgress = Progress;
	}

	// Auto-complete if progress reaches 100% (fallback in case server message is missed)
	if (Progress >= 1.0f)
	{
		UE_LOG(LogGameplay, Warning, TEXT("HarvestManager: Harvest progress completed locally"));
		// Note: We don't auto-complete here, we wait for server response
	}
}

void UHarvestManager::HandleHarvestComplete(const FHarvestCompleteStruct& HarvestData)
{
	// End harvest state
	bIsHarvesting = false;
	HideHarvestProgress();
	UnlockPlayerMovement();

	// Store available loot
	CurrentAvailableLoot = HarvestData.availableLoot;
	CurrentCorpseUID = HarvestData.corpseId;

	if (HarvestData.availableLoot.Num() == 0)
		KnownEmptyCorpses.Add(HarvestData.corpseId);
	else
		KnownEmptyCorpses.Remove(HarvestData.corpseId);

	// Mark the mob as harvested (O(1) registry lookup)
	if (worldContext && gameInstance && gameInstance->GetMOBManager())
	{
		if (ABasicMOB* Mob = Cast<ABasicMOB>(gameInstance->GetMOBManager()->FindMobActor(HarvestData.corpseId)))
		{
			Mob->SetHarvested(true);
			UE_LOG(LogGameplay, Warning, TEXT("HarvestManager: Marked mob %s as harvested"), *Mob->GetMobName());
		}
	}

	// Show loot window if enabled and there's loot
	if (bAutoShowLootWindow && CurrentAvailableLoot.Num() > 0)
	{
		ShowLootWindow();
	}

	// Show completion message
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Green, 
			FString::Printf(TEXT("Harvest completed! Found %d items."), HarvestData.totalItems));
	}

	OnHarvestCompleted.Broadcast(HarvestData);
}

void UHarvestManager::HandleLootPickupResponse(const FCorpseLootPickupResponseStruct& PickupData)
{
	// Update available loot with remaining items
	CurrentAvailableLoot = PickupData.remainingLoot;

	// Add picked up items to inventory
	if (gameInstance)
	{
		UInventoryManager* InventoryManager = gameInstance->GetInventoryManager();
		if (InventoryManager)
		{
			for (const FHarvestItemStruct& PickedUpItem : PickupData.pickedUpItems)
			{
				// Convert HarvestItemStruct to InventoryItemStruct
				FInventoryItemStruct InventoryItem;
				InventoryItem.itemId = PickedUpItem.itemId;
				InventoryItem.quantity = PickedUpItem.quantity;
				InventoryItem.name = PickedUpItem.name;
				InventoryItem.slug = PickedUpItem.itemSlug;
				InventoryItem.description = PickedUpItem.description;
				InventoryItem.type = PickedUpItem.itemType;
				InventoryItem.rarity = PickedUpItem.rarityName;
				InventoryItem.weight = PickedUpItem.weight;
				
				// Add to local inventory (this will trigger UI updates)
				InventoryManager->AddItemToLocalInventory(InventoryItem);
			}
		}
	}

	// Hide loot window if no more loot
	if (CurrentAvailableLoot.Num() == 0)
	{
		KnownEmptyCorpses.Add(CurrentCorpseUID);
		HideLootWindow();
		CurrentCorpseUID = 0;
	}

	// Show pickup success message
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Green, 
			FString::Printf(TEXT("Picked up %d items"), PickupData.itemsPickedUp));
	}

	OnLootPickupSuccess.Broadcast(PickupData);
}

void UHarvestManager::HandleLootInspectResponse(const FCorpseLootInspectResponseStruct& InspectData)
{
	// Store available loot and corpse ID
	CurrentAvailableLoot = InspectData.availableLoot;
	CurrentCorpseUID = InspectData.corpseUID;

	if (InspectData.availableLoot.Num() == 0)
		KnownEmptyCorpses.Add(InspectData.corpseUID);
	else
		KnownEmptyCorpses.Remove(InspectData.corpseUID);

	// Show loot window if there's loot available
	if (InspectData.availableLoot.Num() > 0)
	{
		ShowLootWindow();
		
		// Show success message
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Green, 
				FString::Printf(TEXT("Found %d items in corpse"), InspectData.totalItems));
		}
	}
	else
	{
		// Show message that corpse has no loot
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Yellow, 
				TEXT("No loot remaining in this corpse"));
		}
	}

	OnLootInspectSuccess.Broadcast(InspectData);
}

UWorld* UHarvestManager::GetValidWorld() const
{
	// Use stored world context since we're now a UObject
	return worldContext;
}

void UHarvestManager::StartTicking()
{
	if (GetValidWorld())
	{
		// Start a timer that calls Tick every frame (approximately 60 FPS)
		GetValidWorld()->GetTimerManager().SetTimer(TickTimerHandle, [this]()
		{
			Tick(0.016f); // Approximate delta time for 60 FPS
		}, 0.016f, true);
		
		UE_LOG(LogGameplay, Warning, TEXT("HarvestManager: Started ticking with timer"));
	}
	else
	{
		UE_LOG(LogGameplay, Error, TEXT("HarvestManager: Cannot start ticking - no world context"));
	}
}

void UHarvestManager::StopTicking()
{
	if (GetValidWorld())
	{
		GetValidWorld()->GetTimerManager().ClearTimer(TickTimerHandle);
		UE_LOG(LogGameplay, Warning, TEXT("HarvestManager: Stopped ticking"));
	}
}
