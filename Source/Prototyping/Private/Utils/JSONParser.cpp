// Fill out your copyright notice in the Description page of Project Settings.


#include "Utils/JSONParser.h"
#include "Services/TimeSyncService.h"
#include "MyGameInstance.h"
#include "Engine/World.h"
#include "Utils/PlayerAttributeParser.h"

// Helper method to get TimeSyncService instance
UTimeSyncService* JSONParser::GetTimeSyncService()
{
    // Try to get TimeSyncService from the current world's game instance
    if (GWorld)
    {
        UMyGameInstance* GameInstance = Cast<UMyGameInstance>(GWorld->GetGameInstance());
        if (GameInstance)
        {
            return GameInstance->GetTimeSyncService();
        }
    }
    return nullptr;
}

// serialize a json object to a string
 FString JSONParser::SerializeJson(const FString& EventType, const TMap<FString, TSharedPtr<FJsonValue>>& HeaderData, const TMap<FString, TSharedPtr<FJsonValue>>& BodyData)
{
    TSharedPtr<FJsonObject> HeaderObject = MakeShareable(new FJsonObject);
    HeaderObject->SetStringField("eventType", EventType);

    // Automatically add clientSendMs timestamp if TimeSyncService is available
    UTimeSyncService* TimeSyncService = GetTimeSyncService();
    if (TimeSyncService)
    {
        int64 ClientSendMs = TimeSyncService->GetCurrentClientTimeMs();
        HeaderObject->SetNumberField("clientSendMs", ClientSendMs);
    }

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

	// Remove any newline characters
	OutputString.ReplaceInline(TEXT("\n"), TEXT(""));
	OutputString.ReplaceInline(TEXT("\r"), TEXT(""));

    return OutputString;
}

// New method with explicit TimeSyncService parameter and server type
FString JSONParser::SerializeJsonWithTimeSync(const FString& EventType, const TMap<FString, TSharedPtr<FJsonValue>>& HeaderData, const TMap<FString, TSharedPtr<FJsonValue>>& BodyData, UTimeSyncService* TimeSyncService, EServerType ServerType)
{
    TSharedPtr<FJsonObject> HeaderObject = MakeShareable(new FJsonObject);
    HeaderObject->SetStringField("eventType", EventType);

    // Generate request ID and register with TimeSyncService for ALL requests
    FString RequestId;
    if (TimeSyncService)
    {
        UE_LOG(LogTemp, Verbose, TEXT("JSONParser::SerializeJsonWithTimeSync - TimeSyncService available, calling GenerateAndRegisterSyncRequest for ServerType: %d, EventType: %s"), 
            static_cast<int32>(ServerType), *EventType);
            
        // Generate request ID for every request without restrictions
        RequestId = TimeSyncService->GenerateAndRegisterSyncRequest(ServerType);
        
        UE_LOG(LogTemp, Verbose, TEXT("JSONParser::SerializeJsonWithTimeSync - Generated RequestId: '%s' (IsEmpty: %s)"), 
            *RequestId, RequestId.IsEmpty() ? TEXT("true") : TEXT("false"));
        
        if (!RequestId.IsEmpty())
        {
            // Add requestId to header but DO NOT set clientSendMs here
            // NetworkSenderWorker will set the precise clientSendMs right before sending
            HeaderObject->SetStringField("requestId", RequestId);
            
            UE_LOG(LogTemp, Verbose, TEXT("JSONParser::SerializeJsonWithTimeSync - Added requestId: %s (clientSendMs will be set by NetworkSenderWorker)"), 
                *RequestId);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("JSONParser::SerializeJsonWithTimeSync - RequestId is empty, not adding time sync fields"));
        }
    }
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("TimeSyncService is null. Cannot add requestId to header."));
	}

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

	// Remove any newline characters
	OutputString.ReplaceInline(TEXT("\n"), TEXT(""));
	OutputString.ReplaceInline(TEXT("\r"), TEXT(""));

    UE_LOG(LogTemp, Verbose, TEXT("JSONParser::SerializeJsonWithTimeSync - Final JSON (without clientSendMs): %s"), *OutputString);

    return OutputString;
}

