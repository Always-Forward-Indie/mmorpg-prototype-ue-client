// Fill out your copyright notice in the Description page of Project Settings.


#include "Utils/JSONParser.h"


// serialize a json object to a string
 FString JSONParser::SerializeJson(const FString& EventType, const TMap<FString, TSharedPtr<FJsonValue>>& HeaderData, const TMap<FString, TSharedPtr<FJsonValue>>& BodyData)
{
    TSharedPtr<FJsonObject> HeaderObject = MakeShareable(new FJsonObject);
    HeaderObject->SetStringField("eventType", EventType);

    for (const auto& Elem : HeaderData)
    {
        HeaderObject->SetField(Elem.Key, Elem.Value);
    }

    TSharedPtr<FJsonObject> BodyObject = MakeShareable(new FJsonObject);

    for (const auto& Elem : BodyData)
    {
        BodyObject->SetField(Elem.Key, Elem.Value);
	}

    TSharedPtr<FJsonObject> MainJsonObject = MakeShareable(new FJsonObject);

    if (HeaderObject->Values.Num() > 0)
    {
        MainJsonObject->SetObjectField("header", HeaderObject);
    }

    if (BodyObject->Values.Num() > 0)
    {
        MainJsonObject->SetObjectField("body", BodyObject);
    }

    FString OutputString;
	TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&OutputString);
    FJsonSerializer::Serialize(MainJsonObject.ToSharedRef(), Writer);

	// Удалим все символы перевода строки
	OutputString.ReplaceInline(TEXT("\n"), TEXT(""));
	OutputString.ReplaceInline(TEXT("\r"), TEXT(""));

    return OutputString;
}


 // Universal: Parse Position from JSON object
 FPositionDataStruct JSONParser::DeserializePositionData(const TSharedPtr<FJsonObject>& PosObj)
 {
	 FPositionDataStruct Pos;
	 if (!PosObj.IsValid()) return Pos;
	 Pos.positionX = PosObj->GetNumberField("x");
	 Pos.positionY = PosObj->GetNumberField("y");
	 Pos.positionZ = PosObj->GetNumberField("z");
	 Pos.rotationZ = PosObj->GetNumberField("rotationZ");
	 return Pos;
 }

 // Universal: Parse Attributes from array
 TMap<FString, FAttributeDataStruct> JSONParser::DeserializeAttributesArray(const TArray<TSharedPtr<FJsonValue>>& JsonArray)
 {
	 TMap<FString, FAttributeDataStruct> Result;
	 for (const TSharedPtr<FJsonValue>& Value : JsonArray)
	 {
		 const TSharedPtr<FJsonObject> AttrObj = Value->AsObject();
		 if (!AttrObj.IsValid()) continue;
		 FAttributeDataStruct Attr;
		 Attr.attributeId = AttrObj->GetIntegerField("id");
		 Attr.attributeName = AttrObj->GetStringField("name");
		 Attr.attributeSlug = AttrObj->GetStringField("slug");
		 Attr.attributeValue = AttrObj->GetIntegerField("value");
		 if (!Attr.attributeSlug.IsEmpty())
		 {
			 Result.Add(Attr.attributeSlug, Attr);
		 }
	 }
	 return Result;
 }

 // Internal reusable function for character parsing
 FCharacterDataStruct JSONParser::DeserializeCharacterData(const TSharedPtr<FJsonObject>& CD)
 {
	 FCharacterDataStruct Character;
	 if (!CD.IsValid()) return Character;
	 Character.characterId = CD->GetIntegerField("id");
	 Character.characterName = CD->GetStringField("name");
	 Character.characterClass = CD->GetStringField("class");
	 Character.characterRace = CD->GetStringField("race");
	 Character.characterLevel = CD->GetIntegerField("level");

	 //is dead
	 if (CD->HasField("isDead")) {
		 Character.bIsDead = CD->GetBoolField("isDead");
	 } else {
		 Character.bIsDead = false; // Default value if not present
	 }

	 if (CD->HasField("exp")) {
		 TSharedPtr<FJsonObject> Exp = CD->GetObjectField("exp");
		 Character.characterExperiencePoints = Exp->GetIntegerField("current");
		 Character.characterExpForNextLevel = Exp->GetIntegerField("nextLevel");
	 }

	 if (CD->HasField("stats")) {
		 TSharedPtr<FJsonObject> Stats = CD->GetObjectField("stats");
		 TSharedPtr<FJsonObject> Health = Stats->GetObjectField("health");
		 TSharedPtr<FJsonObject> Mana = Stats->GetObjectField("mana");
		 Character.characterCurrentHealth = Health->GetIntegerField("current");
		 Character.characterCurrentMana = Mana->GetIntegerField("current");
	 }

	 if (CD->HasField("position"))
		 Character.characterPosition = JSONParser::DeserializePositionData(CD->GetObjectField("position"));

	 if (CD->HasField("attributes"))
		 Character.characterAttributes.attributesData = JSONParser::DeserializeAttributesArray(CD->GetArrayField("attributes"));

	 return Character;
 }

 // Entry point for string-based JSON input
 FCharacterDataStruct JSONParser::DeserializeCharacterData(const FString& JsonString)
 {
	 TSharedPtr<FJsonObject> Root;
	 TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	 if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()) return FCharacterDataStruct();
	 TSharedPtr<FJsonObject> Body = Root->GetObjectField("body");
	 return JSONParser::DeserializeCharacterData(Body->GetObjectField("character"));
 }


 TArray<FClientDataStruct> JSONParser::DeserializeCharactersList(const TSharedPtr<FJsonObject>& Body)
 {
	 TArray<FClientDataStruct> Result;
	 if (!Body.IsValid()) return Result;

	 const TArray<TSharedPtr<FJsonValue>>* CharactersArray = nullptr;
	 if (Body->TryGetArrayField(TEXT("characters"), CharactersArray) && CharactersArray != nullptr)
	 {
		 for (const TSharedPtr<FJsonValue>& Value : *CharactersArray)
		 {
			 TSharedPtr<FJsonObject> CharacterObj = Value->AsObject();
			 if (CharacterObj.IsValid())
			 {
				 FClientDataStruct ClientData;
				 ClientData.clientId = CharacterObj->GetIntegerField(TEXT("clientId"));
				 ClientData.characterData = JSONParser::DeserializeCharacterData(CharacterObj->GetObjectField(TEXT("character")));
				 Result.Add(ClientData);
			 }
		 }
	 }

	 return Result;
 }


 // deserialize ClientData from a json string
