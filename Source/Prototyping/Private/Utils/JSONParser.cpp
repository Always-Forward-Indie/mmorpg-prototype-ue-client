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
			if ((*BodyObject)->HasField(TEXT("characterPosX")))
			{
				PositionData.positionX = (*BodyObject)->GetNumberField(TEXT("characterPosX"));
			}

			if ((*BodyObject)->HasField(TEXT("characterPosY")))
			{
				PositionData.positionY = (*BodyObject)->GetNumberField(TEXT("characterPosY"));
			}

			if ((*BodyObject)->HasField(TEXT("characterPosZ")))
			{
				PositionData.positionZ = (*BodyObject)->GetNumberField(TEXT("characterPosZ"));
			}

			if ((*BodyObject)->HasField(TEXT("characterRotZ")))
			{
				PositionData.rotationZ = (*BodyObject)->GetNumberField(TEXT("characterRotZ"));
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
					if (CharacterObject->HasField(TEXT("characterPosX")))
					{
						CharacterData.characterPosition.positionX = CharacterObject->GetNumberField(TEXT("characterPosX"));
					}

					if (CharacterObject->HasField(TEXT("characterPosY")))
					{
						CharacterData.characterPosition.positionY = CharacterObject->GetNumberField(TEXT("characterPosY"));
					}

					if (CharacterObject->HasField(TEXT("characterPosZ")))
					{
						CharacterData.characterPosition.positionZ = CharacterObject->GetNumberField(TEXT("characterPosZ"));
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