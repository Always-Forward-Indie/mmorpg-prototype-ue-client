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
        HeaderObject->SetNumberField(TEXT("clientSendMs"), ClientSendMs);
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
        MainJsonObject->SetObjectField(TEXT("header"), HeaderObject);
    }

    if (BodyObject->Values.Num() > 0)
    {
        MainJsonObject->SetObjectField(TEXT("body"), BodyObject);
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
    HeaderObject->SetStringField(TEXT("eventType"), EventType);

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
            // Per protocol: create a "timestamps" sub-object in header
            // with clientSendMsEcho and requestId. NetworkSenderWorker will
            // update clientSendMsEcho with the precise value right before sending.
            TSharedPtr<FJsonObject> TimestampsObject = MakeShareable(new FJsonObject);
            TimestampsObject->SetStringField(TEXT("requestId"), RequestId);
            TimestampsObject->SetNumberField(TEXT("clientSendMsEcho"), 0); // placeholder, updated by sender
            HeaderObject->SetObjectField(TEXT("timestamps"), TimestampsObject);
            
            UE_LOG(LogTemp, Verbose, TEXT("JSONParser::SerializeJsonWithTimeSync - Added timestamps.requestId: %s (clientSendMsEcho will be set by NetworkSenderWorker)"), 
                *RequestId);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("JSONParser::SerializeJsonWithTimeSync - RequestId is empty, not adding timestamps sub-object"));
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
        MainJsonObject->SetObjectField(TEXT("header"), HeaderObject);
    }

    if (BodyObject->Values.Num() > 0)
    {
        MainJsonObject->SetObjectField(TEXT("body"), BodyObject);
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
	 Pos.positionX = PosObj->GetNumberField(TEXT("x"));
	 Pos.positionY = PosObj->GetNumberField(TEXT("y"));
	 Pos.positionZ = PosObj->GetNumberField(TEXT("z"));
	 Pos.rotationZ = PosObj->GetNumberField(TEXT("rotationZ"));
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
		 Attr.attributeId = AttrObj->GetIntegerField(TEXT("id"));
		 Attr.attributeName = AttrObj->GetStringField(TEXT("name"));
		 Attr.attributeSlug = AttrObj->GetStringField(TEXT("slug"));
		 Attr.attributeValue = AttrObj->GetIntegerField(TEXT("value"));
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
	 Character.characterId = CD->GetIntegerField(TEXT("id"));
	 Character.characterName = CD->GetStringField(TEXT("name"));
	 Character.characterClass = CD->GetStringField(TEXT("class"));
	 Character.characterRace = CD->GetStringField(TEXT("race"));
	 if (CD->HasField(TEXT("gender")))
	 {
		 Character.characterGender = CD->GetStringField(TEXT("gender"));
	 }
	 Character.characterLevel = CD->GetIntegerField(TEXT("level"));

	 //is dead
	 if (CD->HasField(TEXT("isDead"))) {
		 Character.bIsDead = CD->GetBoolField(TEXT("isDead"));
	 } else {
		 Character.bIsDead = false; // Default value if not present
	 }

	 if (CD->HasField(TEXT("exp"))) {
		 TSharedPtr<FJsonObject> Exp = CD->GetObjectField(TEXT("exp"));
		 Character.characterExperiencePoints = Exp->GetIntegerField(TEXT("current"));
		 Character.characterExpForLevelStart = Exp->GetIntegerField(TEXT("levelStart"));
		 Character.characterExpForLevelEnd = Exp->GetIntegerField(TEXT("levelEnd"));
	 }

	 if (CD->HasField(TEXT("stats"))) {
		 TSharedPtr<FJsonObject> Stats = CD->GetObjectField(TEXT("stats"));
		 TSharedPtr<FJsonObject> Health = Stats->GetObjectField(TEXT("health"));
		 TSharedPtr<FJsonObject> Mana = Stats->GetObjectField(TEXT("mana"));
		 Character.characterCurrentHealth = Health->GetIntegerField(TEXT("current"));
		 Character.characterCurrentMana = Mana->GetIntegerField(TEXT("current"));

		 // Also store max values in characterAttributes so RefreshHUD can use them
		 // immediately after spawn, before the first stats_update arrives.
		 const int32 MaxHealth = Health->HasField(TEXT("max")) ? Health->GetIntegerField(TEXT("max")) : 0;
		 const int32 MaxMana   = Mana->HasField(TEXT("max"))   ? Mana->GetIntegerField(TEXT("max"))   : 0;

		 if (MaxHealth > 0)
		 {
			 FAttributeDataStruct HealthAttr;
			 HealthAttr.attributeSlug  = TEXT("max_health");
			 HealthAttr.attributeName  = TEXT("Max Health");
			 HealthAttr.attributeValue = MaxHealth;
			 Character.characterAttributes.attributesData.Add(TEXT("max_health"), HealthAttr);
		 }

		 if (MaxMana >= 0)
		 {
			 FAttributeDataStruct ManaAttr;
			 ManaAttr.attributeSlug  = TEXT("max_mana");
			 ManaAttr.attributeName  = TEXT("Max Mana");
			 ManaAttr.attributeValue = MaxMana;
			 Character.characterAttributes.attributesData.Add(TEXT("max_mana"), ManaAttr);
		 }
	 }

	 if (CD->HasField(TEXT("position")))
		 Character.characterPosition = JSONParser::DeserializePositionData(CD->GetObjectField(TEXT("position")));

	 if (CD->HasField(TEXT("attributes")))
		 Character.characterAttributes.attributesData = JSONParser::DeserializeAttributesArray(CD->GetArrayField(TEXT("attributes")));

	 CD->TryGetStringField(TEXT("equippedTitleSlug"), Character.equippedTitleSlug);
	 CD->TryGetStringField(TEXT("equippedTitleDisplayName"), Character.equippedTitleDisplayName);

	 return Character;
 }

 // Entry point for string-based JSON input
 FCharacterDataStruct JSONParser::DeserializeCharacterData(const FString& JsonString)
 {
	 TSharedPtr<FJsonObject> Root;
	 TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	 if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()) return FCharacterDataStruct();
	 TSharedPtr<FJsonObject> Body = Root->GetObjectField(TEXT("body"));
	 return JSONParser::DeserializeCharacterData(Body->GetObjectField(TEXT("character")));
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

		 // Per protocol (�1.4 pongClient): timestamps may also be a root-level object
		 const TSharedPtr<FJsonObject>* TimestampsObject = nullptr;
		 if (JsonObject->TryGetObjectField(TEXT("timestamps"), TimestampsObject) && TimestampsObject != nullptr)
		 {
			 if ((*TimestampsObject)->HasField(TEXT("serverRecvMs")))
			 {
				 MessageData.serverRecvMs = static_cast<int64>((*TimestampsObject)->GetNumberField(TEXT("serverRecvMs")));
			 }
			 if ((*TimestampsObject)->HasField(TEXT("serverSendMs")))
			 {
				 MessageData.serverSendMs = static_cast<int64>((*TimestampsObject)->GetNumberField(TEXT("serverSendMs")));
			 }
			 if ((*TimestampsObject)->HasField(TEXT("clientSendMsEcho")))
			 {
				 MessageData.clientSendMsEcho = static_cast<int64>((*TimestampsObject)->GetNumberField(TEXT("clientSendMsEcho")));
				 MessageData.clientSendMs = MessageData.clientSendMsEcho;
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

//deserialize a JSON containing a list of characters (login screen — uses slug fields from server v0.1.12+)
TArray<FLoginCharacterEntry> JSONParser::DeserializeLoginCharactersList(const FString& JsonString)
{
	TArray<FLoginCharacterEntry> CharacterList;
	TSharedPtr<FJsonObject> JsonObject;

	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		return CharacterList;
	}

	const TSharedPtr<FJsonObject>* BodyObject = nullptr;
	if (!JsonObject->TryGetObjectField(TEXT("body"), BodyObject) || BodyObject == nullptr)
	{
		return CharacterList;
	}

	const TArray<TSharedPtr<FJsonValue>>* CharactersArray = nullptr;
	if (!(*BodyObject)->TryGetArrayField(TEXT("charactersList"), CharactersArray) || CharactersArray == nullptr)
	{
		return CharacterList;
	}

	for (const TSharedPtr<FJsonValue>& CharacterValue : *CharactersArray)
	{
		TSharedPtr<FJsonObject> CharacterObject = CharacterValue->AsObject();
		if (!CharacterObject.IsValid()) continue;

		FLoginCharacterEntry Entry;

		CharacterObject->TryGetNumberField(TEXT("characterId"),    Entry.CharacterId);
		CharacterObject->TryGetStringField(TEXT("characterName"),  Entry.CharacterName);
		CharacterObject->TryGetNumberField(TEXT("characterLevel"), Entry.CharacterLevel);

		// Server v0.1.12+: slug fields
		CharacterObject->TryGetStringField(TEXT("classSlug"),   Entry.CharacterClass);
		CharacterObject->TryGetStringField(TEXT("raceSlug"),    Entry.CharacterRace);
		CharacterObject->TryGetStringField(TEXT("genderSlug"),  Entry.CharacterGender);

		// Equipment preview array
		const TArray<TSharedPtr<FJsonValue>>* EquipArray = nullptr;
		if (CharacterObject->TryGetArrayField(TEXT("equipment"), EquipArray) && EquipArray != nullptr)
		{
			for (const TSharedPtr<FJsonValue>& EquipValue : *EquipArray)
			{
				TSharedPtr<FJsonObject> EquipObject = EquipValue->AsObject();
				if (!EquipObject.IsValid()) continue;

				FLoginEquipmentEntry EquipEntry;
				EquipObject->TryGetNumberField(TEXT("slotId"),   EquipEntry.SlotId);
				EquipObject->TryGetStringField(TEXT("itemSlug"), EquipEntry.ItemSlug);
				Entry.Equipment.Add(MoveTemp(EquipEntry));
			}
		}

		CharacterList.Add(MoveTemp(Entry));
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
	const TArray<TSharedPtr<FJsonValue>>* MobsArray;

	if (Body->TryGetArrayField(TEXT("mobs"), MobsArray))
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

// Parse item attribute from JSON object
FItemAttributeStruct JSONParser::DeserializeItemAttribute(const TSharedPtr<FJsonObject>& AttributeObj)
{
	FItemAttributeStruct ItemAttribute;
	if (!AttributeObj.IsValid()) return ItemAttribute;

	if (AttributeObj->HasField(TEXT("id")))
		ItemAttribute.id = AttributeObj->GetIntegerField(TEXT("id"));
	
	if (AttributeObj->HasField(TEXT("name")))
		ItemAttribute.name = AttributeObj->GetStringField(TEXT("name"));
	
	if (AttributeObj->HasField(TEXT("slug")))
		ItemAttribute.slug = AttributeObj->GetStringField(TEXT("slug"));
	
	if (AttributeObj->HasField(TEXT("value")))
		ItemAttribute.value = AttributeObj->GetNumberField(TEXT("value"));

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

	if (ItemObj->HasField(TEXT("id")))
		Item.id = ItemObj->GetIntegerField(TEXT("id"));
	
	if (ItemObj->HasField(TEXT("name")))
		Item.name = ItemObj->GetStringField(TEXT("name"));
	
	if (ItemObj->HasField(TEXT("slug")))
		Item.slug = ItemObj->GetStringField(TEXT("slug"));
	
	if (ItemObj->HasField(TEXT("description")))
		Item.description = ItemObj->GetStringField(TEXT("description"));
	
	if (ItemObj->HasField(TEXT("isQuestItem")))
		Item.isQuestItem = ItemObj->GetBoolField(TEXT("isQuestItem"));
	
	if (ItemObj->HasField(TEXT("itemType")))
	{
		Item.itemType = static_cast<EItemType>(ItemObj->GetIntegerField(TEXT("itemType")));
	}
	
	if (ItemObj->HasField(TEXT("itemTypeName")))
		Item.itemTypeName = ItemObj->GetStringField(TEXT("itemTypeName"));
	
	if (ItemObj->HasField(TEXT("itemTypeSlug")))
		Item.itemTypeSlug = ItemObj->GetStringField(TEXT("itemTypeSlug"));
	
	if (ItemObj->HasField(TEXT("attributes")))
	{
		const TArray<TSharedPtr<FJsonValue>>* AttributesArray;
		if (ItemObj->TryGetArrayField(TEXT("attributes"), AttributesArray))
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

	if (DroppedItemObj->HasField(TEXT("uid")))
		DroppedItem.uid = DroppedItemObj->GetIntegerField(TEXT("uid"));
	
	if (DroppedItemObj->HasField(TEXT("itemId")))
		DroppedItem.itemId = DroppedItemObj->GetIntegerField(TEXT("itemId"));
	
	if (DroppedItemObj->HasField(TEXT("droppedByMobUID")))
	{
		// Field can be a string or a number depending on server format.
		// Server sends 0 / "0" / "" when the item was NOT dropped by a mob —
		// treat all of those as "no mob" so IsEmpty() works correctly downstream.
		FString StrVal;
		double NumVal = 0.0;
		if (DroppedItemObj->TryGetStringField(TEXT("droppedByMobUID"), StrVal))
		{
			DroppedItem.droppedByMobUID = (StrVal == TEXT("0")) ? TEXT("") : StrVal;
		}
		else if (DroppedItemObj->TryGetNumberField(TEXT("droppedByMobUID"), NumVal))
		{
			const int32 IntVal = (int32)NumVal;
			DroppedItem.droppedByMobUID = (IntVal == 0) ? TEXT("") : FString::FromInt(IntVal);
		}
	}
	
	if (DroppedItemObj->HasField(TEXT("droppedByCharacterId")))
		DroppedItem.droppedByCharacterId = DroppedItemObj->GetIntegerField(TEXT("droppedByCharacterId"));

	if (DroppedItemObj->HasField(TEXT("quantity")))
		DroppedItem.quantity = DroppedItemObj->GetIntegerField(TEXT("quantity"));
	
	if (DroppedItemObj->HasField(TEXT("canBePickedUp")))
		DroppedItem.canBePickedUp = DroppedItemObj->GetBoolField(TEXT("canBePickedUp"));

	if (DroppedItemObj->HasField(TEXT("reservedForCharacterId")))
		DroppedItem.reservedForCharacterId = DroppedItemObj->GetIntegerField(TEXT("reservedForCharacterId"));

	if (DroppedItemObj->HasField(TEXT("reservationSecondsLeft")))
	{
		double ResvSecs = 0.0;
		DroppedItemObj->TryGetNumberField(TEXT("reservationSecondsLeft"), ResvSecs);
		DroppedItem.reservationSecondsLeft = static_cast<int64>(ResvSecs);
	}
	
	if (DroppedItemObj->HasField(TEXT("position")) && DroppedItemObj->GetObjectField(TEXT("position")).IsValid())
		DroppedItem.position = JSONParser::DeserializePositionData(DroppedItemObj->GetObjectField(TEXT("position")));
	
	if (DroppedItemObj->HasField(TEXT("item")) && DroppedItemObj->GetObjectField(TEXT("item")).IsValid())
		DroppedItem.item = JSONParser::DeserializeItemData(DroppedItemObj->GetObjectField(TEXT("item")));

	return DroppedItem;
}

// Parse item drop response from JSON object
FItemDropResponseStruct JSONParser::DeserializeItemDropResponse(const TSharedPtr<FJsonObject>& Body)
{
	FItemDropResponseStruct ItemDropResponse;
	if (!Body.IsValid()) return ItemDropResponse;

	// Server sends the array as "items" (itemDrop broadcast) or legacy "droppedItems"
	const TArray<TSharedPtr<FJsonValue>>* DroppedItemsArray = nullptr;
	if (!Body->TryGetArrayField(TEXT("items"), DroppedItemsArray))
		Body->TryGetArrayField(TEXT("droppedItems"), DroppedItemsArray);

	if (DroppedItemsArray)
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

	if (AnimationObj->HasField(TEXT("animationName")))
		AnimationData.AnimationName = AnimationObj->GetStringField(TEXT("animationName"));
	
	if (AnimationObj->HasField(TEXT("characterId")))
		AnimationData.CharacterId = AnimationObj->GetIntegerField(TEXT("characterId"));
	
	if (AnimationObj->HasField(TEXT("duration")))
		AnimationData.Duration = AnimationObj->GetNumberField(TEXT("duration"));
	
	if (AnimationObj->HasField(TEXT("isLooping")))
		AnimationData.bIsLooping = AnimationObj->GetBoolField(TEXT("isLooping"));

	return AnimationData;
}

FCombatActionData JSONParser::DeserializeCombatAction(const TSharedPtr<FJsonObject>& ActionObj)
{
	FCombatActionData ActionData;
	if (!ActionObj.IsValid()) return ActionData;

	if (ActionObj->HasField(TEXT("actionId")))
		ActionData.ActionId = ActionObj->GetIntegerField(TEXT("actionId"));
	
	if (ActionObj->HasField(TEXT("actionName")))
		ActionData.ActionName = ActionObj->GetStringField(TEXT("actionName"));
	
	if (ActionObj->HasField(TEXT("actionType")))
		ActionData.ActionType = ActionObj->GetIntegerField(TEXT("actionType"));
	
	if (ActionObj->HasField(TEXT("casterId")))
		ActionData.CasterId = ActionObj->GetIntegerField(TEXT("casterId"));
	
	if (ActionObj->HasField(TEXT("targetId")))
		ActionData.TargetId = ActionObj->GetIntegerField(TEXT("targetId"));
	
	if (ActionObj->HasField(TEXT("targetType")))
		ActionData.TargetType = ActionObj->GetIntegerField(TEXT("targetType"));
	
	if (ActionObj->HasField(TEXT("targetTypeString")))
		ActionData.TargetTypeString = ActionObj->GetStringField(TEXT("targetTypeString"));

	return ActionData;
}

FCombatResultData JSONParser::DeserializeCombatResult(const TSharedPtr<FJsonObject>& ResultObj)
{
	FCombatResultData ResultData;
	if (!ResultObj.IsValid()) return ResultData;

	if (ResultObj->HasField(TEXT("actionId")))
		ResultData.ActionId = ResultObj->GetIntegerField(TEXT("actionId"));
	
	if (ResultObj->HasField(TEXT("casterId")))
		ResultData.CasterId = ResultObj->GetIntegerField(TEXT("casterId"));
	
	if (ResultObj->HasField(TEXT("damageDealt")))
		ResultData.DamageDealt = ResultObj->GetIntegerField(TEXT("damageDealt"));
	
	if (ResultObj->HasField(TEXT("healingDone")))
		ResultData.HealingDone = ResultObj->GetIntegerField(TEXT("healingDone"));
	
	if (ResultObj->HasField(TEXT("isBlocked")))
		ResultData.bIsBlocked = ResultObj->GetBoolField(TEXT("isBlocked"));
	
	if (ResultObj->HasField(TEXT("isCritical")))
		ResultData.bIsCritical = ResultObj->GetBoolField(TEXT("isCritical"));
	
	if (ResultObj->HasField(TEXT("isDodged")))
		ResultData.bIsDodged = ResultObj->GetBoolField(TEXT("isDodged"));
	
	if (ResultObj->HasField(TEXT("isResisted")))
		ResultData.bIsResisted = ResultObj->GetBoolField(TEXT("isResisted"));
	
	if (ResultObj->HasField(TEXT("remainingHealth")))
		ResultData.RemainingHealth = ResultObj->GetIntegerField(TEXT("remainingHealth"));
	
	if (ResultObj->HasField(TEXT("remainingMana")))
		ResultData.RemainingMana = ResultObj->GetIntegerField(TEXT("remainingMana"));
	
	if (ResultObj->HasField(TEXT("targetDied")))
		ResultData.bTargetDied = ResultObj->GetBoolField(TEXT("targetDied"));
	
	if (ResultObj->HasField(TEXT("isDamaged")))
		ResultData.bIsDamaged = ResultObj->GetBoolField(TEXT("isDamaged"));
	
	if (ResultObj->HasField(TEXT("targetId")))
		ResultData.TargetId = ResultObj->GetIntegerField(TEXT("targetId"));
	
	if (ResultObj->HasField(TEXT("targetType")))
		ResultData.TargetType = ResultObj->GetIntegerField(TEXT("targetType"));
	
	if (ResultObj->HasField(TEXT("targetTypeString")))
		ResultData.TargetTypeString = ResultObj->GetStringField(TEXT("targetTypeString"));

	return ResultData;
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
// Helper: parse item fields from a JSON object into an FInventoryItemStruct.
// Used both for the flat format and for the nested "item" sub-object.
static void ParseItemFields(const TSharedPtr<FJsonObject>& Src, FInventoryItemStruct& Out)
{
	if (!Src.IsValid()) return;

	if (Src->HasField(TEXT("slug")))          Out.slug          = Src->GetStringField(TEXT("slug"));
	if (Src->HasField(TEXT("name")))          Out.name          = Src->GetStringField(TEXT("name"));
	if (Src->HasField(TEXT("description")))   Out.description   = Src->GetStringField(TEXT("description"));
	if (Src->HasField(TEXT("itemTypeSlug")))  Out.itemTypeSlug  = Src->GetStringField(TEXT("itemTypeSlug"));
	if (Src->HasField(TEXT("itemTypeName")))  { Out.type = Src->GetStringField(TEXT("itemTypeName")); Out.itemTypeName = Out.type; }
	if (Src->HasField(TEXT("raritySlug")))    Out.raritySlug    = Src->GetStringField(TEXT("raritySlug"));
	if (Src->HasField(TEXT("rarityName")))    Out.rarity        = Src->GetStringField(TEXT("rarityName"));
	if (Src->HasField(TEXT("equipSlotSlug"))) Out.equipSlotSlug = Src->GetStringField(TEXT("equipSlotSlug"));
	if (Src->HasField(TEXT("masterySlug")))   Out.masterySlug   = Src->GetStringField(TEXT("masterySlug"));
	if (Src->HasField(TEXT("setSlug")))       Out.setSlug       = Src->GetStringField(TEXT("setSlug"));

	if (Src->HasField(TEXT("weight")))            Out.weight           = (float)Src->GetNumberField(TEXT("weight"));
	if (Src->HasField(TEXT("stackMax")))          Out.stackSize        = Src->GetIntegerField(TEXT("stackMax"));
	if (Src->HasField(TEXT("durabilityMax")))     Out.durabilityMax    = Src->GetIntegerField(TEXT("durabilityMax"));
	if (Src->HasField(TEXT("levelRequirement")))  { Out.levelRequirement = Src->GetIntegerField(TEXT("levelRequirement")); Out.level_requirement = Out.levelRequirement; }
	if (Src->HasField(TEXT("vendorPriceBuy")))    Out.priceBuy         = Src->GetIntegerField(TEXT("vendorPriceBuy"));
	if (Src->HasField(TEXT("vendorPriceSell")))   Out.priceSell        = Src->GetIntegerField(TEXT("vendorPriceSell"));

	if (Src->HasField(TEXT("isDurable")))     Out.isDurable     = Src->GetBoolField(TEXT("isDurable"));
	if (Src->HasField(TEXT("isTradable")))    Out.isTradable    = Src->GetBoolField(TEXT("isTradable"));
	if (Src->HasField(TEXT("isEquippable")))  Out.isEquippable  = Src->GetBoolField(TEXT("isEquippable"));
	if (Src->HasField(TEXT("isUsable")))      Out.isUsable      = Src->GetBoolField(TEXT("isUsable"));
	if (Src->HasField(TEXT("isQuestItem")))   Out.isQuestItem   = Src->GetBoolField(TEXT("isQuestItem"));
	if (Src->HasField(TEXT("isContainer")))   Out.isContainer   = Src->GetBoolField(TEXT("isContainer"));
	if (Src->HasField(TEXT("isTwoHanded")))   Out.isTwoHanded   = Src->GetBoolField(TEXT("isTwoHanded"));

	if (Src->HasField(TEXT("killCount")))     Out.killCount     = Src->GetIntegerField(TEXT("killCount"));

	// Legacy: derive isEquippable from numeric equipSlot field when explicit bool is absent
	if (!Src->HasField(TEXT("isEquippable")) && Src->HasField(TEXT("equipSlot")))
	{
		int32 SlotId = Src->GetIntegerField(TEXT("equipSlot"));
		Out.isEquippable = (SlotId > 0);
	}

	// Typed attribute array (supports apply_on field)
	if (Src->HasTypedField<EJson::Array>(TEXT("attributes")))
	{
		const TArray<TSharedPtr<FJsonValue>> AttrArray = Src->GetArrayField(TEXT("attributes"));
		for (const TSharedPtr<FJsonValue>& AttrVal : AttrArray)
		{
			TSharedPtr<FJsonObject> AttrObj = AttrVal->AsObject();
			if (!AttrObj.IsValid()) continue;

			FItemAttributeStruct TypedAttr;
			if (AttrObj->TryGetStringField(TEXT("slug"), TypedAttr.slug)) {}
			if (AttrObj->TryGetStringField(TEXT("name"), TypedAttr.name)) {}
			if (AttrObj->TryGetStringField(TEXT("apply_on"), TypedAttr.apply_on)) {}
			double AttrVal2 = 0.0;
			if (AttrObj->TryGetNumberField(TEXT("value"), AttrVal2))
				TypedAttr.value = (float)AttrVal2;
			Out.itemAttributes.Add(TypedAttr);

			// Also populate legacy TMap for backward compat
			const FString& DisplayName = TypedAttr.name.IsEmpty() ? TypedAttr.slug : TypedAttr.name;
			if (!DisplayName.IsEmpty())
				Out.attributes.Add(DisplayName, FString::SanitizeFloat(TypedAttr.value));
		}
	}

	// Use effects (consumables)
	if (Src->HasTypedField<EJson::Array>(TEXT("useEffects")))
	{
		const TArray<TSharedPtr<FJsonValue>> EffectArray = Src->GetArrayField(TEXT("useEffects"));
		for (const TSharedPtr<FJsonValue>& EffVal : EffectArray)
		{
			TSharedPtr<FJsonObject> EffObj = EffVal->AsObject();
			if (!EffObj.IsValid()) continue;

			FItemUseEffectEntry Effect;
			if (EffObj->HasField(TEXT("effectSlug")))       Effect.effectSlug       = EffObj->GetStringField(TEXT("effectSlug"));
			if (EffObj->HasField(TEXT("attributeSlug")))    Effect.attributeSlug    = EffObj->GetStringField(TEXT("attributeSlug"));
			if (EffObj->HasField(TEXT("value")))            Effect.value            = (float)EffObj->GetNumberField(TEXT("value"));
			if (EffObj->HasField(TEXT("isInstant")))        Effect.isInstant        = EffObj->GetBoolField(TEXT("isInstant"));
			if (EffObj->HasField(TEXT("durationSeconds")))  Effect.durationSeconds  = EffObj->GetIntegerField(TEXT("durationSeconds"));
			if (EffObj->HasField(TEXT("tickMs")))           Effect.tickMs           = EffObj->GetIntegerField(TEXT("tickMs"));
			if (EffObj->HasField(TEXT("cooldownSeconds")))  Effect.cooldownSeconds  = EffObj->GetIntegerField(TEXT("cooldownSeconds"));
			Out.useEffects.Add(Effect);
		}
	}
}

FInventoryItemStruct JSONParser::DeserializeInventoryItem(const TSharedPtr<FJsonObject>& ItemObj)
{
	FInventoryItemStruct Item;
	if (!ItemObj.IsValid()) return Item;

	// Top-level inventory record fields
	if (ItemObj->HasField(TEXT("id")))       Item.id       = ItemObj->GetIntegerField(TEXT("id"));   // player_inventory PK
	if (ItemObj->HasField(TEXT("itemId")))   Item.itemId   = ItemObj->GetIntegerField(TEXT("itemId"));
	if (ItemObj->HasField(TEXT("quantity"))) Item.quantity = ItemObj->GetIntegerField(TEXT("quantity"));

	// Durability from top-level (overridden by nested "item" if present)
	if (ItemObj->HasField(TEXT("durabilityCurrent"))) Item.durabilityCurrent = ItemObj->GetIntegerField(TEXT("durabilityCurrent"));
	if (ItemObj->HasField(TEXT("durabilityMax")))     Item.durabilityMax     = ItemObj->GetIntegerField(TEXT("durabilityMax"));

	// is_equipped flag lives at top-level in some server responses
	if (ItemObj->HasField(TEXT("is_equipped"))) Item.is_equipped = ItemObj->GetBoolField(TEXT("is_equipped"));

	// Flat fields (older format or already-flattened responses)
	ParseItemFields(ItemObj, Item);

	// Nested "item" sub-object from protocol (getPlayerInventory format):
	// { "id": 101, "itemId": 5, "quantity": 1, "item": { ... } }
	const TSharedPtr<FJsonObject>* NestedItemPtr = nullptr;
	if (ItemObj->TryGetObjectField(TEXT("item"), NestedItemPtr) && NestedItemPtr && (*NestedItemPtr).IsValid())
	{
		const TSharedPtr<FJsonObject>& NestedItem = *NestedItemPtr;
		ParseItemFields(NestedItem, Item);

		// id inside nested item is the template id, not the inventory record id - do not overwrite Item.id
		// but we do want itemId from nested if top-level was missing
		if (Item.itemId == 0 && NestedItem->HasField(TEXT("id")))
			Item.itemId = NestedItem->GetIntegerField(TEXT("id"));
	}

	// Ensure durabilityCurrent has a sane default
	if (Item.durabilityCurrent == 0 && Item.durabilityMax > 0 && Item.isDurable)
		Item.durabilityCurrent = Item.durabilityMax;

	return Item;
}


FCharacterInventoryStruct JSONParser::DeserializeCharacterInventory(const FString& JsonString)
{
	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		return FCharacterInventoryStruct();

	TSharedPtr<FJsonObject> Body = Root->GetObjectField(TEXT("body"));
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
	if (Body->TryGetNumberField(TEXT("characterId"), CharacterId))
	{
		Inventory.characterId = CharacterId;
	}

	int32 Gold = 0;
	if (Body->TryGetNumberField(TEXT("gold"), Gold))
	{
		Inventory.gold = Gold;
	}

	// Items
	const TArray<TSharedPtr<FJsonValue>>* ItemsArray = nullptr;
	if (Body->TryGetArrayField(TEXT("items"), ItemsArray))
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
	
	if (UpdateObj->HasField(TEXT("eventType")))
		Update.eventType = UpdateObj->GetStringField(TEXT("eventType"));
	
	if (UpdateObj->HasField(TEXT("characterId")))
		Update.characterId = UpdateObj->GetIntegerField(TEXT("characterId"));

	// Parse data object
	if (UpdateObj->HasField(TEXT("data")))
	{
		TSharedPtr<FJsonObject> DataObj = UpdateObj->GetObjectField(TEXT("data"));
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

	if (ItemObj->HasField(TEXT("itemId")))
		Item.itemId = ItemObj->GetIntegerField(TEXT("itemId"));

	if (ItemObj->HasField(TEXT("itemSlug")))
		Item.itemSlug = ItemObj->GetStringField(TEXT("itemSlug"));

	if (ItemObj->HasField(TEXT("quantity")))
		Item.quantity = ItemObj->GetIntegerField(TEXT("quantity"));

	if (ItemObj->HasField(TEXT("name")))
		Item.name = ItemObj->GetStringField(TEXT("name"));

	if (ItemObj->HasField(TEXT("description")))
		Item.description = ItemObj->GetStringField(TEXT("description"));

	if (ItemObj->HasField(TEXT("rarityId")))
		Item.rarityId = ItemObj->GetIntegerField(TEXT("rarityId"));

	if (ItemObj->HasField(TEXT("rarityName")))
		Item.rarityName = ItemObj->GetStringField(TEXT("rarityName"));

	if (ItemObj->HasField(TEXT("itemType")))
		Item.itemType = ItemObj->GetStringField(TEXT("itemType"));

	if (ItemObj->HasField(TEXT("weight")))
		Item.weight = ItemObj->GetNumberField(TEXT("weight"));

	if (ItemObj->HasField(TEXT("addedToInventory")))
		Item.addedToInventory = ItemObj->GetBoolField(TEXT("addedToInventory"));

	if (ItemObj->HasField(TEXT("isHarvestItem")))
		Item.isHarvestItem = ItemObj->GetBoolField(TEXT("isHarvestItem"));

	return Item;
}

FHarvestStartedStruct JSONParser::DeserializeHarvestStarted(const TSharedPtr<FJsonObject>& Body)
{
	FHarvestStartedStruct HarvestStarted;
	if (!Body.IsValid()) return HarvestStarted;

	if (Body->HasField(TEXT("type")))
		HarvestStarted.type = Body->GetStringField(TEXT("type"));

	if (Body->HasField(TEXT("clientId")))
		HarvestStarted.clientId = Body->GetIntegerField(TEXT("clientId"));

	if (Body->HasField(TEXT("playerId")))
		HarvestStarted.playerId = Body->GetIntegerField(TEXT("playerId"));

	if (Body->HasField(TEXT("corpseId")))
		HarvestStarted.corpseId = Body->GetIntegerField(TEXT("corpseId"));

	if (Body->HasField(TEXT("duration")))
		HarvestStarted.duration = Body->GetIntegerField(TEXT("duration"));

	if (Body->HasField(TEXT("startTime")))
		HarvestStarted.startTime = static_cast<int64>(Body->GetNumberField(TEXT("startTime")));

	return HarvestStarted;
}

FHarvestStartedStruct JSONParser::DeserializeHarvestStarted(const FString& JsonString)
{
	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		return FHarvestStartedStruct();

	TSharedPtr<FJsonObject> Body = Root->GetObjectField(TEXT("body"));
	if (!Body.IsValid())
		return FHarvestStartedStruct();

	return JSONParser::DeserializeHarvestStarted(Body);
}

FHarvestCompleteStruct JSONParser::DeserializeHarvestComplete(const TSharedPtr<FJsonObject>& Body)
{
	FHarvestCompleteStruct HarvestComplete;
	if (!Body.IsValid()) return HarvestComplete;

	if (Body->HasField(TEXT("type")))
		HarvestComplete.type = Body->GetStringField(TEXT("type"));

	if (Body->HasField(TEXT("clientId")))
		HarvestComplete.clientId = Body->GetIntegerField(TEXT("clientId"));

	if (Body->HasField(TEXT("playerId")))
		HarvestComplete.playerId = Body->GetIntegerField(TEXT("playerId"));

	if (Body->HasField(TEXT("corpseId")))
		HarvestComplete.corpseId = Body->GetIntegerField(TEXT("corpseId"));

	if (Body->HasField(TEXT("success")))
		HarvestComplete.success = Body->GetBoolField(TEXT("success"));

	if (Body->HasField(TEXT("totalItems")))
		HarvestComplete.totalItems = Body->GetIntegerField(TEXT("totalItems"));

	// Parse available loot
	const TArray<TSharedPtr<FJsonValue>>* LootArray = nullptr;
	if (Body->TryGetArrayField(TEXT("availableLoot"), LootArray))
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

	TSharedPtr<FJsonObject> Body = Root->GetObjectField(TEXT("body"));
	if (!Body.IsValid())
		return FHarvestCompleteStruct();
	
	return JSONParser::DeserializeHarvestComplete(Body);
}

FHarvestErrorStruct JSONParser::DeserializeHarvestError(const TSharedPtr<FJsonObject>& Body)
{
	FHarvestErrorStruct HarvestError;
	if (!Body.IsValid()) return HarvestError;

	if (Body->HasField(TEXT("type")))
		HarvestError.type = Body->GetStringField(TEXT("type"));

	if (Body->HasField(TEXT("clientId")))
		HarvestError.clientId = Body->GetIntegerField(TEXT("clientId"));

	if (Body->HasField(TEXT("playerId")))
		HarvestError.playerId = Body->GetIntegerField(TEXT("playerId"));

	if (Body->HasField(TEXT("corpseId")))
		HarvestError.corpseId = Body->GetIntegerField(TEXT("corpseId"));

	if (Body->HasField(TEXT("errorCode")))
		HarvestError.errorCode = Body->GetStringField(TEXT("errorCode"));

	if (Body->HasField(TEXT("message")))
		HarvestError.message = Body->GetStringField(TEXT("message"));

	return HarvestError;
}

FHarvestErrorStruct JSONParser::DeserializeHarvestError(const FString& JsonString)
{
	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		return FHarvestErrorStruct();

	TSharedPtr<FJsonObject> Body = Root->GetObjectField(TEXT("body"));
	if (!Body.IsValid())
		return FHarvestErrorStruct();

	return JSONParser::DeserializeHarvestError(Body);
}

FCorpseLootPickupResponseStruct JSONParser::DeserializeCorpseLootPickupResponse(const TSharedPtr<FJsonObject>& Body)
{
	FCorpseLootPickupResponseStruct Response;
	if (!Body.IsValid()) return Response;

	if (Body->HasField(TEXT("success")))
		Response.success = Body->GetBoolField(TEXT("success"));

	if (Body->HasField(TEXT("corpseUID")))
		Response.corpseUID = Body->GetIntegerField(TEXT("corpseUID"));

	if (Body->HasField(TEXT("itemsPickedUp")))
		Response.itemsPickedUp = Body->GetIntegerField(TEXT("itemsPickedUp"));

	// Parse picked up items
	const TArray<TSharedPtr<FJsonValue>>* PickedUpArray = nullptr;
	if (Body->TryGetArrayField(TEXT("pickedUpItems"), PickedUpArray))
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
	if (Body->TryGetArrayField(TEXT("remainingLoot"), RemainingArray))
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

	TSharedPtr<FJsonObject> Body = Root->GetObjectField(TEXT("body"));
	if (!Body.IsValid())
		return FCorpseLootPickupResponseStruct();

	return JSONParser::DeserializeCorpseLootPickupResponse(Body);
}

FCorpseLootPickupErrorStruct JSONParser::DeserializeCorpseLootPickupError(const TSharedPtr<FJsonObject>& Body)
{
	FCorpseLootPickupErrorStruct Error;
	if (!Body.IsValid()) return Error;

	if (Body->HasField(TEXT("success")))
		Error.success = Body->GetBoolField(TEXT("success"));

	if (Body->HasField(TEXT("errorCode")))
		Error.errorCode = Body->GetStringField(TEXT("errorCode"));

	if (Body->HasField(TEXT("corpseUID")))
		Error.corpseUID = Body->GetIntegerField(TEXT("corpseUID"));

	return Error;
}

FCorpseLootPickupErrorStruct JSONParser::DeserializeCorpseLootPickupError(const FString& JsonString)
{
	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		return FCorpseLootPickupErrorStruct();

	TSharedPtr<FJsonObject> Body = Root->GetObjectField(TEXT("body"));
	if (!Body.IsValid())
		return FCorpseLootPickupErrorStruct();

	return JSONParser::DeserializeCorpseLootPickupError(Body);
}

FCorpseLootInspectResponseStruct JSONParser::DeserializeCorpseLootInspectResponse(const TSharedPtr<FJsonObject>& Body)
{
	FCorpseLootInspectResponseStruct Response;
	if (!Body.IsValid()) return Response;

	if (Body->HasField(TEXT("success")))
		Response.success = Body->GetBoolField(TEXT("success"));

	if (Body->HasField(TEXT("corpseUID")))
		Response.corpseUID = Body->GetIntegerField(TEXT("corpseUID"));

	if (Body->HasField(TEXT("type")))
		Response.type = Body->GetStringField(TEXT("type"));

	if (Body->HasField(TEXT("totalItems")))
		Response.totalItems = Body->GetIntegerField(TEXT("totalItems"));

	// Parse available loot
	const TArray<TSharedPtr<FJsonValue>>* LootArray = nullptr;
	if (Body->TryGetArrayField(TEXT("availableLoot"), LootArray))
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

	TSharedPtr<FJsonObject> Body = Root->GetObjectField(TEXT("body"));
	if (!Body.IsValid())
		return FCorpseLootInspectResponseStruct();

	return JSONParser::DeserializeCorpseLootInspectResponse(Body);
}

FCorpseLootInspectErrorStruct JSONParser::DeserializeCorpseLootInspectError(const TSharedPtr<FJsonObject>& Body)
{
	FCorpseLootInspectErrorStruct Error;
	if (!Body.IsValid()) return Error;

	if (Body->HasField(TEXT("success")))
		Error.success = Body->GetBoolField(TEXT("success"));

	if (Body->HasField(TEXT("errorCode")))
		Error.errorCode = Body->GetStringField(TEXT("errorCode"));

	return Error;
}

FCorpseLootInspectErrorStruct JSONParser::DeserializeCorpseLootInspectError(const FString& JsonString)
{
	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		return FCorpseLootInspectErrorStruct();

	TSharedPtr<FJsonObject> Body = Root->GetObjectField(TEXT("body"));
	if (!Body.IsValid())
		return FCorpseLootInspectErrorStruct();

	return JSONParser::DeserializeCorpseLootInspectError(Body);
}

// New combat system parsing functions
ESkillEffectType JSONParser::ParseSkillEffectType(const FString& EffectTypeString)
{
	if (EffectTypeString.Equals(TEXT("damage"), ESearchCase::IgnoreCase))
		return ESkillEffectType::Damage;
	else if (EffectTypeString.Equals(TEXT("heal"), ESearchCase::IgnoreCase)
		  || EffectTypeString.Equals(TEXT("healing"), ESearchCase::IgnoreCase))
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
	else if (SchoolString.Equals(TEXT("holy"), ESearchCase::IgnoreCase)
		  || SchoolString.Equals(TEXT("healing"), ESearchCase::IgnoreCase))
		return ESkillSchool::Holy;

	return ESkillSchool::None;
}
ECasterType JSONParser::ParseCasterType(const FString& CasterTypeString)
{
	if (CasterTypeString.Equals(TEXT("SELF"), ESearchCase::IgnoreCase))
		return ECasterType::Self;
	if (CasterTypeString.Equals(TEXT("PLAYER"), ESearchCase::IgnoreCase) ||
		CasterTypeString.Equals(TEXT("Player"), ESearchCase::IgnoreCase))
		return ECasterType::Player;
	if (CasterTypeString.Equals(TEXT("MOB"), ESearchCase::IgnoreCase) ||
		CasterTypeString.Equals(TEXT("Mob"), ESearchCase::IgnoreCase))
		return ECasterType::Mob;
	if (CasterTypeString.Equals(TEXT("AREA"), ESearchCase::IgnoreCase))
		return ECasterType::Area;
	if (CasterTypeString.Equals(TEXT("NPC"), ESearchCase::IgnoreCase) ||
		CasterTypeString.Equals(TEXT("Npc"), ESearchCase::IgnoreCase))
		return ECasterType::NPC;
	
	return ECasterType::None;
}

FAppliedEffectData JSONParser::DeserializeAppliedEffect(const TSharedPtr<FJsonObject>& EffectObj)
{
	FAppliedEffectData Effect;
	if (!EffectObj.IsValid()) return Effect;

	if (EffectObj->HasField(TEXT("effectName")))
		Effect.effectName = EffectObj->GetStringField(TEXT("effectName"));
	
	if (EffectObj->HasField(TEXT("duration")))
		Effect.duration = EffectObj->GetNumberField(TEXT("duration"));
	
	if (EffectObj->HasField(TEXT("value")))
		Effect.value = EffectObj->GetIntegerField(TEXT("value"));
	
	if (EffectObj->HasField(TEXT("effectType")))
		Effect.effectType = EffectObj->GetStringField(TEXT("effectType"));

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
		else if (Value->Type == EJson::String)
		{
			// skillExecutionResult format: appliedEffects is an array of slug strings
			FAppliedEffectData Effect;
			Effect.effectName = Value->AsString();
			Effects.Add(Effect);
		}
	}
	return Effects;
}

FSkillInitiationData JSONParser::DeserializeSkillInitiation(const TSharedPtr<FJsonObject>& InitiationObj)
{
	FSkillInitiationData SkillData;
	if (!InitiationObj.IsValid()) return SkillData;

	if (InitiationObj->HasField(TEXT("skillName")))
		SkillData.skillName = InitiationObj->GetStringField(TEXT("skillName"));

	if (InitiationObj->HasField(TEXT("skillSlug")))
		SkillData.skillSlug = InitiationObj->GetStringField(TEXT("skillSlug"));
	
	if (InitiationObj->HasField(TEXT("animationName")))
		SkillData.animationName = InitiationObj->GetStringField(TEXT("animationName"));
	
	if (InitiationObj->HasField(TEXT("animationDuration")))
		SkillData.animationDuration = InitiationObj->GetNumberField(TEXT("animationDuration"));
	
	if (InitiationObj->HasField(TEXT("castTime")))
		SkillData.castTime = InitiationObj->GetNumberField(TEXT("castTime"));
	
	if (InitiationObj->HasField(TEXT("casterId")))
		SkillData.casterId = InitiationObj->GetIntegerField(TEXT("casterId"));
	
	if (InitiationObj->HasField(TEXT("casterType")))
		SkillData.casterType = InitiationObj->GetIntegerField(TEXT("casterType"));
	
	if (InitiationObj->HasField(TEXT("casterTypeString")))
		SkillData.casterTypeString = InitiationObj->GetStringField(TEXT("casterTypeString"));
	
	if (InitiationObj->HasField(TEXT("targetId")))
		SkillData.targetId = InitiationObj->GetIntegerField(TEXT("targetId"));
	
	if (InitiationObj->HasField(TEXT("targetType")))
		SkillData.targetType = InitiationObj->GetIntegerField(TEXT("targetType"));
	
	if (InitiationObj->HasField(TEXT("targetTypeString")))
		SkillData.targetTypeString = InitiationObj->GetStringField(TEXT("targetTypeString"));
	
	if (InitiationObj->HasField(TEXT("success")))
		SkillData.success = InitiationObj->GetBoolField(TEXT("success"));

	// Parse effect type and school
	if (InitiationObj->HasField(TEXT("skillEffectType")))
	{
		FString EffectTypeStr = InitiationObj->GetStringField(TEXT("skillEffectType"));
		SkillData.skillEffectType = JSONParser::ParseSkillEffectType(EffectTypeStr);
	}
	
	if (InitiationObj->HasField(TEXT("skillSchool")))
	{
		FString SchoolStr = InitiationObj->GetStringField(TEXT("skillSchool"));
		SkillData.skillSchool = JSONParser::ParseSkillSchool(SchoolStr);
	}

	if (InitiationObj->HasField(TEXT("cooldownMs")))
		SkillData.cooldownMs = InitiationObj->GetIntegerField(TEXT("cooldownMs"));

	if (InitiationObj->HasField(TEXT("gcdMs")))
		SkillData.gcdMs = InitiationObj->GetIntegerField(TEXT("gcdMs"));

	if (InitiationObj->HasField(TEXT("serverTimestamp")))
		SkillData.serverTimestamp = static_cast<int64>(InitiationObj->GetNumberField(TEXT("serverTimestamp")));

	if (InitiationObj->HasField(TEXT("castStartedAt")))
		SkillData.castStartedAt = static_cast<int64>(InitiationObj->GetNumberField(TEXT("castStartedAt")));

	if (InitiationObj->HasField(TEXT("errorReason")))
		SkillData.errorReason = InitiationObj->GetStringField(TEXT("errorReason"));

	return SkillData;
}

FSkillInitiationData JSONParser::DeserializeSkillInitiation(const FString& JsonString)
{
	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		return FSkillInitiationData();

	TSharedPtr<FJsonObject> Body = Root->GetObjectField(TEXT("body"));
	if (!Body.IsValid())
		return FSkillInitiationData();

	TSharedPtr<FJsonObject> SkillInitiation = Body->GetObjectField(TEXT("skillInitiation"));
	if (!SkillInitiation.IsValid())
		return FSkillInitiationData();

	return JSONParser::DeserializeSkillInitiation(SkillInitiation);
}

FSkillResultData JSONParser::DeserializeSkillResult(const TSharedPtr<FJsonObject>& ResultObj)
{
	FSkillResultData SkillResult;
	if (!ResultObj.IsValid()) return SkillResult;

	if (ResultObj->HasField(TEXT("skillName")))
		SkillResult.skillName = ResultObj->GetStringField(TEXT("skillName"));
	
	if (ResultObj->HasField(TEXT("skillSlug")))
		SkillResult.skillSlug = ResultObj->GetStringField(TEXT("skillSlug"));
	
	if (ResultObj->HasField(TEXT("casterId")))
		SkillResult.casterId = ResultObj->GetIntegerField(TEXT("casterId"));
	
	if (ResultObj->HasField(TEXT("casterType")))
		SkillResult.casterType = ResultObj->GetIntegerField(TEXT("casterType"));
	
	if (ResultObj->HasField(TEXT("casterTypeString")))
		SkillResult.casterTypeString = ResultObj->GetStringField(TEXT("casterTypeString"));
	
	if (ResultObj->HasField(TEXT("targetId")))
		SkillResult.targetId = ResultObj->GetIntegerField(TEXT("targetId"));
	
	if (ResultObj->HasField(TEXT("targetType")))
		SkillResult.targetType = ResultObj->GetIntegerField(TEXT("targetType"));
	
	if (ResultObj->HasField(TEXT("targetTypeString")))
		SkillResult.targetTypeString = ResultObj->GetStringField(TEXT("targetTypeString"));
	
	if (ResultObj->HasField(TEXT("damage")))
		SkillResult.damage = ResultObj->GetIntegerField(TEXT("damage"));
	
	if (ResultObj->HasField(TEXT("healing")))
		SkillResult.healing = ResultObj->GetIntegerField(TEXT("healing"));
	else if (ResultObj->HasField(TEXT("healAmount")))
		SkillResult.healing = ResultObj->GetIntegerField(TEXT("healAmount"));

	if (ResultObj->HasField(TEXT("manaHealing")))
		SkillResult.manaHealing = ResultObj->GetIntegerField(TEXT("manaHealing"));
	
	if (ResultObj->HasField(TEXT("finalTargetHealth")))
		SkillResult.finalTargetHealth = ResultObj->GetIntegerField(TEXT("finalTargetHealth"));
	
	if (ResultObj->HasField(TEXT("finalTargetMana")))
		SkillResult.finalTargetMana = ResultObj->GetIntegerField(TEXT("finalTargetMana"));
	
	if (ResultObj->HasField(TEXT("finalCasterMana")))
		SkillResult.finalCasterMana = ResultObj->GetIntegerField(TEXT("finalCasterMana"));
	
	if (ResultObj->HasField(TEXT("isCritical")))
		SkillResult.isCritical = ResultObj->GetBoolField(TEXT("isCritical"));
	
	if (ResultObj->HasField(TEXT("isBlocked")))
		SkillResult.isBlocked = ResultObj->GetBoolField(TEXT("isBlocked"));
	
	if (ResultObj->HasField(TEXT("isMissed")))
		SkillResult.isMissed = ResultObj->GetBoolField(TEXT("isMissed"));
	
	if (ResultObj->HasField(TEXT("targetDied")))
		SkillResult.targetDied = ResultObj->GetBoolField(TEXT("targetDied"));
	
	if (ResultObj->HasField(TEXT("success")))
		SkillResult.success = ResultObj->GetBoolField(TEXT("success"));

	if (ResultObj->HasField(TEXT("serverTimestamp")))
		SkillResult.serverTimestamp = static_cast<int64>(ResultObj->GetNumberField(TEXT("serverTimestamp")));

	if (ResultObj->HasField(TEXT("errorReason")))
		SkillResult.errorReason = ResultObj->GetStringField(TEXT("errorReason"));

	// Parse effect type and school
	if (ResultObj->HasField(TEXT("skillEffectType")))
	{
		FString EffectTypeStr = ResultObj->GetStringField(TEXT("skillEffectType"));
		SkillResult.skillEffectType = JSONParser::ParseSkillEffectType(EffectTypeStr);
	}
	
	if (ResultObj->HasField(TEXT("skillSchool")))
	{
		FString SchoolStr = ResultObj->GetStringField(TEXT("skillSchool"));
		SkillResult.skillSchool = JSONParser::ParseSkillSchool(SchoolStr);
	}

	// Parse applied effects array
	if (ResultObj->HasField(TEXT("appliedEffects")))
	{
		const TArray<TSharedPtr<FJsonValue>>* EffectsArray;
		if (ResultObj->TryGetArrayField(TEXT("appliedEffects"), EffectsArray))
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

	TSharedPtr<FJsonObject> Body = Root->GetObjectField(TEXT("body"));
	if (!Body.IsValid())
		return FSkillResultData();

	// Try nested skillResult object first (combatResult / healingResult format)
	TSharedPtr<FJsonObject> SkillResult = Body->GetObjectField(TEXT("skillResult"));
	if (SkillResult.IsValid())
	{
		return JSONParser::DeserializeSkillResult(SkillResult);
	}

	// Fallback: fields directly in body (skillExecutionResult format)
	FSkillResultData Result = JSONParser::DeserializeSkillResult(Body);

	// skillExecutionResult wraps success in the header, not in body
	if (!Body->HasField(TEXT("success")))
	{
		TSharedPtr<FJsonObject> Header = Root->GetObjectField(TEXT("header"));
		if (Header.IsValid() && Header->HasField(TEXT("status")))
		{
			Result.success = Header->GetStringField(TEXT("status")).Equals(TEXT("success"), ESearchCase::IgnoreCase);
		}
	}

	return Result;
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

        // Per protocol (�1.4 pongClient), timestamps may be a root-level object
        // (sibling of "header") instead of fields inside "header". Check and override
        // if present so that pongClient responses are parsed correctly.
        const TSharedPtr<FJsonObject>* TimestampsObject = nullptr;
        if (JsonObject->TryGetObjectField(TEXT("timestamps"), TimestampsObject) && TimestampsObject != nullptr)
        {
            if ((*TimestampsObject)->HasField(TEXT("serverRecvMs")))
            {
                NetworkHeader.serverRecvMs = static_cast<int64>((*TimestampsObject)->GetNumberField(TEXT("serverRecvMs")));
            }
            if ((*TimestampsObject)->HasField(TEXT("serverSendMs")))
            {
                NetworkHeader.serverSendMs = static_cast<int64>((*TimestampsObject)->GetNumberField(TEXT("serverSendMs")));
            }
            if ((*TimestampsObject)->HasField(TEXT("clientSendMsEcho")))
            {
                NetworkHeader.clientSendMsEcho = static_cast<int64>((*TimestampsObject)->GetNumberField(TEXT("clientSendMsEcho")));
            }
            if ((*TimestampsObject)->HasField(TEXT("requestId")))
            {
                NetworkHeader.requestId = (*TimestampsObject)->GetStringField(TEXT("requestId"));
            }
            else if ((*TimestampsObject)->HasField(TEXT("requestIdEcho")))
            {
                NetworkHeader.requestId = (*TimestampsObject)->GetStringField(TEXT("requestIdEcho"));
            }
        }
    }

    return NetworkHeader;
}

void JSONParser::ProcessTimeSyncFromHeader(const FString& JsonString, UTimeSyncService* TimeSyncService)
{
	if (!TimeSyncService)
	{
		return;
	}

	FNetworkHeaderStruct NetworkHeader = DeserializeNetworkHeader(JsonString);

	// ��������� requestIdEcho ��� ������� �������
	FString RequestIdToUse;
	if (!NetworkHeader.requestId.IsEmpty())
	{
		RequestIdToUse = NetworkHeader.requestId; // ����� ���� requestIdEcho �� �������
	}

	// ������������ ������ ���� ���� ��������� ����������
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

	if (Body->HasField(TEXT("characterId")))
		ExperienceUpdate.characterId = Body->GetIntegerField(TEXT("characterId"));

	if (Body->HasField(TEXT("oldLevel")))
		ExperienceUpdate.oldLevel = Body->GetIntegerField(TEXT("oldLevel"));

	if (Body->HasField(TEXT("newLevel")))
		ExperienceUpdate.newLevel = Body->GetIntegerField(TEXT("newLevel"));

	if (Body->HasField(TEXT("oldExperience")))
		ExperienceUpdate.oldExperience = Body->GetIntegerField(TEXT("oldExperience"));

	if (Body->HasField(TEXT("newExperience")))
		ExperienceUpdate.newExperience = Body->GetIntegerField(TEXT("newExperience"));

	if (Body->HasField(TEXT("experienceChange")))
		ExperienceUpdate.experienceChange = Body->GetIntegerField(TEXT("experienceChange"));

	if (Body->HasField(TEXT("expForCurrentLevel")))
		ExperienceUpdate.expForCurrentLevel = Body->GetIntegerField(TEXT("expForCurrentLevel"));

	if (Body->HasField(TEXT("expForNextLevel")))
		ExperienceUpdate.expForNextLevel = Body->GetIntegerField(TEXT("expForNextLevel"));

	if (Body->HasField(TEXT("levelUp")))
		ExperienceUpdate.levelUp = Body->GetBoolField(TEXT("levelUp"));

	if (Body->HasField(TEXT("reason")))
		ExperienceUpdate.reason = Body->GetStringField(TEXT("reason"));

	if (Body->HasField(TEXT("sourceId")))
		ExperienceUpdate.sourceId = Body->GetIntegerField(TEXT("sourceId"));

	return ExperienceUpdate;
}

FExperienceUpdateStruct JSONParser::DeserializeExperienceUpdate(const FString& JsonString)
{
	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		return FExperienceUpdateStruct();

	TSharedPtr<FJsonObject> Body = Root->GetObjectField(TEXT("body"));
	if (!Body.IsValid())
		return FExperienceUpdateStruct();

	return JSONParser::DeserializeExperienceUpdate(Body);
}

FPlayerProgressionStruct JSONParser::DeserializePlayerProgression(const TSharedPtr<FJsonObject>& ProgressionObj)
{
	FPlayerProgressionStruct Progression;
	if (!ProgressionObj.IsValid()) return Progression;

	if (ProgressionObj->HasField(TEXT("characterId")))
		Progression.characterId = ProgressionObj->GetIntegerField(TEXT("characterId"));

	if (ProgressionObj->HasField(TEXT("currentLevel")))
		Progression.currentLevel = ProgressionObj->GetIntegerField(TEXT("currentLevel"));

	if (ProgressionObj->HasField(TEXT("currentExperience")))
		Progression.currentExperience = ProgressionObj->GetIntegerField(TEXT("currentExperience"));

	if (ProgressionObj->HasField(TEXT("totalExperience")))
		Progression.totalExperience = ProgressionObj->GetIntegerField(TEXT("totalExperience"));

	if (ProgressionObj->HasField(TEXT("expForNextLevel")))
		Progression.expForNextLevel = ProgressionObj->GetIntegerField(TEXT("expForNextLevel"));

	if (ProgressionObj->HasField(TEXT("expForCurrentLevel")))
		Progression.expForCurrentLevel = ProgressionObj->GetIntegerField(TEXT("expForCurrentLevel"));

	if (ProgressionObj->HasField(TEXT("hasPendingLevelUp")))
		Progression.bHasPendingLevelUp = ProgressionObj->GetBoolField(TEXT("hasPendingLevelUp"));

	if (ProgressionObj->HasField(TEXT("pendingLevelGained")))
		Progression.pendingLevelGained = ProgressionObj->GetIntegerField(TEXT("pendingLevelGained"));

	return Progression;
}

FPlayerProgressionStruct JSONParser::DeserializePlayerProgression(const FString& JsonString)
{
	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		return FPlayerProgressionStruct();

	TSharedPtr<FJsonObject> Body = Root->GetObjectField(TEXT("body"));
	if (!Body.IsValid())
		return FPlayerProgressionStruct();

	TSharedPtr<FJsonObject> Progression = Body->GetObjectField(TEXT("progression"));
	if (!Progression.IsValid())
		return FPlayerProgressionStruct();

	return JSONParser::DeserializePlayerProgression(Progression);
}

FExperienceGainEventStruct JSONParser::DeserializeExperienceGainEvent(const TSharedPtr<FJsonObject>& EventObj)
{
	FExperienceGainEventStruct Event;
	if (!EventObj.IsValid()) return Event;

	if (EventObj->HasField(TEXT("experienceGained")))
		Event.experienceGained = EventObj->GetIntegerField(TEXT("experienceGained"));

	if (EventObj->HasField(TEXT("reason")))
		Event.reasonText = EventObj->GetStringField(TEXT("reason"));

	if (EventObj->HasField(TEXT("sourceId")))
		Event.sourceId = EventObj->GetIntegerField(TEXT("sourceId"));

	if (EventObj->HasField(TEXT("sourceName")))
		Event.sourceName = EventObj->GetStringField(TEXT("sourceName"));

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

	TSharedPtr<FJsonObject> Body = Root->GetObjectField(TEXT("body"));
	if (!Body.IsValid())
		return FExperienceGainEventStruct();

	TSharedPtr<FJsonObject> Event = Body->GetObjectField(TEXT("experienceEvent"));
	if (!Event.IsValid())
	{
		// Try to parse directly from body if no experienceEvent object
		return JSONParser::DeserializeExperienceGainEvent(Body);
	}

	return JSONParser::DeserializeExperienceGainEvent(Event);
}

//FPlayerStatsUpdateStruct JSONParser::DeserializePlayerStatsUpdate(const FString& JsonString)
//{
//    return PlayerAttributeParser::DeserializePlayerStatsUpdate(JsonString);
//}

FPlayerStatsUpdateStruct JSONParser::DeserializePlayerStatsUpdate(const TSharedPtr<FJsonObject>& Body)
{
    return PlayerAttributeParser::DeserializePlayerStatsUpdate(Body);
}

FPlayerStatsUpdateStruct JSONParser::DeserializePlayerStatsUpdate(const FString& JsonString)
{
	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

	FPlayerStatsUpdateStruct PlayerStatsUpdate;

	if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
	{
		if (JsonObject->HasField(TEXT("body")))
		{
			TSharedPtr<FJsonObject> Body = JsonObject->GetObjectField(TEXT("body"));
			PlayerStatsUpdate = DeserializePlayerStatsUpdate(Body);
		}
	}

	return PlayerStatsUpdate;
}

// Player skills system parsers
FPlayerSkillNetworkData JSONParser::DeserializePlayerSkillNetworkData(const TSharedPtr<FJsonObject>& SkillObj)
{
	FPlayerSkillNetworkData SkillData;

	if (SkillObj.IsValid())
	{
		if (SkillObj->HasField(TEXT("skillSlug")))
		{
			SkillData.skillSlug = SkillObj->GetStringField(TEXT("skillSlug"));
		}

		if (SkillObj->HasField(TEXT("skillLevel")))
		{
			SkillData.skillLevel = SkillObj->GetIntegerField(TEXT("skillLevel"));
		}

		if (SkillObj->HasField(TEXT("castMs")))
		{
			SkillData.castMs = SkillObj->GetIntegerField(TEXT("castMs"));
		}

		if (SkillObj->HasField(TEXT("coeff")))
		{
			SkillData.coeff = SkillObj->GetNumberField(TEXT("coeff"));
		}

		if (SkillObj->HasField(TEXT("cooldownMs")))
		{
			SkillData.cooldownMs = SkillObj->GetIntegerField(TEXT("cooldownMs"));
		}

		if (SkillObj->HasField(TEXT("costMp")))
		{
			SkillData.costMp = SkillObj->GetIntegerField(TEXT("costMp"));
		}

		if (SkillObj->HasField(TEXT("flatAdd")))
		{
			SkillData.flatAdd = SkillObj->GetNumberField(TEXT("flatAdd"));
		}

		if (SkillObj->HasField(TEXT("gcdMs")))
		{
			SkillData.gcdMs = SkillObj->GetIntegerField(TEXT("gcdMs"));
		}

		if (SkillObj->HasField(TEXT("maxRange")))
		{
			SkillData.maxRange = SkillObj->GetNumberField(TEXT("maxRange"));
		}
	}

	return SkillData;
}

TArray<FPlayerSkillNetworkData> JSONParser::DeserializePlayerSkillsArray(const TArray<TSharedPtr<FJsonValue>>& JsonArray)
{
	TArray<FPlayerSkillNetworkData> Skills;

	for (const TSharedPtr<FJsonValue>& JsonValue : JsonArray)
	{
		if (JsonValue.IsValid() && JsonValue->Type == EJson::Object)
		{
			TSharedPtr<FJsonObject> SkillObj = JsonValue->AsObject();
			FPlayerSkillNetworkData SkillData = DeserializePlayerSkillNetworkData(SkillObj);
			Skills.Add(SkillData);
		}
	}

	return Skills;
}

FPlayerSkillsInitializationData JSONParser::DeserializePlayerSkillsInitialization(const TSharedPtr<FJsonObject>& Body)
{
	FPlayerSkillsInitializationData InitData;

	if (Body.IsValid())
	{
		if (Body->HasField(TEXT("characterId")))
		{
			InitData.characterId = Body->GetIntegerField(TEXT("characterId"));
		}

		if (Body->HasField(TEXT("skills")))
		{
			const TArray<TSharedPtr<FJsonValue>>* SkillsArray;
			if (Body->TryGetArrayField(TEXT("skills"), SkillsArray))
			{
				InitData.skills = DeserializePlayerSkillsArray(*SkillsArray);
			}
		}
	}

	return InitData;
}

FPlayerSkillsInitializationData JSONParser::DeserializePlayerSkillsInitialization(const FString& JsonString)
{
	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

	FPlayerSkillsInitializationData InitData;

	if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
	{
		if (JsonObject->HasField(TEXT("body")))
		{
			TSharedPtr<FJsonObject> Body = JsonObject->GetObjectField(TEXT("body"));
			InitData = DeserializePlayerSkillsInitialization(Body);
		}
	}

	return InitData;
}

// NPC system parsers implementation
FNPCStatsStruct JSONParser::DeserializeNPCStats(const TSharedPtr<FJsonObject>& StatsObj)
{
    FNPCStatsStruct Stats;
    
    if (StatsObj.IsValid())
    {
        StatsObj->TryGetNumberField(TEXT("current"), Stats.current);
        StatsObj->TryGetNumberField(TEXT("max"), Stats.max);
    }
    
    return Stats;
}

FNPCHealthManaStruct JSONParser::DeserializeNPCHealthMana(const TSharedPtr<FJsonObject>& HealthManaObj)
{
    FNPCHealthManaStruct HealthMana;
    
    if (HealthManaObj.IsValid())
    {
        const TSharedPtr<FJsonObject>* HealthObj = nullptr;
        if (HealthManaObj->TryGetObjectField(TEXT("health"), HealthObj) && HealthObj)
        {
            HealthMana.health = DeserializeNPCStats(*HealthObj);
        }
        
        const TSharedPtr<FJsonObject>* ManaObj = nullptr;
        if (HealthManaObj->TryGetObjectField(TEXT("mana"), ManaObj) && ManaObj)
        {
            HealthMana.mana = DeserializeNPCStats(*ManaObj);
        }
    }
    
    return HealthMana;
}

FNPCStruct JSONParser::DeserializeNPCData(const TSharedPtr<FJsonObject>& NPCObj)
{
    FNPCStruct NPC;
    
    if (NPCObj.IsValid())
    {
        NPCObj->TryGetNumberField(TEXT("id"), NPC.id);
        NPCObj->TryGetStringField(TEXT("name"), NPC.name);
        NPCObj->TryGetStringField(TEXT("slug"), NPC.slug);
        NPCObj->TryGetStringField(TEXT("race"), NPC.race);
        NPCObj->TryGetNumberField(TEXT("level"), NPC.level);
        NPCObj->TryGetStringField(TEXT("npcType"), NPC.npcType);
        NPCObj->TryGetBoolField(TEXT("isInteractable"), NPC.isInteractable);
        NPCObj->TryGetStringField(TEXT("dialogueId"), NPC.dialogueId);
        NPCObj->TryGetStringField(TEXT("questId"), NPC.questId);
        
        // Parse position
        const TSharedPtr<FJsonObject>* PositionObj = nullptr;
        if (NPCObj->TryGetObjectField(TEXT("position"), PositionObj) && PositionObj)
        {
            NPC.position = DeserializePositionData(*PositionObj);
        }
        
        // Parse attributes
        const TArray<TSharedPtr<FJsonValue>>* AttributesArray = nullptr;
        if (NPCObj->TryGetArrayField(TEXT("attributes"), AttributesArray))
        {
            for (const auto& AttributeValue : *AttributesArray)
            {
                const TSharedPtr<FJsonObject>* AttributeObj = nullptr;
                if (AttributeValue->TryGetObject(AttributeObj) && AttributeObj)
                {
                    FAttributeDataStruct Attribute;
                    (*AttributeObj)->TryGetNumberField(TEXT("id"), Attribute.attributeId);
                    (*AttributeObj)->TryGetStringField(TEXT("name"), Attribute.attributeName);
                    (*AttributeObj)->TryGetStringField(TEXT("slug"), Attribute.attributeSlug);
                    (*AttributeObj)->TryGetNumberField(TEXT("value"), Attribute.attributeValue);
                    NPC.attributes.Add(Attribute);
                }
            }
        }
        
        // Parse quests
        const TArray<TSharedPtr<FJsonValue>>* QuestsArray = nullptr;
        if (NPCObj->TryGetArrayField(TEXT("quests"), QuestsArray))
        {
            for (const auto& QuestValue : *QuestsArray)
            {
                const TSharedPtr<FJsonObject>* QuestObj = nullptr;
                if (QuestValue->TryGetObject(QuestObj) && QuestObj)
                {
                    FNPCQuestEntry Entry;
                    (*QuestObj)->TryGetStringField(TEXT("slug"), Entry.slug);
                    (*QuestObj)->TryGetStringField(TEXT("status"), Entry.status);
                    NPC.quests.Add(Entry);
                }
            }
        }

        // Parse stats
        const TSharedPtr<FJsonObject>* StatsObj = nullptr;
        if (NPCObj->TryGetObjectField(TEXT("stats"), StatsObj) && StatsObj)
        {
            NPC.stats = DeserializeNPCHealthMana(*StatsObj);
        }
    }
    
    return NPC;
}

TArray<FNPCStruct> JSONParser::DeserializeNPCsList(const TArray<TSharedPtr<FJsonValue>>& JsonArray)
{
    TArray<FNPCStruct> NPCs;
    
    for (const auto& NPCValue : JsonArray)
    {
        const TSharedPtr<FJsonObject>* NPCObj = nullptr;
        if (NPCValue->TryGetObject(NPCObj) && NPCObj)
        {
            FNPCStruct NPC = DeserializeNPCData(*NPCObj);
            NPCs.Add(NPC);
        }
    }
    
    return NPCs;
}

FNPCSpawnDataStruct JSONParser::DeserializeNPCSpawnData(const TSharedPtr<FJsonObject>& Body)
{
    FNPCSpawnDataStruct SpawnData;
    
    if (Body.IsValid())
    {
        Body->TryGetNumberField(TEXT("npcCount"), SpawnData.npcCount);
        Body->TryGetNumberField(TEXT("spawnRadius"), SpawnData.spawnRadius);
        
        const TArray<TSharedPtr<FJsonValue>>* NPCsArray = nullptr;
        if (Body->TryGetArrayField(TEXT("npcsSpawn"), NPCsArray))
        {
            SpawnData.npcsSpawn = DeserializeNPCsList(*NPCsArray);
        }
    }
    
    return SpawnData;
}

FNPCSpawnDataStruct JSONParser::DeserializeNPCSpawnData(const FString& JsonString)
{
    FNPCSpawnDataStruct SpawnData;
    
    TSharedPtr<FJsonObject> Root;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
    
    if (FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid())
    {
        const TSharedPtr<FJsonObject>* BodyObject = nullptr;
        if (Root->TryGetObjectField(TEXT("body"), BodyObject) && BodyObject)
        {
            SpawnData = DeserializeNPCSpawnData(*BodyObject);
        }
    }
    
    return SpawnData;
}

FEffectTickData JSONParser::DeserializeEffectTick(const FString& JsonString)
{
    FEffectTickData TickData;

    TSharedPtr<FJsonObject> Root;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()) return TickData;

    const TSharedPtr<FJsonObject>* BodyPtr = nullptr;
    if (!Root->TryGetObjectField(TEXT("body"), BodyPtr) || !BodyPtr) return TickData;

    const TSharedPtr<FJsonObject>& Body = *BodyPtr;
    Body->TryGetNumberField(TEXT("characterId"), TickData.characterId);
    Body->TryGetStringField(TEXT("effectSlug"), TickData.effectSlug);
    Body->TryGetStringField(TEXT("effectTypeSlug"), TickData.effectTypeSlug);
    Body->TryGetNumberField(TEXT("value"), TickData.value);
    Body->TryGetBoolField(TEXT("isHeal"), TickData.bIsHeal);
    Body->TryGetNumberField(TEXT("newHealth"), TickData.newHealth);
    Body->TryGetNumberField(TEXT("newMana"), TickData.newMana);
    Body->TryGetBoolField(TEXT("targetDied"), TickData.targetDied);

    return TickData;
}

TArray<FActiveEffectEntry> JSONParser::DeserializeActiveEffectsPacket(const FString& JsonString)
{
    TArray<FActiveEffectEntry> Effects;

    TSharedPtr<FJsonObject> Root;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()) return Effects;

    const TSharedPtr<FJsonObject>* BodyPtr = nullptr;
    if (!Root->TryGetObjectField(TEXT("body"), BodyPtr) || !BodyPtr) return Effects;

    const TArray<TSharedPtr<FJsonValue>>* EffectsArray = nullptr;
    if (!(*BodyPtr)->TryGetArrayField(TEXT("activeEffects"), EffectsArray)) return Effects;

    for (const TSharedPtr<FJsonValue>& Val : *EffectsArray)
    {
        const TSharedPtr<FJsonObject>* ObjPtr = nullptr;
        if (!Val->TryGetObject(ObjPtr)) continue;

        FActiveEffectEntry Entry;
        (*ObjPtr)->TryGetStringField(TEXT("slug"), Entry.slug);
        (*ObjPtr)->TryGetStringField(TEXT("effectType"), Entry.effectType);
        (*ObjPtr)->TryGetStringField(TEXT("effectTypeSlug"), Entry.effectTypeSlug);
        (*ObjPtr)->TryGetStringField(TEXT("attributeSlug"), Entry.attributeSlug);
        (*ObjPtr)->TryGetStringField(TEXT("sourceType"), Entry.sourceType);
        double Tmp = 0.0;
        if ((*ObjPtr)->TryGetNumberField(TEXT("value"), Tmp)) Entry.value = static_cast<float>(Tmp);
        if ((*ObjPtr)->TryGetNumberField(TEXT("expiresAt"), Tmp)) Entry.expiresAt = static_cast<int64>(Tmp);
        (*ObjPtr)->TryGetBoolField(TEXT("isPercentage"), Entry.bIsPercentage);
        (*ObjPtr)->TryGetBoolField(TEXT("isPermanent"), Entry.bIsPermanent);

        Effects.Add(Entry);
    }

    return Effects;
}

// ─────────────────────────────────────────────────────────────────────────────
// World Interactive Objects (WIO) parsers
// ─────────────────────────────────────────────────────────────────────────────

FWorldObjectData JSONParser::DeserializeWorldObject(const TSharedPtr<FJsonObject>& Obj)
{
    FWorldObjectData Data;
    if (!Obj.IsValid()) return Data;

    Obj->TryGetNumberField(TEXT("id"),                  Data.ObjectId);
    Obj->TryGetStringField(TEXT("slug"),                Data.Slug);
    Obj->TryGetStringField(TEXT("nameKey"),             Data.NameKey);

    FString TypeStr, ScopeStr, StateStr;
    Obj->TryGetStringField(TEXT("objectType"),          TypeStr);
    Obj->TryGetStringField(TEXT("scope"),               ScopeStr);

    Data.ObjectType = WIOHelpers::ParseObjectType(TypeStr);
    Data.Scope      = WIOHelpers::ParseScope(ScopeStr);

    double Tmp = 0.0;
    if (Obj->TryGetNumberField(TEXT("posX"), Tmp))               Data.PosX = static_cast<float>(Tmp);
    if (Obj->TryGetNumberField(TEXT("posY"), Tmp))               Data.PosY = static_cast<float>(Tmp);
    if (Obj->TryGetNumberField(TEXT("posZ"), Tmp))               Data.PosZ = static_cast<float>(Tmp);
    if (Obj->TryGetNumberField(TEXT("rotZ"), Tmp))               Data.RotZ = static_cast<float>(Tmp);
    if (Obj->TryGetNumberField(TEXT("interactionRadius"), Tmp))  Data.InteractionRadius = static_cast<float>(Tmp);

    Obj->TryGetNumberField(TEXT("zoneId"),              Data.ZoneId);
    Obj->TryGetNumberField(TEXT("channelTimeSec"),      Data.ChannelTimeSec);
    Obj->TryGetNumberField(TEXT("respawnSec"),          Data.RespawnSec);
    Obj->TryGetNumberField(TEXT("minLevel"),            Data.MinLevel);
    Obj->TryGetNumberField(TEXT("dialogueId"),          Data.DialogueId);
    Obj->TryGetNumberField(TEXT("requiredItemId"),      Data.RequiredItemId);

    // Server sends "currentState" or "state" depending on the packet
    if (Obj->TryGetStringField(TEXT("currentState"), StateStr) || Obj->TryGetStringField(TEXT("state"), StateStr))
    {
        Data.CurrentState = WIOHelpers::ParseState(StateStr);
    }

    return Data;
}

TArray<FWorldObjectData> JSONParser::DeserializeWorldObjectsList(const TSharedPtr<FJsonObject>& Body)
{
    TArray<FWorldObjectData> Result;
    if (!Body.IsValid()) return Result;

    const TArray<TSharedPtr<FJsonValue>>* ObjectsArray = nullptr;
    if (!Body->TryGetArrayField(TEXT("worldObjects"), ObjectsArray) || !ObjectsArray)
    {
        return Result;
    }

    for (const TSharedPtr<FJsonValue>& Val : *ObjectsArray)
    {
        const TSharedPtr<FJsonObject>* ObjPtr = nullptr;
        if (!Val->TryGetObject(ObjPtr) || !ObjPtr) continue;
        Result.Add(DeserializeWorldObject(*ObjPtr));
    }

    return Result;
}

TArray<FWorldObjectData> JSONParser::DeserializeWorldObjectsList(const FString& JsonString)
{
    TSharedPtr<FJsonObject> Root;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()) return {};

    const TSharedPtr<FJsonObject>* BodyPtr = nullptr;
    if (!Root->TryGetObjectField(TEXT("body"), BodyPtr) || !BodyPtr) return {};

    return DeserializeWorldObjectsList(*BodyPtr);
}

FWIOInteractResult JSONParser::DeserializeWIOInteractResult(const TSharedPtr<FJsonObject>& Body)
{
    FWIOInteractResult Result;
    if (!Body.IsValid()) return Result;

    Body->TryGetNumberField(TEXT("objectId"),       Result.ObjectId);
    Body->TryGetBoolField  (TEXT("success"),        Result.bSuccess);
    Body->TryGetStringField(TEXT("errorCode"),      Result.ErrorCode);
    Body->TryGetStringField(TEXT("interactionType"),Result.InteractionType);
    Body->TryGetNumberField(TEXT("channelTimeSec"), Result.ChannelTimeSec);

    const TArray<TSharedPtr<FJsonValue>>* LootArray = nullptr;
    if (Body->TryGetArrayField(TEXT("lootItems"), LootArray) && LootArray)
    {
        for (const TSharedPtr<FJsonValue>& Val : *LootArray)
        {
            const TSharedPtr<FJsonObject>* ItemPtr = nullptr;
            if (!Val->TryGetObject(ItemPtr)) continue;

            FWIOLootItem Item;
            (*ItemPtr)->TryGetNumberField(TEXT("itemId"),   Item.ItemId);
            (*ItemPtr)->TryGetNumberField(TEXT("quantity"), Item.Quantity);
            Result.LootItems.Add(Item);
        }
    }

    return Result;
}

FWIOInteractResult JSONParser::DeserializeWIOInteractResult(const FString& JsonString)
{
    TSharedPtr<FJsonObject> Root;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()) return {};

    const TSharedPtr<FJsonObject>* BodyPtr = nullptr;
    if (!Root->TryGetObjectField(TEXT("body"), BodyPtr) || !BodyPtr) return {};

    return DeserializeWIOInteractResult(*BodyPtr);
}

FWIOStateUpdate JSONParser::DeserializeWIOStateUpdate(const TSharedPtr<FJsonObject>& Body)
{
    FWIOStateUpdate Update;
    if (!Body.IsValid()) return Update;

    Body->TryGetNumberField(TEXT("objectId"),   Update.ObjectId);
    Body->TryGetNumberField(TEXT("respawnSec"), Update.RespawnSec);

    FString StateStr;
    if (Body->TryGetStringField(TEXT("state"), StateStr))
    {
        Update.NewState = WIOHelpers::ParseState(StateStr);
    }

    return Update;
}

FWIOStateUpdate JSONParser::DeserializeWIOStateUpdate(const FString& JsonString)
{
    TSharedPtr<FJsonObject> Root;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()) return {};

    const TSharedPtr<FJsonObject>* BodyPtr = nullptr;
    if (!Root->TryGetObjectField(TEXT("body"), BodyPtr) || !BodyPtr) return {};

    return DeserializeWIOStateUpdate(*BodyPtr);
}

