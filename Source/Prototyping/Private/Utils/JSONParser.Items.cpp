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

		AttributeObj->TryGetNumberField(TEXT("id"), ItemAttribute.id);
	
		AttributeObj->TryGetStringField(TEXT("name"), ItemAttribute.name);
	
		AttributeObj->TryGetStringField(TEXT("slug"), ItemAttribute.slug);
	
		AttributeObj->TryGetNumberField(TEXT("value"), ItemAttribute.value);

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

		ItemObj->TryGetNumberField(TEXT("id"), Item.id);
	
		ItemObj->TryGetStringField(TEXT("name"), Item.name);
	
		ItemObj->TryGetStringField(TEXT("slug"), Item.slug);
	
		ItemObj->TryGetStringField(TEXT("description"), Item.description);
	
		ItemObj->TryGetBoolField(TEXT("isQuestItem"), Item.isQuestItem);
	
	int32 ItemTypeRaw = 0;
	if (ItemObj->TryGetNumberField(TEXT("itemType"), ItemTypeRaw))
	{
		Item.itemType = static_cast<EItemType>(ItemTypeRaw);
	}
	
		ItemObj->TryGetStringField(TEXT("itemTypeName"), Item.itemTypeName);
	
		ItemObj->TryGetStringField(TEXT("itemTypeSlug"), Item.itemTypeSlug);
	
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

		DroppedItemObj->TryGetNumberField(TEXT("uid"), DroppedItem.uid);
	
		DroppedItemObj->TryGetNumberField(TEXT("itemId"), DroppedItem.itemId);
	
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
	
		DroppedItemObj->TryGetNumberField(TEXT("droppedByCharacterId"), DroppedItem.droppedByCharacterId);

		DroppedItemObj->TryGetNumberField(TEXT("quantity"), DroppedItem.quantity);
	
		DroppedItemObj->TryGetBoolField(TEXT("canBePickedUp"), DroppedItem.canBePickedUp);

		DroppedItemObj->TryGetNumberField(TEXT("reservedForCharacterId"), DroppedItem.reservedForCharacterId);

	if (DroppedItemObj->HasField(TEXT("reservationSecondsLeft")))
	{
		double ResvSecs = 0.0;
		DroppedItemObj->TryGetNumberField(TEXT("reservationSecondsLeft"), ResvSecs);
		DroppedItem.reservationSecondsLeft = static_cast<int64>(ResvSecs);
	}
	
	const TSharedPtr<FJsonObject>* PositionPtr = nullptr;
	if (DroppedItemObj->TryGetObjectField(TEXT("position"), PositionPtr) && PositionPtr && (*PositionPtr).IsValid())
		DroppedItem.position = JSONParser::DeserializePositionData(*PositionPtr);

	const TSharedPtr<FJsonObject>* ItemPtr = nullptr;
	if (DroppedItemObj->TryGetObjectField(TEXT("item"), ItemPtr) && ItemPtr && (*ItemPtr).IsValid())
		DroppedItem.item = JSONParser::DeserializeItemData(*ItemPtr);

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