// Legacy method with automatic server type detection
FString JSONParser::SerializeJsonWithTimeSync(const FString& EventType, const TMap<FString, TSharedPtr<FJsonValue>>& HeaderData, const TMap<FString, TSharedPtr<FJsonValue>>& BodyData, UTimeSyncService* TimeSyncService)
{
    // Determine server type based on event type
    EServerType ServerType = EServerType::ChunkServer; // Default
    
    if (EventType == TEXT("authentificationClient") || 
        EventType == TEXT("getCharactersList") ||
        EventType == TEXT("disconnectClient"))
    {
        ServerType = EServerType::LoginServer;
    }
    else if (EventType == TEXT("joinGame") ||
             EventType == TEXT("joinGameClient"))
    {
        ServerType = EServerType::GameServer;
    }
    // Everything else goes to ChunkServer by default
    
    return SerializeJsonWithTimeSync(EventType, HeaderData, BodyData, TimeSyncService, ServerType);
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

             // Parse time sync fields
             if ((*HeaderObject)->HasField(TEXT("clientSendMs")))
             {
                 MessageData.clientSendMs = static_cast<int64>((*HeaderObject)->GetNumberField(TEXT("clientSendMs")));
             }

			 if ((*HeaderObject)->HasField(TEXT("clientSendMsEcho")))
			 {
				 MessageData.clientSendMs = static_cast<int64>((*HeaderObject)->GetNumberField(TEXT("clientSendMsEcho")));
			 }

			 if ((*HeaderObject)->HasField(TEXT("clientSendMsEcho")))
			 {
				 MessageData.clientSendMsEcho = static_cast<int64>((*HeaderObject)->GetNumberField(TEXT("clientSendMsEcho")));
			 }

             if ((*HeaderObject)->HasField(TEXT("serverRecvMs")))
             {
                 MessageData.serverRecvMs = static_cast<int64>((*HeaderObject)->GetNumberField(TEXT("serverRecvMs")));
             }

             if ((*HeaderObject)->HasField(TEXT("serverSendMs")))
             {
                 MessageData.serverSendMs = static_cast<int64>((*HeaderObject)->GetNumberField(TEXT("serverSendMs")));
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

	// Простые числовые/строковые
	if (ItemObj->HasField("itemId"))
		Item.itemId = ItemObj->GetIntegerField("itemId");

	if (ItemObj->HasField("quantity"))
		Item.quantity = ItemObj->GetIntegerField("quantity");

	if (ItemObj->HasField("name"))
		Item.name = ItemObj->GetStringField("name");

	if (ItemObj->HasField("slug"))
		Item.slug = ItemObj->GetStringField("slug");

	if (ItemObj->HasField("description"))
		Item.description = ItemObj->GetStringField("description");

	if (ItemObj->HasField("itemTypeName"))
		Item.type = ItemObj->GetStringField("itemTypeName");

	if (ItemObj->HasField("rarityName"))
		Item.rarity = ItemObj->GetStringField("rarityName");

	if (ItemObj->HasField("levelRequirement"))
		Item.level_requirement = ItemObj->GetIntegerField("levelRequirement");

	if (ItemObj->HasField("weight"))
		Item.weight = ItemObj->GetNumberField("weight");

	if (ItemObj->HasField("stackMax"))
		Item.stackSize = ItemObj->GetIntegerField("stackMax");

	if (ItemObj->HasField("durabilityMax"))
		Item.durability_max = ItemObj->GetIntegerField("durabilityMax");

	// durabilityCurrent в пакете пока нет, оставляем дефолт = max
	Item.durability_current = Item.durability_max;

	// Флаги
	if (ItemObj->HasField("isDurable"))
		Item.is_durable = ItemObj->GetBoolField("isDurable");

	if (ItemObj->HasField("isTradable"))
		Item.is_tradable = ItemObj->GetBoolField("isTradable");

	if (ItemObj->HasField("isContainer"))
		Item.is_container = ItemObj->GetBoolField("isContainer");

	if (ItemObj->HasField("isQuestItem"))
		Item.is_quest_item = ItemObj->GetBoolField("isQuestItem");

	// Эквип — делаем флажок true если есть слот > 0
	if (ItemObj->HasField("equipSlot"))
	{
		int32 SlotId = ItemObj->GetIntegerField("equipSlot");
		Item.is_equippable = (SlotId > 0);
	}

	// Цены
	if (ItemObj->HasField("vendorPriceBuy"))
		Item.vendor_price_buy = ItemObj->GetIntegerField("vendorPriceBuy");

	if (ItemObj->HasField("vendorPriceSell"))
		Item.vendor_price_sell = ItemObj->GetIntegerField("vendorPriceSell");

	// Attributes (массив объектов {name,value})
	if (ItemObj->HasTypedField<EJson::Array>("attributes"))
	{
		const TArray<TSharedPtr<FJsonValue>> AttrArray = ItemObj->GetArrayField("attributes");
		for (const TSharedPtr<FJsonValue>& AttrVal : AttrArray)
		{
			TSharedPtr<FJsonObject> AttrObj = AttrVal->AsObject();
			if (!AttrObj.IsValid()) continue;

			FString AttrName;
			if (AttrObj->TryGetStringField("name", AttrName))
			{
				// value может быть числом или строкой
				FString StrVal;
				double NumVal;
				if (AttrObj->TryGetStringField("value", StrVal))
				{
					Item.attributes.Add(AttrName, StrVal);
				}
				else if (AttrObj->TryGetNumberField("value", NumVal))
				{
					Item.attributes.Add(AttrName, FString::SanitizeFloat(NumVal));
				}
			}
		}
	}

	return Item;
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

FCharacterInventoryStruct JSONParser::DeserializeCharacterInventory(const TSharedPtr<FJsonObject>& Body)
{
	FCharacterInventoryStruct Inventory;
	if (!Body.IsValid())
		return Inventory;

	int32 CharacterId = 0;
	if (Body->TryGetNumberField("characterId", CharacterId))
	{
		Inventory.characterId = CharacterId;
	}

	// Items
	const TArray<TSharedPtr<FJsonValue>>* ItemsArray = nullptr;
	if (Body->TryGetArrayField("items", ItemsArray))
	{
		for (const TSharedPtr<FJsonValue>& ItemValue : *ItemsArray)
		{
			TSharedPtr<FJsonObject> ItemObj = ItemValue->AsObject();
			if (!ItemObj.IsValid()) continue;

			FInventoryItemStruct Item = JSONParser::DeserializeInventoryItem(ItemObj);
			Inventory.items.Add(Item);
		}
	}

	return Inventory;
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

// Harvest system parsing functions
FHarvestItemStruct JSONParser::DeserializeHarvestItem(const TSharedPtr<FJsonObject>& ItemObj)
{
	FHarvestItemStruct Item;
	if (!ItemObj.IsValid()) return Item;

	if (ItemObj->HasField("itemId"))
		Item.itemId = ItemObj->GetIntegerField("itemId");

	if (ItemObj->HasField("itemSlug"))
		Item.itemSlug = ItemObj->GetStringField("itemSlug");

	if (ItemObj->HasField("quantity"))
		Item.quantity = ItemObj->GetIntegerField("quantity");

	if (ItemObj->HasField("name"))
		Item.name = ItemObj->GetStringField("name");

	if (ItemObj->HasField("description"))
		Item.description = ItemObj->GetStringField("description");

	if (ItemObj->HasField("rarityId"))
		Item.rarityId = ItemObj->GetIntegerField("rarityId");

	if (ItemObj->HasField("rarityName"))
		Item.rarityName = ItemObj->GetStringField("rarityName");

	if (ItemObj->HasField("itemType"))
		Item.itemType = ItemObj->GetStringField("itemType");

	if (ItemObj->HasField("weight"))
		Item.weight = ItemObj->GetNumberField("weight");

	if (ItemObj->HasField("addedToInventory"))
		Item.addedToInventory = ItemObj->GetBoolField("addedToInventory");

	if (ItemObj->HasField("isHarvestItem"))
		Item.isHarvestItem = ItemObj->GetBoolField("isHarvestItem");

	return Item;
}

FHarvestStartedStruct JSONParser::DeserializeHarvestStarted(const TSharedPtr<FJsonObject>& Body)
{
	FHarvestStartedStruct HarvestStarted;
	if (!Body.IsValid()) return HarvestStarted;

	if (Body->HasField("type"))
		HarvestStarted.type = Body->GetStringField("type");

	if (Body->HasField("clientId"))
		HarvestStarted.clientId = Body->GetIntegerField("clientId");

	if (Body->HasField("playerId"))
		HarvestStarted.playerId = Body->GetIntegerField("playerId");

	if (Body->HasField("corpseId"))
		HarvestStarted.corpseId = Body->GetIntegerField("corpseId");

	if (Body->HasField("duration"))
		HarvestStarted.duration = Body->GetIntegerField("duration");

	if (Body->HasField("startTime"))
		HarvestStarted.startTime = static_cast<int64>(Body->GetNumberField("startTime"));

	return HarvestStarted;
}

FHarvestStartedStruct JSONParser::DeserializeHarvestStarted(const FString& JsonString)
{
	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		return FHarvestStartedStruct();

	TSharedPtr<FJsonObject> Body = Root->GetObjectField("body");
	if (!Body.IsValid())
		return FHarvestStartedStruct();

	return JSONParser::DeserializeHarvestStarted(Body);
}

FHarvestCompleteStruct JSONParser::DeserializeHarvestComplete(const TSharedPtr<FJsonObject>& Body)
{
	FHarvestCompleteStruct HarvestComplete;
	if (!Body.IsValid()) return HarvestComplete;

	if (Body->HasField("type"))
		HarvestComplete.type = Body->GetStringField("type");

	if (Body->HasField("clientId"))
		HarvestComplete.clientId = Body->GetIntegerField("clientId");

	if (Body->HasField("playerId"))
		HarvestComplete.playerId = Body->GetIntegerField("playerId");

	if (Body->HasField("corpseId"))
		HarvestComplete.corpseId = Body->GetIntegerField("corpseId");

	if (Body->HasField("success"))
		HarvestComplete.success = Body->GetBoolField("success");

	if (Body->HasField("totalItems"))
		HarvestComplete.totalItems = Body->GetIntegerField("totalItems");

	// Parse available loot
	const TArray<TSharedPtr<FJsonValue>>* LootArray = nullptr;
	if (Body->TryGetArrayField("availableLoot", LootArray))
	{
		for (const TSharedPtr<FJsonValue>& LootValue : *LootArray)
		{
			TSharedPtr<FJsonObject> LootObj = LootValue->AsObject();
			if (LootObj.IsValid())
			{
				FHarvestItemStruct Item = JSONParser::DeserializeHarvestItem(LootObj);
				HarvestComplete.availableLoot.Add(Item);
			}
		}
	}

	return HarvestComplete;
}

FHarvestCompleteStruct JSONParser::DeserializeHarvestComplete(const FString& JsonString)
{
	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		return FHarvestCompleteStruct();

	TSharedPtr<FJsonObject> Body = Root->GetObjectField("body");
	if (!Body.IsValid())
		return FHarvestCompleteStruct();
	
	return JSONParser::DeserializeHarvestComplete(Body);
}

FHarvestErrorStruct JSONParser::DeserializeHarvestError(const TSharedPtr<FJsonObject>& Body)
{
	FHarvestErrorStruct HarvestError;
	if (!Body.IsValid()) return HarvestError;

	if (Body->HasField("type"))
		HarvestError.type = Body->GetStringField("type");

	if (Body->HasField("clientId"))
		HarvestError.clientId = Body->GetIntegerField("clientId");

	if (Body->HasField("playerId"))
		HarvestError.playerId = Body->GetIntegerField("playerId");

	if (Body->HasField("corpseId"))
		HarvestError.corpseId = Body->GetIntegerField("corpseId");

	if (Body->HasField("errorCode"))
		HarvestError.errorCode = Body->GetStringField("errorCode");

	if (Body->HasField("message"))
		HarvestError.message = Body->GetStringField("message");

	return HarvestError;
}

FHarvestErrorStruct JSONParser::DeserializeHarvestError(const FString& JsonString)
{
	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		return FHarvestErrorStruct();

	TSharedPtr<FJsonObject> Body = Root->GetObjectField("body");
	if (!Body.IsValid())
		return FHarvestErrorStruct();

	return JSONParser::DeserializeHarvestError(Body);
}

FCorpseLootPickupResponseStruct JSONParser::DeserializeCorpseLootPickupResponse(const TSharedPtr<FJsonObject>& Body)
{
	FCorpseLootPickupResponseStruct Response;
	if (!Body.IsValid()) return Response;

	if (Body->HasField("success"))
		Response.success = Body->GetBoolField("success");

	if (Body->HasField("corpseUID"))
		Response.corpseUID = Body->GetIntegerField("corpseUID");

	if (Body->HasField("itemsPickedUp"))
		Response.itemsPickedUp = Body->GetIntegerField("itemsPickedUp");

	// Parse picked up items
	const TArray<TSharedPtr<FJsonValue>>* PickedUpArray = nullptr;
	if (Body->TryGetArrayField("pickedUpItems", PickedUpArray))
	{
		for (const TSharedPtr<FJsonValue>& ItemValue : *PickedUpArray)
		{
			TSharedPtr<FJsonObject> ItemObj = ItemValue->AsObject();
			if (ItemObj.IsValid())
			{
				FHarvestItemStruct Item = JSONParser::DeserializeHarvestItem(ItemObj);
				Response.pickedUpItems.Add(Item);
			}
		}
	}

	// Parse remaining loot
	const TArray<TSharedPtr<FJsonValue>>* RemainingArray = nullptr;
	if (Body->TryGetArrayField("remainingLoot", RemainingArray))
	{
		for (const TSharedPtr<FJsonValue>& ItemValue : *RemainingArray)
		{
			TSharedPtr<FJsonObject> ItemObj = ItemValue->AsObject();
			if (ItemObj.IsValid())
			{
				FHarvestItemStruct Item = JSONParser::DeserializeHarvestItem(ItemObj);
				Response.remainingLoot.Add(Item);
			}
		}
	}

	return Response;
}

FCorpseLootPickupResponseStruct JSONParser::DeserializeCorpseLootPickupResponse(const FString& JsonString)
{
	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		return FCorpseLootPickupResponseStruct();

	TSharedPtr<FJsonObject> Body = Root->GetObjectField("body");
	if (!Body.IsValid())
		return FCorpseLootPickupResponseStruct();

	return JSONParser::DeserializeCorpseLootPickupResponse(Body);
}

FCorpseLootPickupErrorStruct JSONParser::DeserializeCorpseLootPickupError(const TSharedPtr<FJsonObject>& Body)
{
	FCorpseLootPickupErrorStruct Error;
	if (!Body.IsValid()) return Error;

	if (Body->HasField("success"))
		Error.success = Body->GetBoolField("success");

	if (Body->HasField("errorCode"))
		Error.errorCode = Body->GetStringField("errorCode");

	if (Body->HasField("corpseUID"))
		Error.corpseUID = Body->GetIntegerField("corpseUID");

	return Error;
}

FCorpseLootPickupErrorStruct JSONParser::DeserializeCorpseLootPickupError(const FString& JsonString)
{
	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		return FCorpseLootPickupErrorStruct();

	TSharedPtr<FJsonObject> Body = Root->GetObjectField("body");
	if (!Body.IsValid())
		return FCorpseLootPickupErrorStruct();

	return JSONParser::DeserializeCorpseLootPickupError(Body);
}

FCorpseLootInspectResponseStruct JSONParser::DeserializeCorpseLootInspectResponse(const TSharedPtr<FJsonObject>& Body)
{
	FCorpseLootInspectResponseStruct Response;
	if (!Body.IsValid()) return Response;

	if (Body->HasField("success"))
		Response.success = Body->GetBoolField("success");

	if (Body->HasField("corpseUID"))
		Response.corpseUID = Body->GetIntegerField("corpseUID");

	if (Body->HasField("type"))
		Response.type = Body->GetStringField("type");

	if (Body->HasField("totalItems"))
		Response.totalItems = Body->GetIntegerField("totalItems");

	// Parse available loot
	const TArray<TSharedPtr<FJsonValue>>* LootArray = nullptr;
	if (Body->TryGetArrayField("availableLoot", LootArray))
	{
		for (const TSharedPtr<FJsonValue>& LootValue : *LootArray)
		{
			TSharedPtr<FJsonObject> LootObj = LootValue->AsObject();
			if (LootObj.IsValid())
			{
				FHarvestItemStruct Item = JSONParser::DeserializeHarvestItem(LootObj);
				Response.availableLoot.Add(Item);
			}
		}
	}

	return Response;
}

FCorpseLootInspectResponseStruct JSONParser::DeserializeCorpseLootInspectResponse(const FString& JsonString)
{
	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		return FCorpseLootInspectResponseStruct();

	TSharedPtr<FJsonObject> Body = Root->GetObjectField("body");
	if (!Body.IsValid())
		return FCorpseLootInspectResponseStruct();

	return JSONParser::DeserializeCorpseLootInspectResponse(Body);
}

FCorpseLootInspectErrorStruct JSONParser::DeserializeCorpseLootInspectError(const TSharedPtr<FJsonObject>& Body)
{
	FCorpseLootInspectErrorStruct Error;
	if (!Body.IsValid()) return Error;

	if (Body->HasField("success"))
		Error.success = Body->GetBoolField("success");

	if (Body->HasField("errorCode"))
		Error.errorCode = Body->GetStringField("errorCode");

	return Error;
}

FCorpseLootInspectErrorStruct JSONParser::DeserializeCorpseLootInspectError(const FString& JsonString)
{
	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		return FCorpseLootInspectErrorStruct();

	TSharedPtr<FJsonObject> Body = Root->GetObjectField("body");
	if (!Body.IsValid())
		return FCorpseLootInspectErrorStruct();

	return JSONParser::DeserializeCorpseLootInspectError(Body);
}

// New combat system parsing functions
ESkillEffectType JSONParser::ParseSkillEffectType(const FString& EffectTypeString)
{
	if (EffectTypeString.Equals(TEXT("damage"), ESearchCase::IgnoreCase))
		return ESkillEffectType::Damage;
	else if (EffectTypeString.Equals(TEXT("healing"), ESearchCase::IgnoreCase))
		return ESkillEffectType::Healing;
	else if (EffectTypeString.Equals(TEXT("buff"), ESearchCase::IgnoreCase))
		return ESkillEffectType::Buff;
	else if (EffectTypeString.Equals(TEXT("debuff"), ESearchCase::IgnoreCase))
		return ESkillEffectType::Debuff;
	else if (EffectTypeString.Equals(TEXT("resource"), ESearchCase::IgnoreCase))
		return ESkillEffectType::Resource;
	
	return ESkillEffectType::None;
}

ESkillSchool JSONParser::ParseSkillSchool(const FString& SchoolString)
{
	if (SchoolString.Equals(TEXT("physical"), ESearchCase::IgnoreCase))
		return ESkillSchool::Physical;
	else if (SchoolString.Equals(TEXT("fire"), ESearchCase::IgnoreCase))
		return ESkillSchool::Fire;
	else if (SchoolString.Equals(TEXT("ice"), ESearchCase::IgnoreCase))
		return ESkillSchool::Ice;
	else if (SchoolString.Equals(TEXT("nature"), ESearchCase::IgnoreCase))
		return ESkillSchool::Nature;
	else if (SchoolString.Equals(TEXT("arcane"), ESearchCase::IgnoreCase))
		return ESkillSchool::Arcane;
	else if (SchoolString.Equals(TEXT("shadow"), ESearchCase::IgnoreCase))
		return ESkillSchool::Shadow;
	else if (SchoolString.Equals(TEXT("holy"), ESearchCase::IgnoreCase))
		return ESkillSchool::Holy;
	
	return ESkillSchool::None;
}

ECasterType JSONParser::ParseCasterType(const FString& CasterTypeString)
{
	if (CasterTypeString.Equals(TEXT("PLAYER"), ESearchCase::IgnoreCase) || 
		CasterTypeString.Equals(TEXT("Player"), ESearchCase::IgnoreCase))
		return ECasterType::Player;
	else if (CasterTypeString.Equals(TEXT("MOB"), ESearchCase::IgnoreCase) || 
			 CasterTypeString.Equals(TEXT("Mob"), ESearchCase::IgnoreCase))
		return ECasterType::Mob;
	else if (CasterTypeString.Equals(TEXT("NPC"), ESearchCase::IgnoreCase) || 
			 CasterTypeString.Equals(TEXT("Npc"), ESearchCase::IgnoreCase))
		return ECasterType::NPC;
	
	return ECasterType::None;
}

FAppliedEffectData JSONParser::DeserializeAppliedEffect(const TSharedPtr<FJsonObject>& EffectObj)
{
	FAppliedEffectData Effect;
	if (!EffectObj.IsValid()) return Effect;

	if (EffectObj->HasField("effectName"))
		Effect.effectName = EffectObj->GetStringField("effectName");
	
	if (EffectObj->HasField("duration"))
		Effect.duration = EffectObj->GetNumberField("duration");
	
	if (EffectObj->HasField("value"))
		Effect.value = EffectObj->GetIntegerField("value");
	
	if (EffectObj->HasField("effectType"))
		Effect.effectType = EffectObj->GetStringField("effectType");

	return Effect;
}

TArray<FAppliedEffectData> JSONParser::DeserializeAppliedEffects(const TArray<TSharedPtr<FJsonValue>>& JsonArray)
{
	TArray<FAppliedEffectData> Effects;
	for (const TSharedPtr<FJsonValue>& Value : JsonArray)
	{
		const TSharedPtr<FJsonObject> EffectObj = Value->AsObject();
		if (EffectObj.IsValid())
		{
			Effects.Add(JSONParser::DeserializeAppliedEffect(EffectObj));
		}
	}
	return Effects;
}

FSkillInitiationData JSONParser::DeserializeSkillInitiation(const TSharedPtr<FJsonObject>& InitiationObj)
{
	FSkillInitiationData SkillData;
	if (!InitiationObj.IsValid()) return SkillData;

	if (InitiationObj->HasField("skillName"))
		SkillData.skillName = InitiationObj->GetStringField("skillName");
	
	if (InitiationObj->HasField("animationName"))
		SkillData.animationName = InitiationObj->GetStringField("animationName");
	
	if (InitiationObj->HasField("animationDuration"))
		SkillData.animationDuration = InitiationObj->GetNumberField("animationDuration");
	
	if (InitiationObj->HasField("castTime"))
		SkillData.castTime = InitiationObj->GetNumberField("castTime");
	
	if (InitiationObj->HasField("casterId"))
		SkillData.casterId = InitiationObj->GetIntegerField("casterId");
	
	if (InitiationObj->HasField("casterType"))
		SkillData.casterType = InitiationObj->GetIntegerField("casterType");
	
	if (InitiationObj->HasField("casterTypeString"))
		SkillData.casterTypeString = InitiationObj->GetStringField("casterTypeString");
	
	if (InitiationObj->HasField("targetId"))
		SkillData.targetId = InitiationObj->GetIntegerField("targetId");
	
	if (InitiationObj->HasField("targetType"))
		SkillData.targetType = InitiationObj->GetIntegerField("targetType");
	
	if (InitiationObj->HasField("targetTypeString"))
		SkillData.targetTypeString = InitiationObj->GetStringField("targetTypeString");
	
	if (InitiationObj->HasField("success"))
		SkillData.success = InitiationObj->GetBoolField("success");

	// Parse effect type and school
	if (InitiationObj->HasField("skillEffectType"))
	{
		FString EffectTypeStr = InitiationObj->GetStringField("skillEffectType");
		SkillData.skillEffectType = JSONParser::ParseSkillEffectType(EffectTypeStr);
	}
	
	if (InitiationObj->HasField("skillSchool"))
	{
		FString SchoolStr = InitiationObj->GetStringField("skillSchool");
		SkillData.skillSchool = JSONParser::ParseSkillSchool(SchoolStr);
	}

	return SkillData;
}

FSkillInitiationData JSONParser::DeserializeSkillInitiation(const FString& JsonString)
{
	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		return FSkillInitiationData();

	TSharedPtr<FJsonObject> Body = Root->GetObjectField("body");
	if (!Body.IsValid())
		return FSkillInitiationData();

	TSharedPtr<FJsonObject> SkillInitiation = Body->GetObjectField("skillInitiation");
	if (!SkillInitiation.IsValid())
		return FSkillInitiationData();

	return JSONParser::DeserializeSkillInitiation(SkillInitiation);
}

FSkillResultData JSONParser::DeserializeSkillResult(const TSharedPtr<FJsonObject>& ResultObj)
{
	FSkillResultData SkillResult;
	if (!ResultObj.IsValid()) return SkillResult;

	if (ResultObj->HasField("skillName"))
		SkillResult.skillName = ResultObj->GetStringField("skillName");
	
	if (ResultObj->HasField("casterId"))
		SkillResult.casterId = ResultObj->GetIntegerField("casterId");
	
	if (ResultObj->HasField("casterType"))
		SkillResult.casterType = ResultObj->GetIntegerField("casterType");
	
	if (ResultObj->HasField("casterTypeString"))
		SkillResult.casterTypeString = ResultObj->GetStringField("casterTypeString");
	
	if (ResultObj->HasField("targetId"))
		SkillResult.targetId = ResultObj->GetIntegerField("targetId");
	
	if (ResultObj->HasField("targetType"))
		SkillResult.targetType = ResultObj->GetIntegerField("targetType");
	
	if (ResultObj->HasField("targetTypeString"))
		SkillResult.targetTypeString = ResultObj->GetStringField("targetTypeString");
	
	if (ResultObj->HasField("damage"))
		SkillResult.damage = ResultObj->GetIntegerField("damage");
	
	if (ResultObj->HasField("healing"))
		SkillResult.healing = ResultObj->GetIntegerField("healing");
	
	if (ResultObj->HasField("finalTargetHealth"))
		SkillResult.finalTargetHealth = ResultObj->GetIntegerField("finalTargetHealth");
	
	if (ResultObj->HasField("finalTargetMana"))
		SkillResult.finalTargetMana = ResultObj->GetIntegerField("finalTargetMana");
	
	if (ResultObj->HasField("isCritical"))
		SkillResult.isCritical = ResultObj->GetBoolField("isCritical");
	
	if (ResultObj->HasField("isBlocked"))
		SkillResult.isBlocked = ResultObj->GetBoolField("isBlocked");
	
	if (ResultObj->HasField("isMissed"))
		SkillResult.isMissed = ResultObj->GetBoolField("isMissed");
	
	if (ResultObj->HasField("targetDied"))
		SkillResult.targetDied = ResultObj->GetBoolField("targetDied");
	
	if (ResultObj->HasField("success"))
		SkillResult.success = ResultObj->GetBoolField("success");

	// Parse effect type and school
	if (ResultObj->HasField("skillEffectType"))
	{
		FString EffectTypeStr = ResultObj->GetStringField("skillEffectType");
		SkillResult.skillEffectType = JSONParser::ParseSkillEffectType(EffectTypeStr);
	}
	
	if (ResultObj->HasField("skillSchool"))
	{
		FString SchoolStr = ResultObj->GetStringField("skillSchool");
		SkillResult.skillSchool = JSONParser::ParseSkillSchool(SchoolStr);
	}

	// Parse applied effects array
	if (ResultObj->HasField("appliedEffects"))
	{
		const TArray<TSharedPtr<FJsonValue>>* EffectsArray;
		if (ResultObj->TryGetArrayField("appliedEffects", EffectsArray))
		{
			SkillResult.appliedEffects = JSONParser::DeserializeAppliedEffects(*EffectsArray);
		}
	}

	return SkillResult;
}

FSkillResultData JSONParser::DeserializeSkillResult(const FString& JsonString)
{
	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		return FSkillResultData();

	TSharedPtr<FJsonObject> Body = Root->GetObjectField("body");
	if (!Body.IsValid())
		return FSkillResultData();

	TSharedPtr<FJsonObject> SkillResult = Body->GetObjectField("skillResult");
	if (!SkillResult.IsValid())
		return FSkillResultData();

	return JSONParser::DeserializeSkillResult(SkillResult);
}

// Deserialize network header with time sync data
FNetworkHeaderStruct JSONParser::DeserializeNetworkHeader(const FString& JsonString)
{
    TSharedPtr<FJsonObject> JsonObject;
    FNetworkHeaderStruct NetworkHeader;

    // Convert the string to a JSON object
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
    if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
    {
        const TSharedPtr<FJsonObject>* HeaderObject = nullptr;
        if (JsonObject->TryGetObjectField(TEXT("header"), HeaderObject) && HeaderObject != nullptr)
        {
            if ((*HeaderObject)->HasField(TEXT("eventType")))
            {
                NetworkHeader.eventType = (*HeaderObject)->GetStringField(TEXT("eventType"));
            }

            if ((*HeaderObject)->HasField(TEXT("status")))
            {
                NetworkHeader.status = (*HeaderObject)->GetStringField(TEXT("status"));
            }

            if ((*HeaderObject)->HasField(TEXT("clientId")))
            {
                NetworkHeader.clientId = (*HeaderObject)->GetIntegerField(TEXT("clientId"));
            }

            if ((*HeaderObject)->HasField(TEXT("hash")))
            {
                NetworkHeader.hash = (*HeaderObject)->GetStringField(TEXT("hash"));
            }

            if ((*HeaderObject)->HasField(TEXT("message")))
            {
                NetworkHeader.message = (*HeaderObject)->GetStringField(TEXT("message"));
            }

            // Check for requestIdEcho first (server response), then requestId (client request)
            if ((*HeaderObject)->HasField(TEXT("requestIdEcho")))
            {
                NetworkHeader.requestId = (*HeaderObject)->GetStringField(TEXT("requestIdEcho"));
            }
            else if ((*HeaderObject)->HasField(TEXT("requestId")))
            {
                NetworkHeader.requestId = (*HeaderObject)->GetStringField(TEXT("requestId"));
            }

            // Time sync fields
            if ((*HeaderObject)->HasField(TEXT("clientSendMs")))
            {
                NetworkHeader.clientSendMs = static_cast<int64>((*HeaderObject)->GetNumberField(TEXT("clientSendMs")));
            }

            if ((*HeaderObject)->HasField(TEXT("serverRecvMs")))
            {
                NetworkHeader.serverRecvMs = static_cast<int64>((*HeaderObject)->GetNumberField(TEXT("serverRecvMs")));
            }

            if ((*HeaderObject)->HasField(TEXT("serverSendMs")))
            {
                NetworkHeader.serverSendMs = static_cast<int64>((*HeaderObject)->GetNumberField(TEXT("serverSendMs")));
            }

            if ((*HeaderObject)->HasField(TEXT("clientSendMsEcho")))
            {
                NetworkHeader.clientSendMsEcho = static_cast<int64>((*HeaderObject)->GetNumberField(TEXT("clientSendMsEcho")));
            }
        }
    }

    return NetworkHeader;
}

// Helper method to process time sync from network headers
//void JSONParser::ProcessTimeSyncFromHeader(const FString& JsonString, UTimeSyncService* TimeSyncService)
//{
//    if (!TimeSyncService)
//    {
//        return;
//    }
//
//    FNetworkHeaderStruct NetworkHeader = DeserializeNetworkHeader(JsonString);
//    
//    // Check for Echo fields (response from server with requestIdEcho and clientSendMsEcho)
//    if (!NetworkHeader.requestId.IsEmpty() && 
//        NetworkHeader.serverRecvMs > 0 && 
//        NetworkHeader.serverSendMs > 0)
//    {
//        // Server returns requestIdEcho as requestId field and includes timing data
//        bool bUpdated = TimeSyncService->UpdateTimeSyncData(
//            NetworkHeader.requestId, // Это на самом деле requestIdEcho от сервера
//            NetworkHeader.serverRecvMs,
//            NetworkHeader.serverSendMs
//        );
//        
//        if (bUpdated)
//        {
//            UE_LOG(LogTemp, Verbose, TEXT("JSONParser: Updated time sync for requestIdEcho: %s"), *NetworkHeader.requestId);
//        }
//    }
//    // Also log clientSendMsEcho if present for validation
//    if (NetworkHeader.clientSendMsEcho > 0)
//    {
//        UE_LOG(LogTemp, Verbose, TEXT("JSONParser: Received clientSendMsEcho: %lld"), NetworkHeader.clientSendMsEcho);
//    }
//}

void JSONParser::ProcessTimeSyncFromHeader(const FString& JsonString, UTimeSyncService* TimeSyncService)
{
	if (!TimeSyncService)
	{
		return;
	}

	FNetworkHeaderStruct NetworkHeader = DeserializeNetworkHeader(JsonString);

	// Приоритет requestIdEcho для ответов сервера
	FString RequestIdToUse;
	if (!NetworkHeader.requestId.IsEmpty())
	{
		RequestIdToUse = NetworkHeader.requestId; // Может быть requestIdEcho от сервера
	}

	// Обрабатываем только если есть серверные таймстампы
	if (!RequestIdToUse.IsEmpty() &&
		NetworkHeader.serverRecvMs > 0 &&
		NetworkHeader.serverSendMs > 0)
	{
		bool bUpdated = TimeSyncService->UpdateTimeSyncData(
			RequestIdToUse,
			NetworkHeader.serverRecvMs,
			NetworkHeader.serverSendMs
		);

		if (bUpdated)
		{
			UE_LOG(LogTemp, VeryVerbose, TEXT("JSONParser: Updated time sync for request: %s"), *RequestIdToUse);
		}
		else
		{
			UE_LOG(LogTemp, VeryVerbose, TEXT("JSONParser: Time sync update failed or filtered for: %s"), *RequestIdToUse);
		}
	}
}

// Experience system parsing functions
FExperienceUpdateStruct JSONParser::DeserializeExperienceUpdate(const TSharedPtr<FJsonObject>& Body)
{
	FExperienceUpdateStruct ExperienceUpdate;
	if (!Body.IsValid()) return ExperienceUpdate;

	if (Body->HasField("characterId"))
		ExperienceUpdate.characterId = Body->GetIntegerField("characterId");

	if (Body->HasField("oldLevel"))
		ExperienceUpdate.oldLevel = Body->GetIntegerField("oldLevel");

	if (Body->HasField("newLevel"))
		ExperienceUpdate.newLevel = Body->GetIntegerField("newLevel");

	if (Body->HasField("oldExperience"))
		ExperienceUpdate.oldExperience = Body->GetIntegerField("oldExperience");

	if (Body->HasField("newExperience"))
		ExperienceUpdate.newExperience = Body->GetIntegerField("newExperience");

	if (Body->HasField("experienceChange"))
		ExperienceUpdate.experienceChange = Body->GetIntegerField("experienceChange");

	if (Body->HasField("expForCurrentLevel"))
		ExperienceUpdate.expForCurrentLevel = Body->GetIntegerField("expForCurrentLevel");

	if (Body->HasField("expForNextLevel"))
		ExperienceUpdate.expForNextLevel = Body->GetIntegerField("expForNextLevel");

	if (Body->HasField("levelUp"))
		ExperienceUpdate.levelUp = Body->GetBoolField("levelUp");

	if (Body->HasField("reason"))
		ExperienceUpdate.reason = Body->GetStringField("reason");

	if (Body->HasField("sourceId"))
		ExperienceUpdate.sourceId = Body->GetIntegerField("sourceId");

	return ExperienceUpdate;
}

FExperienceUpdateStruct JSONParser::DeserializeExperienceUpdate(const FString& JsonString)
{
	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		return FExperienceUpdateStruct();

	TSharedPtr<FJsonObject> Body = Root->GetObjectField("body");
	if (!Body.IsValid())
		return FExperienceUpdateStruct();

	return JSONParser::DeserializeExperienceUpdate(Body);
}

FPlayerProgressionStruct JSONParser::DeserializePlayerProgression(const TSharedPtr<FJsonObject>& ProgressionObj)
{
	FPlayerProgressionStruct Progression;
	if (!ProgressionObj.IsValid()) return Progression;

	if (ProgressionObj->HasField("characterId"))
		Progression.characterId = ProgressionObj->GetIntegerField("characterId");

	if (ProgressionObj->HasField("currentLevel"))
		Progression.currentLevel = ProgressionObj->GetIntegerField("currentLevel");

	if (ProgressionObj->HasField("currentExperience"))
		Progression.currentExperience = ProgressionObj->GetIntegerField("currentExperience");

	if (ProgressionObj->HasField("totalExperience"))
		Progression.totalExperience = ProgressionObj->GetIntegerField("totalExperience");

	if (ProgressionObj->HasField("expForNextLevel"))
		Progression.expForNextLevel = ProgressionObj->GetIntegerField("expForNextLevel");

	if (ProgressionObj->HasField("expForCurrentLevel"))
		Progression.expForCurrentLevel = ProgressionObj->GetIntegerField("expForCurrentLevel");

	if (ProgressionObj->HasField("hasPendingLevelUp"))
		Progression.bHasPendingLevelUp = ProgressionObj->GetBoolField("hasPendingLevelUp");

	if (ProgressionObj->HasField("pendingLevelGained"))
		Progression.pendingLevelGained = ProgressionObj->GetIntegerField("pendingLevelGained");

	return Progression;
}

FPlayerProgressionStruct JSONParser::DeserializePlayerProgression(const FString& JsonString)
{
	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		return FPlayerProgressionStruct();

	TSharedPtr<FJsonObject> Body = Root->GetObjectField("body");
	if (!Body.IsValid())
		return FPlayerProgressionStruct();

	TSharedPtr<FJsonObject> Progression = Body->GetObjectField("progression");
	if (!Progression.IsValid())
		return FPlayerProgressionStruct();

	return JSONParser::DeserializePlayerProgression(Progression);
}

FExperienceGainEventStruct JSONParser::DeserializeExperienceGainEvent(const TSharedPtr<FJsonObject>& EventObj)
{
	FExperienceGainEventStruct Event;
	if (!EventObj.IsValid()) return Event;

	if (EventObj->HasField("experienceGained"))
		Event.experienceGained = EventObj->GetIntegerField("experienceGained");

	if (EventObj->HasField("reason"))
		Event.reasonText = EventObj->GetStringField("reason");

	if (EventObj->HasField("sourceId"))
		Event.sourceId = EventObj->GetIntegerField("sourceId");

	if (EventObj->HasField("sourceName"))
		Event.sourceName = EventObj->GetStringField("sourceName");

	// Parse reason enum from string
	if (!Event.reasonText.IsEmpty())
	{
		if (Event.reasonText.Equals(TEXT("mob_kill"), ESearchCase::IgnoreCase))
			Event.reason = EExperienceReason::MobKill;
		else if (Event.reasonText.Equals(TEXT("quest_complete"), ESearchCase::IgnoreCase))
			Event.reason = EExperienceReason::QuestComplete;
		else if (Event.reasonText.Equals(TEXT("quest_turn_in"), ESearchCase::IgnoreCase))
			Event.reason = EExperienceReason::QuestTurnIn;
		else if (Event.reasonText.Equals(TEXT("discovery"), ESearchCase::IgnoreCase))
			Event.reason = EExperienceReason::Discovery;
		else if (Event.reasonText.Equals(TEXT("crafting"), ESearchCase::IgnoreCase))
			Event.reason = EExperienceReason::Crafting;
		else if (Event.reasonText.Equals(TEXT("gathering"), ESearchCase::IgnoreCase))
			Event.reason = EExperienceReason::Gathering;
		else if (Event.reasonText.Equals(TEXT("pvp_kill"), ESearchCase::IgnoreCase))
			Event.reason = EExperienceReason::PvPKill;
		else if (Event.reasonText.Equals(TEXT("boss_kill"), ESearchCase::IgnoreCase))
			Event.reason = EExperienceReason::BossKill;
		else if (Event.reasonText.Equals(TEXT("group_bonus"), ESearchCase::IgnoreCase))
			Event.reason = EExperienceReason::GroupBonus;
		else if (Event.reasonText.Equals(TEXT("event"), ESearchCase::IgnoreCase))
			Event.reason = EExperienceReason::Event;
		else if (Event.reasonText.Equals(TEXT("admin"), ESearchCase::IgnoreCase))
			Event.reason = EExperienceReason::Admin;
		else
			Event.reason = EExperienceReason::None;
	}

	// Set timestamp to now (server timestamp parsing could be added here if needed)
	Event.timestamp = FDateTime::Now();

	return Event;
}

FExperienceGainEventStruct JSONParser::DeserializeExperienceGainEvent(const FString& JsonString)
{
	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		return FExperienceGainEventStruct();

	TSharedPtr<FJsonObject> Body = Root->GetObjectField("body");
	if (!Body.IsValid())
		return FExperienceGainEventStruct();

	TSharedPtr<FJsonObject> Event = Body->GetObjectField("experienceEvent");
	if (!Event.IsValid())
	{
		// Try to parse directly from body if no experienceEvent object
		return JSONParser::DeserializeExperienceGainEvent(Body);
	}

	return JSONParser::DeserializeExperienceGainEvent(Event);
}

FPlayerStatsUpdateStruct JSONParser::DeserializePlayerStatsUpdate(const FString& JsonString)
{
    return PlayerAttributeParser::DeserializePlayerStatsUpdate(JsonString);
}

FPlayerStatsUpdateStruct JSONParser::DeserializePlayerStatsUpdate(const TSharedPtr<FJsonObject>& Body)
{
    return PlayerAttributeParser::DeserializePlayerStatsUpdate(Body);
}