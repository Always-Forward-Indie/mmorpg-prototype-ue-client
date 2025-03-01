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
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
    FJsonSerializer::Serialize(MainJsonObject.ToSharedRef(), Writer);

    return OutputString;
}

 // deserialize CharacterData from a json string
 FCharacterDataStruct JSONParser::DeserializeCharacterData(const FString& JsonString)
 {
     TSharedPtr<FJsonObject> JsonObject;
     FCharacterDataStruct CharacterData;

     // Convert the string to a JSON object
     TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
     if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
     {
		 const TSharedPtr<FJsonObject>* BodyObject = nullptr;
		 if (JsonObject->TryGetObjectField(TEXT("body"), BodyObject) && BodyObject != nullptr)
		 {
			 if ((*BodyObject)->HasField(TEXT("characterId")))
			 {
				 CharacterData.characterId = (*BodyObject)->GetIntegerField(TEXT("characterId"));
			 }

			 if ((*BodyObject)->HasField(TEXT("characterName")))
			 {
				 CharacterData.characterName = (*BodyObject)->GetStringField(TEXT("characterName"));
			 }

			 if ((*BodyObject)->HasField(TEXT("characterClass")))
			 {
				 CharacterData.characterClass = (*BodyObject)->GetStringField(TEXT("characterClass"));
			 }

			 if ((*BodyObject)->HasField(TEXT("characterRace")))
			 {
				 CharacterData.characterRace = (*BodyObject)->GetStringField(TEXT("characterRace"));
			 }

			 if ((*BodyObject)->HasField(TEXT("characterLevel")))
			 {
				 CharacterData.characterLevel = (*BodyObject)->GetIntegerField(TEXT("characterLevel"));
			 }

			 if ((*BodyObject)->HasField(TEXT("characterExp")))
			 {
				 CharacterData.characterExperiencePoints = (*BodyObject)->GetIntegerField(TEXT("characterExp"));
			 }

			 if ((*BodyObject)->HasField(TEXT("characterCurrentHealth")))
			 {
				 CharacterData.characterCurrentHealth = (*BodyObject)->GetIntegerField(TEXT("characterCurrentHealth"));
			 }

			 if ((*BodyObject)->HasField(TEXT("characterCurrentMana")))
			 {
				 CharacterData.characterCurrentMana = (*BodyObject)->GetIntegerField(TEXT("characterCurrentMana"));
			 }
		 }
     }

     return CharacterData;
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

	// Convert the string to a JSON object
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
	{
		const TSharedPtr<FJsonObject>* HeaderObject = nullptr;
		if ((*HeaderObject)->TryGetObjectField(TEXT("header"), HeaderObject) && HeaderObject != nullptr)
		{
			if ((*HeaderObject)->HasField(TEXT("eventType")))
			{
				EventType = (*HeaderObject)->GetStringField(TEXT("eventType"));
			}
		}
	}

	return EventType;
}

// deserialize a PositionData from a json string
FPositionDataStruct JSONParser::DeserializePositionData(const FString& JsonString)
{
	TSharedPtr<FJsonObject> JsonObject;
	FPositionDataStruct PositionData;

	// Convert the string to a JSON object
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
	{
		const TSharedPtr<FJsonObject>* BodyObject = nullptr;
		if (JsonObject->TryGetObjectField(TEXT("body"), BodyObject) && BodyObject != nullptr)
		{
			if ((*BodyObject)->HasField(TEXT("posX")))
			{
				PositionData.positionX = (*BodyObject)->GetNumberField(TEXT("posX"));
			}

			if ((*BodyObject)->HasField(TEXT("posY")))
			{
				PositionData.positionY = (*BodyObject)->GetNumberField(TEXT("posY"));
			}

			if ((*BodyObject)->HasField(TEXT("posZ")))
			{
				PositionData.positionZ = (*BodyObject)->GetNumberField(TEXT("posZ"));
			}

			if ((*BodyObject)->HasField(TEXT("rotZ")))
			{
				PositionData.rotationZ = (*BodyObject)->GetNumberField(TEXT("rotZ"));
			}
		}
	}

	return PositionData;
}

