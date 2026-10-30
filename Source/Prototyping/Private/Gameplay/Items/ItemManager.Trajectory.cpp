// Fill out your copyright notice in the Description page of Project Settings.


#include "Gameplay/Items/ItemManager.h"
#include "Gameplay/Items/InventoryManager.h"
#include "Prototyping.h"
#include "Gameplay/Items/DroppedItemActor.h"
#include "Gameplay/Players/BasicPlayer.h"
#include "Gameplay/Players/PlayerAnimInstance.h"
#include "Gameplay/Mobs/BasicMOB.h"
#include "Utils/JSONParser.h"
#include "MyGameInstance.h"
#include "Networking/NetworkManager.h"
#include "UI/UIManager.h"
#include "UI/WorldNotificationManager.h"
#include "UI/NotificationToastWidget.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/AssetManager.h"
#include "CrashDiagnostics.h"

void UItemManager::BindPickupPointDelegate()
{
	if (bPickupDelegateBound || !worldContext) return;

	APlayerController* PC = worldContext->GetFirstPlayerController();
	if (!PC) return;

	ABasicPlayer* Player = Cast<ABasicPlayer>(PC->GetPawn());
	if (!Player) return;

	UPlayerAnimInstance* AnimInst = Cast<UPlayerAnimInstance>(
		Player->GetMesh()->GetAnimInstance());
	if (!AnimInst) return;

	AnimInst->OnPickupPoint.AddUObject(this, &UItemManager::OnPickupPointFired);
	bPickupDelegateBound = true;

	UE_LOG(LogGameplay, Verbose, TEXT("ItemManager: bound OnPickupPoint delegate"));
}

// ---------------------------------------------------------------------------
// OnPickupPointFired
//   Called by AnimNotify_PickupPoint (or the fallback timer inside
//   PlayerAnimInstance::NotifyPickup) at the visual moment of pickup.
//   1. Plays the Niagara pickup VFX and destroys the DroppedItemActor.
//   2. Broadcasts ProcessItemPickup so inventory / UI refresh happens here.
//   3. Unlocks player movement.
//   4. Triggers inventory refresh from server.
// ---------------------------------------------------------------------------
void UItemManager::OnPickupPointFired()
{
	UE_LOG(LogGameplay, Warning, TEXT("ItemManager: OnPickupPointFired count=%d"), PendingPickupItemUIDs.Num());

	for (int32 i = 0; i < PendingPickupItemUIDs.Num(); ++i)
	{
		const int32 UID = PendingPickupItemUIDs[i];

		// Resolve the real picked quantity from the dropped actor before the
		// effect below recycles it (server confirmations often omit it).
		int32 PickupQty = 1;
		ADroppedItemActor* DroppedActor = nullptr;
		if (UID > 0 && DroppedItemsMap.Contains(UID))
		{
			DroppedActor = DroppedItemsMap[UID];
			if (IsValid(DroppedActor))
			{
				PickupQty = FMath::Max(1, DroppedActor->GetItemData().quantity);
			}
		}

		// Destroy the world item and play pickup VFX
		if (IsValid(DroppedActor))
		{
			DroppedActor->PlayPickupEffect();
		}
		if (UID > 0)
		{
			DroppedItemsMap.Remove(UID);
		}

		// Broadcast item to inventory/UI systems. Catalog-resolved drops carry
		// slug but no name — gate on either, matching the toast path below.
		if (i < PendingPickupItems.Num()
			&& (!PendingPickupItems[i].name.IsEmpty() || !PendingPickupItems[i].slug.IsEmpty()))
		{
			ProcessItemPickup(PendingPickupItems[i], PickupQty);
		}
	}

	// Unlock player movement
	if (worldContext)
	{
		if (APlayerController* PC = worldContext->GetFirstPlayerController())
		{
			if (ABasicPlayer* Player = Cast<ABasicPlayer>(PC->GetPawn()))
			{
				Player->UnlockMovementAfterPickup();
			}
		}
	}

	// Refresh inventory from server
	if (gameInstance && gameInstance->GetInventoryManager())
	{
		gameInstance->GetInventoryManager()->RequestInventoryData(gameInstance->GetCurrentCharacterID());
	}

	// Reset pending state
	PendingPickupItemUIDs.Empty();
	PendingPickupItems.Empty();
}

// ---------------------------------------------------------------------------
// FlushDropsForMob
//   Called by BasicMOB::Die() once the mob is confirmed dead.
//   Spawns every item that was deferred while the mob was still alive.
// ---------------------------------------------------------------------------
