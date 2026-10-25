#include "Gameplay/Items/HarvestManager.h"
#include "Prototyping.h"
#include "Networking/NetworkManager.h"
#include "MyGameInstance.h"
#include "Utils/JSONParser.h"
#include "Engine/World.h"
#include "Gameplay/Mobs/BasicMOB.h"
#include "Gameplay/Players/BasicPlayer.h"
#include "Gameplay/Players/PlayerAnimInstance.h"
#include "Gameplay/Mobs/MOBManager.h"
#include "Utils/NetCore/ProximityRule.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"
#include "UI/HarvestProgressWidget.h"
#include "UI/HarvestLootWidget.h"
#include "TimerManager.h"
#include "CrashDiagnostics.h"

void UHarvestManager::TryHarvestNearbyCorpse()
{
	if (bIsHarvesting)
	{
		UE_LOG(LogGameplay, Warning, TEXT("HarvestManager: Already harvesting"));
		return;
	}

	ABasicMOB* NearestCorpse = GetNearestHarvestableCorpse(MaxHarvestDistance);
	if (NearestCorpse)
	{
		int32 CorpseUID = FCString::Atoi(*NearestCorpse->GetMOBUId());
		UE_LOG(LogGameplay, Warning, TEXT("HarvestManager: Starting harvest for corpse UID: %d"), CorpseUID);
		StartHarvest(CorpseUID);
	}
	else
	{
		// Check for already harvested corpses to inspect
		ABasicMOB* NearestHarvestedCorpse = GetNearestHarvestedCorpse(MaxHarvestDistance);
		if (NearestHarvestedCorpse)
		{
			int32 CorpseUID = FCString::Atoi(*NearestHarvestedCorpse->GetMOBUId());
			UE_LOG(LogGameplay, Warning, TEXT("HarvestManager: Inspecting harvested corpse UID: %d"), CorpseUID);
			InspectCorpseLoot(CorpseUID);
		}
		else
		{
			UE_LOG(LogGameplay, Warning, TEXT("HarvestManager: No harvestable or harvested corpse found within range"));
			
			// Show message to player
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Yellow, 
					TEXT("No harvestable or harvested corpse found within range"));
			}
		}
	}
}

void UHarvestManager::TryHarvestSpecificCorpse(ABasicMOB* TargetCorpse)
{
    if (!IsValid(TargetCorpse))
    {
        UE_LOG(LogGameplay, Warning, TEXT("HarvestManager: TryHarvestSpecificCorpse - null target"));
        return;
    }

    if (bIsHarvesting)
    {
        UE_LOG(LogGameplay, Warning, TEXT("HarvestManager: Already harvesting"));
        return;
    }

    const int32 CorpseUID = FCString::Atoi(*TargetCorpse->GetMOBUId());

    // MOB is harvestable (not yet harvested) → start harvest
    if (!TargetCorpse->GetMOBData().bHasBeenHarvested)
    {
        UE_LOG(LogGameplay, Warning, TEXT("HarvestManager: Harvesting specific corpse UID=%d"), CorpseUID);
        StartHarvest(CorpseUID);
    }
    else
    {
        // Already harvested → inspect loot instead
        UE_LOG(LogGameplay, Warning, TEXT("HarvestManager: Inspecting already-harvested corpse UID=%d"), CorpseUID);
        InspectCorpseLoot(CorpseUID);
    }
}

void UHarvestManager::StartHarvest(int32 CorpseUID){
	if (bIsHarvesting)
	{
		UE_LOG(LogGameplay, Warning, TEXT("HarvestManager: Already harvesting"));
		return;
	}

	if (!networkManager || !gameInstance)
	{
		UE_LOG(LogGameplay, Error, TEXT("HarvestManager: Cannot start harvest - missing dependencies"));
		return;
	}

	if (!networkManager->IsChunkServerConnected())
	{
		UE_LOG(LogGameplay, Warning, TEXT("HarvestManager: Chunk server not connected, cannot start harvest"));
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red,
				TEXT("Cannot harvest: not connected to world server"));
		}
		return;
	}

	CurrentCorpseUID = CorpseUID;
	SendHarvestStartRequest(CorpseUID);
}

void UHarvestManager::LockPlayerMovement()
{
	if (UWorld* World = GetValidWorld())
	{
		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			if (ABasicPlayer* Player = Cast<ABasicPlayer>(PC->GetPawn()))
			{
				Player->LockMovement();

				// Play harvest animation scaled to the server's desired duration
				if (HarvestDuration > 0)
				{
					if (UPlayerAnimInstance* AnimInst = Cast<UPlayerAnimInstance>(Player->GetMesh()->GetAnimInstance()))
					{
						AnimInst->NotifyHarvest(static_cast<float>(HarvestDuration) / 1000.0f);
					}
				}
			}
		}
	}
}

void UHarvestManager::UnlockPlayerMovement()
{
	if (UWorld* World = GetValidWorld())
	{
		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			if (ABasicPlayer* Player = Cast<ABasicPlayer>(PC->GetPawn()))
			{
				Player->UnlockMovement();
			}
		}
	}
}