//deserialize a JSON containing a list of characters
TArray<FClientDataStruct> JSONParser::DeserializeCharactersList(const FString& JsonString)
{	
	// create an array of characters
	TArray<FClientDataStruct> CharacterList;
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
					// create client data
					FClientDataStruct ClientData;
					// create character data
					FCharacterDataStruct CharacterData;
					TSharedPtr<FJsonObject> CharacterObject = CharacterValue->AsObject();

					// get client id from character data
					if (CharacterObject->HasField(TEXT("clientId")))
					{
						ClientData.clientId = CharacterObject->GetIntegerField(TEXT("clientId"));
					}

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

					//set position data
					if (CharacterObject->HasField(TEXT("posX")))
					{
						CharacterData.characterPosition.positionX = CharacterObject->GetNumberField(TEXT("posX"));
					}

					if (CharacterObject->HasField(TEXT("posY")))
					{
						CharacterData.characterPosition.positionY = CharacterObject->GetNumberField(TEXT("posY"));
					}

					if (CharacterObject->HasField(TEXT("posZ")))
					{
						CharacterData.characterPosition.positionZ = CharacterObject->GetNumberField(TEXT("posZ"));
					}

					ClientData.characterData = CharacterData;

					CharacterList.Add(ClientData);
				}
			}
		}
	}

	return CharacterList;
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

					CharacterList.Add(CharacterData);
				}
			}
		}
	}

	return CharacterList;
}

//deserialize a JSON containing a list of spawn zones
TArray<FSpawnZoneStruct> JSONParser::DeserializeSpawnZonesData(const FString& JsonString)
{
	// create an array of spawn zones
	TArray<FSpawnZoneStruct> SpawnZonesList;
	TSharedPtr<FJsonObject> JsonObject;

	// Convert the string to a JSON object
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
	{
		const TSharedPtr<FJsonObject>* BodyObject = nullptr;
		if (JsonObject->TryGetObjectField(TEXT("body"), BodyObject) && BodyObject != nullptr)
		{
			// get the array of spawn zones
			const TArray<TSharedPtr<FJsonValue>>* SpawnZonesArray = nullptr;
			if ((*BodyObject)->TryGetArrayField(TEXT("spawnZonesData"), SpawnZonesArray) && SpawnZonesArray != nullptr)
			{
				// iterate through the array of spawn zones
				for (const TSharedPtr<FJsonValue>& SpawnZoneValue : *SpawnZonesArray)
				{
					// create spawn zone data
					FSpawnZoneStruct SpawnZoneData;
					TSharedPtr<FJsonObject> SpawnZoneObject = SpawnZoneValue->AsObject();

					if (SpawnZoneObject->HasField(TEXT("zoneId")))
					{
						SpawnZoneData.zoneID = SpawnZoneObject->GetIntegerField(TEXT("zoneId"));
					}

					if (SpawnZoneObject->HasField(TEXT("zoneName")))
					{
						SpawnZoneData.zoneName = SpawnZoneObject->GetStringField(TEXT("zoneName"));
					}

					//mob id to spawn 
					if (SpawnZoneObject->HasField(TEXT("spawnMobId")))
					{
						SpawnZoneData.MobIDToSpawn = SpawnZoneObject->GetIntegerField(TEXT("spawnMobId"));	
					}

					//current mobs count
					if (SpawnZoneObject->HasField(TEXT("spawnedMobsCount")))
					{
						SpawnZoneData.currentMobsCount = SpawnZoneObject->GetIntegerField(TEXT("spawnedMobsCount"));
					}

					// count of mobs to spawn
					if (SpawnZoneObject->HasField(TEXT("maxSpawnCount")))
					{
						SpawnZoneData.MaxMobs = SpawnZoneObject->GetIntegerField(TEXT("maxSpawnCount"));
					}

					// respawn time
					if (SpawnZoneObject->HasField(TEXT("respawnTime")))
					{
						SpawnZoneData.respawnTime = SpawnZoneObject->GetIntegerField(TEXT("respawnTime"));
					}

					// spawning enabled
					if (SpawnZoneObject->HasField(TEXT("spawnEnabled")))
					{
						SpawnZoneData.bSpawningEnabled = SpawnZoneObject->GetBoolField(TEXT("spawnEnabled"));
					}

					// spawn zone size with x, y, z
					if (SpawnZoneObject->HasField(TEXT("minX")))
					{
						SpawnZoneData.spawnStartPos = FVector(SpawnZoneObject->GetNumberField(TEXT("minX")), SpawnZoneObject->GetNumberField(TEXT("minY")), SpawnZoneObject->GetNumberField(TEXT("minZ")));
					}

					if (SpawnZoneObject->HasField(TEXT("maxX")))
					{
						SpawnZoneData.spawnSize = FVector(SpawnZoneObject->GetNumberField(TEXT("maxX")), SpawnZoneObject->GetNumberField(TEXT("maxY")), SpawnZoneObject->GetNumberField(TEXT("maxZ")));
					}

					SpawnZonesList.Add(SpawnZoneData);
				}
			}
		}
	}

	return SpawnZonesList;
}

