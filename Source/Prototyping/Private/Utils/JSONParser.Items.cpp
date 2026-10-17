// Fill out your copyright notice in the Description page of Project Settings.


#include "Utils/JSONParser.h"
#include "Services/TimeSyncService.h"
#include "MyGameInstance.h"
#include "Engine/World.h"
#include "Utils/PlayerAttributeParser.h"
#include "Utils/NetCore/InventoryRules.h"

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
		// TryGetObject (not AsObject): wrong-typed entries skip quietly.
		const TSharedPtr<FJsonObject>* AttrPtr = nullptr;
		if (!Value.IsValid() || !Value->TryGetObject(AttrPtr) || !AttrPtr || !(*AttrPtr).IsValid())
		{
			continue;
		}
		Attributes.Add(JSONParser::DeserializeItemAttribute(*AttrPtr));
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
			// TryGetObject (not AsObject): wrong-typed entries skip quietly.
			const TSharedPtr<FJsonObject>* ItemPtr = nullptr;
			if (!ItemValue.IsValid() || !ItemValue->TryGetObject(ItemPtr) || !ItemPtr || !(*ItemPtr).IsValid())
			{
				continue;
			}
			ItemDropResponse.droppedItems.Add(JSONParser::DeserializeDroppedItem(*ItemPtr));
		}
	}

	return ItemDropResponse;
}

