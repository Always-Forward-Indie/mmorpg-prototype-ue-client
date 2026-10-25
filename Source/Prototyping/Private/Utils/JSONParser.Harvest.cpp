// Fill out your copyright notice in the Description page of Project Settings.


#include "Utils/JSONParser.h"
#include "Prototyping.h"
#include "Services/TimeSyncService.h"
#include "MyGameInstance.h"
#include "Engine/World.h"
#include "Utils/PlayerAttributeParser.h"
#include "Utils/NetCore/InventoryRules.h"

// Harvest system parsing functions
FHarvestItemStruct JSONParser::DeserializeHarvestItem(const TSharedPtr<FJsonObject>& ItemObj)
{
	FHarvestItemStruct Item;
	if (!ItemObj.IsValid()) return Item;

		ItemObj->TryGetNumberField(TEXT("itemId"), Item.itemId);

		ItemObj->TryGetStringField(TEXT("itemSlug"), Item.itemSlug);

		ItemObj->TryGetNumberField(TEXT("quantity"), Item.quantity);

		ItemObj->TryGetStringField(TEXT("name"), Item.name);

		ItemObj->TryGetStringField(TEXT("description"), Item.description);

		ItemObj->TryGetNumberField(TEXT("rarityId"), Item.rarityId);

		ItemObj->TryGetStringField(TEXT("rarityName"), Item.rarityName);

		ItemObj->TryGetStringField(TEXT("itemType"), Item.itemType);

		ItemObj->TryGetStringField(TEXT("itemTypeSlug"), Item.itemTypeSlug);

		ItemObj->TryGetNumberField(TEXT("weight"), Item.weight);

		ItemObj->TryGetBoolField(TEXT("addedToInventory"), Item.addedToInventory);

		ItemObj->TryGetBoolField(TEXT("isHarvestItem"), Item.isHarvestItem);

	return Item;
}

FHarvestStartedStruct JSONParser::DeserializeHarvestStarted(const TSharedPtr<FJsonObject>& Body)
{
	FHarvestStartedStruct HarvestStarted;
	if (!Body.IsValid()) return HarvestStarted;

		Body->TryGetStringField(TEXT("type"), HarvestStarted.type);

		Body->TryGetNumberField(TEXT("clientId"), HarvestStarted.clientId);

		Body->TryGetNumberField(TEXT("playerId"), HarvestStarted.playerId);

		Body->TryGetNumberField(TEXT("corpseId"), HarvestStarted.corpseId);

		Body->TryGetNumberField(TEXT("duration"), HarvestStarted.duration);

		double StartTimeMs = 0.0;
		if (Body->TryGetNumberField(TEXT("startTime"), StartTimeMs))
			HarvestStarted.startTime = static_cast<int64>(StartTimeMs);

	return HarvestStarted;
}

FHarvestStartedStruct JSONParser::DeserializeHarvestStarted(const FString& JsonString)
{
	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		UE_LOG(LogNetPacket, Error, TEXT("JSONParser: DeserializeHarvestStarted failed to parse JSON"));
		return FHarvestStartedStruct();
	}

	const TSharedPtr<FJsonObject>* BodyPtr = nullptr;
	if (!Root->TryGetObjectField(TEXT("body"), BodyPtr) || !BodyPtr || !(*BodyPtr).IsValid())
	{
		UE_LOG(LogNetPacket, Error, TEXT("JSONParser: DeserializeHarvestStarted missing body"));
		return FHarvestStartedStruct();
	}

	return JSONParser::DeserializeHarvestStarted(*BodyPtr);
}

FHarvestCompleteStruct JSONParser::DeserializeHarvestComplete(const TSharedPtr<FJsonObject>& Body)
{
	FHarvestCompleteStruct HarvestComplete;
	if (!Body.IsValid()) return HarvestComplete;

		Body->TryGetStringField(TEXT("type"), HarvestComplete.type);

		Body->TryGetNumberField(TEXT("clientId"), HarvestComplete.clientId);

		Body->TryGetNumberField(TEXT("playerId"), HarvestComplete.playerId);

		Body->TryGetNumberField(TEXT("corpseId"), HarvestComplete.corpseId);

		Body->TryGetBoolField(TEXT("success"), HarvestComplete.success);

		Body->TryGetNumberField(TEXT("totalItems"), HarvestComplete.totalItems);

	// Parse available loot
	const TArray<TSharedPtr<FJsonValue>>* LootArray = nullptr;
	if (Body->TryGetArrayField(TEXT("availableLoot"), LootArray))
	{
		for (const TSharedPtr<FJsonValue>& LootValue : *LootArray)
		{
			// TryGetObject (not AsObject): wrong-typed entries skip quietly.
			const TSharedPtr<FJsonObject>* LootPtr = nullptr;
			if (!LootValue.IsValid() || !LootValue->TryGetObject(LootPtr) || !LootPtr || !(*LootPtr).IsValid())
			{
				continue;
			}
			FHarvestItemStruct Item = JSONParser::DeserializeHarvestItem(*LootPtr);
			HarvestComplete.availableLoot.Add(Item);
		}
	}

	return HarvestComplete;
}