// deserialize a JSON containing a spawn zone
FSpawnZoneStruct JSONParser::DeserializeSpawnZoneData(const FString& JsonString)
{
	TSharedPtr<FJsonObject> JsonObject;
	FSpawnZoneStruct SpawnZoneData;

	// Convert the string to a JSON object
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
	{
		const TSharedPtr<FJsonObject>* BodyObject = nullptr;
		if (JsonObject->TryGetObjectField(TEXT("body"), BodyObject) && BodyObject != nullptr)
		{
			// get object of the spawn zone
			const TSharedPtr<FJsonObject>* spawnZoneDataObject = nullptr;
			if ((*BodyObject)->TryGetObjectField(TEXT("spawnZoneData"), spawnZoneDataObject) && spawnZoneDataObject != nullptr)
			{
				if ((*spawnZoneDataObject)->HasField(TEXT("zoneId")))
				{
					SpawnZoneData.zoneID = (*spawnZoneDataObject)->GetIntegerField(TEXT("zoneId"));
				}

				if ((*spawnZoneDataObject)->HasField(TEXT("zoneName")))
				{
					SpawnZoneData.zoneName = (*spawnZoneDataObject)->GetStringField(TEXT("zoneName"));
				}

				//mob id to spawn 
				if ((*spawnZoneDataObject)->HasField(TEXT("spawnMobId")))
				{
					SpawnZoneData.MobIDToSpawn = (*spawnZoneDataObject)->GetIntegerField(TEXT("spawnMobId"));
				}

				//current mobs count
				if ((*spawnZoneDataObject)->HasField(TEXT("spawnedMobsCount")))
				{
					SpawnZoneData.currentMobsCount = (*spawnZoneDataObject)->GetIntegerField(TEXT("spawnedMobsCount"));
				}

				// count of mobs to spawn
				if ((*spawnZoneDataObject)->HasField(TEXT("maxSpawnCount")))
				{
					SpawnZoneData.MaxMobs = (*spawnZoneDataObject)->GetIntegerField(TEXT("maxSpawnCount"));
				}

				// respawn time
				if ((*spawnZoneDataObject)->HasField(TEXT("respawnTime")))
				{
					SpawnZoneData.respawnTime = (*spawnZoneDataObject)->GetIntegerField(TEXT("respawnTime"));
				}

				// spawning enabled
				if ((*spawnZoneDataObject)->HasField(TEXT("spawnEnabled")))
				{
					SpawnZoneData.bSpawningEnabled = (*spawnZoneDataObject)->GetBoolField(TEXT("spawnEnabled"));
				}

				// spawn zone size with x, y, z
				if ((*spawnZoneDataObject)->HasField(TEXT("minX")))
				{
					SpawnZoneData.spawnStartPos = FVector((*spawnZoneDataObject)->GetNumberField(TEXT("minX")), (*spawnZoneDataObject)->GetNumberField(TEXT("minY")), (*spawnZoneDataObject)->GetNumberField(TEXT("minZ")));
				}

				if ((*spawnZoneDataObject)->HasField(TEXT("maxX")))
				{
					SpawnZoneData.spawnSize = FVector((*spawnZoneDataObject)->GetNumberField(TEXT("maxX")), (*spawnZoneDataObject)->GetNumberField(TEXT("maxY")), (*spawnZoneDataObject)->GetNumberField(TEXT("maxZ")));
				}
			}

		}
	}

	return SpawnZoneData;
}

