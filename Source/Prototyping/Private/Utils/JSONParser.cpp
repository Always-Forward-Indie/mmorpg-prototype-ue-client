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

// Parse item attribute from JSON object
FItemAttributeStruct JSONParser::DeserializeItemAttribute(const TSharedPtr<FJsonObject>& AttributeObj)
{
	FItemAttributeStruct ItemAttribute;
	if (!AttributeObj.IsValid()) return ItemAttribute;

	if (AttributeObj->HasField("id"))
		ItemAttribute.id = AttributeObj->GetIntegerField("id");
	
	if (AttributeObj->HasField("name"))
		ItemAttribute.name = AttributeObj->GetStringField("name");
	
	if (AttributeObj->HasField("slug"))
		ItemAttribute.slug = AttributeObj->GetStringField("slug");
	
	if (AttributeObj->HasField("value"))
		ItemAttribute.value = AttributeObj->GetNumberField("value");

	return ItemAttribute;
}

// Parse item attributes array
TArray<FItemAttributeStruct> JSONParser::DeserializeItemAttributes(const TArray<TSharedPtr<FJsonValue>>& JsonArray)
{
	TArray<FItemAttributeStruct> Attributes;
	for (const TSharedPtr<FJsonValue>& Value : JsonArray)
	{
		const TSharedPtr<FJsonObject> AttrObj = Value->AsObject();
		if (AttrObj.IsValid())
		{
			Attributes.Add(JSONParser::DeserializeItemAttribute(AttrObj));
		}
	}
	return Attributes;
}

// Parse item data from JSON object
FItemBaseStruct JSONParser::DeserializeItemData(const TSharedPtr<FJsonObject>& ItemObj)
{
	FItemBaseStruct Item;
	if (!ItemObj.IsValid()) return Item;

	if (ItemObj->HasField("id"))
		Item.id = ItemObj->GetIntegerField("id");
	
	if (ItemObj->HasField("name"))
		Item.name = ItemObj->GetStringField("name");
	
	if (ItemObj->HasField("slug"))
		Item.slug = ItemObj->GetStringField("slug");
	
	if (ItemObj->HasField("description"))
		Item.description = ItemObj->GetStringField("description");
	
	if (ItemObj->HasField("isQuestItem"))
		Item.isQuestItem = ItemObj->GetBoolField("isQuestItem");
	
	if (ItemObj->HasField("itemType"))
	{
		Item.itemType = static_cast<EItemType>(ItemObj->GetIntegerField("itemType"));
	}
	
	if (ItemObj->HasField("itemTypeName"))
		Item.itemTypeName = ItemObj->GetStringField("itemTypeName");
	
	if (ItemObj->HasField("itemTypeSlug"))
		Item.itemTypeSlug = ItemObj->GetStringField("itemTypeSlug");
	
	if (ItemObj->HasField("attributes"))
	{
		const TArray<TSharedPtr<FJsonValue>>* AttributesArray;
		if (ItemObj->TryGetArrayField("attributes", AttributesArray))
		{
			Item.attributes = JSONParser::DeserializeItemAttributes(*AttributesArray);
		}
	}

	return Item;
}

// Parse dropped item from JSON object
FDroppedItemStruct JSONParser::DeserializeDroppedItem(const TSharedPtr<FJsonObject>& DroppedItemObj)
{
	FDroppedItemStruct DroppedItem;
	if (!DroppedItemObj.IsValid()) return DroppedItem;

	if (DroppedItemObj->HasField("uid"))
		DroppedItem.uid = DroppedItemObj->GetIntegerField("uid");
	
	if (DroppedItemObj->HasField("itemId"))
		DroppedItem.itemId = DroppedItemObj->GetIntegerField("itemId");
	
	if (DroppedItemObj->HasField("droppedByMobUID"))
		DroppedItem.droppedByMobUID = DroppedItemObj->GetStringField("droppedByMobUID");
	
	if (DroppedItemObj->HasField("quantity"))
		DroppedItem.quantity = DroppedItemObj->GetIntegerField("quantity");
	
	if (DroppedItemObj->HasField("canBePickedUp"))
		DroppedItem.canBePickedUp = DroppedItemObj->GetBoolField("canBePickedUp");
	
	if (DroppedItemObj->HasField("position") && DroppedItemObj->GetObjectField("position").IsValid())
		DroppedItem.position = JSONParser::DeserializePositionData(DroppedItemObj->GetObjectField("position"));
	
	if (DroppedItemObj->HasField("item") && DroppedItemObj->GetObjectField("item").IsValid())
		DroppedItem.item = JSONParser::DeserializeItemData(DroppedItemObj->GetObjectField("item"));

	return DroppedItem;
}

