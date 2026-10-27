// Fill out your copyright notice in the Description page of Project Settings.


#include "Utils/JSONParser.h"
#include "Data/DataStructs.h"
#include "Services/TimeSyncService.h"
#include "MyGameInstance.h"
#include "Engine/World.h"
#include "Utils/PlayerAttributeParser.h"
#include "Utils/NetCore/InventoryRules.h"

//deserialize a JSON containing a list of mobs
TArray<FMOBStruct> JSONParser::DeserializeMobsList(const TSharedPtr<FJsonObject>& Body)
{
	TArray<FMOBStruct> MobsList;
	if (!Body.IsValid()) return MobsList;

	const TArray<TSharedPtr<FJsonValue>>* MobsArray = nullptr;
	if (!Body->TryGetArrayField(TEXT("mobs"), MobsArray) || MobsArray == nullptr)
	{
		// No mobs array: empty. Chunk always sends the flat body.mobs[] shape.
		return MobsList;
	}

	if (MobsArray != nullptr)
	{
		for (const TSharedPtr<FJsonValue>& MobValue : *MobsArray)
		{
			// TryGetObject (not AsObject): wrong-typed entries skip quietly.
			const TSharedPtr<FJsonObject>* MobObjPtr = nullptr;
			if (!MobValue.IsValid() || !MobValue->TryGetObject(MobObjPtr) || !MobObjPtr || !(*MobObjPtr).IsValid())
			{
				continue;
			}
			MobsList.Add(JSONParser::DeserializeMobData(*MobObjPtr));
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
		const TSharedPtr<FJsonObject>* ObjPtr = nullptr;
		if (!MobValue.IsValid() || !MobValue->TryGetObject(ObjPtr) || !ObjPtr || !(*ObjPtr).IsValid()) continue;
		const TSharedPtr<FJsonObject>& Obj = *ObjPtr;

		FMobMovePacketEntry Entry;
		Obj->TryGetNumberField(TEXT("uid"), Entry.uid);

		// position
		const TSharedPtr<FJsonObject>* PosObjPtr = nullptr;
		if (Obj->TryGetObjectField(TEXT("position"), PosObjPtr) && PosObjPtr && (*PosObjPtr).IsValid())
		{
			(*PosObjPtr)->TryGetNumberField(TEXT("x"), Entry.moveEntry.position.positionX);
			(*PosObjPtr)->TryGetNumberField(TEXT("y"), Entry.moveEntry.position.positionY);
			(*PosObjPtr)->TryGetNumberField(TEXT("z"), Entry.moveEntry.position.positionZ);
			(*PosObjPtr)->TryGetNumberField(TEXT("rotationZ"), Entry.moveEntry.position.rotationZ);
		}

		// velocity
		const TSharedPtr<FJsonObject>* VelObjPtr = nullptr;
		if (Obj->TryGetObjectField(TEXT("velocity"), VelObjPtr) && VelObjPtr && (*VelObjPtr).IsValid())
		{
			double DirX = 0.0, DirY = 0.0, Speed = 0.0;
			(*VelObjPtr)->TryGetNumberField(TEXT("dirX"), DirX);
			(*VelObjPtr)->TryGetNumberField(TEXT("dirY"), DirY);
			(*VelObjPtr)->TryGetNumberField(TEXT("speed"), Speed);
			Entry.moveEntry.velocityX = static_cast<float>(DirX);
			Entry.moveEntry.velocityY = static_cast<float>(DirY);
			Entry.moveEntry.speed     = static_cast<float>(Speed);
		}

		Obj->TryGetNumberField(TEXT("combatState"), Entry.moveEntry.combatState);
		double RawStepMs = 0.0;
		Obj->TryGetNumberField(TEXT("stepTimestampMs"), RawStepMs);
		Entry.moveEntry.stepTimestampMs = static_cast<int64>(RawStepMs);

		// optional waypoint
		const TSharedPtr<FJsonObject>* WpObjPtr = nullptr;
		if (Obj->TryGetObjectField(TEXT("waypoint"), WpObjPtr) && WpObjPtr && (*WpObjPtr).IsValid())
		{
			double WpX = 0.0, WpY = 0.0;
			(*WpObjPtr)->TryGetNumberField(TEXT("x"), WpX);
			(*WpObjPtr)->TryGetNumberField(TEXT("y"), WpY);
			Entry.moveEntry.waypointX    = static_cast<float>(WpX);
			Entry.moveEntry.waypointY    = static_cast<float>(WpY);
			Entry.moveEntry.bHasWaypoint = true;
		}

		Result.Add(Entry);
	}

	return Result;
}

FMOBStruct JSONParser::DeserializeMobData(const TSharedPtr<FJsonObject>& MobObject)
{
	FMOBStruct Mob;
	if (!MobObject.IsValid()) return Mob;

	MobObject->TryGetNumberField(TEXT("id"), Mob.mobID);
	// Missing uid keeps the legacy effective default ("0"), quietly.
	int32 Uid = 0;
	MobObject->TryGetNumberField(TEXT("uid"), Uid);
	Mob.mobUniqueID = FString::FromInt(Uid);
	MobObject->TryGetNumberField(TEXT("zoneId"), Mob.mobZoneID);
	MobObject->TryGetStringField(TEXT("name"), Mob.mobName);
	MobObject->TryGetStringField(TEXT("slug"), Mob.mobSlug);
	MobObject->TryGetStringField(TEXT("race"), Mob.mobRace);
	MobObject->TryGetNumberField(TEXT("level"), Mob.mobLevel);
	MobObject->TryGetStringField(TEXT("rankCode"), Mob.mobRankCode);
	MobObject->TryGetBoolField(TEXT("isChampion"), Mob.bIsChampion);
	MobObject->TryGetStringField(TEXT("championKind"), Mob.championKind);
	MobObject->TryGetBoolField(TEXT("isAggressive"), Mob.bIsAggressive);
	MobObject->TryGetBoolField(TEXT("isDead"), Mob.bIsDead);


	const TSharedPtr<FJsonObject>* PositionPtr = nullptr;
	if (MobObject->TryGetObjectField(TEXT("position"), PositionPtr) && PositionPtr && (*PositionPtr).IsValid())
	{
		Mob.mobPosition = JSONParser::DeserializePositionData(*PositionPtr);
	}

	const TSharedPtr<FJsonObject>* StatsPtr = nullptr;
	if (MobObject->TryGetObjectField(TEXT("stats"), StatsPtr) && StatsPtr && (*StatsPtr).IsValid())
	{
		const TSharedPtr<FJsonObject>& Stats = *StatsPtr;
		const TSharedPtr<FJsonObject>* HealthPtr = nullptr;
		const TSharedPtr<FJsonObject>* ManaPtr = nullptr;
		if (Stats->TryGetObjectField(TEXT("health"), HealthPtr) && HealthPtr && (*HealthPtr).IsValid())
		{
			(*HealthPtr)->TryGetNumberField(TEXT("current"), Mob.mobCurrentHealth);
		}
		if (Stats->TryGetObjectField(TEXT("mana"), ManaPtr) && ManaPtr && (*ManaPtr).IsValid())
		{
			(*ManaPtr)->TryGetNumberField(TEXT("current"), Mob.mobCurrentMana);
		}
	}

	const TArray<TSharedPtr<FJsonValue>>* AttributesArray = nullptr;
	if (MobObject->TryGetArrayField(TEXT("attributes"), AttributesArray) && AttributesArray != nullptr)
	{
		Mob.mobAttributes.attributesData = JSONParser::DeserializeAttributesArray(*AttributesArray);
	}

	// Parse velocity from spawn data (server sends current velocity for mobs
	// that are already moving when the client joins)
	const TSharedPtr<FJsonObject>* VelObjPtr = nullptr;
	if (MobObject->TryGetObjectField(TEXT("velocity"), VelObjPtr) && VelObjPtr && (*VelObjPtr).IsValid())
	{
		double DirX = 0.0, DirY = 0.0, Speed = 0.0;
		(*VelObjPtr)->TryGetNumberField(TEXT("dirX"), DirX);
		(*VelObjPtr)->TryGetNumberField(TEXT("dirY"), DirY);
		(*VelObjPtr)->TryGetNumberField(TEXT("speed"), Speed);
		Mob.mobVelocity.dirX  = static_cast<float>(DirX);
		Mob.mobVelocity.dirY  = static_cast<float>(DirY);
		Mob.mobVelocity.speed = static_cast<float>(Speed);
	}

	// Parse combatState from spawn data
	MobObject->TryGetNumberField(TEXT("combatState"), Mob.mobCombatState);

	return Mob;
}


FMobTargetLostStruct JSONParser::DeserializeMobTargetLost(const TSharedPtr<FJsonObject>& Body)
{
	FMobTargetLostStruct MobTargetLostData;
	if (!Body.IsValid()) return MobTargetLostData;

	Body->TryGetNumberField(TEXT("lostTargetPlayerId"), MobTargetLostData.lostTargetPlayerId);
	Body->TryGetNumberField(TEXT("mobId"), MobTargetLostData.mobId);
	Body->TryGetNumberField(TEXT("mobUID"), MobTargetLostData.mobUID);

	// Parse position data from individual fields (each optional, quiet).
	Body->TryGetNumberField(TEXT("positionX"), MobTargetLostData.position.positionX);
	Body->TryGetNumberField(TEXT("positionY"), MobTargetLostData.position.positionY);
	Body->TryGetNumberField(TEXT("positionZ"), MobTargetLostData.position.positionZ);
	Body->TryGetNumberField(TEXT("rotationZ"), MobTargetLostData.position.rotationZ);

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