FClientDataStruct JSONParser::DeserializeClientData(const FString& JsonString)
 {
	 TSharedPtr<FJsonObject> JsonObject;
	 FClientDataStruct ClientData;

	 // Convert the string to a JSON object
	 TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	 if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
	 {
		 const TSharedPtr<FJsonObject>* HeaderObject = nullptr;
		 if (JsonObject->TryGetObjectField(TEXT("header"), HeaderObject) && HeaderObject != nullptr)
		 {
			 if ((*HeaderObject)->HasField(TEXT("clientId")))
			 {
				 ClientData.clientId = (*HeaderObject)->GetIntegerField(TEXT("clientId"));
			 }

			 if ((*HeaderObject)->HasField(TEXT("hash")))
			 {
				 ClientData.hash = (*HeaderObject)->GetStringField(TEXT("hash"));
			 }
		 }
	 }

	 return ClientData;
 }

 // deserialize MessageData from a json string
 FMessageDataStruct JSONParser::DeserializeMessageData(const FString& JsonString)
 {
	 TSharedPtr<FJsonObject> JsonObject;
	 FMessageDataStruct MessageData;

	 // Convert the string to a JSON object
	 TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	 if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
	 {
		 const TSharedPtr<FJsonObject>* HeaderObject = nullptr;
		 if (JsonObject->TryGetObjectField(TEXT("header"), HeaderObject) && HeaderObject != nullptr)
		 {
			 if ((*HeaderObject)->HasField(TEXT("status")))
			 {
				 MessageData.status = (*HeaderObject)->GetStringField(TEXT("status"));
			 }

			 if ((*HeaderObject)->HasField(TEXT("message")))
			 {
				 MessageData.message = (*HeaderObject)->GetStringField(TEXT("message"));
			 }

			 if ((*HeaderObject)->HasField(TEXT("eventType")))
			 {
				 MessageData.eventType = (*HeaderObject)->GetStringField(TEXT("eventType"));
			 }

			 if ((*HeaderObject)->HasField(TEXT("timestamp")))
			 {
				 MessageData.timestamp = (*HeaderObject)->GetStringField(TEXT("timestamp"));
			 }
		 }
	 }

	 return MessageData;
 }

// deserialize EventType from a json string
 FString JSONParser::DeserializeEventTypeData(const FString& JsonString)
 {
	 TSharedPtr<FJsonObject> JsonObject;
	 FString EventType;

	 TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	 if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
	 {
		 const TSharedPtr<FJsonObject>* HeaderObject = nullptr;
		 if (JsonObject->TryGetObjectField(TEXT("header"), HeaderObject) && HeaderObject != nullptr)
		 {
			 if ((*HeaderObject)->HasField(TEXT("eventType")))
			 {
				 EventType = (*HeaderObject)->GetStringField(TEXT("eventType"));
			 }
		 }
	 }
	 return EventType;
 }

