// Fill out your copyright notice in the Description page of Project Settings.


#include "Utils/JSONParser.h"
#include "Services/TimeSyncService.h"
#include "MyGameInstance.h"
#include "Engine/World.h"
#include "Utils/PlayerAttributeParser.h"
#include "Utils/NetCore/InventoryRules.h"

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
	// Numeric template type id (S2.2: catalog entries + id-only resolutions).
	if (Src->HasField(TEXT("itemType"))) Out.item_type_id = Src->GetIntegerField(TEXT("itemType"));
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
			// TryGetObject (not AsObject): wrong-typed entries skip quietly.
			const TSharedPtr<FJsonObject>* AttrPtr = nullptr;
			if (!AttrVal.IsValid() || !AttrVal->TryGetObject(AttrPtr) || !AttrPtr || !(*AttrPtr).IsValid()) continue;
			const TSharedPtr<FJsonObject>& AttrObj = *AttrPtr;

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
			// TryGetObject (not AsObject): wrong-typed entries skip quietly.
			const TSharedPtr<FJsonObject>* EffPtr = nullptr;
			if (!EffVal.IsValid() || !EffVal->TryGetObject(EffPtr) || !EffPtr || !(*EffPtr).IsValid()) continue;
			const TSharedPtr<FJsonObject>& EffObj = *EffPtr;

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
	if (ItemObj->HasField(TEXT("slotIndex")))  Item.slotIndex  = ItemObj->GetIntegerField(TEXT("slotIndex"));
	if (ItemObj->HasField(TEXT("killCount")))  Item.killCount  = ItemObj->GetIntegerField(TEXT("killCount"));

	// Durability from top-level (overridden by nested "item" if present)
	if (ItemObj->HasField(TEXT("durabilityCurrent"))) Item.durabilityCurrent = ItemObj->GetIntegerField(TEXT("durabilityCurrent"));
	if (ItemObj->HasField(TEXT("durabilityMax")))     Item.durabilityMax     = ItemObj->GetIntegerField(TEXT("durabilityMax"));

	// isEquipped flag — server sends camelCase "isEquipped"
	if (ItemObj->HasField(TEXT("isEquipped"))) Item.is_equipped = ItemObj->GetBoolField(TEXT("isEquipped"));

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

	// Ensure durabilityCurrent has a sane default, but never mask an explicit 0:
	// 0 from the server means broken. Fall back to max only when nobody sent the field.
	const bool bDurabilitySent = ItemObj->HasField(TEXT("durabilityCurrent")) || (NestedItemPtr && (*NestedItemPtr).IsValid() && (*NestedItemPtr)->HasField(TEXT("durabilityCurrent")));
	Item.durabilityCurrent = FInventoryRules::ResolveDurabilityCurrent(
		bDurabilitySent, Item.durabilityCurrent, Item.durabilityMax, Item.isDurable);

	return Item;
}


FCharacterInventoryStruct JSONParser::DeserializeCharacterInventory(const FString& JsonString)
{
	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("JSONParser: DeserializeCharacterInventory failed to parse JSON"));
		return FCharacterInventoryStruct();
	}

	const TSharedPtr<FJsonObject>* BodyPtr = nullptr;
	if (!Root->TryGetObjectField(TEXT("body"), BodyPtr) || !BodyPtr || !(*BodyPtr).IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("JSONParser: DeserializeCharacterInventory missing body"));
		return FCharacterInventoryStruct();
	}

	return JSONParser::DeserializeCharacterInventory(*BodyPtr);
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
			// TryGetObject (not AsObject): wrong-typed entries skip quietly.
			const TSharedPtr<FJsonObject>* ItemPtr = nullptr;
			if (!ItemValue.IsValid() || !ItemValue->TryGetObject(ItemPtr) || !ItemPtr || !(*ItemPtr).IsValid()) continue;
			const TSharedPtr<FJsonObject>& ItemObj = *ItemPtr;

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

	UpdateObj->TryGetStringField(TEXT("eventType"), Update.eventType);
	UpdateObj->TryGetNumberField(TEXT("characterId"), Update.characterId);

	// Parse data object
	const TSharedPtr<FJsonObject>* DataPtr = nullptr;
	if (UpdateObj->TryGetObjectField(TEXT("data"), DataPtr) && DataPtr && (*DataPtr).IsValid())
	{
		Update.data = JSONParser::DeserializeCharacterInventory(*DataPtr);
	}

	return Update;
}

FInventoryUpdateStruct JSONParser::DeserializeInventoryUpdate(const FString& JsonString)
{
	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("JSONParser: DeserializeInventoryUpdate failed to parse JSON"));
		return FInventoryUpdateStruct();
	}

	// Packet convention (header/body like every other entry): the update
	// object lives in body. The object overload stays the source of truth
	// for the unwrapped shape.
	const TSharedPtr<FJsonObject>* BodyPtr = nullptr;
	if (!Root->TryGetObjectField(TEXT("body"), BodyPtr) || !BodyPtr || !(*BodyPtr).IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("JSONParser: DeserializeInventoryUpdate missing body"));
		return FInventoryUpdateStruct();
	}

	return JSONParser::DeserializeInventoryUpdate(*BodyPtr);
}

