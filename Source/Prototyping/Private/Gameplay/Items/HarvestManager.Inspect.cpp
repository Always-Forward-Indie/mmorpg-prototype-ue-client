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

void UHarvestManager::TryInspectNearbyCorpseLoot()
{
	if (bIsHarvesting)
	{
		UE_LOG(LogGameplay, Warning, TEXT("HarvestManager: Cannot inspect while harvesting"));
		return;
	}

	ABasicMOB* NearestHarvestedCorpse = GetNearestHarvestedCorpse(MaxHarvestDistance);
	if (NearestHarvestedCorpse)
	{
		int32 CorpseUID = FCString::Atoi(*NearestHarvestedCorpse->GetMOBUId());
		UE_LOG(LogGameplay, Warning, TEXT("HarvestManager: Inspecting harvested corpse UID: %d"), CorpseUID);
		InspectCorpseLoot(CorpseUID);
	}
	else
	{
		UE_LOG(LogGameplay, Warning, TEXT("HarvestManager: No harvested corpse found within range"));
		
		// Show message to player
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Yellow, 
				TEXT("No harvested corpse found within range"));
		}
	}
}

void UHarvestManager::InspectCorpseLoot(int32 CorpseUID)
{
	if (!networkManager || !gameInstance)
	{
		UE_LOG(LogGameplay, Error, TEXT("HarvestManager: Cannot inspect corpse loot - missing dependencies"));
		return;
	}

	SendLootInspectRequest(CorpseUID);
}

ABasicMOB* UHarvestManager::GetNearestHarvestedCorpse(float MaxDistance) const
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
	ABasicMOB* ClosestHarvestedCorpse = nullptr;
	float ClosestDistance = MaxDistance;

	if (UMOBManager* MobMgr = gameInstance ? gameInstance->GetMOBManager() : nullptr)
	{
		MobMgr->ForEachMob([&](ABasicMOB* Mob)
		{
			if (Mob && Mob->GetMOBIsDead() && Mob->HasBeenHarvested())
			{
				const int32 MobUID = FCString::Atoi(*Mob->GetMOBUId());
				if (IsCorpseEmpty(MobUID))
					return;

				const float Distance = FVector::Dist(PlayerLocation, Mob->GetActorLocation());
				if (FProximityRule::IsInRange(Distance, MaxDistance) && Distance < ClosestDistance)
				{
					ClosestDistance = Distance;
					ClosestHarvestedCorpse = Mob;
				}
			}
		});
	}

	return ClosestHarvestedCorpse;
}