void UHarvestManager::PickupLootItem(int32 ItemId, int32 Quantity)
{
	if (CurrentCorpseUID == 0)
	{
		UE_LOG(LogGameplay, Warning, TEXT("HarvestManager: No active corpse to pickup from"));
		return;
	}

	TArray<FCorpseLootPickupRequestItem> RequestedItems;
	FCorpseLootPickupRequestItem Item;
	Item.itemId = ItemId;
	Item.quantity = Quantity;
	RequestedItems.Add(Item);

	SendLootPickupRequest(CurrentCorpseUID, RequestedItems);
}

void UHarvestManager::PickupAllLoot()
{
	if (CurrentCorpseUID == 0 || CurrentAvailableLoot.Num() == 0)
	{
		UE_LOG(LogGameplay, Warning, TEXT("HarvestManager: No loot available to pickup"));
		return;
	}

	TArray<FCorpseLootPickupRequestItem> RequestedItems;
	for (const FHarvestItemStruct& LootItem : CurrentAvailableLoot)
	{
		FCorpseLootPickupRequestItem Item;
		Item.itemId = LootItem.itemId;
		Item.quantity = LootItem.quantity;
		RequestedItems.Add(Item);
	}

	SendLootPickupRequest(CurrentCorpseUID, RequestedItems);
}

void UHarvestManager::CancelHarvest()
{
	if (!bIsHarvesting)
	{
		return;
	}

	UE_LOG(LogGameplay, Warning, TEXT("HarvestManager: Harvest cancelled"));

	// Notify the server so it can broadcast harvestCancelBroadcast to other clients
	SendHarvestCancelRequest(CurrentCorpseUID);

	bIsHarvesting = false;
	CurrentCorpseUID = 0;
	HarvestStartTime = 0.0f;
	HarvestDuration = 0.0f;
	ServerStartTime = 0;

	HideHarvestProgress();
	UnlockPlayerMovement();
}

void UHarvestManager::SendHarvestCancelRequest(int32 CorpseUID)
{
	if (!networkManager || !gameInstance) return;

	TMap<FString, TSharedPtr<FJsonValue>> HeaderData;
	TMap<FString, TSharedPtr<FJsonValue>> BodyData;

	HeaderData.Add(TEXT("clientId"), MakeShareable(new FJsonValueNumber(gameInstance->GetCurrentClientID())));
	HeaderData.Add(TEXT("hash"),     MakeShareable(new FJsonValueString(gameInstance->GetCurrentClientHash())));

	BodyData.Add(TEXT("corpseUID"), MakeShareable(new FJsonValueNumber(CorpseUID)));

	FString JsonString = JSONParser::SerializeJsonWithTimeSync(
		TEXT("harvestCancel"), HeaderData, BodyData,
		gameInstance->GetTimeSyncService(), EServerType::ChunkServer);

	networkManager->SendDataToChunkServer(JsonString);
	UE_LOG(LogGameplay, Warning, TEXT("HarvestManager: Sent harvestCancel request for corpse: %d"), CorpseUID);
}

float UHarvestManager::GetHarvestProgress() const
{
	UE_LOG(LogGameplay, Warning, TEXT("HarvestManager::GetHarvestProgress - Starting calculation"));
	
	if (!bIsHarvesting || HarvestDuration <= 0.0f)
	{
		UE_LOG(LogGameplay, Warning, TEXT("HarvestManager::GetHarvestProgress - Not harvesting or invalid duration. bIsHarvesting: %s, HarvestDuration: %.0f"), 
			bIsHarvesting ? TEXT("true") : TEXT("false"), HarvestDuration);
		return 0.0f;
	}

	UWorld* World = GetValidWorld();
	if (!World)
	{
		UE_LOG(LogGameplay, Error, TEXT("HarvestManager::GetHarvestProgress - GetValidWorld() returned null!"));
		return 0.0f;
	}

	float CurrentTime = World->GetTimeSeconds();
	float ElapsedTime = CurrentTime - HarvestStartTime;
	float TotalDurationInSeconds = HarvestDuration / 1000.0f;
	float Progress = FMath::Clamp(ElapsedTime / TotalDurationInSeconds, 0.0f, 1.0f);
	
	// Always log for debugging
	UE_LOG(LogGameplay, Warning, TEXT("HarvestManager::GetHarvestProgress - Progress: %.1f%%, ElapsedTime: %.2fs, TotalDuration: %.2fs, HarvestDuration(ms): %.0f"), 
		Progress * 100.0f, ElapsedTime, TotalDurationInSeconds, HarvestDuration);
	
	return Progress;
}

ABasicMOB* UHarvestManager::GetNearestHarvestableCorpse(float MaxDistance) const
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

	// Iterate the mob registry (O(registered mobs), no world scan).
	ABasicMOB* ClosestCorpse = nullptr;
	float ClosestDistance = MaxDistance;

	if (UMOBManager* MobMgr = gameInstance ? gameInstance->GetMOBManager() : nullptr)
	{
		MobMgr->ForEachMob([&](ABasicMOB* Mob)
		{
			if (Mob && Mob->CanBeHarvested())
			{
				const float Distance = FVector::Dist(PlayerLocation, Mob->GetActorLocation());
				if (FProximityRule::IsInRange(Distance, MaxDistance) && Distance < ClosestDistance)
				{
					ClosestDistance = Distance;
					ClosestCorpse = Mob;
				}
			}
		});
	}

	return ClosestCorpse;
}