// deserialize a JSON containing a list of mobs
TArray<FMOBStruct> JSONParser::DeserializeMobsDataList(const FString& JsonString)
{
	// create an array of mobs
	TArray<FMOBStruct> MobsList;
	TSharedPtr<FJsonObject> JsonObject;

	// Convert the string to a JSON object
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
	{
		const TSharedPtr<FJsonObject>* BodyObject = nullptr;
		if (JsonObject->TryGetObjectField(TEXT("body"), BodyObject) && BodyObject != nullptr)
		{
			// get the array of mobs
			const TArray<TSharedPtr<FJsonValue>>* MobsArray = nullptr;
			if ((*BodyObject)->TryGetArrayField(TEXT("mobsData"), MobsArray) && MobsArray != nullptr)
			{
				// iterate through the array of mobs
				for (const TSharedPtr<FJsonValue>& MobValue : *MobsArray)
				{
					// create mob data
					FMOBStruct MobData;
					TSharedPtr<FJsonObject> MobObject = MobValue->AsObject();

					if (MobObject->HasField(TEXT("mobId")))
					{
						MobData.mobID = MobObject->GetIntegerField(TEXT("mobId"));
					}

					if (MobObject->HasField(TEXT("mobUID")))
					{
						MobData.mobUniqueID = MobObject->GetStringField(TEXT("mobUID"));
					}

					if (MobObject->HasField(TEXT("mobZoneId")))
					{
						MobData.mobZoneID = MobObject->GetIntegerField(TEXT("mobZoneId"));
					}

					if (MobObject->HasField(TEXT("mobName")))
					{
						MobData.mobName = MobObject->GetStringField(TEXT("mobName"));
					}

					if (MobObject->HasField(TEXT("mobRaceName")))
					{
						MobData.mobRace = MobObject->GetStringField(TEXT("mobRaceName"));
					}

					if (MobObject->HasField(TEXT("mobLevel")))
					{
						MobData.mobLevel = MobObject->GetIntegerField(TEXT("mobLevel"));
					}

					// mob position
					if (MobObject->HasField(TEXT("posX")))
					{
						MobData.mobPosition.positionX = MobObject->GetNumberField(TEXT("posX"));
					}

					if (MobObject->HasField(TEXT("posY")))
					{
						MobData.mobPosition.positionY = MobObject->GetNumberField(TEXT("posY"));
					}

					if (MobObject->HasField(TEXT("posZ")))
					{
						MobData.mobPosition.positionZ = MobObject->GetNumberField(TEXT("posZ"));
					}

					//rotation
					if (MobObject->HasField(TEXT("rotZ")))
					{
						MobData.mobPosition.rotationZ = MobObject->GetNumberField(TEXT("rotZ"));
					}

					// mob current health
					if (MobObject->HasField(TEXT("mobCurrentHealth")))
					{
						MobData.mobCurrentHealth = MobObject->GetIntegerField(TEXT("mobCurrentHealth"));
					}

					// mob current mana
					if (MobObject->HasField(TEXT("mobCurrentMana")))
					{
						MobData.mobCurrentMana = MobObject->GetIntegerField(TEXT("mobCurrentMana"));
					}

					// attributes data
					if (MobObject->HasField(TEXT("attributesData")))
					{
						// get the array of attributes
						const TArray<TSharedPtr<FJsonValue>>* AttributesArray = nullptr;
						if (MobObject->TryGetArrayField(TEXT("attributesData"), AttributesArray) && AttributesArray != nullptr)
						{
							// iterate through the array of attributes
							for (const TSharedPtr<FJsonValue>& AttributeValue : *AttributesArray)
							{
								// create attribute data
								FAttributeDataStruct AttributeData;
								TSharedPtr<FJsonObject> AttributeObject = AttributeValue->AsObject();

								if (AttributeObject->HasField(TEXT("attributeId")))
								{
									AttributeData.attributeId = AttributeObject->GetIntegerField(TEXT("attributeId"));
								}

								if (AttributeObject->HasField(TEXT("attributeSlug")))
								{
									AttributeData.attributeSlug = AttributeObject->GetStringField(TEXT("attributeSlug"));
								}

								if (AttributeObject->HasField(TEXT("attributeName")))
								{
									AttributeData.attributeName = AttributeObject->GetStringField(TEXT("attributeName"));
								}

								if (AttributeObject->HasField(TEXT("attributeValue")))
								{
									AttributeData.attributeValue = AttributeObject->GetIntegerField(TEXT("attributeValue"));
								}

								// add attribute to the list
								MobData.mobAttributes.attributesData.Add(AttributeData.attributeSlug, AttributeData);
							}
						}
						
						
						//MobData.mobAttributes = DeserializeAttributesData(MobObject->GetStringField(TEXT("attributesData")));
					}

					// add mob to the list
					MobsList.Add(MobData);
				}
			}
		}
	}

	return MobsList;
}

