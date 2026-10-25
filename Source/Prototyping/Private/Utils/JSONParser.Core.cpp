// Fill out your copyright notice in the Description page of Project Settings.


#include "Utils/JSONParser.h"
#include "Data/DataStructs.h"
#include "Prototyping.h"
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


 // Universal: Parse Position from JSON object.
 // Missing or wrong-typed fields stay at the struct default (0) — quiet,
 // so one truncated move packet never spams the log per field.
 FPositionDataStruct JSONParser::DeserializePositionData(const TSharedPtr<FJsonObject>& PosObj)
 {
	 FPositionDataStruct Pos;
	 if (!PosObj.IsValid()) return Pos;
	 PosObj->TryGetNumberField(TEXT("x"), Pos.positionX);
	 PosObj->TryGetNumberField(TEXT("y"), Pos.positionY);
	 PosObj->TryGetNumberField(TEXT("z"), Pos.positionZ);
	 PosObj->TryGetNumberField(TEXT("rotationZ"), Pos.rotationZ);
	 return Pos;
 }

 // Universal: Parse Attributes from array.
 // Entries without a slug are skipped (same as before); missing fields
 // keep struct defaults instead of Warning-spamming per field.
 TMap<FString, FAttributeDataStruct> JSONParser::DeserializeAttributesArray(const TArray<TSharedPtr<FJsonValue>>& JsonArray)
 {
	 TMap<FString, FAttributeDataStruct> Result;
	 for (const TSharedPtr<FJsonValue>& Value : JsonArray)
	 {
		 // NOTE: never AsObject() blindly — wrong-typed values log a LogJson
		 // Error. TryGetObject skips them quietly (same skip contract).
		 const TSharedPtr<FJsonObject>* AttrPtr = nullptr;
		 if (!Value.IsValid() || !Value->TryGetObject(AttrPtr) || !AttrPtr || !(*AttrPtr).IsValid()) continue;
		 const TSharedPtr<FJsonObject>& AttrObj = *AttrPtr;
		 FAttributeDataStruct Attr;
		 AttrObj->TryGetNumberField(TEXT("id"), Attr.attributeId);
		 AttrObj->TryGetStringField(TEXT("name"), Attr.attributeName);
		 AttrObj->TryGetStringField(TEXT("slug"), Attr.attributeSlug);
		 AttrObj->TryGetNumberField(TEXT("value"), Attr.attributeValue);
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
	 CD->TryGetNumberField(TEXT("id"), Character.characterId);
	 	CD->TryGetStringField(TEXT("name"), Character.characterName);
	 	CD->TryGetStringField(TEXT("class"), Character.characterClass);
	 	CD->TryGetStringField(TEXT("race"), Character.characterRace);
	 	CD->TryGetStringField(TEXT("gender"), Character.characterGender);
	 CD->TryGetNumberField(TEXT("level"), Character.characterLevel);

	 //is dead (missing = false by struct default, quietly)
	 CD->TryGetBoolField(TEXT("isDead"), Character.bIsDead);

	 const TSharedPtr<FJsonObject>* ExpPtr = nullptr;
	 if (CD->TryGetObjectField(TEXT("exp"), ExpPtr) && ExpPtr && (*ExpPtr).IsValid())
	 {
		 const TSharedPtr<FJsonObject>& Exp = *ExpPtr;
		 Exp->TryGetNumberField(TEXT("current"), Character.characterExperiencePoints);
		 Exp->TryGetNumberField(TEXT("levelStart"), Character.characterExpForLevelStart);
		 Exp->TryGetNumberField(TEXT("levelEnd"), Character.characterExpForLevelEnd);
	 }

	 if (CD->HasField(TEXT("stats"))) {
		 const TSharedPtr<FJsonObject>* StatsPtr = nullptr;
		 if (CD->TryGetObjectField(TEXT("stats"), StatsPtr) && StatsPtr && (*StatsPtr).IsValid())
		 {
			 const TSharedPtr<FJsonObject>& Stats = *StatsPtr;
			 const TSharedPtr<FJsonObject>* HealthPtr = nullptr;
			 const TSharedPtr<FJsonObject>* ManaPtr = nullptr;
			 const bool bHasHealth = Stats->TryGetObjectField(TEXT("health"), HealthPtr) && HealthPtr && (*HealthPtr).IsValid();
			 const bool bHasMana = Stats->TryGetObjectField(TEXT("mana"), ManaPtr) && ManaPtr && (*ManaPtr).IsValid();
			 if (bHasHealth)
			 {
				 (*HealthPtr)->TryGetNumberField(TEXT("current"), Character.characterCurrentHealth);
			 }
			 if (bHasMana)
			 {
				 (*ManaPtr)->TryGetNumberField(TEXT("current"), Character.characterCurrentMana);
			 }

		 // Also store max values in characterAttributes so RefreshHUD can use them
		 // immediately after spawn, before the first stats_update arrives.
		 int32 MaxHealth = 0;
		 int32 MaxMana = 0;
		 if (bHasHealth)
		 {
			 (*HealthPtr)->TryGetNumberField(TEXT("max"), MaxHealth);
		 }
		 if (bHasMana)
		 {
			 (*ManaPtr)->TryGetNumberField(TEXT("max"), MaxMana);
		 }

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
		 } // TryGet stats object
	 } // HasField(stats)

	 const TSharedPtr<FJsonObject>* PositionPtr = nullptr;
	 if (CD->TryGetObjectField(TEXT("position"), PositionPtr) && PositionPtr && (*PositionPtr).IsValid())
		 Character.characterPosition = JSONParser::DeserializePositionData(*PositionPtr);

	 const TArray<TSharedPtr<FJsonValue>>* AttributesArray = nullptr;
	 if (CD->TryGetArrayField(TEXT("attributes"), AttributesArray) && AttributesArray != nullptr)
		 Character.characterAttributes.attributesData = JSONParser::DeserializeAttributesArray(*AttributesArray);

		CD->TryGetBoolField(TEXT("isFalling"), Character.bIsFalling);

	CD->TryGetStringField(TEXT("equippedTitleSlug"), Character.equippedTitleSlug);
	CD->TryGetStringField(TEXT("equippedTitleDisplayName"), Character.equippedTitleDisplayName);

	return Character;
 }

 // Entry point for string-based JSON input.
 // Never crashes: garbage JSON or a missing body/character yields a default
 // struct plus one error-level log (malformed server packet = seam failure).
 FCharacterDataStruct JSONParser::DeserializeCharacterData(const FString& JsonString)
 {
	 TSharedPtr<FJsonObject> Root;
	 TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	 if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	 {
		 UE_LOG(LogNetPacket, Error, TEXT("JSONParser: DeserializeCharacterData failed to parse JSON"));
		 return FCharacterDataStruct();
	 }
	 const TSharedPtr<FJsonObject>* BodyPtr = nullptr;
	 const TSharedPtr<FJsonObject>* CharacterPtr = nullptr;
	 if (!Root->TryGetObjectField(TEXT("body"), BodyPtr) || !BodyPtr || !(*BodyPtr).IsValid()
		 || !(*BodyPtr)->TryGetObjectField(TEXT("character"), CharacterPtr) || !CharacterPtr || !(*CharacterPtr).IsValid())
	 {
		 UE_LOG(LogNetPacket, Error, TEXT("JSONParser: DeserializeCharacterData missing body/character"));
		 return FCharacterDataStruct();
	 }
	 return JSONParser::DeserializeCharacterData(*CharacterPtr);
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
			 // TryGetObject (not AsObject): wrong-typed entries skip quietly
			 // instead of logging a LogJson Error per packet.
			 const TSharedPtr<FJsonObject>* CharacterObjPtr = nullptr;
			 if (!Value.IsValid() || !Value->TryGetObject(CharacterObjPtr) || !CharacterObjPtr || !(*CharacterObjPtr).IsValid())
			 {
				 continue;
			 }
			 const TSharedPtr<FJsonObject>& CharacterObj = *CharacterObjPtr;
				 FClientDataStruct ClientData;
				 CharacterObj->TryGetNumberField(TEXT("clientId"), ClientData.clientId);
				 const TSharedPtr<FJsonObject>* CharacterPtr = nullptr;
				 if (CharacterObj->TryGetObjectField(TEXT("character"), CharacterPtr) && CharacterPtr && (*CharacterPtr).IsValid())
				 {
					 ClientData.characterData = JSONParser::DeserializeCharacterData(*CharacterPtr);
				 }
				 Result.Add(ClientData);
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
		 return DeserializeClientData(JsonObject);
	 }

	 return ClientData;
 }

 // deserialize ClientData from an already-parsed root (S05: no re-parse)
