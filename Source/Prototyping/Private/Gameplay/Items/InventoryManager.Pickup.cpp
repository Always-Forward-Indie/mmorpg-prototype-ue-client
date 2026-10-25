#include "Gameplay/Items/InventoryManager.h"
#include "Networking/NetworkManager.h"
#include "Prototyping.h"
#include "MyGameInstance.h"
#include "Gameplay/Items/HarvestManager.h"
#include "Utils/JSONParser.h"
#include "Engine/World.h"
#include "UI/InventoryWidget.h"
#include "Gameplay/Equipment/EquipmentManager.h"
#include "Gameplay/Players/BasicPlayer.h"
#include "Audio/AudioManager.h"
#include "Gameplay/Items/ItemManager.h"
#include "Utils/NetCore/ProximityRule.h"
#include "Utils/AudioSpawnHelpers.h"
#include "CrashDiagnostics.h"

void UInventoryManager::PickupNearbyItem()
{
	if (!worldContext)
	{
		UE_LOG(LogGameplay, Error, TEXT("InventoryManager: World context not found"));
		return;
	}

	// Prevent pickup while harvesting
	if (gameInstance && gameInstance->GetHarvestManager() && gameInstance->GetHarvestManager()->IsHarvesting())
	{
		return;
	}

	// Get the player character
	APlayerController* PC = worldContext->GetFirstPlayerController();
	if (!PC || !PC->GetPawn())
	{
		UE_LOG(LogGameplay, Error, TEXT("InventoryManager: Player pawn not found"));
		return;
	}

	FVector PlayerLocation = PC->GetPawn()->GetActorLocation();

	// Iterate the dropped-item registry (O(registered items), no world scan).
	ADroppedItemActor* ClosestItem = nullptr;
	float ClosestDistance = FLT_MAX;

	if (UItemManager* Items = gameInstance ? gameInstance->GetItemManager() : nullptr)
	{
		Items->ForEachDroppedItem([&](ADroppedItemActor* DroppedItem)
		{
			if (DroppedItem && DroppedItem->CanBePickedUp())
			{
				const float Distance = FVector::Dist(PlayerLocation, DroppedItem->GetActorLocation());
				const float ItemPickupRadius = DroppedItem->GetPickupRadius();

				if (FProximityRule::IsInRange(Distance, ItemPickupRadius) && Distance < ClosestDistance)
				{
					ClosestDistance = Distance;
					ClosestItem = DroppedItem;
				}
			}
		});
	}

	// Attempt to pickup the closest item
	if (ClosestItem)
	{
		UE_LOG(LogGameplay, Warning, TEXT("InventoryManager: Attempting to pickup item %s at distance %.2f"),
			*ClosestItem->GetItemName(), ClosestDistance);

		// Rotate the player to face the item, then lock movement.
		if (ABasicPlayer* Player = Cast<ABasicPlayer>(PC->GetPawn()))
		{
			FVector ToItem = ClosestItem->GetActorLocation() - PlayerLocation;
			ToItem.Z = 0.f;
			if (!ToItem.IsNearlyZero())
			{
				Player->SetDesiredFaceYaw(ToItem.Rotation().Yaw);
			}
			Player->LockMovementForPickup();
		}

		bool bPickupSuccess = ClosestItem->AttemptPickup();

		if (bPickupSuccess)
		{
			// Broadcast pickup attempt event
			OnItemPickupAttempted.Broadcast(ClosestItem->GetItemBaseData());
		}
		else
		{
			// Request failed locally (e.g. canBePickedUp == false) — unlock immediately
			if (ABasicPlayer* Player = Cast<ABasicPlayer>(PC->GetPawn()))
			{
				Player->UnlockMovementAfterPickup();
			}
		}
	}
	else
	{
		UE_LOG(LogGameplay, Warning, TEXT("InventoryManager: No items within pickup range"));
		// Optionally broadcast a "no items found" event
		OnNoItemsInRange.Broadcast();
	}
}

