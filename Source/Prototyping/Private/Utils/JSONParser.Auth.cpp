// Fill out your copyright notice in the Description page of Project Settings.


#include "Utils/JSONParser.h"
#include "Data/DataStructs.h"
#include "Authentication/LoginFlowTypes.h"
#include "Prototyping.h"
#include "Services/TimeSyncService.h"
#include "MyGameInstance.h"
#include "Engine/World.h"
#include "Utils/PlayerAttributeParser.h"
#include "Utils/NetCore/InventoryRules.h"

//deserialize a JSON containing a list of characters (login screen — uses slug fields from server v0.1.12+)
TArray<FLoginCharacterEntry> JSONParser::DeserializeLoginCharactersList(const FString& JsonString)
{
	TArray<FLoginCharacterEntry> CharacterList;
	TSharedPtr<FJsonObject> JsonObject;

	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		UE_LOG(LogNetPacket, Error, TEXT("JSONParser: DeserializeLoginCharactersList failed to parse JSON"));
		return CharacterList;
	}

	const TSharedPtr<FJsonObject>* BodyObject = nullptr;
	if (!JsonObject->TryGetObjectField(TEXT("body"), BodyObject) || BodyObject == nullptr)
	{
		UE_LOG(LogNetPacket, Error, TEXT("JSONParser: DeserializeLoginCharactersList missing body"));
		return CharacterList;
	}

	const TArray<TSharedPtr<FJsonValue>>* CharactersArray = nullptr;
	if (!(*BodyObject)->TryGetArrayField(TEXT("charactersList"), CharactersArray) || CharactersArray == nullptr)
	{
		return CharacterList;
	}

	for (const TSharedPtr<FJsonValue>& CharacterValue : *CharactersArray)
	{
		// TryGetObject (not AsObject): wrong-typed entries skip quietly.
		const TSharedPtr<FJsonObject>* CharacterPtr = nullptr;
		if (!CharacterValue.IsValid() || !CharacterValue->TryGetObject(CharacterPtr) || !CharacterPtr || !(*CharacterPtr).IsValid())
		{
			continue;
		}
		const TSharedPtr<FJsonObject>& CharacterObject = *CharacterPtr;

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
				const TSharedPtr<FJsonObject>* EquipPtr = nullptr;
				if (!EquipValue.IsValid() || !EquipValue->TryGetObject(EquipPtr) || !EquipPtr || !(*EquipPtr).IsValid()) continue;
				const TSharedPtr<FJsonObject>& EquipObject = *EquipPtr;

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



FCharacterCreationOptions JSONParser::DeserializeCharacterCreationOptions(const FString& JsonString)
{
    FCharacterCreationOptions Options;

    TSharedPtr<FJsonObject> Root;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
    {
        UE_LOG(LogNetPacket, Error, TEXT("JSONParser: DeserializeCharacterCreationOptions failed to parse JSON"));
        return Options;
    }

    const TSharedPtr<FJsonObject>* BodyPtr = nullptr;
    if (!Root->TryGetObjectField(TEXT("body"), BodyPtr) || !BodyPtr)
    {
        UE_LOG(LogNetPacket, Error, TEXT("JSONParser: DeserializeCharacterCreationOptions missing body"));
        return Options;
    }
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
