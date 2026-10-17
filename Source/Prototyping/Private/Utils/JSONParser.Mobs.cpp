// Fill out your copyright notice in the Description page of Project Settings.


#include "Utils/JSONParser.h"
#include "Services/TimeSyncService.h"
#include "MyGameInstance.h"
#include "Engine/World.h"
#include "Utils/PlayerAttributeParser.h"
#include "Utils/NetCore/InventoryRules.h"

//deserialize a JSON containing a list of spawn zones
TArray<FSpawnZoneStruct> JSONParser::DeserializeSpawnZonesList(const TSharedPtr<FJsonObject>& Body)
{
	TArray<FSpawnZoneStruct> SpawnZonesList;
	const TArray<TSharedPtr<FJsonValue>>* SpawnZonesArray;

	if (Body.IsValid() && Body->TryGetArrayField(TEXT("spawnZones"), SpawnZonesArray))
	{
		for (const TSharedPtr<FJsonValue>& SpawnZoneValue : *SpawnZonesArray)
		{
			TSharedPtr<FJsonObject> SpawnZoneObject = SpawnZoneValue->AsObject();
			if (SpawnZoneObject.IsValid())
			{
				SpawnZonesList.Add(JSONParser::DeserializeSpawnZoneData(SpawnZoneObject));
			}
		}
	}

	return SpawnZonesList;
}


FSpawnZoneStruct JSONParser::DeserializeSpawnZoneData(const TSharedPtr<FJsonObject>& SpawnZoneObject)
{
	FSpawnZoneStruct SpawnZoneData;

	if (!SpawnZoneObject.IsValid()) return SpawnZoneData;

	SpawnZoneData.zoneID = SpawnZoneObject->GetIntegerField(TEXT("id"));
	SpawnZoneData.zoneName = SpawnZoneObject->GetStringField(TEXT("name"));
	SpawnZoneData.MobIDToSpawn = SpawnZoneObject->GetIntegerField(TEXT("spawnMobId"));
	SpawnZoneData.MaxMobs = SpawnZoneObject->GetIntegerField(TEXT("maxSpawnCount"));
	SpawnZoneData.currentMobsCount = SpawnZoneObject->GetIntegerField(TEXT("spawnedMobsCount"));
	SpawnZoneData.respawnTime = SpawnZoneObject->GetIntegerField(TEXT("respawnTime"));
	SpawnZoneData.bSpawningEnabled = SpawnZoneObject->GetBoolField(TEXT("spawnEnabled"));

	if (SpawnZoneObject->HasField(TEXT("bounds")))
	{
		const TSharedPtr<FJsonObject> Bounds = SpawnZoneObject->GetObjectField(TEXT("bounds"));

		const float MinX = (float)Bounds->GetNumberField(TEXT("minX"));
		const float MinY = (float)Bounds->GetNumberField(TEXT("minY"));
		const float MinZ = (float)Bounds->GetNumberField(TEXT("minZ"));
		const float MaxX = (float)Bounds->GetNumberField(TEXT("maxX"));
		const float MaxY = (float)Bounds->GetNumberField(TEXT("maxY"));
		const float MaxZ = (float)Bounds->GetNumberField(TEXT("maxZ"));

		// spawnStartPos = world position of the bottom-center of the zone
		SpawnZoneData.spawnStartPos = FVector(
			(MinX + MaxX) * 0.5f,
			(MinY + MaxY) * 0.5f,
			MinZ);

		// spawnSize = full extents of the zone (used as BoxExtent * 2 in ChangeSpawnZoneSize)
		SpawnZoneData.spawnSize = FVector(
			MaxX - MinX,
			MaxY - MinY,
			MaxZ - MinZ);
	}

	return SpawnZoneData;
}