FClientDataStruct JSONParser::DeserializeClientData(const TSharedPtr<FJsonObject>& JsonObject)
 {
	 FClientDataStruct ClientData;
	 if (!JsonObject.IsValid())
	 {
		 return ClientData;
	 }

	 const TSharedPtr<FJsonObject>* HeaderObject = nullptr;
	 if (JsonObject->TryGetObjectField(TEXT("header"), HeaderObject) && HeaderObject != nullptr)
	 {
		 	(*HeaderObject)->TryGetNumberField(TEXT("clientId"), ClientData.clientId);

		 	(*HeaderObject)->TryGetStringField(TEXT("hash"), ClientData.hash);
	 }

	 return ClientData;
 }

 // Single-parse entry point (S05): one deserialize, shares Root/Body.
 FParsedPacket JSONParser::ParsePacket(const FString& JsonString)
 {
 	FParsedPacket Packet;
 	Packet.Raw = JsonString;

 	// Crash-proofing: truncated/dropped packets yield invalid, never AV
 	// (same quiet-defaults contract as DeserializeMessageData).
 	TSharedPtr<FJsonObject> Root;
 	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
 	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
 	{
 		return Packet;
 	}
 	Packet.Root = Root;

 	const TSharedPtr<FJsonObject>* HeaderPtr = nullptr;
 	if (Root->TryGetObjectField(TEXT("header"), HeaderPtr) && HeaderPtr && (*HeaderPtr).IsValid())
 	{
 		(*HeaderPtr)->TryGetStringField(TEXT("eventType"), Packet.EventType);
 		// Legacy quirk (chunk characterExperience): header.event instead of
 		// header.eventType. A packet carrying BOTH is a contract violation —
 		// eventType wins. Remove once the server fleet guarantees eventType.
 		if (Packet.EventType.IsEmpty())
 		{
 			(*HeaderPtr)->TryGetStringField(TEXT("event"), Packet.EventType);
 		}
 		(*HeaderPtr)->TryGetStringField(TEXT("status"), Packet.Status);
 		(*HeaderPtr)->TryGetStringField(TEXT("message"), Packet.Message);
 	}

 	const TSharedPtr<FJsonObject>* BodyPtr = nullptr;
 	if (Root->TryGetObjectField(TEXT("body"), BodyPtr) && BodyPtr && (*BodyPtr).IsValid())
 	{
 		Packet.Body = *BodyPtr;
 	}

 	Packet.bValid = !Packet.EventType.IsEmpty();
 	return Packet;
 }

 // deserialize MessageData from a json string
 FMessageDataStruct JSONParser::DeserializeMessageData(const FString& JsonString)
{
	return DeserializeMessageData(ParsePacket(JsonString));
}