void UInventoryManager::PickupSpecificItem(ADroppedItemActor* TargetItem)
{
    if (!IsValid(TargetItem))
    {
        UE_LOG(LogGameplay, Warning, TEXT("InventoryManager: PickupSpecificItem - null target"));
        return;
    }

    if (!TargetItem->CanBePickedUp())
    {
        UE_LOG(LogGameplay, Warning, TEXT("InventoryManager: PickupSpecificItem - item cannot be picked up"));
        return;
    }

    if (!worldContext) return;

    APlayerController* PC = worldContext->GetFirstPlayerController();
    if (!PC || !PC->GetPawn()) return;

    ABasicPlayer* Player = Cast<ABasicPlayer>(PC->GetPawn());
    if (!Player) return;

    // Guard against starting a new pickup while already picking up —
    // prevents PendingPickupItemUIDs from filling with items that are
    // never cleaned up because the second animation fires first.
    if (Player->IsPickingUp()) return;

    // Prevent starting a pickup while harvesting a corpse
    if (gameInstance && gameInstance->GetHarvestManager() && gameInstance->GetHarvestManager()->IsHarvesting())
    {
        UE_LOG(LogGameplay, Warning, TEXT("InventoryManager: PickupSpecificItem blocked - player is harvesting"));
        return;
    }

    FVector PlayerLocation = Player->GetActorLocation();

    // Face the item and lock movement
    {
        FVector ToItem = TargetItem->GetActorLocation() - PlayerLocation;
        ToItem.Z = 0.f;
        if (!ToItem.IsNearlyZero())
        {
            Player->SetDesiredFaceYaw(ToItem.Rotation().Yaw);
        }
        Player->LockMovementForPickup();
    }

    bool bSuccess = TargetItem->AttemptPickup();
    if (bSuccess)
    {
        OnItemPickupAttempted.Broadcast(TargetItem->GetItemBaseData());
    }
    else
    {
        Player->UnlockMovementAfterPickup();
    }

    UE_LOG(LogGameplay, Warning, TEXT("InventoryManager: PickupSpecificItem %s - %s"),
        *TargetItem->GetItemName(), bSuccess ? TEXT("OK") : TEXT("FAILED"));
}

ADroppedItemActor* UInventoryManager::GetNearestDroppedItem(float MaxDistance) const
{
	if (!worldContext)
	{
		return nullptr;
	}

	// Get the player character
	APlayerController* PC = worldContext->GetFirstPlayerController();
	if (!PC || !PC->GetPawn())
	{
		return nullptr;
	}

	FVector PlayerLocation = PC->GetPawn()->GetActorLocation();

	// Iterate the dropped-item registry (O(registered items), no world scan).
	ADroppedItemActor* ClosestItem = nullptr;
	float ClosestDistance = MaxDistance + 1.f;

	if (UItemManager* Items = gameInstance ? gameInstance->GetItemManager() : nullptr)
	{
		Items->ForEachDroppedItem([&](ADroppedItemActor* DroppedItem)
		{
			if (DroppedItem && DroppedItem->CanBePickedUp())
			{
				const float Distance = FVector::Dist(PlayerLocation, DroppedItem->GetActorLocation());

				if (FProximityRule::IsInRange(Distance, DroppedItem->GetPickupRadius()) && Distance <= ClosestDistance)
				{
					ClosestDistance = Distance;
					ClosestItem = DroppedItem;
				}
			}
		});
	}

	return ClosestItem;
}

TArray<ADroppedItemActor*> UInventoryManager::GetAllDroppedItemsInRange(float MaxDistance) const
{
	TArray<ADroppedItemActor*> ItemsInRange;

	if (!worldContext)
	{
		return ItemsInRange;
	}

	// Get the player character
	APlayerController* PC = worldContext->GetFirstPlayerController();
	if (!PC || !PC->GetPawn())
	{
		return ItemsInRange;
	}

	FVector PlayerLocation = PC->GetPawn()->GetActorLocation();

	// Iterate the dropped-item registry (O(registered items), no world scan).
	if (UItemManager* Items = gameInstance ? gameInstance->GetItemManager() : nullptr)
	{
		Items->ForEachDroppedItem([&](ADroppedItemActor* DroppedItem)
		{
			if (DroppedItem && DroppedItem->CanBePickedUp())
			{
				const float Distance = FVector::Dist(PlayerLocation, DroppedItem->GetActorLocation());
				const float ItemPickupRadius = FMath::Min(MaxDistance, DroppedItem->GetPickupRadius());

				if (FProximityRule::IsInRange(Distance, ItemPickupRadius))
				{
					ItemsInRange.Add(DroppedItem);
				}
			}
		});
	}

	// Sort by distance (closest first)
	ItemsInRange.Sort([PlayerLocation](const ADroppedItemActor& A, const ADroppedItemActor& B) {
		float DistA = FVector::Dist(PlayerLocation, A.GetActorLocation());
		float DistB = FVector::Dist(PlayerLocation, B.GetActorLocation());
		return DistA < DistB;
		});

	return ItemsInRange;
}

