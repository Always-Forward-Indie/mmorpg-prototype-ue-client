// Fill out your copyright notice in the Description page of Project Settings.


#include "Utils/JSONParser.h"
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

