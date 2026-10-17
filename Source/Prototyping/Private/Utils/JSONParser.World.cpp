// Fill out your copyright notice in the Description page of Project Settings.


#include "Utils/JSONParser.h"
#include "Services/TimeSyncService.h"
#include "MyGameInstance.h"
#include "Engine/World.h"
#include "Utils/PlayerAttributeParser.h"
#include "Utils/NetCore/InventoryRules.h"

// ─────────────────────────────────────────────────────────────────────────────

FWorldObjectData JSONParser::DeserializeWorldObject(const TSharedPtr<FJsonObject>& Obj)
{
    FWorldObjectData Data;
    if (!Obj.IsValid()) return Data;

    Obj->TryGetNumberField(TEXT("id"),                  Data.ObjectId);
    Obj->TryGetStringField(TEXT("slug"),                Data.Slug);
    Obj->TryGetStringField(TEXT("nameKey"),             Data.NameKey);

    FString TypeStr, ScopeStr, StateStr;
    Obj->TryGetStringField(TEXT("objectType"),          TypeStr);
    Obj->TryGetStringField(TEXT("scope"),               ScopeStr);

    Data.ObjectType = WIOHelpers::ParseObjectType(TypeStr);
    Data.Scope      = WIOHelpers::ParseScope(ScopeStr);

    double Tmp = 0.0;
    if (Obj->TryGetNumberField(TEXT("posX"), Tmp))               Data.PosX = static_cast<float>(Tmp);
    if (Obj->TryGetNumberField(TEXT("posY"), Tmp))               Data.PosY = static_cast<float>(Tmp);
    if (Obj->TryGetNumberField(TEXT("posZ"), Tmp))               Data.PosZ = static_cast<float>(Tmp);
    if (Obj->TryGetNumberField(TEXT("rotZ"), Tmp))               Data.RotZ = static_cast<float>(Tmp);
    if (Obj->TryGetNumberField(TEXT("interactionRadius"), Tmp))  Data.InteractionRadius = static_cast<float>(Tmp);

    Obj->TryGetNumberField(TEXT("zoneId"),              Data.ZoneId);
    Obj->TryGetNumberField(TEXT("channelTimeSec"),      Data.ChannelTimeSec);
    Obj->TryGetNumberField(TEXT("respawnSec"),          Data.RespawnSec);
    Obj->TryGetNumberField(TEXT("minLevel"),            Data.MinLevel);
    Obj->TryGetNumberField(TEXT("dialogueId"),          Data.DialogueId);
    Obj->TryGetNumberField(TEXT("requiredItemId"),      Data.RequiredItemId);

    // Server sends "currentState" or "state" depending on the packet
    if (Obj->TryGetStringField(TEXT("currentState"), StateStr) || Obj->TryGetStringField(TEXT("state"), StateStr))
    {
        Data.CurrentState = WIOHelpers::ParseState(StateStr);
    }

    return Data;
}

TArray<FWorldObjectData> JSONParser::DeserializeWorldObjectsList(const TSharedPtr<FJsonObject>& Body)
{
    TArray<FWorldObjectData> Result;
    if (!Body.IsValid()) return Result;

    const TArray<TSharedPtr<FJsonValue>>* ObjectsArray = nullptr;
    if (!Body->TryGetArrayField(TEXT("worldObjects"), ObjectsArray) || !ObjectsArray)
    {
        return Result;
    }

    for (const TSharedPtr<FJsonValue>& Val : *ObjectsArray)
    {
        const TSharedPtr<FJsonObject>* ObjPtr = nullptr;
        if (!Val->TryGetObject(ObjPtr) || !ObjPtr) continue;
        Result.Add(DeserializeWorldObject(*ObjPtr));
    }

    return Result;
}

TArray<FWorldObjectData> JSONParser::DeserializeWorldObjectsList(const FString& JsonString)
{
    TSharedPtr<FJsonObject> Root;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()) return {};

    const TSharedPtr<FJsonObject>* BodyPtr = nullptr;
    if (!Root->TryGetObjectField(TEXT("body"), BodyPtr) || !BodyPtr) return {};

    return DeserializeWorldObjectsList(*BodyPtr);
}