FHarvestCompleteStruct JSONParser::DeserializeHarvestComplete(const FString& JsonString)
{
	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		UE_LOG(LogNetPacket, Error, TEXT("JSONParser: DeserializeHarvestComplete failed to parse JSON"));
		return FHarvestCompleteStruct();
	}

	const TSharedPtr<FJsonObject>* BodyPtr = nullptr;
	if (!Root->TryGetObjectField(TEXT("body"), BodyPtr) || !BodyPtr || !(*BodyPtr).IsValid())
	{
		UE_LOG(LogNetPacket, Error, TEXT("JSONParser: DeserializeHarvestComplete missing body"));
		return FHarvestCompleteStruct();
	}

	return JSONParser::DeserializeHarvestComplete(*BodyPtr);
}

FHarvestErrorStruct JSONParser::DeserializeHarvestError(const TSharedPtr<FJsonObject>& Body)
{
	FHarvestErrorStruct HarvestError;
	if (!Body.IsValid()) return HarvestError;

		Body->TryGetStringField(TEXT("type"), HarvestError.type);

		Body->TryGetNumberField(TEXT("clientId"), HarvestError.clientId);

		Body->TryGetNumberField(TEXT("playerId"), HarvestError.playerId);

		Body->TryGetNumberField(TEXT("corpseId"), HarvestError.corpseId);

		Body->TryGetStringField(TEXT("errorCode"), HarvestError.errorCode);

		Body->TryGetStringField(TEXT("message"), HarvestError.message);

	return HarvestError;
}

FHarvestErrorStruct JSONParser::DeserializeHarvestError(const FString& JsonString)
{
	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		UE_LOG(LogNetPacket, Error, TEXT("JSONParser: DeserializeHarvestError failed to parse JSON"));
		return FHarvestErrorStruct();
	}

	const TSharedPtr<FJsonObject>* BodyPtr = nullptr;
	if (!Root->TryGetObjectField(TEXT("body"), BodyPtr) || !BodyPtr || !(*BodyPtr).IsValid())
	{
		UE_LOG(LogNetPacket, Error, TEXT("JSONParser: DeserializeHarvestError missing body"));
		return FHarvestErrorStruct();
	}

	return JSONParser::DeserializeHarvestError(*BodyPtr);
}

FCorpseLootPickupResponseStruct JSONParser::DeserializeCorpseLootPickupResponse(const TSharedPtr<FJsonObject>& Body)
{
	FCorpseLootPickupResponseStruct Response;
	if (!Body.IsValid()) return Response;

		Body->TryGetBoolField(TEXT("success"), Response.success);

		Body->TryGetNumberField(TEXT("corpseUID"), Response.corpseUID);

		Body->TryGetNumberField(TEXT("itemsPickedUp"), Response.itemsPickedUp);

	// Parse picked up items
	const TArray<TSharedPtr<FJsonValue>>* PickedUpArray = nullptr;
	if (Body->TryGetArrayField(TEXT("pickedUpItems"), PickedUpArray))
	{
		for (const TSharedPtr<FJsonValue>& ItemValue : *PickedUpArray)
		{
			// TryGetObject (not AsObject): wrong-typed entries skip quietly.
			const TSharedPtr<FJsonObject>* ItemPtr = nullptr;
			if (!ItemValue.IsValid() || !ItemValue->TryGetObject(ItemPtr) || !ItemPtr || !(*ItemPtr).IsValid())
			{
				continue;
			}
			FHarvestItemStruct Item = JSONParser::DeserializeHarvestItem(*ItemPtr);
			Response.pickedUpItems.Add(Item);
		}
	}

	// Parse remaining loot
	const TArray<TSharedPtr<FJsonValue>>* RemainingArray = nullptr;
	if (Body->TryGetArrayField(TEXT("remainingLoot"), RemainingArray))
	{
		for (const TSharedPtr<FJsonValue>& ItemValue : *RemainingArray)
		{
			// TryGetObject (not AsObject): wrong-typed entries skip quietly.
			const TSharedPtr<FJsonObject>* ItemPtr = nullptr;
			if (!ItemValue.IsValid() || !ItemValue->TryGetObject(ItemPtr) || !ItemPtr || !(*ItemPtr).IsValid())
			{
				continue;
			}
			FHarvestItemStruct Item = JSONParser::DeserializeHarvestItem(*ItemPtr);
			Response.remainingLoot.Add(Item);
		}
	}

	return Response;
}

FCorpseLootPickupResponseStruct JSONParser::DeserializeCorpseLootPickupResponse(const FString& JsonString)
{
	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		UE_LOG(LogNetPacket, Error, TEXT("JSONParser: DeserializeCorpseLootPickupResponse failed to parse JSON"));
		return FCorpseLootPickupResponseStruct();
	}

	const TSharedPtr<FJsonObject>* BodyPtr = nullptr;
	if (!Root->TryGetObjectField(TEXT("body"), BodyPtr) || !BodyPtr || !(*BodyPtr).IsValid())
	{
		UE_LOG(LogNetPacket, Error, TEXT("JSONParser: DeserializeCorpseLootPickupResponse missing body"));
		return FCorpseLootPickupResponseStruct();
	}

	return JSONParser::DeserializeCorpseLootPickupResponse(*BodyPtr);
}