// deserialize a JSON containing a mob
FMOBStruct JSONParser::DeserializeMobData(const FString& JsonString)
{
	TSharedPtr<FJsonObject> JsonObject;
	FMOBStruct MobData;

	// Convert the string to a JSON object
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
	{
		const TSharedPtr<FJsonObject>* BodyObject = nullptr;
		if (JsonObject->TryGetObjectField(TEXT("body"), BodyObject) && BodyObject != nullptr)
		{
			if ((*BodyObject)->HasField(TEXT("mobId")))
			{
				MobData.mobID = (*BodyObject)->GetIntegerField(TEXT("mobId"));
			}

			if ((*BodyObject)->HasField(TEXT("mobUID")))
			{
				MobData.mobUniqueID = (*BodyObject)->GetStringField(TEXT("mobUID"));
			}

			if ((*BodyObject)->HasField(TEXT("mobZoneId")))
			{
				MobData.mobZoneID = (*BodyObject)->GetIntegerField(TEXT("mobZoneId"));
			}

			if ((*BodyObject)->HasField(TEXT("mobName")))
			{
				MobData.mobName = (*BodyObject)->GetStringField(TEXT("mobName"));
			}

			if ((*BodyObject)->HasField(TEXT("mobRaceName")))
			{
				MobData.mobRace = (*BodyObject)->GetStringField(TEXT("mobRaceName"));
			}

			if ((*BodyObject)->HasField(TEXT("mobLevel")))
			{
				MobData.mobLevel = (*BodyObject)->GetIntegerField(TEXT("mobLevel"));
			}

			// mob position
			if ((*BodyObject)->HasField(TEXT("posX")))
			{
				MobData.mobPosition.positionX = (*BodyObject)->GetNumberField(TEXT("posX"));
			}

			if ((*BodyObject)->HasField(TEXT("posY")))
			{
				MobData.mobPosition.positionY = (*BodyObject)->GetNumberField(TEXT("posY"));
			}

			if ((*BodyObject)->HasField(TEXT("posZ")))
			{
				MobData.mobPosition.positionZ = (*BodyObject)->GetNumberField(TEXT("posZ"));
			}

			//rotation
			if ((*BodyObject)->HasField(TEXT("rotZ")))
			{
				MobData.mobPosition.rotationZ = (*BodyObject)->GetNumberField(TEXT("rotZ"));
			}

			// mob current health
			if ((*BodyObject)->HasField(TEXT("mobCurrentHealth")))
			{
				MobData.mobCurrentHealth = (*BodyObject)->GetIntegerField(TEXT("mobCurrentHealth"));
			}

			// mob current mana
			if ((*BodyObject)->HasField(TEXT("mobCurrentMana")))
			{
				MobData.mobCurrentMana = (*BodyObject)->GetIntegerField(TEXT("mobCurrentMana"));
			
			}

			// attributes data
			if ((*BodyObject)->HasField(TEXT("attributesData")))
			{
				// get the array of attributes
				const TArray<TSharedPtr<FJsonValue>>* AttributesArray = nullptr;
				if ((*BodyObject)->TryGetArrayField(TEXT("attributesData"), AttributesArray) && AttributesArray != nullptr)
				{
					// iterate through the array of attributes
					for (const TSharedPtr<FJsonValue>& AttributeValue : *AttributesArray)
					{
						// create attribute data
						FAttributeDataStruct AttributeData;
						TSharedPtr<FJsonObject> AttributeObject = AttributeValue->AsObject();

						if (AttributeObject->HasField(TEXT("attributeId")))
						{
							AttributeData.attributeId = AttributeObject->GetIntegerField(TEXT("attributeId"));
						}

						if (AttributeObject->HasField(TEXT("attributeSlug")))
						{
							AttributeData.attributeSlug = AttributeObject->GetStringField(TEXT("attributeSlug"));
						}

						if (AttributeObject->HasField(TEXT("attributeName")))
						{
							AttributeData.attributeName = AttributeObject->GetStringField(TEXT("attributeName"));
						}

						if (AttributeObject->HasField(TEXT("attributeValue")))
						{
							AttributeData.attributeValue = AttributeObject->GetIntegerField(TEXT("attributeValue"));
						}

						// add attribute to the list
						MobData.mobAttributes.attributesData.Add(AttributeData.attributeSlug, AttributeData);
					}
				}
			}
		}
	}

	return MobData;
}

