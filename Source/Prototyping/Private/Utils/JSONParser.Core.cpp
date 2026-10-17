// Fill out your copyright notice in the Description page of Project Settings.


#include "Utils/JSONParser.h"
#include "Services/TimeSyncService.h"
#include "MyGameInstance.h"
#include "Engine/World.h"
#include "Utils/PlayerAttributeParser.h"
#include "Utils/NetCore/InventoryRules.h"

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
    HeaderObject->SetStringField(TEXT("eventType"), EventType);

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

FString JSONParser::SerializeJsonWithTimeSync(const FString& EventType, const TMap<FString, TSharedPtr<FJsonValue>>& HeaderData, const TMap<FString, TSharedPtr<FJsonValue>>& BodyData, UTimeSyncService* TimeSyncService, EServerType ServerType)
{
	TSharedPtr<FJsonObject> HeaderObject = MakeShareable(new FJsonObject);
	HeaderObject->SetStringField(TEXT("eventType"), EventType);

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

	OutputString.ReplaceInline(TEXT("\n"), TEXT(""));
	OutputString.ReplaceInline(TEXT("\r"), TEXT(""));

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
	 if (CD->HasField(TEXT("name")))
		 Character.characterName = CD->GetStringField(TEXT("name"));
	 if (CD->HasField(TEXT("class")))
		 Character.characterClass = CD->GetStringField(TEXT("class"));
	 if (CD->HasField(TEXT("race")))
		 Character.characterRace = CD->GetStringField(TEXT("race"));
	 if (CD->HasField(TEXT("gender")))
	 {
		 Character.characterGender = CD->GetStringField(TEXT("gender"));
	 }
	 if (CD->HasField(TEXT("level")))
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

	if (CD->HasField(TEXT("isFalling")))
		Character.bIsFalling = CD->GetBoolField(TEXT("isFalling"));

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

			 if ((*HeaderObject)->HasField(TEXT("version")))
			 {
				 MessageData.version = (*HeaderObject)->GetStringField(TEXT("version"));
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

	 // Central errorCode: body.errorCode mirrors header.message on status:error.
const TSharedPtr<FJsonObject>* BodyObject = nullptr;
if (JsonObject->TryGetObjectField(TEXT("body"), BodyObject) && BodyObject != nullptr)
{
FString BodyError;
if ((*BodyObject)->TryGetStringField(TEXT("errorCode"), BodyError) && !BodyError.IsEmpty())
{
MessageData.errorCode = BodyError;
}
(*BodyObject)->TryGetStringField(TEXT("serverVersion"), MessageData.serverVersion);
(*BodyObject)->TryGetStringField(TEXT("minClientVersion"), MessageData.minClientVersion);
(*BodyObject)->TryGetStringField(TEXT("maxClientVersion"), MessageData.maxClientVersion);
(*BodyObject)->TryGetStringField(TEXT("banReason"), MessageData.banReason);
(*BodyObject)->TryGetStringField(TEXT("banExpiresAt"), MessageData.banExpiresAt);
}
if (MessageData.errorCode.IsEmpty())
{
MessageData.errorCode = MessageData.message;
}
return MessageData;
 }

FString JSONParser::ResolveErrorCode(const TSharedPtr<FJsonObject>& Root)
{
if (!Root.IsValid()) return FString();
FString HeaderMsg;
const TSharedPtr<FJsonObject>* HeaderPtr = nullptr;
if (Root->TryGetObjectField(TEXT("header"), HeaderPtr) && HeaderPtr)
{
(*HeaderPtr)->TryGetStringField(TEXT("message"), HeaderMsg);
}
const TSharedPtr<FJsonObject>* BodyPtr = nullptr;
if (Root->TryGetObjectField(TEXT("body"), BodyPtr) && BodyPtr)
{
FString BodyCode;
if ((*BodyPtr)->TryGetStringField(TEXT("errorCode"), BodyCode) && !BodyCode.IsEmpty())
{
return BodyCode;
}
}
return HeaderMsg;
}

FString JSONParser::ResolveErrorCode(const FString& JsonString)
{
TSharedPtr<FJsonObject> Root;
TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
{
return FString();
}
return ResolveErrorCode(Root);
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