FCorpseLootPickupErrorStruct JSONParser::DeserializeCorpseLootPickupError(const TSharedPtr<FJsonObject>& Body)
{
	FCorpseLootPickupErrorStruct Error;
	if (!Body.IsValid()) return Error;

		Body->TryGetBoolField(TEXT("success"), Error.success);

		Body->TryGetStringField(TEXT("errorCode"), Error.errorCode);

		Body->TryGetNumberField(TEXT("corpseUID"), Error.corpseUID);

	return Error;
}

FCorpseLootPickupErrorStruct JSONParser::DeserializeCorpseLootPickupError(const FString& JsonString)
{
	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		UE_LOG(LogNetPacket, Error, TEXT("JSONParser: DeserializeCorpseLootPickupError failed to parse JSON"));
		return FCorpseLootPickupErrorStruct();
	}

	const TSharedPtr<FJsonObject>* BodyPtr = nullptr;
	if (!Root->TryGetObjectField(TEXT("body"), BodyPtr) || !BodyPtr || !(*BodyPtr).IsValid())
	{
		UE_LOG(LogNetPacket, Error, TEXT("JSONParser: DeserializeCorpseLootPickupError missing body"));
		return FCorpseLootPickupErrorStruct();
	}

	return JSONParser::DeserializeCorpseLootPickupError(*BodyPtr);
}

FCorpseLootInspectResponseStruct JSONParser::DeserializeCorpseLootInspectResponse(const TSharedPtr<FJsonObject>& Body)
{
	FCorpseLootInspectResponseStruct Response;
	if (!Body.IsValid()) return Response;

		Body->TryGetBoolField(TEXT("success"), Response.success);

		Body->TryGetNumberField(TEXT("corpseUID"), Response.corpseUID);

		Body->TryGetStringField(TEXT("type"), Response.type);

		Body->TryGetNumberField(TEXT("totalItems"), Response.totalItems);

	// Parse available loot
	const TArray<TSharedPtr<FJsonValue>>* LootArray = nullptr;
	if (Body->TryGetArrayField(TEXT("availableLoot"), LootArray))
	{
		for (const TSharedPtr<FJsonValue>& LootValue : *LootArray)
		{
			// TryGetObject (not AsObject): wrong-typed entries skip quietly.
			const TSharedPtr<FJsonObject>* LootPtr = nullptr;
			if (!LootValue.IsValid() || !LootValue->TryGetObject(LootPtr) || !LootPtr || !(*LootPtr).IsValid())
			{
				continue;
			}
			FHarvestItemStruct Item = JSONParser::DeserializeHarvestItem(*LootPtr);
			Response.availableLoot.Add(Item);
		}
	}

	return Response;
}

FCorpseLootInspectResponseStruct JSONParser::DeserializeCorpseLootInspectResponse(const FString& JsonString)
{
	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		UE_LOG(LogNetPacket, Error, TEXT("JSONParser: DeserializeCorpseLootInspectResponse failed to parse JSON"));
		return FCorpseLootInspectResponseStruct();
	}

	const TSharedPtr<FJsonObject>* BodyPtr = nullptr;
	if (!Root->TryGetObjectField(TEXT("body"), BodyPtr) || !BodyPtr || !(*BodyPtr).IsValid())
	{
		UE_LOG(LogNetPacket, Error, TEXT("JSONParser: DeserializeCorpseLootInspectResponse missing body"));
		return FCorpseLootInspectResponseStruct();
	}

	return JSONParser::DeserializeCorpseLootInspectResponse(*BodyPtr);
}

FCorpseLootInspectErrorStruct JSONParser::DeserializeCorpseLootInspectError(const TSharedPtr<FJsonObject>& Body)
{
	FCorpseLootInspectErrorStruct Error;
	if (!Body.IsValid()) return Error;

		Body->TryGetBoolField(TEXT("success"), Error.success);

		Body->TryGetStringField(TEXT("errorCode"), Error.errorCode);

	return Error;
}

FCorpseLootInspectErrorStruct JSONParser::DeserializeCorpseLootInspectError(const FString& JsonString)
{
	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		UE_LOG(LogNetPacket, Error, TEXT("JSONParser: DeserializeCorpseLootInspectError failed to parse JSON"));
		return FCorpseLootInspectErrorStruct();
	}

	const TSharedPtr<FJsonObject>* BodyPtr = nullptr;
	if (!Root->TryGetObjectField(TEXT("body"), BodyPtr) || !BodyPtr || !(*BodyPtr).IsValid())
	{
		UE_LOG(LogNetPacket, Error, TEXT("JSONParser: DeserializeCorpseLootInspectError missing body"));
		return FCorpseLootInspectErrorStruct();
	}

	return JSONParser::DeserializeCorpseLootInspectError(*BodyPtr);
}