// Parse item drop response from JSON object
FItemDropResponseStruct JSONParser::DeserializeItemDropResponse(const TSharedPtr<FJsonObject>& Body)
{
	FItemDropResponseStruct ItemDropResponse;
	if (!Body.IsValid()) return ItemDropResponse;

	const TArray<TSharedPtr<FJsonValue>>* DroppedItemsArray;
	if (Body->TryGetArrayField("droppedItems", DroppedItemsArray))
	{
		for (const TSharedPtr<FJsonValue>& ItemValue : *DroppedItemsArray)
		{
			TSharedPtr<FJsonObject> ItemObject = ItemValue->AsObject();
			if (ItemObject.IsValid())
			{
				ItemDropResponse.droppedItems.Add(JSONParser::DeserializeDroppedItem(ItemObject));
			}
		}
	}

	return ItemDropResponse;
}

// Parse item drop response from JSON string
FItemDropResponseStruct JSONParser::DeserializeItemDropResponse(const FString& JsonString)
{
	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()) 
		return FItemDropResponseStruct();
	
	TSharedPtr<FJsonObject> Body = Root->GetObjectField("body");
	if (!Body.IsValid())
		return FItemDropResponseStruct();
	
	return JSONParser::DeserializeItemDropResponse(Body);
}

// Combat data deserializers
FCombatAnimationData JSONParser::DeserializeCombatAnimation(const TSharedPtr<FJsonObject>& AnimationObj)
{
	FCombatAnimationData AnimationData;
	if (!AnimationObj.IsValid()) return AnimationData;

	if (AnimationObj->HasField("animationName"))
		AnimationData.AnimationName = AnimationObj->GetStringField("animationName");
	
	if (AnimationObj->HasField("characterId"))
		AnimationData.CharacterId = AnimationObj->GetIntegerField("characterId");
	
	if (AnimationObj->HasField("duration"))
		AnimationData.Duration = AnimationObj->GetNumberField("duration");
	
	if (AnimationObj->HasField("isLooping"))
		AnimationData.bIsLooping = AnimationObj->GetBoolField("isLooping");

	return AnimationData;
}

FCombatActionData JSONParser::DeserializeCombatAction(const TSharedPtr<FJsonObject>& ActionObj)
{
	FCombatActionData ActionData;
	if (!ActionObj.IsValid()) return ActionData;

	if (ActionObj->HasField("actionId"))
		ActionData.ActionId = ActionObj->GetIntegerField("actionId");
	
	if (ActionObj->HasField("actionName"))
		ActionData.ActionName = ActionObj->GetStringField("actionName");
	
	if (ActionObj->HasField("actionType"))
		ActionData.ActionType = ActionObj->GetIntegerField("actionType");
	
	if (ActionObj->HasField("casterId"))
		ActionData.CasterId = ActionObj->GetIntegerField("casterId");
	
	if (ActionObj->HasField("targetId"))
		ActionData.TargetId = ActionObj->GetIntegerField("targetId");
	
	if (ActionObj->HasField("targetType"))
		ActionData.TargetType = ActionObj->GetIntegerField("targetType");
	
	if (ActionObj->HasField("targetTypeString"))
		ActionData.TargetTypeString = ActionObj->GetStringField("targetTypeString");

	return ActionData;
}