// deserialize a JSON containing a list of mobs
TArray<FMOBStruct> JSONParser::DeserializeMobsList(const TSharedPtr<FJsonObject>& Body)
{
	TArray<FMOBStruct> MobsList;
	if (!Body.IsValid()) return MobsList;

	const TArray<TSharedPtr<FJsonValue>>* MobsArray = nullptr;
	if (!Body->TryGetArrayField(TEXT("mobs"), MobsArray) || MobsArray == nullptr)
	{
		// Fallback: legacy nested spawnZone format (body.spawnZone.mobs).
		// Current flat format is body.zoneId + body.mobs[].
		const TSharedPtr<FJsonObject>* SpawnZonePtr = nullptr;
		if (Body->TryGetObjectField(TEXT("spawnZone"), SpawnZonePtr) && SpawnZonePtr != nullptr && (*SpawnZonePtr).IsValid())
		{
			(*SpawnZonePtr)->TryGetArrayField(TEXT("mobs"), MobsArray);
		}
	}

	if (MobsArray != nullptr)
	{
		for (const TSharedPtr<FJsonValue>& MobValue : *MobsArray)
		{
			TSharedPtr<FJsonObject> MobObject = MobValue->AsObject();
			if (MobObject.IsValid())
			{
				MobsList.Add(JSONParser::DeserializeMobData(MobObject));
			}
		}
	}
	return MobsList;
}

// Parse a mobMoveUpdate packet body into lightweight per-mob move entries
TArray<JSONParser::FMobMovePacketEntry> JSONParser::DeserializeMobMoveUpdate(const TSharedPtr<FJsonObject>& Body, int64 ServerSendMs)
{
	TArray<FMobMovePacketEntry> Result;
	if (!Body.IsValid()) return Result;

	const TArray<TSharedPtr<FJsonValue>>* MobsArray = nullptr;
	if (!Body->TryGetArrayField(TEXT("mobs"), MobsArray) || !MobsArray) return Result;

	for (const TSharedPtr<FJsonValue>& MobValue : *MobsArray)
	{
		TSharedPtr<FJsonObject> Obj = MobValue->AsObject();
		if (!Obj.IsValid()) continue;

		FMobMovePacketEntry Entry;
		Entry.uid = Obj->GetIntegerField(TEXT("uid"));

		// position
		const TSharedPtr<FJsonObject>* PosObjPtr = nullptr;
		if (Obj->TryGetObjectField(TEXT("position"), PosObjPtr) && PosObjPtr)
		{
			Entry.moveEntry.position.positionX = (*PosObjPtr)->GetNumberField(TEXT("x"));
			Entry.moveEntry.position.positionY = (*PosObjPtr)->GetNumberField(TEXT("y"));
			Entry.moveEntry.position.positionZ = (*PosObjPtr)->GetNumberField(TEXT("z"));
			Entry.moveEntry.position.rotationZ = (*PosObjPtr)->GetNumberField(TEXT("rotationZ"));
		}

		// velocity
		const TSharedPtr<FJsonObject>* VelObjPtr = nullptr;
		if (Obj->TryGetObjectField(TEXT("velocity"), VelObjPtr) && VelObjPtr)
		{
			Entry.moveEntry.velocityX = static_cast<float>((*VelObjPtr)->GetNumberField(TEXT("dirX")));
			Entry.moveEntry.velocityY = static_cast<float>((*VelObjPtr)->GetNumberField(TEXT("dirY")));
			Entry.moveEntry.speed     = static_cast<float>((*VelObjPtr)->GetNumberField(TEXT("speed")));
		}

		Entry.moveEntry.combatState     = Obj->GetIntegerField(TEXT("combatState"));
		double RawStepMs = 0.0;
		Obj->TryGetNumberField(TEXT("stepTimestampMs"), RawStepMs);
		Entry.moveEntry.stepTimestampMs = static_cast<int64>(RawStepMs);

		// optional waypoint
		const TSharedPtr<FJsonObject>* WpObjPtr = nullptr;
		if (Obj->TryGetObjectField(TEXT("waypoint"), WpObjPtr) && WpObjPtr)
		{
			Entry.moveEntry.waypointX    = static_cast<float>((*WpObjPtr)->GetNumberField(TEXT("x")));
			Entry.moveEntry.waypointY    = static_cast<float>((*WpObjPtr)->GetNumberField(TEXT("y")));
			Entry.moveEntry.bHasWaypoint = true;
		}

		Result.Add(Entry);
	}

	return Result;
}