// deserialize MessageData from an already-parsed packet (S05: no re-parse)
FMessageDataStruct JSONParser::DeserializeMessageData(const FParsedPacket& Packet)
{
	FMessageDataStruct MessageData;
	const TSharedPtr<FJsonObject>& JsonObject = Packet.Root;
	if (!JsonObject.IsValid())
	{
		return MessageData;
	}

	const TSharedPtr<FJsonObject>* HeaderObject = nullptr;
	if (JsonObject->TryGetObjectField(TEXT("header"), HeaderObject) && HeaderObject && (*HeaderObject).IsValid())
	{
		const TSharedPtr<FJsonObject>& Header = *HeaderObject;
		Header->TryGetStringField(TEXT("status"), MessageData.status);
		Header->TryGetStringField(TEXT("message"), MessageData.message);
		Header->TryGetStringField(TEXT("eventType"), MessageData.eventType);
		Header->TryGetStringField(TEXT("timestamp"), MessageData.timestamp);
		Header->TryGetStringField(TEXT("version"), MessageData.version);

		// Parse time sync fields
		double ClientSendMs = 0.0;
		if (Header->TryGetNumberField(TEXT("clientSendMs"), ClientSendMs))
			MessageData.clientSendMs = static_cast<int64>(ClientSendMs);

		double ClientSendMsEcho = 0.0;
		if (Header->TryGetNumberField(TEXT("clientSendMsEcho"), ClientSendMsEcho))
		{
			MessageData.clientSendMs = static_cast<int64>(ClientSendMsEcho);
			MessageData.clientSendMsEcho = static_cast<int64>(ClientSendMsEcho);
		}

		double ServerRecvMs = 0.0;
		if (Header->TryGetNumberField(TEXT("serverRecvMs"), ServerRecvMs))
			MessageData.serverRecvMs = static_cast<int64>(ServerRecvMs);

		double ServerSendMs = 0.0;
		if (Header->TryGetNumberField(TEXT("serverSendMs"), ServerSendMs))
			MessageData.serverSendMs = static_cast<int64>(ServerSendMs);
	}

	// Per protocol (§1.4 pongClient), timestamps may also be a root-level object
	// (sibling of "header") instead of fields inside "header". Check and override
	// if present so that pongClient responses are parsed correctly.
	const TSharedPtr<FJsonObject>* TimestampsObject = nullptr;
	if (JsonObject->TryGetObjectField(TEXT("timestamps"), TimestampsObject) && TimestampsObject && (*TimestampsObject).IsValid())
	{
		const TSharedPtr<FJsonObject>& Timestamps = *TimestampsObject;
		double ServerRecvMs = 0.0;
		if (Timestamps->TryGetNumberField(TEXT("serverRecvMs"), ServerRecvMs))
			MessageData.serverRecvMs = static_cast<int64>(ServerRecvMs);
		double ServerSendMs = 0.0;
		if (Timestamps->TryGetNumberField(TEXT("serverSendMs"), ServerSendMs))
			MessageData.serverSendMs = static_cast<int64>(ServerSendMs);
		double ClientSendMsEcho = 0.0;
		if (Timestamps->TryGetNumberField(TEXT("clientSendMsEcho"), ClientSendMsEcho))
		{
			MessageData.clientSendMsEcho = static_cast<int64>(ClientSendMsEcho);
			MessageData.clientSendMs = MessageData.clientSendMsEcho;
		}
	}

	// Central errorCode: body.errorCode mirrors header.message on status:error.
	const TSharedPtr<FJsonObject>* BodyObject = nullptr;
	if (JsonObject->TryGetObjectField(TEXT("body"), BodyObject) && BodyObject && (*BodyObject).IsValid())
	{
		const TSharedPtr<FJsonObject>& Body = *BodyObject;
		FString BodyError;
		if (Body->TryGetStringField(TEXT("errorCode"), BodyError) && !BodyError.IsEmpty())
		{
			MessageData.errorCode = BodyError;
		}
		Body->TryGetStringField(TEXT("serverVersion"), MessageData.serverVersion);
		Body->TryGetStringField(TEXT("minClientVersion"), MessageData.minClientVersion);
		Body->TryGetStringField(TEXT("maxClientVersion"), MessageData.maxClientVersion);
		Body->TryGetStringField(TEXT("banReason"), MessageData.banReason);
		Body->TryGetStringField(TEXT("banExpiresAt"), MessageData.banExpiresAt);
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

FString JSONParser::HumanizeSlugForDisplay(const FString& Slug)
{
	if (Slug.IsEmpty()) return FString();
	FString Out = Slug.Replace(TEXT("_"), TEXT(" "));
	Out[0] = FChar::ToUpper(Out[0]);
	return Out;
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
			 	(*HeaderObject)->TryGetStringField(TEXT("eventType"), EventType);
		 }
	 }
	 return EventType;
 }

