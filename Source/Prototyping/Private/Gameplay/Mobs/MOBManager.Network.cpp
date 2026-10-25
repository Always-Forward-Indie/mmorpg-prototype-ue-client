// Fill out your copyright notice in the Description page of Project Settings.


#include "Gameplay/Mobs/MOBManager.h"
#include "Prototyping.h"
#include "MyGameInstance.h"
#include "Gameplay/Players/BasicPlayer.h"
#include "CrashDiagnostics.h"

void UMOBManager::HandleChunkPacket(const FParsedPacket& Packet)
{
    CRASH_GUARD("MOBManager::HandleChunkPacket");
    if (!worldContext) return;

	// Single-parse path (S05): EventType/Status/Body come from PollLink, no re-parse.
	if (!Packet.bValid || !Packet.Root.IsValid()) return;

	// Compat: effectTick still deserializes from the raw string (no Body overload exists).
	const FString& ReceivedData = Packet.Raw;

	const FString& EventType = Packet.EventType;
	const FString& Status = Packet.Status;
	const TSharedPtr<FJsonObject>& Root = Packet.Root;
	const TSharedPtr<FJsonObject>& Body = Packet.Body;

	UE_LOG(LogGameplay, Warning, TEXT("MOBManager: Received event type: %s"), *EventType);

	// Check if this is a combat event - if so, let CombatNetworkHandler handle it
	//if (IsCombatEvent(EventType))
	//{
	//	// Combat events are handled by CombatNetworkHandler which is already subscribed to the same event
	//	// No need to process them here
	//	return;
	//}

	// (no re-parse: Root/Body come from the dispatcher packet)

	if (EventType == "spawnMobsInZone" && Status == "success")
	{
		UE_LOG(LogGameplay, Warning, TEXT("MOBManager: Received spawnMobsInZone event"));
		TArray<FMOBStruct> MobsData = JSONParser::DeserializeMobsList(Body);
		for (FMOBStruct MobData : MobsData)
		{
			SpawnMOB(MobData);
		}
	}
	else if (EventType == "getMOBData" && Status == "success")
	{
		UE_LOG(LogGameplay, Warning, TEXT("Received getMOBData event"));
		// Deserialize the JSON string into a single MOB data
		FMOBStruct MobData = JSONParser::DeserializeMobData(Body->GetObjectField(TEXT("mob")));
	}
	else if (EventType == "zoneMoveMobs" && Status == "success")
	{
		UE_LOG(LogGameplay, Warning, TEXT("Received moveMOB event"));

		if (!worldContext || !worldContext->IsValidLowLevel())
		{
			UE_LOG(LogGameplay, Error, TEXT("MOBManager: Cannot process zoneMoveMobs - no valid world context"));
			return;
		}

		// Deserialize the JSON string into a list of MOBs
		TArray<FMOBStruct> MobsData = JSONParser::DeserializeMobsList(Body);
		for (FMOBStruct MobData : MobsData)
		{
			if (ABasicMOB* MOB = Cast<ABasicMOB>(FindMobActor(FCString::Atoi(*MobData.mobUniqueID))))
			{
				MOB->OnReceiveServerPacket(MobData.mobPosition);
			}
		}
	}
	else if (EventType == "mobDeath" && Status == "success")
	{
		int32 MobUID = 0;
		if (!Body.IsValid() || !Body->TryGetNumberField(TEXT("mobUID"), MobUID) || MobUID <= 0)
		{
			UE_LOG(LogGameplay, Error, TEXT("MOBManager: mobDeath - missing or invalid mobUID"));
			return;
		}

		TWeakObjectPtr<ABasicMOB>* FoundWeak = MobActorRegistry.Find(MobUID);
		if (!FoundWeak || !FoundWeak->IsValid())
		{
			UE_LOG(LogGameplay, Warning, TEXT("MOBManager: mobDeath for UID %d - actor not found (already cleaned up?)"), MobUID);
			return;
		}

		ABasicMOB* MOB = FoundWeak->Get();
		if (!IsValid(MOB))
		{
			UE_LOG(LogGameplay, Warning, TEXT("MOBManager: mobDeath for UID %d - actor is pending kill"), MobUID);
			return;
		}

		UE_LOG(LogGameplay, Warning, TEXT("MOBManager: mobDeath for UID %d (%s)"), MobUID, *MOB->GetMobName());

		if (!MOB->GetMOBIsDead())
		{
			MOB->Die();
		}

		ClearLockedTargetOnAllPlayers(MOB);

		MOB->Destroy();
	}
	else if (EventType == "mobMoveUpdate" && Status == "success")
	{
		if (!worldContext || !worldContext->IsValidLowLevel()) return;

		int64 ServerSendMs = 0;
		{
			const TSharedPtr<FJsonObject>* HdrPtr = nullptr;
			if (Root->TryGetObjectField(TEXT("header"), HdrPtr) && HdrPtr)
			{
				double RawMs = 0.0;
				if ((*HdrPtr)->TryGetNumberField(TEXT("serverSendMs"), RawMs))
				{
					ServerSendMs = static_cast<int64>(RawMs);
				}
			}
		}

		const FDateTime UtcNow    = FDateTime::UtcNow();
		const int64 ClientRecvMs  = UtcNow.ToUnixTimestamp() * 1000 + UtcNow.GetMillisecond();

		TArray<JSONParser::FMobMovePacketEntry> Entries =
			JSONParser::DeserializeMobMoveUpdate(Body, ServerSendMs);

		for (const JSONParser::FMobMovePacketEntry& Entry : Entries)
		{
			TWeakObjectPtr<ABasicMOB>* FoundWeak = MobActorRegistry.Find(Entry.uid);
			if (!FoundWeak || !FoundWeak->IsValid()) continue;

			ABasicMOB* MOB = FoundWeak->Get();
			if (IsValid(MOB))
			{
				MOB->OnReceiveMovePacket(Entry.moveEntry, ServerSendMs, ClientRecvMs);
			}
		}
	}
	else if (EventType == "mobHealthUpdate")
	{
		UE_LOG(LogGameplay, Verbose, TEXT("MOBManager: Received mobHealthUpdate event"));

		if (!worldContext || !worldContext->IsValidLowLevel()) return;

		int32 MobUID = 0;
		int32 CurrentHP = 0;
		int32 MaxHP = 0;
		if (Body.IsValid())
		{
			Body->TryGetNumberField(TEXT("mobUID"), MobUID);
			Body->TryGetNumberField(TEXT("currentHealth"), CurrentHP);
			Body->TryGetNumberField(TEXT("maxHealth"), MaxHP);
		}

		if (MobUID > 0)
		{
			TWeakObjectPtr<ABasicMOB>* FoundWeak = MobActorRegistry.Find(MobUID);
			if (FoundWeak && FoundWeak->IsValid())
			{
				ABasicMOB* MOB = FoundWeak->Get();
				FMobHealthUpdateStruct HealthUpdate;
				HealthUpdate.mobUID = MobUID;
				HealthUpdate.mobId  = Body.IsValid() ? Body->GetIntegerField(TEXT("mobId")) : 0;
				HealthUpdate.currentHealth = CurrentHP;
				HealthUpdate.maxHealth = MaxHP;
				MOB->OnReceiveMobHealthUpdate(HealthUpdate);
			}
			else
			{
				UE_LOG(LogGameplay, Warning, TEXT("MOBManager: Mob UID %d not found in registry for mobHealthUpdate"), MobUID);
			}
		}
	}
	else if (EventType == "mobTargetLost" && Status == "success")
	{
		UE_LOG(LogGameplay, Warning, TEXT("MOBManager: Received mob target lost event"));

		if (!worldContext || !worldContext->IsValidLowLevel())
		{
			UE_LOG(LogGameplay, Error, TEXT("MOBManager: Cannot process mobTargetLost - no valid world context"));
			return;
		}

		FMobTargetLostStruct TargetLostData = JSONParser::DeserializeMobTargetLost(Body);

		UE_LOG(LogGameplay, Warning, TEXT("Mob %d (UID: %d) lost target player %d at position (%.2f, %.2f, %.2f)"),
			TargetLostData.mobId, TargetLostData.mobUID, TargetLostData.lostTargetPlayerId,
			TargetLostData.position.positionX, TargetLostData.position.positionY, TargetLostData.position.positionZ);

		if (ABasicMOB* MOB = Cast<ABasicMOB>(FindMobActor(TargetLostData.mobUID)))
		{
						// ���������� ���� ����
						MOB->SetMobTargetId(0);
						MOB->SetMobTargetType("");
						MOB->SetMOBIsAggressive(false);

						// ��������� ������� ����
						MOB->OnReceiveServerPacket(TargetLostData.position);

						// Notify the mob (animation transition, delegate broadcast)
						MOB->OnReceiveTargetLost();

						// If the local player had this mob locked, clear the lock target
						if (worldContext)
						{
							APlayerController* PC = worldContext->GetFirstPlayerController();
							if (PC)
							{
								ABasicPlayer* LocalPlayer = Cast<ABasicPlayer>(PC->GetPawn());
								if (LocalPlayer && LocalPlayer->GetLockedTarget() == MOB)
								{
									LocalPlayer->ClearLockedTarget();
									UE_LOG(LogGameplay, Verbose, TEXT("MOBManager: Cleared player lock target � mob %d returned to zone"),
										TargetLostData.mobUID);
								}
							}
						}

						UE_LOG(LogGameplay, Warning, TEXT("MOB %s (UID: %d) target cleared and position updated"),
							*MOB->GetMobName(), TargetLostData.mobUID);
		}
		else
		{
			UE_LOG(LogGameplay, Warning, TEXT("Mob with UID %d not found for target lost event"), TargetLostData.mobUID);
		}
	}
	else if (EventType == "effectTick")
	{
		// Route effectTick to the target mob so its HP bar and visual feedback updates.
		// Player effectTick is already handled by PlayerStatsNetworkHandler; here we only
		// forward to mobs whose characterId matches a mob UID in the registry.
		if (!worldContext || !worldContext->IsValidLowLevel()) return;

		FEffectTickData TickData = JSONParser::DeserializeEffectTick(ReceivedData);
		if (TickData.characterId > 0)
		{
			TWeakObjectPtr<ABasicMOB>* FoundWeak = MobActorRegistry.Find(TickData.characterId);
			if (FoundWeak && FoundWeak->IsValid())
			{
				FoundWeak->Get()->OnReceiveEffectTick(TickData);
			}
		}
	}
	else if (EventType == "corpseRemoved")
	{
		if (!worldContext || !worldContext->IsValidLowLevel())
		{
			UE_LOG(LogGameplay, Error, TEXT("MOBManager: Cannot process corpseRemoved - no valid world context"));
			return;
		}

		if (!Body.IsValid())
		{
			UE_LOG(LogGameplay, Error, TEXT("MOBManager: corpseRemoved packet has no body"));
			return;
		}

		int32 CorpseUID = 0;
		if (!Body->TryGetNumberField(TEXT("corpseUID"), CorpseUID) || CorpseUID <= 0)
		{
			UE_LOG(LogGameplay, Error, TEXT("MOBManager: corpseRemoved - missing or invalid corpseUID"));
			return;
		}

		TWeakObjectPtr<ABasicMOB>* FoundWeak = MobActorRegistry.Find(CorpseUID);
		if (!FoundWeak || !FoundWeak->IsValid())
		{
			UE_LOG(LogGameplay, Warning, TEXT("MOBManager: corpseRemoved for UID %d - actor not found (already cleaned up?)"), CorpseUID);
			return;
		}

		ABasicMOB* MOB = FoundWeak->Get();
		if (!IsValid(MOB))
		{
			UE_LOG(LogGameplay, Warning, TEXT("MOBManager: corpseRemoved for UID %d - actor is pending kill"), CorpseUID);
			return;
		}

		if (!MOB->GetMOBIsDead())
		{
			UE_LOG(LogGameplay, Warning,
				TEXT("MOBManager: corpseRemoved for UID %d (%s) - mob is NOT marked dead on client. Server-authoritative removal applied."),
				CorpseUID, *MOB->GetMobName());
		}
		else
		{
			UE_LOG(LogGameplay, Verbose, TEXT("MOBManager: Removing corpse UID %d (%s)"), CorpseUID, *MOB->GetMobName());
		}

		ClearLockedTargetOnAllPlayers(MOB);

		MOB->Destroy();
		RemovedCorpseUIDs.Add(CorpseUID);
	}
	else if (EventType == "mobCellLeft")
	{
		// Interest v2 evict: server lists UIDs standing in cells we just left.
		// Over-inclusive by design — unknown UIDs are dropped silently (no crash).
		if (!worldContext || !worldContext->IsValidLowLevel())
		{
			UE_LOG(LogGameplay, Error, TEXT("MOBManager: Cannot process mobCellLeft - no valid world context"));
			return;
		}

		if (!Body.IsValid())
		{
			UE_LOG(LogGameplay, Error, TEXT("MOBManager: mobCellLeft packet has no body"));
			return;
		}

		const TArray<TSharedPtr<FJsonValue>>* UidsArray = nullptr;
		if (!Body->TryGetArrayField(TEXT("uids"), UidsArray) || UidsArray == nullptr)
		{
			UE_LOG(LogNetPacket, Warning, TEXT("MOBManager: mobCellLeft - missing uids array"));
			return;
		}

		for (const TSharedPtr<FJsonValue>& UidValue : *UidsArray)
		{
			int32 EvictUID = 0;
			if (UidValue.IsValid())
			{
				double RawUid = 0.0;
				if (UidValue->TryGetNumber(RawUid))
				{
					EvictUID = static_cast<int32>(RawUid);
				}
			}
			if (EvictUID <= 0) continue;

			TWeakObjectPtr<ABasicMOB>* FoundWeak = MobActorRegistry.Find(EvictUID);
			if (!FoundWeak || !FoundWeak->IsValid()) continue;

			ABasicMOB* MOB = FoundWeak->Get();
			if (!IsValid(MOB)) continue;

			UE_LOG(LogGameplay, Verbose, TEXT("MOBManager: mobCellLeft evicting UID %d (%s)"), EvictUID, *MOB->GetMobName());

			ClearLockedTargetOnAllPlayers(MOB);

			MOB->Destroy();
		}
	}
	else if (EventType == "world_notification")
	{
		// Champion despawn arrives wrapped in world_notification (channel screen_center).
		// Only the actor removal is handled here; the visual toast is routed by WorldNotificationManager.
		if (!Body.IsValid()) return;

		FString NotificationType;
		if (!Body->TryGetStringField(TEXT("notificationType"), NotificationType)) return;
		if (NotificationType != TEXT("champion_despawned")) return;

		if (!worldContext || !worldContext->IsValidLowLevel()) return;

		int32 ChampionUID = 0;
		FString ChampionSlug;
		const TSharedPtr<FJsonObject>* DataPtr = nullptr;
		if (Body->TryGetObjectField(TEXT("data"), DataPtr) && DataPtr && (*DataPtr).IsValid())
		{
			(*DataPtr)->TryGetNumberField(TEXT("uid"), ChampionUID);
			(*DataPtr)->TryGetStringField(TEXT("mobSlug"), ChampionSlug);
		}

		ABasicMOB* Target = nullptr;
		if (ChampionUID > 0)
		{
			TWeakObjectPtr<ABasicMOB>* FoundWeak = MobActorRegistry.Find(ChampionUID);
			if (!FoundWeak || !FoundWeak->IsValid() || !IsValid(FoundWeak->Get()))
			{
				UE_LOG(LogGameplay, Warning, TEXT("MOBManager: champion_despawned for UID %d - actor not found (already cleaned up?)"), ChampionUID);
				return;
			}
			Target = FoundWeak->Get();
		}
		else if (!ChampionSlug.IsEmpty())
		{
			for (const auto& Pair : MobActorRegistry)
			{
				if (Pair.Value.IsValid() && IsValid(Pair.Value.Get())
					&& Pair.Value.Get()->GetMOBData().mobSlug == ChampionSlug)
				{
					Target = Pair.Value.Get();
					break;
				}
			}
			if (!Target)
			{
				UE_LOG(LogGameplay, Warning, TEXT("MOBManager: champion_despawned for slug %s - actor not found (already cleaned up?)"), *ChampionSlug);
				return;
			}
		}
		else
		{
			UE_LOG(LogGameplay, Error, TEXT("MOBManager: champion_despawned - missing both uid and mobSlug"));
			return;
		}

		UE_LOG(LogGameplay, Warning, TEXT("MOBManager: champion_despawned removing %s (uid=%d)"), *Target->GetMobName(), ChampionUID);

		ClearLockedTargetOnAllPlayers(Target);

		Target->Destroy();
	}
}

// Send join game request
