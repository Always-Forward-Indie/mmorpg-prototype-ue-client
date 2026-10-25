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
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"
#include "UI/HarvestProgressWidget.h"
#include "UI/HarvestLootWidget.h"
#include "TimerManager.h"
#include "CrashDiagnostics.h"

void UHarvestManager::ProcessGameServerData(const FString& ReceivedData)
{
	CRASH_GUARD("HarvestManager::ProcessGameServerData");
	if (!worldContext) return;

	// Process time sync data first
	//if (gameInstance && gameInstance->GetTimeSyncService())
	//{
	//	JSONParser::ProcessTimeSyncFromHeader(ReceivedData, gameInstance->GetTimeSyncService());
	//}

	FMessageDataStruct MessageData = JSONParser::DeserializeMessageData(ReceivedData);
	UE_LOG(LogGameplay, Warning, TEXT("HarvestManager: Received event type: %s"), *MessageData.eventType);

	// Handle harvest started response
	if (MessageData.eventType == "harvestStarted" && MessageData.status == "success")
	{
		UE_LOG(LogGameplay, Warning, TEXT("HarvestManager: Received harvest started"));
		FHarvestStartedStruct HarvestData = JSONParser::DeserializeHarvestStarted(ReceivedData);
		
		// Validate harvest data before proceeding
		if (HarvestData.duration <= 0)
		{
			UE_LOG(LogGameplay, Error, TEXT("HarvestManager: Invalid harvest duration: %d ms"), HarvestData.duration);
			return;
		}
		
		if (HarvestData.corpseId <= 0)
		{
			UE_LOG(LogGameplay, Error, TEXT("HarvestManager: Invalid corpse ID: %d"), HarvestData.corpseId);
			return;
		}
		
		UE_LOG(LogGameplay, Warning, TEXT("HarvestManager: About to set harvest state variables"));
		
		// Start local harvest progress
		bIsHarvesting = true;
		CurrentCorpseUID = HarvestData.corpseId;
		HarvestDuration = HarvestData.duration;
		ServerStartTime = HarvestData.startTime;
		
		UWorld* World = GetValidWorld();
		if (World)
		{
			HarvestStartTime = World->GetTimeSeconds();
			UE_LOG(LogGameplay, Warning, TEXT("HarvestManager: Successfully got World time: %.2f"), HarvestStartTime);
		}
		else
		{
			UE_LOG(LogGameplay, Error, TEXT("HarvestManager: GetValidWorld() returned null when setting HarvestStartTime!"));
			HarvestStartTime = 0.0f;
		}

		// Add detailed debug logging
		UE_LOG(LogGameplay, Warning, TEXT("HarvestManager: Harvest started - CorpseID: %d, Duration: %dms (%.2fs), ServerStartTime: %lld, ClientStartTime: %.2fs"), 
			HarvestData.corpseId, HarvestData.duration, HarvestData.duration / 1000.0f, HarvestData.startTime, HarvestStartTime);

		UE_LOG(LogGameplay, Warning, TEXT("HarvestManager: State after setting - bIsHarvesting: %s, HarvestDuration: %.0f, HarvestStartTime: %.2f"), 
			bIsHarvesting ? TEXT("true") : TEXT("false"), HarvestDuration, HarvestStartTime);

		ShowHarvestProgress();
		LockPlayerMovement();
		OnHarvestStarted.Broadcast(HarvestData);
		
		UE_LOG(LogGameplay, Warning, TEXT("HarvestManager: Harvest started event processing completed"));
	}
	// Handle harvest complete response
	else if (MessageData.eventType == "harvestComplete" && MessageData.status == "success")
	{
		UE_LOG(LogGameplay, Warning, TEXT("HarvestManager: Received harvest complete"));
		FHarvestCompleteStruct HarvestData = JSONParser::DeserializeHarvestComplete(ReceivedData);
		HandleHarvestComplete(HarvestData);
	}
	// Handle harvest error
	else if (MessageData.eventType == "harvestError" && MessageData.status == "error")
	{
		UE_LOG(LogGameplay, Warning, TEXT("HarvestManager: Received harvest error"));
		FHarvestErrorStruct ErrorData = JSONParser::DeserializeHarvestError(ReceivedData);
		
		// Cancel local harvest
		CancelHarvest();
		
		// Show error message
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, 
				FString::Printf(TEXT("Harvest Error: %s"), *ErrorData.message));
		}

		OnHarvestError.Broadcast(ErrorData);
	}
	// Handle loot pickup success
	else if (MessageData.eventType == "corpseLootPickup" && MessageData.status == "success")
	{
		UE_LOG(LogGameplay, Warning, TEXT("HarvestManager: Received loot pickup success"));
		FCorpseLootPickupResponseStruct PickupData = JSONParser::DeserializeCorpseLootPickupResponse(ReceivedData);
		HandleLootPickupResponse(PickupData);
	}
	// Handle loot pickup error
	else if (MessageData.eventType == "corpseLootPickup" && MessageData.status == "error")
	{
		UE_LOG(LogGameplay, Warning, TEXT("HarvestManager: Received loot pickup error"));
		FCorpseLootPickupErrorStruct ErrorData = JSONParser::DeserializeCorpseLootPickupError(ReceivedData);
		
		// Show error message
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, 
				FString::Printf(TEXT("Loot Pickup Error: %s"), *ErrorData.errorCode));
		}

		OnLootPickupError.Broadcast(ErrorData);
	}
	// Handle loot inspect success
	else if (MessageData.eventType == "corpseLootInspect" && MessageData.status == "success")
	{
		UE_LOG(LogGameplay, Warning, TEXT("HarvestManager: Received loot inspect success"));
		FCorpseLootInspectResponseStruct InspectData = JSONParser::DeserializeCorpseLootInspectResponse(ReceivedData);
		HandleLootInspectResponse(InspectData);
	}
	// Handle loot inspect error
	else if (MessageData.eventType == "corpseLootInspect" && MessageData.status == "error")
	{
		UE_LOG(LogGameplay, Warning, TEXT("HarvestManager: Received loot inspect error"));
		FCorpseLootInspectErrorStruct ErrorData = JSONParser::DeserializeCorpseLootInspectError(ReceivedData);
		
		// Show error message based on error code
		FString ErrorMessage = TEXT("Loot Inspect Error");
		if (ErrorData.errorCode == "CORPSE_NOT_FOUND")
		{
			ErrorMessage = TEXT("Corpse not found");
		}
		else if (ErrorData.errorCode == "CORPSE_NOT_HARVESTED")
		{
			ErrorMessage = TEXT("Corpse has not been harvested yet");
		}
		else if (ErrorData.errorCode == "NOT_YOUR_HARVEST")
		{
			ErrorMessage = TEXT("You can only inspect loot from corpses you harvested");
		}
		else if (ErrorData.errorCode == "SECURITY_VIOLATION")
		{
			ErrorMessage = TEXT("Security violation: player ID mismatch");
		}
		else
		{
			ErrorMessage = FString::Printf(TEXT("Loot Inspect Error: %s"), *ErrorData.errorCode);
		}

		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, ErrorMessage);
		}

		OnLootInspectError.Broadcast(ErrorData);
	}
	// harvestCancelled — unicast confirmation to the player who sent harvestCancel
	else if (MessageData.eventType == "harvestCancelled")
	{
		UE_LOG(LogGameplay, Warning, TEXT("HarvestManager: Received harvestCancelled (server confirmed our cancel)"));
		// The cancel was already applied locally in CancelHarvest(); nothing more to do here.
	}
	// harvestStartBroadcast — another player started harvesting on a corpse
	else if (MessageData.eventType == "harvestStartBroadcast")
	{
		TSharedPtr<FJsonObject> Root;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ReceivedData);
		if (FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid())
		{
			const TSharedPtr<FJsonObject>* BodyPtr = nullptr;
			if (Root->TryGetObjectField(TEXT("body"), BodyPtr))
			{
				int32 HarvesterCharId = 0;
				int32 BroadcastCorpseUID = 0;
				(*BodyPtr)->TryGetNumberField(TEXT("characterId"), HarvesterCharId);
				(*BodyPtr)->TryGetNumberField(TEXT("corpseUID"),   BroadcastCorpseUID);

				UE_LOG(LogGameplay, Warning, TEXT("HarvestManager: harvestStartBroadcast charId=%d corpseUID=%d"),
					HarvesterCharId, BroadcastCorpseUID);

				// Update the corresponding BasicMOB actor's harvesting state
				if (worldContext && BroadcastCorpseUID > 0 && gameInstance && gameInstance->GetMOBManager())
				{
					if (ABasicMOB* Mob = Cast<ABasicMOB>(gameInstance->GetMOBManager()->FindMobActor(BroadcastCorpseUID)))
					{
						Mob->SetCurrentHarvesterId(HarvesterCharId);
					}
				}

				// Play harvest animation on the remote player's actor so all clients see it.
				if (gameInstance && HarvesterCharId > 0)
				{
					if (ABasicPlayer* Harvester = gameInstance->GetPlayerByCharacterId(HarvesterCharId))
					{
						if (UPlayerAnimInstance* HarvesterAnim = Cast<UPlayerAnimInstance>(
							Harvester->GetMesh()->GetAnimInstance()))
						{
							// Prefer server-provided duration; default to 3.0s if absent.
							double DurationMs = 3000.0;
							(*BodyPtr)->TryGetNumberField(TEXT("durationMs"), DurationMs);
							HarvesterAnim->NotifyHarvest(static_cast<float>(DurationMs) / 1000.0f);
						}
					}
				}
			}
		}
	}
	// harvestCompleteBroadcast — another player finished harvesting a corpse
	else if (MessageData.eventType == "harvestCompleteBroadcast")
	{
		TSharedPtr<FJsonObject> Root;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ReceivedData);
		if (FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid())
		{
			const TSharedPtr<FJsonObject>* BodyPtr = nullptr;
			if (Root->TryGetObjectField(TEXT("body"), BodyPtr))
			{
				int32 HarvesterCharId = 0;
				int32 BroadcastCorpseUID = 0;
				(*BodyPtr)->TryGetNumberField(TEXT("characterId"), HarvesterCharId);
				(*BodyPtr)->TryGetNumberField(TEXT("corpseUID"), BroadcastCorpseUID);

				UE_LOG(LogGameplay, Warning, TEXT("HarvestManager: harvestCompleteBroadcast charId=%d corpseUID=%d"),
					HarvesterCharId, BroadcastCorpseUID);

				if (worldContext && BroadcastCorpseUID > 0 && gameInstance && gameInstance->GetMOBManager())
				{
					if (ABasicMOB* Mob = Cast<ABasicMOB>(gameInstance->GetMOBManager()->FindMobActor(BroadcastCorpseUID)))
					{
						Mob->SetCurrentHarvesterId(0);
						Mob->SetHarvested(true);
					}
				}

				// Stop harvest animation on the remote player's actor.
				if (gameInstance && HarvesterCharId > 0)
				{
					if (ABasicPlayer* Harvester = gameInstance->GetPlayerByCharacterId(HarvesterCharId))
					{
						if (UPlayerAnimInstance* HarvesterAnim = Cast<UPlayerAnimInstance>(
							Harvester->GetMesh()->GetAnimInstance()))
						{
							if (HarvesterAnim->HarvestMontage)
							{
								HarvesterAnim->Montage_Stop(0.2f, HarvesterAnim->HarvestMontage);
							}
						}
					}
				}
			}
		}
	}
	// harvestCancelBroadcast — any player's harvest was cancelled (includes yourself via server-side interrupt)
	else if (MessageData.eventType == "harvestCancelBroadcast")
	{
		TSharedPtr<FJsonObject> Root;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ReceivedData);
		if (FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid())
		{
			const TSharedPtr<FJsonObject>* BodyPtr = nullptr;
			if (Root->TryGetObjectField(TEXT("body"), BodyPtr))
			{
				int32 CancelledCharId  = 0;
				int32 BroadcastCorpseUID = 0;
				(*BodyPtr)->TryGetNumberField(TEXT("characterId"), CancelledCharId);
				(*BodyPtr)->TryGetNumberField(TEXT("corpseUID"),   BroadcastCorpseUID);

				UE_LOG(LogGameplay, Warning, TEXT("HarvestManager: harvestCancelBroadcast charId=%d corpseUID=%d"),
					CancelledCharId, BroadcastCorpseUID);

				// Clear harvester state on the MOB actor
				if (worldContext && BroadcastCorpseUID > 0 && gameInstance && gameInstance->GetMOBManager())
				{
					if (ABasicMOB* Mob = Cast<ABasicMOB>(gameInstance->GetMOBManager()->FindMobActor(BroadcastCorpseUID)))
					{
						Mob->SetCurrentHarvesterId(0);
					}
				}

				// If the server interrupted our own harvest (e.g. player moved too far), cancel locally
				if (bIsHarvesting && BroadcastCorpseUID == CurrentCorpseUID)
				{
					const int32 LocalCharId = gameInstance ? gameInstance->GetCurrentCharacterID() : 0;
					if (LocalCharId > 0 && CancelledCharId == LocalCharId)
					{
						UE_LOG(LogGameplay, Warning, TEXT("HarvestManager: Server cancelled our harvest via broadcast, clearing local state"));
						bIsHarvesting   = false;
						CurrentCorpseUID = 0;
						HarvestStartTime = 0.0f;
						HarvestDuration  = 0.0f;
						ServerStartTime  = 0;
						HideHarvestProgress();
					}
				}

				// Stop harvest animation on the remote player whose harvest was cancelled.
				if (gameInstance && CancelledCharId > 0)
				{
					if (ABasicPlayer* CancelledPlayer = gameInstance->GetPlayerByCharacterId(CancelledCharId))
					{
						if (UPlayerAnimInstance* AnimInst = Cast<UPlayerAnimInstance>(
							CancelledPlayer->GetMesh()->GetAnimInstance()))
						{
							if (AnimInst->HarvestMontage)
							{
								AnimInst->Montage_Stop(0.2f, AnimInst->HarvestMontage);
							}
						}
					}
				}
			}
		}
	}
	// corpseRemoved — server despawned the corpse; hide / destroy the corresponding MOB actor
	else if (MessageData.eventType == "corpseRemoved")
	{
		TSharedPtr<FJsonObject> Root;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ReceivedData);
		if (FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid())
		{
			const TSharedPtr<FJsonObject>* BodyPtr = nullptr;
			if (Root->TryGetObjectField(TEXT("body"), BodyPtr))
			{
				int32 RemovedCorpseUID = 0;
				(*BodyPtr)->TryGetNumberField(TEXT("corpseUID"), RemovedCorpseUID);

				UE_LOG(LogGameplay, Warning, TEXT("HarvestManager: corpseRemoved corpseUID=%d"), RemovedCorpseUID);

				KnownEmptyCorpses.Remove(RemovedCorpseUID);

				if (worldContext && RemovedCorpseUID > 0 && gameInstance && gameInstance->GetMOBManager())
				{
					if (ABasicMOB* Mob = Cast<ABasicMOB>(gameInstance->GetMOBManager()->FindMobActor(RemovedCorpseUID)))
					{
						Mob->Destroy();
					}
				}

			// Cancel our harvest if it was on this corpse
			if (bIsHarvesting && CurrentCorpseUID == RemovedCorpseUID)
			{
				bIsHarvesting   = false;
				CurrentCorpseUID = 0;
				HarvestStartTime = 0.0f;
				HarvestDuration  = 0.0f;
				ServerStartTime  = 0;
				HideHarvestProgress();
			}

			// Hide loot window if it was open for this corpse
			if (CurrentCorpseUID == RemovedCorpseUID || CurrentAvailableLoot.Num() > 0)
			{
				CurrentAvailableLoot.Empty();
				CurrentCorpseUID = 0;
				HideLootWindow();
			}
			}
		}
	}
}

