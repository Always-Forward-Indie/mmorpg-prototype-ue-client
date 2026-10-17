// Fill out your copyright notice in the Description page of Project Settings.


#include "Gameplay/Items/ItemManager.h"
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

void UItemManager::FlushDropsForMob(int32 MobUID)
{
	TArray<FDroppedItemStruct>* Pending = PendingMobDrops.Find(MobUID);
	if (!Pending || Pending->Num() == 0) return;

	TArray<FDroppedItemStruct> ToSpawn = MoveTemp(*Pending);
	PendingMobDrops.Remove(MobUID);

	UE_LOG(LogTemp, Log, TEXT("ItemManager: FlushDropsForMob uid=%d — spawning %d deferred item(s)"),
		MobUID, ToSpawn.Num());

	for (const FDroppedItemStruct& Drop : ToSpawn)
	{
		SpawnDroppedItem(Drop);
	}
}

void UItemManager::ClearWorldState()
{
	DroppedItemsMap.Empty();
	PendingMobDrops.Empty();
	PendingNetworkEvents.Empty();

	UE_LOG(LogTemp, Log, TEXT("ItemManager: Cleared world state (dropped items map, pending mob drops, pending events)"));
}

void UItemManager::FlushPendingEvents()
{
	if (PendingNetworkEvents.Num() == 0) return;

	UE_LOG(LogTemp, Warning, TEXT("ItemManager: Flushing %d pending network events"), PendingNetworkEvents.Num());
	TArray<FString> Events = MoveTemp(PendingNetworkEvents);
	PendingNetworkEvents.Empty();

	UWorld* SavedWorld = worldContext;
	for (const FString& Event : Events)
	{
		ProcessGameServerData(Event);
	}
}