//deserialize a JSON containing a list of characters
TArray<FCharacterDataStruct> JSONParser::DeserializeLoginCharactersList(const FString& JsonString)
{
	// create an array of characters
	TArray<FCharacterDataStruct> CharacterList;
	TSharedPtr<FJsonObject> JsonObject;

	// Convert the string to a JSON object
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
	{
		const TSharedPtr<FJsonObject>* BodyObject = nullptr;
		if (JsonObject->TryGetObjectField(TEXT("body"), BodyObject) && BodyObject != nullptr)
		{
			// get the array of characters
			const TArray<TSharedPtr<FJsonValue>>* CharactersArray = nullptr;
			if ((*BodyObject)->TryGetArrayField(TEXT("charactersList"), CharactersArray) && CharactersArray != nullptr)
			{
				// iterate through the array of characters
				for (const TSharedPtr<FJsonValue>& CharacterValue : *CharactersArray)
				{
					// create character data
					FCharacterDataStruct CharacterData;
					TSharedPtr<FJsonObject> CharacterObject = CharacterValue->AsObject();

					if (CharacterObject->HasField(TEXT("characterId")))
					{
						CharacterData.characterId = CharacterObject->GetIntegerField(TEXT("characterId"));
					}

					if (CharacterObject->HasField(TEXT("characterName")))
					{
						CharacterData.characterName = CharacterObject->GetStringField(TEXT("characterName"));
					}

					if (CharacterObject->HasField(TEXT("characterClass")))
					{
						CharacterData.characterClass = CharacterObject->GetStringField(TEXT("characterClass"));
					}

					if (CharacterObject->HasField(TEXT("characterRace")))
					{
						CharacterData.characterRace = CharacterObject->GetStringField(TEXT("characterRace"));
					}

					if (CharacterObject->HasField(TEXT("characterLevel")))
					{
						CharacterData.characterLevel = CharacterObject->GetIntegerField(TEXT("characterLevel"));
					}

					if (CharacterObject->HasField(TEXT("characterExp")))
					{
						CharacterData.characterExperiencePoints = CharacterObject->GetIntegerField(TEXT("characterExp"));
					}

					if (CharacterObject->HasField(TEXT("characterCurrentHealth")))
					{
						CharacterData.characterCurrentHealth = CharacterObject->GetIntegerField(TEXT("characterCurrentHealth"));
					}

					if (CharacterObject->HasField(TEXT("characterCurrentMana")))
					{
						CharacterData.characterCurrentMana = CharacterObject->GetIntegerField(TEXT("characterCurrentMana"));
					}

					//is dead
					if (CharacterObject->HasField(TEXT("isDead")))
					{
						CharacterData.bIsDead = CharacterObject->GetBoolField(TEXT("isDead"));
					}

					CharacterList.Add(CharacterData);
				}
			}
		}
	}

	return CharacterList;
}

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

	SpawnZoneData.zoneID = SpawnZoneObject->GetIntegerField("id");
	SpawnZoneData.zoneName = SpawnZoneObject->GetStringField("name");
	SpawnZoneData.MobIDToSpawn = SpawnZoneObject->GetIntegerField("spawnMobId");
	SpawnZoneData.MaxMobs = SpawnZoneObject->GetIntegerField("maxSpawnCount");
	SpawnZoneData.currentMobsCount = SpawnZoneObject->GetIntegerField("spawnedMobsCount");
	SpawnZoneData.respawnTime = SpawnZoneObject->GetIntegerField("respawnTime");
	SpawnZoneData.bSpawningEnabled = SpawnZoneObject->GetBoolField("spawnEnabled");

	if (SpawnZoneObject->HasField("bounds"))
	{
		const TSharedPtr<FJsonObject> Bounds = SpawnZoneObject->GetObjectField("bounds");

		SpawnZoneData.spawnStartPos = FVector(
			Bounds->GetNumberField("minX"),
			Bounds->GetNumberField("minY"),
			Bounds->GetNumberField("minZ"));

		SpawnZoneData.spawnSize = FVector(
			Bounds->GetNumberField("maxX"),
			Bounds->GetNumberField("maxY"),
			Bounds->GetNumberField("maxZ"));
	}

	return SpawnZoneData;
}