FWIOChannelCancelled JSONParser::DeserializeWIOChannelCancelled(const TSharedPtr<FJsonObject>& Body)
{
    FWIOChannelCancelled Data;
    if (!Body.IsValid()) return Data;

    Body->TryGetNumberField(TEXT("objectId"), Data.ObjectId);
    return Data;
}

FWIOChannelCancelled JSONParser::DeserializeWIOChannelCancelled(const FString& JsonString)
{
    TSharedPtr<FJsonObject> Root;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()) return {};

    const TSharedPtr<FJsonObject>* BodyPtr = nullptr;
    if (!Root->TryGetObjectField(TEXT("body"), BodyPtr) || !BodyPtr) return {};

    return DeserializeWIOChannelCancelled(*BodyPtr);
}

// ─────────────────────────────────────────────────────────────────────────────
// Login Flow — Character Creation Options
// ─────────────────────────────────────────────────────────────────────────────

FCharacterCreationOptions JSONParser::DeserializeCharacterCreationOptions(const FString& JsonString)
{
    FCharacterCreationOptions Options;

    TSharedPtr<FJsonObject> Root;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()) return Options;

    const TSharedPtr<FJsonObject>* BodyPtr = nullptr;
    if (!Root->TryGetObjectField(TEXT("body"), BodyPtr) || !BodyPtr) return Options;
    const TSharedPtr<FJsonObject>& Body = *BodyPtr;

    // Parse classes
    const TArray<TSharedPtr<FJsonValue>>* ClassesArray = nullptr;
    if (Body->TryGetArrayField(TEXT("classes"), ClassesArray))
    {
        for (const TSharedPtr<FJsonValue>& Val : *ClassesArray)
        {
            const TSharedPtr<FJsonObject>* ObjPtr = nullptr;
            if (!Val->TryGetObject(ObjPtr) || !ObjPtr) continue;
            const TSharedPtr<FJsonObject>& Obj = *ObjPtr;

            FCharacterClassOption Cls;
            Obj->TryGetNumberField(TEXT("id"), Cls.Id);
            Obj->TryGetStringField(TEXT("name"), Cls.Name);
            Obj->TryGetStringField(TEXT("slug"), Cls.Slug);
            Obj->TryGetStringField(TEXT("description"), Cls.Description);
            Options.Classes.Add(MoveTemp(Cls));
        }
    }

    // Parse races
    const TArray<TSharedPtr<FJsonValue>>* RacesArray = nullptr;
    if (Body->TryGetArrayField(TEXT("races"), RacesArray))
    {
        for (const TSharedPtr<FJsonValue>& Val : *RacesArray)
        {
            const TSharedPtr<FJsonObject>* ObjPtr = nullptr;
            if (!Val->TryGetObject(ObjPtr) || !ObjPtr) continue;
            const TSharedPtr<FJsonObject>& Obj = *ObjPtr;

            FCharacterRaceOption Race;
            Obj->TryGetNumberField(TEXT("id"), Race.Id);
            Obj->TryGetStringField(TEXT("name"), Race.Name);
            Obj->TryGetStringField(TEXT("slug"), Race.Slug);
            Options.Races.Add(MoveTemp(Race));
        }
    }

    // Parse genders
    const TArray<TSharedPtr<FJsonValue>>* GendersArray = nullptr;
    if (Body->TryGetArrayField(TEXT("genders"), GendersArray))
    {
        for (const TSharedPtr<FJsonValue>& Val : *GendersArray)
        {
            const TSharedPtr<FJsonObject>* ObjPtr = nullptr;
            if (!Val->TryGetObject(ObjPtr) || !ObjPtr) continue;
            const TSharedPtr<FJsonObject>& Obj = *ObjPtr;

            FCharacterGenderOption Gender;
            Obj->TryGetNumberField(TEXT("id"), Gender.Id);
            Obj->TryGetStringField(TEXT("slug"), Gender.Name);
            Obj->TryGetStringField(TEXT("label"), Gender.Label);
            Options.Genders.Add(MoveTemp(Gender));
        }
    }

    return Options;
}
