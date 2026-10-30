#include "Gameplay/Items/HarvestManager.h"
#include "Prototyping.h"
#include "Networking/NetworkManager.h"
#include "MyGameInstance.h"
#include "Gameplay/Items/InventoryManager.h"
#include "Utils/JSONParser.h"
#include "Utils/NetCore/HarvestRules.h"
#include "Services/LocalizationSubsystem.h"
#include "Engine/World.h"
#include "Gameplay/Mobs/BasicMOB.h"
#include "Gameplay/Mobs/MOBManager.h"
#include "Gameplay/Players/BasicPlayer.h"
#include "Gameplay/Players/PlayerAnimInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"
#include "UI/HarvestProgressWidget.h"
#include "UI/HarvestLootWidget.h"
#include "UI/UIManager.h"
#include "UI/WorldNotificationManager.h"
#include "UI/NotificationStackWidget.h"
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
		UE_LOG(LogGameplay, Verbose, TEXT("HarvestManager: Harvest progress completed locally"));
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
				// Convert HarvestItemStruct to InventoryItemStruct.
				// The server omits display strings (S1a: only itemId/itemSlug/
				// rarityId travel), so resolve name/description locally by slug
				// (same source the loot window and toast already use). Packet
				// values win when present.
				FInventoryItemStruct InventoryItem;
				InventoryItem.itemId = PickedUpItem.itemId;
				InventoryItem.quantity = PickedUpItem.quantity;
				InventoryItem.slug = PickedUpItem.itemSlug;
				InventoryItem.itemSlug = PickedUpItem.itemSlug;
				InventoryItem.description = PickedUpItem.description;
				InventoryItem.type = PickedUpItem.itemType;
				InventoryItem.itemTypeSlug = PickedUpItem.itemTypeSlug;
				InventoryItem.rarityId = PickedUpItem.rarityId;
				InventoryItem.rarity = !PickedUpItem.rarityName.IsEmpty()
					? PickedUpItem.rarityName
					: FHarvestRules::RaritySlugForId(PickedUpItem.rarityId);
				InventoryItem.raritySlug = FHarvestRules::RaritySlugForId(PickedUpItem.rarityId);
				InventoryItem.weight = PickedUpItem.weight;
				InventoryItem.isHarvestItem = true;

				if (!InventoryItem.slug.IsEmpty() && gameInstance)
				{
					if (ULocalizationSubsystem* Loc = gameInstance->GetSubsystem<ULocalizationSubsystem>())
					{
						if (InventoryItem.name.IsEmpty())
							InventoryItem.name = PickedUpItem.name;
						if (InventoryItem.name.IsEmpty() || InventoryItem.name == InventoryItem.slug)
						{
							const FText LocName = Loc->GetItemDisplayName(InventoryItem.slug);
							if (!LocName.IsEmpty() && LocName.ToString() != InventoryItem.slug)
								InventoryItem.name = LocName.ToString();
						}
						if (InventoryItem.name.IsEmpty())
						{
							InventoryItem.name = JSONParser::HumanizeSlugForDisplay(InventoryItem.slug);
						}
						if (InventoryItem.description.IsEmpty())
						{
							const FText LocDesc = Loc->GetItemDescription(InventoryItem.slug);
							if (!LocDesc.IsEmpty())
								InventoryItem.description = LocDesc.ToString();
						}
						if (InventoryItem.type.IsEmpty() && !InventoryItem.itemTypeSlug.IsEmpty())
						{
							InventoryItem.type = Loc->GetItemTypeDisplayName(InventoryItem.itemTypeSlug).ToString();
							InventoryItem.itemTypeName = InventoryItem.type;
						}
					}
				}

				// The server emits INVENTORY_UPDATE (absolute row state) and this
				// response in the same millisecond with no guaranteed order.
				// The delta owns existing rows; the optimistic add only fills
				// the unknown (brand-new template) so a delta-first arrival
				// can never double-add. Either order converges on the
				// authoritative quantity; the toast (server quantity) is the
				// instant feedback.
				if (!InventoryManager->HasItem(PickedUpItem.itemId))
				{
					// Add to local inventory (this will trigger UI updates)
					InventoryManager->AddItemToLocalInventory(InventoryItem);
				}
				else
				{
					UE_LOG(LogGameplay, Verbose, TEXT("HarvestManager: Skipping optimistic add for item %d - authoritative delta owns the row"),
						PickedUpItem.itemId);
				}
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

	// Pickup receipts go to the toast stack (one per taken item, real server
	// quantity) — replacing the old on-screen debug print.
	if (gameInstance && gameInstance->Player)
	{
		UUIManager* UIMgr = gameInstance->Player->GetUIManager();
		UWorldNotificationManager* NotifMgr = UIMgr ? UIMgr->GetWorldNotificationManager() : nullptr;
		if (UNotificationStackWidget* Stack = NotifMgr ? NotifMgr->GetToastWidget() : nullptr)
		{
			for (const FHarvestItemStruct& PickedUpItem : PickupData.pickedUpItems)
			{
				if (PickedUpItem.itemSlug.IsEmpty()) continue;
				TMap<FString, FString> Fields;
				Fields.Add(TEXT("itemId"), FString::FromInt(PickedUpItem.itemId));
				Fields.Add(TEXT("itemSlug"), PickedUpItem.itemSlug);
				Fields.Add(TEXT("quantity"), FString::FromInt(FMath::Max(1, PickedUpItem.quantity)));
				Stack->EnqueueActionToast(TEXT("item_received"), Fields);
			}
		}
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