// deserialize a JSON containing a list of mobs
TArray<FMOBStruct> JSONParser::DeserializeMobsList(const TSharedPtr<FJsonObject>& Body)
{
	TArray<FMOBStruct> MobsList;
	const TArray<TSharedPtr<FJsonValue>>* MobsArray;

	if (Body->TryGetArrayField("mobs", MobsArray))
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

FMOBStruct JSONParser::DeserializeMobData(const TSharedPtr<FJsonObject>& MobObject)
{
	FMOBStruct Mob;
	Mob.mobID = MobObject->GetIntegerField("id");
	Mob.mobUniqueID = FString::FromInt(MobObject->GetIntegerField("uid"));
	Mob.mobZoneID = MobObject->GetIntegerField("zoneId");
	Mob.mobName = MobObject->GetStringField("name");
	Mob.mobSlug = MobObject->GetStringField("slug");
	Mob.mobRace = MobObject->GetStringField("race");
	Mob.mobLevel = MobObject->GetIntegerField("level");
	Mob.bIsAggressive = MobObject->GetBoolField("isAggressive");
	Mob.bIsDead = MobObject->GetBoolField("isDead");


	Mob.mobPosition = JSONParser::DeserializePositionData(MobObject->GetObjectField("position"));

	if (MobObject->HasField("stats")) {
		TSharedPtr<FJsonObject> Stats = MobObject->GetObjectField("stats");
		TSharedPtr<FJsonObject> Health = Stats->GetObjectField("health");
		TSharedPtr<FJsonObject> Mana = Stats->GetObjectField("mana");
		Mob.mobCurrentHealth = Health->GetIntegerField("current");
		Mob.mobCurrentMana = Mana->GetIntegerField("current");
	}

	if (MobObject->HasField("attributes"))
	{
		Mob.mobAttributes.attributesData = JSONParser::DeserializeAttributesArray(MobObject->GetArrayField("attributes"));
	}

	return Mob;
}


//combat 

FCombatAnimationData JSONParser::DeserializeCombatAnimation(const TSharedPtr<FJsonObject>& AnimationObj)
{
	FCombatAnimationData Data;
	if (!AnimationObj.IsValid()) return Data;

	Data.AnimationName = AnimationObj->GetStringField("animationName");
	Data.CharacterId = AnimationObj->GetIntegerField("characterId");
	Data.Duration = AnimationObj->GetNumberField("duration");
	Data.bIsLooping = AnimationObj->GetBoolField("isLooping");
	Data.Position = JSONParser::DeserializePositionData(AnimationObj->GetObjectField("position"));
	Data.TargetPosition = JSONParser::DeserializePositionData(AnimationObj->GetObjectField("targetPosition"));
	return Data;
}

FCombatActionData JSONParser::DeserializeCombatAction(const TSharedPtr<FJsonObject>& ActionObj)
{
	FCombatActionData Data;
	if (!ActionObj.IsValid()) return Data;

	Data.ActionId = ActionObj->GetIntegerField("actionId");
	Data.ActionName = ActionObj->GetStringField("actionName");
	Data.ActionType = ActionObj->GetIntegerField("actionType");
	Data.AnimationName = ActionObj->GetStringField("animationName");
	Data.CastTime = ActionObj->GetNumberField("castTime");
	Data.CasterId = ActionObj->GetIntegerField("casterId");
	Data.Damage = ActionObj->GetIntegerField("damage");
	Data.Range = ActionObj->GetNumberField("range");
	Data.State = ActionObj->GetIntegerField("state");
	Data.TargetId = ActionObj->GetIntegerField("targetId");
	Data.TargetPosition = JSONParser::DeserializePositionData(ActionObj->GetObjectField("targetPosition"));
	Data.TargetType = ActionObj->GetIntegerField("targetType");

	// Parse targetTypeString if available
	if (ActionObj->HasField("targetTypeString"))
	{
		Data.TargetTypeString = ActionObj->GetStringField("targetTypeString");
	}


	return Data;
}

FCombatResultData JSONParser::DeserializeCombatResult(const TSharedPtr<FJsonObject>& ResultObj)
{
	FCombatResultData Data;
	if (!ResultObj.IsValid()) return Data;

	Data.ActionId = ResultObj->GetIntegerField("actionId");
	Data.CasterId = ResultObj->GetIntegerField("casterId");
	Data.DamageDealt = ResultObj->GetIntegerField("damageDealt");
	Data.HealingDone = ResultObj->GetIntegerField("healingDone");
	Data.bIsBlocked = ResultObj->GetBoolField("isBlocked");
	Data.bIsCritical = ResultObj->GetBoolField("isCritical");
	Data.bIsDodged = ResultObj->GetBoolField("isDodged");
	Data.bIsResisted = ResultObj->GetBoolField("isResisted");
	Data.RemainingHealth = ResultObj->GetIntegerField("remainingHealth");
	Data.RemainingMana = ResultObj->GetIntegerField("remainingMana");
	Data.bTargetDied = ResultObj->GetBoolField("targetDied");
	Data.bIsDamaged = ResultObj->GetBoolField("isDamaged");
	Data.TargetId = ResultObj->GetIntegerField("targetId");
	Data.TargetType = ResultObj->GetIntegerField("targetType");

	// Parse targetTypeString if available
	if (ResultObj->HasField("targetTypeString"))
	{
		Data.TargetTypeString = ResultObj->GetStringField("targetTypeString");
	}

	return Data;
}