FWIOInteractResult JSONParser::DeserializeWIOInteractResult(const TSharedPtr<FJsonObject>& Body)
{
    FWIOInteractResult Result;
    if (!Body.IsValid()) return Result;

    Body->TryGetNumberField(TEXT("objectId"),       Result.ObjectId);
    Body->TryGetBoolField  (TEXT("success"),        Result.bSuccess);
    Body->TryGetStringField(TEXT("errorCode"),      Result.ErrorCode);
    Body->TryGetStringField(TEXT("interactionType"),Result.InteractionType);
    Body->TryGetNumberField(TEXT("channelTimeSec"), Result.ChannelTimeSec);

    const TArray<TSharedPtr<FJsonValue>>* LootArray = nullptr;
    if (Body->TryGetArrayField(TEXT("lootItems"), LootArray) && LootArray)
    {
        for (const TSharedPtr<FJsonValue>& Val : *LootArray)
        {
            const TSharedPtr<FJsonObject>* ItemPtr = nullptr;
            if (!Val->TryGetObject(ItemPtr)) continue;

            FWIOLootItem Item;
            (*ItemPtr)->TryGetNumberField(TEXT("itemId"),   Item.ItemId);
            (*ItemPtr)->TryGetNumberField(TEXT("quantity"), Item.Quantity);
            Result.LootItems.Add(Item);
        }
    }

    return Result;
}

FWIOInteractResult JSONParser::DeserializeWIOInteractResult(const FString& JsonString)
{
    TSharedPtr<FJsonObject> Root;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()) return {};

    const TSharedPtr<FJsonObject>* BodyPtr = nullptr;
    if (!Root->TryGetObjectField(TEXT("body"), BodyPtr) || !BodyPtr) return {};

    return DeserializeWIOInteractResult(*BodyPtr);
}

FWIOStateUpdate JSONParser::DeserializeWIOStateUpdate(const TSharedPtr<FJsonObject>& Body)
{
    FWIOStateUpdate Update;
    if (!Body.IsValid()) return Update;

    Body->TryGetNumberField(TEXT("objectId"),   Update.ObjectId);
    Body->TryGetNumberField(TEXT("respawnSec"), Update.RespawnSec);

    FString StateStr;
    if (Body->TryGetStringField(TEXT("state"), StateStr))
    {
        Update.NewState = WIOHelpers::ParseState(StateStr);
    }

    return Update;
}

FWIOStateUpdate JSONParser::DeserializeWIOStateUpdate(const FString& JsonString)
{
    TSharedPtr<FJsonObject> Root;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()) return {};

    const TSharedPtr<FJsonObject>* BodyPtr = nullptr;
    if (!Root->TryGetObjectField(TEXT("body"), BodyPtr) || !BodyPtr) return {};

    return DeserializeWIOStateUpdate(*BodyPtr);
}

FWIOChannelCancelled JSONParser::DeserializeWIOChannelCancelled(const TSharedPtr<FJsonObject>& Body)
{
    FWIOChannelCancelled Data;
    if (!Body.IsValid()) return Data;

    Body->TryGetNumberField(TEXT("objectId"), Data.ObjectId);
    return Data;
}

FWIOChannelCancelled JSONParser::DeserializeWIOChannelCancelled(const FString& JsonString)
{
    TSharedPtr<FJsonObject> Root;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()) return {};

    const TSharedPtr<FJsonObject>* BodyPtr = nullptr;
    if (!Root->TryGetObjectField(TEXT("body"), BodyPtr) || !BodyPtr) return {};

    return DeserializeWIOChannelCancelled(*BodyPtr);
}

// ─────────────────────────────────────────────────────────────────────────────
// Login Flow — Character Creation Options
// ─────────────────────────────────────────────────────────────────────────────