// deserialize attributes from a json string
FAttributesDataStruct JSONParser::DeserializeAttributesData(const FString& JsonString)
{
	// create an array of attributes
	FAttributesDataStruct AttributesList;
	TSharedPtr<FJsonObject> JsonObject;

	// Convert the string to a JSON object
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
	{
		const TSharedPtr<FJsonObject>* BodyObject = nullptr;
		if (JsonObject->TryGetObjectField(TEXT("body"), BodyObject) && BodyObject != nullptr)
		{
			// get the array of attributes
			const TArray<TSharedPtr<FJsonValue>>* AttributesArray = nullptr;
			if ((*BodyObject)->TryGetArrayField(TEXT("attributesData"), AttributesArray) && AttributesArray != nullptr)
			{
				// iterate through the array of attributes
				for (const TSharedPtr<FJsonValue>& AttributeValue : *AttributesArray)
				{
					// create attribute data
					FAttributeDataStruct AttributeData;
					TSharedPtr<FJsonObject> AttributeObject = AttributeValue->AsObject();

					if (AttributeObject->HasField(TEXT("attributeId")))
					{
						AttributeData.attributeId = AttributeObject->GetIntegerField(TEXT("attributeId"));
					}

					if (AttributeObject->HasField(TEXT("attributeSlug")))
					{
						AttributeData.attributeSlug = AttributeObject->GetStringField(TEXT("attributeSlug"));
					}

					if (AttributeObject->HasField(TEXT("attributeName")))
					{
						AttributeData.attributeName = AttributeObject->GetStringField(TEXT("attributeName"));
					}

					if (AttributeObject->HasField(TEXT("attributeValue")))
					{
						AttributeData.attributeValue = AttributeObject->GetIntegerField(TEXT("attributeValue"));
					}

					// add attribute to the list
					AttributesList.attributesData.Add(AttributeData.attributeSlug, AttributeData);
				}
			}

		}
	}

	return AttributesList;
}