FCombatResultData JSONParser::DeserializeCombatResult(const TSharedPtr<FJsonObject>& ResultObj)
{
	FCombatResultData ResultData;
	if (!ResultObj.IsValid()) return ResultData;

	if (ResultObj->HasField("actionId"))
		ResultData.ActionId = ResultObj->GetIntegerField("actionId");
	
	if (ResultObj->HasField("casterId"))
		ResultData.CasterId = ResultObj->GetIntegerField("casterId");
	
	if (ResultObj->HasField("damageDealt"))
		ResultData.DamageDealt = ResultObj->GetIntegerField("damageDealt");
	
	if (ResultObj->HasField("healingDone"))
		ResultData.HealingDone = ResultObj->GetIntegerField("healingDone");
	
	if (ResultObj->HasField("isBlocked"))
		ResultData.bIsBlocked = ResultObj->GetBoolField("isBlocked");
	
	if (ResultObj->HasField("isCritical"))
		ResultData.bIsCritical = ResultObj->GetBoolField("isCritical");
	
	if (ResultObj->HasField("isDodged"))
		ResultData.bIsDodged = ResultObj->GetBoolField("isDodged");
	
	if (ResultObj->HasField("isResisted"))
		ResultData.bIsResisted = ResultObj->GetBoolField("isResisted");
	
	if (ResultObj->HasField("remainingHealth"))
		ResultData.RemainingHealth = ResultObj->GetIntegerField("remainingHealth");
	
	if (ResultObj->HasField("remainingMana"))
		ResultData.RemainingMana = ResultObj->GetIntegerField("remainingMana");
	
	if (ResultObj->HasField("targetDied"))
		ResultData.bTargetDied = ResultObj->GetBoolField("targetDied");
	
	if (ResultObj->HasField("isDamaged"))
		ResultData.bIsDamaged = ResultObj->GetBoolField("isDamaged");
	
	if (ResultObj->HasField("targetId"))
		ResultData.TargetId = ResultObj->GetIntegerField("targetId");
	
	if (ResultObj->HasField("targetType"))
		ResultData.TargetType = ResultObj->GetIntegerField("targetType");
	
	if (ResultObj->HasField("targetTypeString"))
		ResultData.TargetTypeString = ResultObj->GetStringField("targetTypeString");

	return ResultData;
}