FMOBStruct JSONParser::DeserializeMobData(const TSharedPtr<FJsonObject>& MobObject)
{
	FMOBStruct Mob;
	Mob.mobID = MobObject->GetIntegerField(TEXT("id"));
	Mob.mobUniqueID = FString::FromInt(MobObject->GetIntegerField(TEXT("uid")));
	Mob.mobZoneID = MobObject->GetIntegerField(TEXT("zoneId"));
	Mob.mobName = MobObject->GetStringField(TEXT("name"));
	Mob.mobSlug = MobObject->GetStringField(TEXT("slug"));
	Mob.mobRace = MobObject->GetStringField(TEXT("race"));
	Mob.mobLevel = MobObject->GetIntegerField(TEXT("level"));
	Mob.bIsAggressive = MobObject->GetBoolField(TEXT("isAggressive"));
	Mob.bIsDead = MobObject->GetBoolField(TEXT("isDead"));


	Mob.mobPosition = JSONParser::DeserializePositionData(MobObject->GetObjectField(TEXT("position")));

	if (MobObject->HasField(TEXT("stats"))) {
		TSharedPtr<FJsonObject> Stats = MobObject->GetObjectField(TEXT("stats"));
		TSharedPtr<FJsonObject> Health = Stats->GetObjectField(TEXT("health"));
		TSharedPtr<FJsonObject> Mana = Stats->GetObjectField(TEXT("mana"));
		Mob.mobCurrentHealth = Health->GetIntegerField(TEXT("current"));
		Mob.mobCurrentMana = Mana->GetIntegerField(TEXT("current"));
	}

	if (MobObject->HasField(TEXT("attributes")))
	{
		Mob.mobAttributes.attributesData = JSONParser::DeserializeAttributesArray(MobObject->GetArrayField(TEXT("attributes")));
	}

	// Parse velocity from spawn data (server sends current velocity for mobs
	// that are already moving when the client joins)
	const TSharedPtr<FJsonObject>* VelObjPtr = nullptr;
	if (MobObject->TryGetObjectField(TEXT("velocity"), VelObjPtr) && VelObjPtr)
	{
		Mob.mobVelocity.dirX  = static_cast<float>((*VelObjPtr)->GetNumberField(TEXT("dirX")));
		Mob.mobVelocity.dirY  = static_cast<float>((*VelObjPtr)->GetNumberField(TEXT("dirY")));
		Mob.mobVelocity.speed = static_cast<float>((*VelObjPtr)->GetNumberField(TEXT("speed")));
	}

	// Parse combatState from spawn data
	if (MobObject->HasField(TEXT("combatState")))
	{
		Mob.mobCombatState = MobObject->GetIntegerField(TEXT("combatState"));
	}

	return Mob;
}


FMobTargetLostStruct JSONParser::DeserializeMobTargetLost(const TSharedPtr<FJsonObject>& Body)
{
	FMobTargetLostStruct MobTargetLostData;
	if (!Body.IsValid()) return MobTargetLostData;

	if (Body->HasField(TEXT("lostTargetPlayerId")))
		MobTargetLostData.lostTargetPlayerId = Body->GetIntegerField(TEXT("lostTargetPlayerId"));

	if (Body->HasField(TEXT("mobId")))
		MobTargetLostData.mobId = Body->GetIntegerField(TEXT("mobId"));

	if (Body->HasField(TEXT("mobUID")))
		MobTargetLostData.mobUID = Body->GetIntegerField(TEXT("mobUID"));

	// Parse position data from individual fields
	if (Body->HasField(TEXT("positionX")) && Body->HasField(TEXT("positionY")) &&
		Body->HasField(TEXT("positionZ")) && Body->HasField(TEXT("rotationZ")))
	{
		MobTargetLostData.position.positionX = Body->GetNumberField(TEXT("positionX"));
		MobTargetLostData.position.positionY = Body->GetNumberField(TEXT("positionY"));
		MobTargetLostData.position.positionZ = Body->GetNumberField(TEXT("positionZ"));
		MobTargetLostData.position.rotationZ = Body->GetNumberField(TEXT("rotationZ"));
	}

	return MobTargetLostData;
}

// Parse mob target lost data from JSON string
FMobTargetLostStruct JSONParser::DeserializeMobTargetLost(const FString& JsonString)
{
	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		return FMobTargetLostStruct();

	TSharedPtr<FJsonObject> Body = Root->GetObjectField(TEXT("body"));
	if (!Body.IsValid())
		return FMobTargetLostStruct();

	return JSONParser::DeserializeMobTargetLost(Body);
}

// Inventory parsing functions
