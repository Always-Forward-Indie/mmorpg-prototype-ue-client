// Fill out your copyright notice in the Description page of Project Settings.


#include "Gameplay/Items/ItemManager.h"
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

void UItemManager::ProcessGameServerData(const FString& ReceivedData)
{
	// Queued-raw re-parse entry for FlushPendingEvents; the live path is
	// HandleChunkPacket via the dispatcher fallback (single parse in PollLink).
	HandleChunkPacket(JSONParser::ParsePacket(ReceivedData));
}

void UItemManager::HandleChunkPacket(const FParsedPacket& Packet)
{
	CRASH_GUARD("ItemManager::HandleChunkPacket");
	if (!worldContext)
	{
		UE_LOG(LogGameplay, Warning, TEXT("ItemManager: No world context — queuing network event for later processing"));
		PendingNetworkEvents.Add(Packet.Raw);
		return;
	}

	// Single-parse path (S05): EventType/Status/Body come from PollLink, no re-parse.
	if (!Packet.bValid || !Packet.Root.IsValid()) return;

	const FString& EventType = Packet.EventType;
	const FString& Status = Packet.Status;
	const TSharedPtr<FJsonObject>& Body = Packet.Body;

	// Process time sync data first
	//if (gameInstance && gameInstance->GetTimeSyncService())
	//{
	//	JSONParser::ProcessTimeSyncFromHeader(ReceivedData, gameInstance->GetTimeSyncService());
	//}

	UE_LOG(LogGameplay, Warning, TEXT("ItemManager: Received event type: %s"), *EventType);

	if (EventType == "itemDrop" && Status == "success")
	{
		UE_LOG(LogGameplay, Warning, TEXT("ItemManager: Received itemDrop event"));
		FItemDropResponseStruct ItemDropResponse = JSONParser::DeserializeItemDropResponse(Body);
		for (FDroppedItemStruct& DroppedItem : ItemDropResponse.droppedItems)
		{
			// S2.2: id-only drops resolve their template from the stat catalog.
			ResolveDropStats(DroppedItem);
		}
		HandleItemDrop(ItemDropResponse);
	}
	else if (EventType == "nearbyItems")
	{
		UE_LOG(LogGameplay, Warning, TEXT("ItemManager: Received nearbyItems event"));
		// Body contains { "items": [ DroppedItemStruct... ] } - same shape as itemDrop
		FItemDropResponseStruct NearbyResponse = JSONParser::DeserializeItemDropResponse(Body);
		for (FDroppedItemStruct& DroppedItem : NearbyResponse.droppedItems)
		{
			// S2.2: id-only drops resolve their template from the stat catalog.
			ResolveDropStats(DroppedItem);
			if (DroppedItem.uid > 0)
				SpawnDroppedItem(DroppedItem);
		}
	}
	else if (EventType == "itemPickup" && Status == "success")
	{
		UE_LOG(LogGameplay, Warning, TEXT("ItemManager: Received pickUpItem event"));

		// If the server broadcasts itemPickup to all clients, only the picker should update
		// inventory / play the LOCAL animation.  However every observer client must still
		// (a) play the pickup animation on the remote player's actor and
		// (b) destroy the DroppedItemActor from the world.
		if (gameInstance)
		{
			int32 ResponseCharId = 0;
			Body->TryGetNumberField(TEXT("characterId"), ResponseCharId);
			if (ResponseCharId > 0 && ResponseCharId != gameInstance->GetCurrentCharacterID())
			{
				UE_LOG(LogGameplay, Warning, TEXT("ItemManager: itemPickup by other player charId=%d — playing remote animation and destroying item"),
					ResponseCharId);

				// Read the UID of the picked-up item.
				int32 OtherPickupUID = -1;
				Body->TryGetNumberField(TEXT("droppedItemUID"), OtherPickupUID);

				if (OtherPickupUID > 0)
				{
					// Play pickup animation on the remote player's actor.
					if (ABasicPlayer* OtherPlayer = gameInstance->GetPlayerByCharacterId(ResponseCharId))
					{
						if (UPlayerAnimInstance* OtherAnimInst = Cast<UPlayerAnimInstance>(
							OtherPlayer->GetMesh()->GetAnimInstance()))
						{
							OtherAnimInst->NotifyPickup();
						}
					}

					// Destroy the DroppedItemActor on this client (play VFX then remove).
					if (DroppedItemsMap.Contains(OtherPickupUID))
					{
						ADroppedItemActor* DroppedActor = DroppedItemsMap[OtherPickupUID];
						if (IsValid(DroppedActor))
						{
							DroppedActor->PlayPickupEffect();
						}
						DroppedItemsMap.Remove(OtherPickupUID);
					}
				}
				return;
			}
		}

		// Cache item data from server confirmation
		FItemBaseStruct PickedItem;
		if (Body->HasField(TEXT("item")))
		{
			PickedItem = JSONParser::DeserializeItemData(Body->GetObjectField(TEXT("item")));
		}

		// Cache the dropped item UID so OnPickupPointFired knows which actor to destroy
		int32 NewUID = -1;
		Body->TryGetNumberField(TEXT("droppedItemUID"), NewUID);
		if (NewUID > 0)
		{
			PendingPickupItemUIDs.Add(NewUID);
			PendingPickupItems.Add(PickedItem);
		}

		// Bind the pickup-point delegate once (lazy, first successful pickup)
		BindPickupPointDelegate();

		// Try to get the local player and play the pickup montage.
		// Movement was already locked in InventoryManager::PickupNearbyItem.
		bool bMontageStarted = false;
		if (worldContext)
		{
			if (APlayerController* PC = worldContext->GetFirstPlayerController())
			{
				if (ABasicPlayer* Player = Cast<ABasicPlayer>(PC->GetPawn()))
				{
					if (UPlayerAnimInstance* AnimInst = Cast<UPlayerAnimInstance>(
						Player->GetMesh()->GetAnimInstance()))
					{
						const float MontageDuration = AnimInst->NotifyPickup();
						bMontageStarted = MontageDuration > 0.0f;
						UE_LOG(LogGameplay, Warning,
							TEXT("ItemManager: pickup montage started, duration=%.3fs"),
							MontageDuration);
					}
				}
			}
		}

		// Fallback: no player / no montage assigned � fire immediately
		if (!bMontageStarted)
		{
			UE_LOG(LogGameplay, Warning,
				TEXT("ItemManager: no pickup montage, resolving pickup immediately"));
			OnPickupPointFired();
		}
	}
	else if (EventType == "itemPickup" && Status != "success")
	{
		int32 FailedUID = -1;
		Body->TryGetNumberField(TEXT("droppedItemUID"), FailedUID);

		// Check if the server rejection message indicates the item no longer exists.
		// If so, clean up the stale client-side actor instead of re-enabling it.
		FString ErrorMessage;
		Body->TryGetStringField(TEXT("message"), ErrorMessage);
		const bool bItemNonExistent = ErrorMessage.Contains(TEXT("non-existent"));

		if (FailedUID > 0 && DroppedItemsMap.Contains(FailedUID))
		{
			if (ADroppedItemActor* DroppedActor = DroppedItemsMap[FailedUID])
			{
				if (bItemNonExistent)
				{
					UE_LOG(LogGameplay, Warning, TEXT("ItemManager: pickup failed (non-existent) — cleaning up stale UID %d"), FailedUID);
					DroppedActor->PlayPickupEffect();
					DroppedItemsMap.Remove(FailedUID);
				}
				else
				{
					DroppedActor->SetCanBePickedUp(true);
				}
			}
		}

		UE_LOG(LogGameplay, Warning, TEXT("ItemManager: itemPickup failed — unlocking player movement"));

		// Server rejected the pickup � cancel any pending pickup-point timer and unlock
		if (worldContext)
		{
			if (APlayerController* PC = worldContext->GetFirstPlayerController())
			{
				if (ABasicPlayer* Player = Cast<ABasicPlayer>(PC->GetPawn()))
				{
					if (UPlayerAnimInstance* AnimInst = Cast<UPlayerAnimInstance>(
						Player->GetMesh()->GetAnimInstance()))
					{
						AnimInst->CancelPickupTimer();
					}
					Player->UnlockMovementAfterPickup();
				}
			}
		}

		PendingPickupItemUIDs.Empty();
		PendingPickupItems.Empty();
	}
	else if (EventType == "itemRemove")
	{
		const TArray<TSharedPtr<FJsonValue>>* UidsArray = nullptr;
		if (Body->TryGetArrayField(TEXT("uids"), UidsArray))
		{
			for (const TSharedPtr<FJsonValue>& UidValue : *UidsArray)
			{
				int32 RemoveUid = static_cast<int32>(UidValue->AsNumber());
				if (DroppedItemsMap.Contains(RemoveUid))
				{
					ADroppedItemActor* DroppedActor = DroppedItemsMap[RemoveUid];
					if (IsValid(DroppedActor))
					{
						DroppedActor->PlayPickupEffect();
					}
					DroppedItemsMap.Remove(RemoveUid);
					UE_LOG(LogGameplay, Verbose, TEXT("ItemManager: itemRemove cleaned up stale UID %d"), RemoveUid);
				}
			}
		}
	}
	else if (EventType == "itemCatalog" && Status == "success")
	{
		// S2.1: versioned item stat templates. Entries reuse the inventory-record
		// wrapper ({itemId, item:{...}}) so the shared parser applies as-is.
		int32 Version = 0;
		Body->TryGetNumberField(TEXT("version"), Version);
		TArray<FInventoryItemStruct> Entries;
		const TArray<TSharedPtr<FJsonValue>>* ItemsArray = nullptr;
		if (Body.IsValid() && Body->TryGetArrayField(TEXT("items"), ItemsArray))
		{
			for (const TSharedPtr<FJsonValue>& ItemValue : *ItemsArray)
			{
				const TSharedPtr<FJsonObject>* ItemPtr = nullptr;
				if (!ItemValue.IsValid() || !ItemValue->TryGetObject(ItemPtr) || !ItemPtr || !(*ItemPtr).IsValid()) continue;
				FInventoryItemStruct Entry = JSONParser::DeserializeInventoryItem(*ItemPtr);
				if (Entry.itemId > 0)
					Entries.Add(Entry);
			}
		}
		SetItemCatalog(Version, Entries);
	}
}

