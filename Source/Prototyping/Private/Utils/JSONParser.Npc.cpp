// Fill out your copyright notice in the Description page of Project Settings.


#include "Utils/JSONParser.h"
#include "Services/TimeSyncService.h"
#include "MyGameInstance.h"
#include "Engine/World.h"
#include "Utils/PlayerAttributeParser.h"
#include "Utils/NetCore/InventoryRules.h"

// NPC system parsers implementation
FNPCStatsStruct JSONParser::DeserializeNPCStats(const TSharedPtr<FJsonObject>& StatsObj)
{
    FNPCStatsStruct Stats;
    
    if (StatsObj.IsValid())
    {
        StatsObj->TryGetNumberField(TEXT("current"), Stats.current);
        StatsObj->TryGetNumberField(TEXT("max"), Stats.max);
    }
    
    return Stats;
}

FNPCHealthManaStruct JSONParser::DeserializeNPCHealthMana(const TSharedPtr<FJsonObject>& HealthManaObj)
{
    FNPCHealthManaStruct HealthMana;
    
    if (HealthManaObj.IsValid())
    {
        const TSharedPtr<FJsonObject>* HealthObj = nullptr;
        if (HealthManaObj->TryGetObjectField(TEXT("health"), HealthObj) && HealthObj)
        {
            HealthMana.health = DeserializeNPCStats(*HealthObj);
        }
        
        const TSharedPtr<FJsonObject>* ManaObj = nullptr;
        if (HealthManaObj->TryGetObjectField(TEXT("mana"), ManaObj) && ManaObj)
        {
            HealthMana.mana = DeserializeNPCStats(*ManaObj);
        }
    }
    
    return HealthMana;
}

FNPCStruct JSONParser::DeserializeNPCData(const TSharedPtr<FJsonObject>& NPCObj)
{
    FNPCStruct NPC;
    
    if (NPCObj.IsValid())
    {
        NPCObj->TryGetNumberField(TEXT("id"), NPC.id);
        NPCObj->TryGetStringField(TEXT("name"), NPC.name);
        NPCObj->TryGetStringField(TEXT("slug"), NPC.slug);
        NPCObj->TryGetStringField(TEXT("race"), NPC.race);
        NPCObj->TryGetNumberField(TEXT("level"), NPC.level);
        NPCObj->TryGetStringField(TEXT("npcType"), NPC.npcType);
        NPCObj->TryGetBoolField(TEXT("isInteractable"), NPC.isInteractable);
        NPCObj->TryGetStringField(TEXT("dialogueId"), NPC.dialogueId);
        NPCObj->TryGetStringField(TEXT("questId"), NPC.questId);
        
        // Parse position
        const TSharedPtr<FJsonObject>* PositionObj = nullptr;
        if (NPCObj->TryGetObjectField(TEXT("position"), PositionObj) && PositionObj)
        {
            NPC.position = DeserializePositionData(*PositionObj);
        }
        
        // Parse attributes
        const TArray<TSharedPtr<FJsonValue>>* AttributesArray = nullptr;
        if (NPCObj->TryGetArrayField(TEXT("attributes"), AttributesArray))
        {
            for (const auto& AttributeValue : *AttributesArray)
            {
                const TSharedPtr<FJsonObject>* AttributeObj = nullptr;
                if (AttributeValue->TryGetObject(AttributeObj) && AttributeObj)
                {
                    FAttributeDataStruct Attribute;
                    (*AttributeObj)->TryGetNumberField(TEXT("id"), Attribute.attributeId);
                    (*AttributeObj)->TryGetStringField(TEXT("name"), Attribute.attributeName);
                    (*AttributeObj)->TryGetStringField(TEXT("slug"), Attribute.attributeSlug);
                    (*AttributeObj)->TryGetNumberField(TEXT("value"), Attribute.attributeValue);
                    NPC.attributes.Add(Attribute);
                }
            }
        }
        
        // Parse quests
        const TArray<TSharedPtr<FJsonValue>>* QuestsArray = nullptr;
        if (NPCObj->TryGetArrayField(TEXT("quests"), QuestsArray))
        {
            for (const auto& QuestValue : *QuestsArray)
            {
                const TSharedPtr<FJsonObject>* QuestObj = nullptr;
                if (QuestValue->TryGetObject(QuestObj) && QuestObj)
                {
                    FNPCQuestEntry Entry;
                    (*QuestObj)->TryGetStringField(TEXT("slug"), Entry.slug);
                    (*QuestObj)->TryGetStringField(TEXT("status"), Entry.status);
                    NPC.quests.Add(Entry);
                }
            }
        }

        // Parse stats
        const TSharedPtr<FJsonObject>* StatsObj = nullptr;
        if (NPCObj->TryGetObjectField(TEXT("stats"), StatsObj) && StatsObj)
        {
            NPC.stats = DeserializeNPCHealthMana(*StatsObj);
        }
    }
    
    return NPC;
}

TArray<FNPCStruct> JSONParser::DeserializeNPCsList(const TArray<TSharedPtr<FJsonValue>>& JsonArray)
{
    TArray<FNPCStruct> NPCs;
    
    for (const auto& NPCValue : JsonArray)
    {
        const TSharedPtr<FJsonObject>* NPCObj = nullptr;
        if (NPCValue->TryGetObject(NPCObj) && NPCObj)
        {
            FNPCStruct NPC = DeserializeNPCData(*NPCObj);
            NPCs.Add(NPC);
        }
    }
    
    return NPCs;
}

FNPCSpawnDataStruct JSONParser::DeserializeNPCSpawnData(const TSharedPtr<FJsonObject>& Body)
{
    FNPCSpawnDataStruct SpawnData;
    
    if (Body.IsValid())
    {
        Body->TryGetNumberField(TEXT("npcCount"), SpawnData.npcCount);
        Body->TryGetNumberField(TEXT("spawnRadius"), SpawnData.spawnRadius);
        
        const TArray<TSharedPtr<FJsonValue>>* NPCsArray = nullptr;
        if (Body->TryGetArrayField(TEXT("npcsSpawn"), NPCsArray))
        {
            SpawnData.npcsSpawn = DeserializeNPCsList(*NPCsArray);
        }
    }
    
    return SpawnData;
}

FNPCSpawnDataStruct JSONParser::DeserializeNPCSpawnData(const FString& JsonString)
{
    FNPCSpawnDataStruct SpawnData;
    
    TSharedPtr<FJsonObject> Root;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
    
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("JSONParser: DeserializeNPCSpawnData failed to parse JSON"));
        return SpawnData;
    }

    const TSharedPtr<FJsonObject>* BodyObject = nullptr;
    if (!Root->TryGetObjectField(TEXT("body"), BodyObject) || !BodyObject || !(*BodyObject).IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("JSONParser: DeserializeNPCSpawnData missing body"));
        return SpawnData;
    }

    return DeserializeNPCSpawnData(*BodyObject);
}