FMobTargetLostStruct JSONParser::DeserializeMobTargetLost(const TSharedPtr<FJsonObject>& Body)
{
	FMobTargetLostStruct MobTargetLostData;
	if (!Body.IsValid()) return MobTargetLostData;

	if (Body->HasField("lostTargetPlayerId"))
		MobTargetLostData.lostTargetPlayerId = Body->GetIntegerField("lostTargetPlayerId");

	if (Body->HasField("mobId"))
		MobTargetLostData.mobId = Body->GetIntegerField("mobId");

	if (Body->HasField("mobUID"))
		MobTargetLostData.mobUID = Body->GetIntegerField("mobUID");

	// Parse position data from individual fields
	if (Body->HasField("positionX") && Body->HasField("positionY") &&
		Body->HasField("positionZ") && Body->HasField("rotationZ"))
	{
		MobTargetLostData.position.positionX = Body->GetNumberField("positionX");
		MobTargetLostData.position.positionY = Body->GetNumberField("positionY");
		MobTargetLostData.position.positionZ = Body->GetNumberField("positionZ");
		MobTargetLostData.position.rotationZ = Body->GetNumberField("rotationZ");
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

	TSharedPtr<FJsonObject> Body = Root->GetObjectField("body");
	if (!Body.IsValid())
		return FMobTargetLostStruct();

	return JSONParser::DeserializeMobTargetLost(Body);
}

// Inventory parsing functions
FInventoryItemStruct JSONParser::DeserializeInventoryItem(const TSharedPtr<FJsonObject>& ItemObj)
{
	FInventoryItemStruct Item;
	if (!ItemObj.IsValid()) return Item;

	if (ItemObj->HasField("itemId"))
		Item.itemId = ItemObj->GetIntegerField("itemId");
	
	if (ItemObj->HasField("quantity"))
		Item.quantity = ItemObj->GetIntegerField("quantity");
	
	if (ItemObj->HasField("name"))
		Item.name = ItemObj->GetStringField("name");
	
	if (ItemObj->HasField("description"))
		Item.description = ItemObj->GetStringField("description");
	
	if (ItemObj->HasField("type"))
		Item.type = ItemObj->GetStringField("type");
	
	if (ItemObj->HasField("rarity"))
		Item.rarity = ItemObj->GetStringField("rarity");
	
	if (ItemObj->HasField("level"))
		Item.level = ItemObj->GetIntegerField("level");

	// Parse attributes object
	if (ItemObj->HasField("attributes"))
	{
		TSharedPtr<FJsonObject> AttributesObj = ItemObj->GetObjectField("attributes");
		if (AttributesObj.IsValid())
		{
			for (const auto& AttributePair : AttributesObj->Values)
			{
				FString AttributeValue;
				if (AttributePair.Value->TryGetString(AttributeValue))
				{
					Item.attributes.Add(AttributePair.Key, AttributeValue);
				}
				else
				{
					// Handle numeric attributes
					double NumericValue;
					if (AttributePair.Value->TryGetNumber(NumericValue))
					{
						Item.attributes.Add(AttributePair.Key, FString::SanitizeFloat(NumericValue));
					}
				}
			}
		}
	}

	return Item;
}

TArray<FInventoryItemStruct> JSONParser::DeserializeInventoryItems(const TArray<TSharedPtr<FJsonValue>>& JsonArray)
{
	TArray<FInventoryItemStruct> Items;
	for (const TSharedPtr<FJsonValue>& Value : JsonArray)
	{
		const TSharedPtr<FJsonObject> ItemObj = Value->AsObject();
		if (ItemObj.IsValid())
		{
			Items.Add(JSONParser::DeserializeInventoryItem(ItemObj));
		}
	}
	return Items;
}

FCharacterInventoryStruct JSONParser::DeserializeCharacterInventory(const TSharedPtr<FJsonObject>& InventoryObj)
{
	FCharacterInventoryStruct Inventory;
	if (!InventoryObj.IsValid()) return Inventory;

	if (InventoryObj->HasField("characterId"))
		Inventory.characterId = InventoryObj->GetIntegerField("characterId");

	// Parse items array
	const TArray<TSharedPtr<FJsonValue>>* ItemsArray = nullptr;
	if (InventoryObj->TryGetArrayField("items", ItemsArray))
	{
		Inventory.items = JSONParser::DeserializeInventoryItems(*ItemsArray);
	}

	return Inventory;
}

FCharacterInventoryStruct JSONParser::DeserializeCharacterInventory(const FString& JsonString)
{
	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		return FCharacterInventoryStruct();

	TSharedPtr<FJsonObject> Body = Root->GetObjectField("body");
	if (!Body.IsValid())
		return FCharacterInventoryStruct();

	return JSONParser::DeserializeCharacterInventory(Body);
}

FInventoryUpdateStruct JSONParser::DeserializeInventoryUpdate(const TSharedPtr<FJsonObject>& UpdateObj)
{
	FInventoryUpdateStruct Update;
	if (!UpdateObj.IsValid()) return Update;

	if (UpdateObj->HasField("eventType"))
		Update.eventType = UpdateObj->GetStringField("eventType");
	
	if (UpdateObj->HasField("characterId"))
		Update.characterId = UpdateObj->GetIntegerField("characterId");

	// Parse data object
	if (UpdateObj->HasField("data"))
	{
		TSharedPtr<FJsonObject> DataObj = UpdateObj->GetObjectField("data");
		if (DataObj.IsValid())
		{
			Update.data = JSONParser::DeserializeCharacterInventory(DataObj);
		}
	}

	return Update;
}

FInventoryUpdateStruct JSONParser::DeserializeInventoryUpdate(const FString& JsonString)
{
	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		return FInventoryUpdateStruct();

	return JSONParser::DeserializeInventoryUpdate(Root);
}