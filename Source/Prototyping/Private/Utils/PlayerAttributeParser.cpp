#include "Utils/PlayerAttributeParser.h"
#include "Prototyping.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"

FPlayerStatsUpdateStruct PlayerAttributeParser::DeserializePlayerStatsUpdate(const FString& JsonString)
{
    FPlayerStatsUpdateStruct StatsUpdate;
    
    // Parse the JSON string
    TSharedPtr<FJsonObject> Root;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
    
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
    {
        UE_LOG(LogNetPacket, Error, TEXT("PlayerAttributeParser: Failed to parse JSON string"));
        return StatsUpdate;
    }
    
    // Get the body object
    const TSharedPtr<FJsonObject>* BodyPtr = nullptr;
    if (!Root->TryGetObjectField(TEXT("body"), BodyPtr) || !BodyPtr || !(*BodyPtr).IsValid())
    {
        UE_LOG(LogNetPacket, Error, TEXT("PlayerAttributeParser: No valid body found in JSON"));
        return StatsUpdate;
    }
    TSharedPtr<FJsonObject> Body = *BodyPtr;
    
    return DeserializePlayerStatsUpdate(Body);
}

FPlayerStatsUpdateStruct PlayerAttributeParser::DeserializePlayerStatsUpdate(const TSharedPtr<FJsonObject>& Body)
{
    FPlayerStatsUpdateStruct StatsUpdate;
    
    if (!Body.IsValid())
    {
        UE_LOG(LogNetPacket, Error, TEXT("PlayerAttributeParser: Invalid body object"));
        return StatsUpdate;
    }
    
    // Parse character ID
        Body->TryGetNumberField(TEXT("characterId"), StatsUpdate.characterId);
    
    // Parse level
        Body->TryGetNumberField(TEXT("level"), StatsUpdate.level);

    // Parse free skill points
        Body->TryGetNumberField(TEXT("freeSkillPoints"), StatsUpdate.freeSkillPoints);

    // Parse health data
    const TSharedPtr<FJsonObject>* HealthPtr = nullptr;
    if (Body->TryGetObjectField(TEXT("health"), HealthPtr) && HealthPtr && (*HealthPtr).IsValid())
    {
        TSharedPtr<FJsonObject> HealthObj = *HealthPtr;
        ParseHealthData(HealthObj, StatsUpdate.healthCurrent, StatsUpdate.healthMax);
    }
    
    // Parse mana data
    const TSharedPtr<FJsonObject>* ManaPtr = nullptr;
    if (Body->TryGetObjectField(TEXT("mana"), ManaPtr) && ManaPtr && (*ManaPtr).IsValid())
    {
        TSharedPtr<FJsonObject> ManaObj = *ManaPtr;
        ParseManaData(ManaObj, StatsUpdate.manaCurrent, StatsUpdate.manaMax);
    }

    // Parse experience data from "experience" sub-object (stats_update format)
    const TSharedPtr<FJsonObject>* ExpPtr = nullptr;
    if (Body->TryGetObjectField(TEXT("experience"), ExpPtr) && ExpPtr && (*ExpPtr).IsValid())
    {
        TSharedPtr<FJsonObject> ExpObj = *ExpPtr;
                ExpObj->TryGetNumberField(TEXT("current"), StatsUpdate.experienceCurrent);
                ExpObj->TryGetNumberField(TEXT("levelStart"), StatsUpdate.experienceLevelStart);
                ExpObj->TryGetNumberField(TEXT("nextLevel"), StatsUpdate.experienceNextLevel);
                ExpObj->TryGetNumberField(TEXT("debt"), StatsUpdate.experienceDebt);
    }

    // Parse attributes array
    if (Body->HasField(TEXT("attributes")))
    {
        const TArray<TSharedPtr<FJsonValue>>* AttrsArray = nullptr;
        if (Body->TryGetArrayField(TEXT("attributes"), AttrsArray))
        {
            for (const TSharedPtr<FJsonValue>& AttrVal : *AttrsArray)
            {
                // TryGetObject (not AsObject): wrong-typed entries skip quietly.
                const TSharedPtr<FJsonObject>* AttrPtr = nullptr;
                if (!AttrVal.IsValid() || !AttrVal->TryGetObject(AttrPtr) || !AttrPtr || !(*AttrPtr).IsValid()) continue;
                const TSharedPtr<FJsonObject>& AttrObj = *AttrPtr;

                FStatAttributeEntry Entry;
                    AttrObj->TryGetStringField(TEXT("slug"), Entry.slug);
                    AttrObj->TryGetStringField(TEXT("name"), Entry.name);
                    AttrObj->TryGetNumberField(TEXT("base"), Entry.base);
                    AttrObj->TryGetNumberField(TEXT("effective"), Entry.effective);
                Entry.baseValue  = Entry.base;
                Entry.totalValue = Entry.effective;
                StatsUpdate.attributes.Add(Entry);
            }
        }
    }

    // Parse activeEffects array
    if (Body->HasField(TEXT("activeEffects")))
    {
        const TArray<TSharedPtr<FJsonValue>>* EffectsArray = nullptr;
        if (Body->TryGetArrayField(TEXT("activeEffects"), EffectsArray))
        {
            for (const TSharedPtr<FJsonValue>& EffectVal : *EffectsArray)
            {
                // TryGetObject (not AsObject): wrong-typed entries skip quietly.
                const TSharedPtr<FJsonObject>* EffPtr = nullptr;
                if (!EffectVal.IsValid() || !EffectVal->TryGetObject(EffPtr) || !EffPtr || !(*EffPtr).IsValid()) continue;
                const TSharedPtr<FJsonObject>& EffObj = *EffPtr;

                FActiveEffectEntry Effect;
                    EffObj->TryGetStringField(TEXT("slug"), Effect.slug);
                    EffObj->TryGetStringField(TEXT("effectTypeSlug"), Effect.effectTypeSlug);
                    EffObj->TryGetStringField(TEXT("attributeSlug"), Effect.attributeSlug);
                if (double TmpValue = 0.0; EffObj->TryGetNumberField(TEXT("value"), TmpValue))
                    Effect.value = static_cast<float>(TmpValue);
                if (double TmpExpiresAt = 0.0; EffObj->TryGetNumberField(TEXT("expiresAt"), TmpExpiresAt))
                    Effect.expiresAt = static_cast<int64>(TmpExpiresAt);
                StatsUpdate.activeEffects.Add(Effect);
            }
        }
    }

    // Parse weight data
    const TSharedPtr<FJsonObject>* WeightPtr = nullptr;
    if (Body->TryGetObjectField(TEXT("weight"), WeightPtr) && WeightPtr && (*WeightPtr).IsValid())
    {
        const TSharedPtr<FJsonObject>& WeightObj = *WeightPtr;
        if (double TmpCurrent = 0.0; WeightObj->TryGetNumberField(TEXT("current"), TmpCurrent))
            StatsUpdate.weightCurrent = static_cast<float>(TmpCurrent);
        if (double TmpMax = 0.0; WeightObj->TryGetNumberField(TEXT("max"), TmpMax))
            StatsUpdate.weightMax = static_cast<float>(TmpMax);
    }

    // Validate the parsed data
    if (!ValidateStatsData(StatsUpdate))
    {
        UE_LOG(LogNetPacket, Warning, TEXT("PlayerAttributeParser: Validation failed for stats data"));
    }
    else
    {
        UE_LOG(LogNetPacket, Verbose, TEXT("PlayerAttributeParser: Successfully parsed stats - CharID: %d, Level: %d, HP: %d/%d, MP: %d/%d"),
            StatsUpdate.characterId, StatsUpdate.level, 
            StatsUpdate.healthCurrent, StatsUpdate.healthMax,
            StatsUpdate.manaCurrent, StatsUpdate.manaMax);
    }
    
    return StatsUpdate;
}

void PlayerAttributeParser::ParseHealthData(const TSharedPtr<FJsonObject>& HealthObj, int32& OutCurrent, int32& OutMax)
{
    if (!HealthObj.IsValid())
    {
        UE_LOG(LogNetPacket, Warning, TEXT("PlayerAttributeParser: Invalid health object"));
        return;
    }
    
        HealthObj->TryGetNumberField(TEXT("current"), OutCurrent);
    
        HealthObj->TryGetNumberField(TEXT("max"), OutMax);
    
    UE_LOG(LogGameplay, VeryVerbose, TEXT("PlayerAttributeParser: Parsed health - Current: %d, Max: %d"), OutCurrent, OutMax);
}

void PlayerAttributeParser::ParseManaData(const TSharedPtr<FJsonObject>& ManaObj, int32& OutCurrent, int32& OutMax)
{
    if (!ManaObj.IsValid())
    {
        UE_LOG(LogNetPacket, Warning, TEXT("PlayerAttributeParser: Invalid mana object"));
        return;
    }
    
        ManaObj->TryGetNumberField(TEXT("current"), OutCurrent);
    
        ManaObj->TryGetNumberField(TEXT("max"), OutMax);
    
    UE_LOG(LogGameplay, VeryVerbose, TEXT("PlayerAttributeParser: Parsed mana - Current: %d, Max: %d"), OutCurrent, OutMax);
}

bool PlayerAttributeParser::ValidateStatsData(const FPlayerStatsUpdateStruct& StatsData)
{
    // Check if character ID is valid
    if (StatsData.characterId <= 0)
    {
        UE_LOG(LogNetPacket, Error, TEXT("PlayerAttributeParser: Invalid character ID: %d"), StatsData.characterId);
        return false;
    }
    
    // Check if level is valid
    // Treat level == 0 as a warning but do not reject the packet: a death
    // stats_update is valid even if the server omits or zeroes the level field.
    if (StatsData.level <= 0)
    {
        UE_LOG(LogNetPacket, Warning, TEXT("PlayerAttributeParser: Unexpected level value: %d (packet still processed)"), StatsData.level);
    }
    
    // Check if health values are valid
    if (StatsData.healthCurrent < 0 || StatsData.healthMax <= 0)
    {
        UE_LOG(LogNetPacket, Error, TEXT("PlayerAttributeParser: Invalid health values - Current: %d, Max: %d"), 
            StatsData.healthCurrent, StatsData.healthMax);
        return false;
    }
    
    if (StatsData.healthCurrent > StatsData.healthMax)
    {
        UE_LOG(LogNetPacket, Warning, TEXT("PlayerAttributeParser: Current health (%d) exceeds max health (%d)"), 
            StatsData.healthCurrent, StatsData.healthMax);
    }
    
    // Check if mana values are valid
    if (StatsData.manaCurrent < 0 || StatsData.manaMax < 0)
    {
        UE_LOG(LogNetPacket, Error, TEXT("PlayerAttributeParser: Invalid mana values - Current: %d, Max: %d"), 
            StatsData.manaCurrent, StatsData.manaMax);
        return false;
    }
    
    if (StatsData.manaCurrent > StatsData.manaMax)
    {
        UE_LOG(LogNetPacket, Warning, TEXT("PlayerAttributeParser: Current mana (%d) exceeds max mana (%d)"), 
            StatsData.manaCurrent, StatsData.manaMax);
    }
    
    return true;
}

void PlayerAttributeParser::UpdateCharacterDataFromStatsUpdate(FCharacterDataStruct& CharacterData, const FPlayerStatsUpdateStruct& StatsUpdate)
{
    // Update basic character info
    CharacterData.characterId = StatsUpdate.characterId;
    CharacterData.characterLevel = StatsUpdate.level;
    
    // Update health and mana
    CharacterData.characterCurrentHealth = StatsUpdate.healthCurrent;
    CharacterData.characterCurrentMana = StatsUpdate.manaCurrent;
    
    // Update max health and mana in attributes if they exist
    if (StatsUpdate.healthMax > 0)
    {
        FAttributeDataStruct HealthAttr;
        HealthAttr.attributeSlug = TEXT("max_health");
        HealthAttr.attributeName = TEXT("Max Health");
        HealthAttr.attributeValue = StatsUpdate.healthMax;
        CharacterData.characterAttributes.attributesData.Add(TEXT("max_health"), HealthAttr);
    }
    
    if (StatsUpdate.manaMax > 0)
    {
        FAttributeDataStruct ManaAttr;
        ManaAttr.attributeSlug = TEXT("max_mana");
        ManaAttr.attributeName = TEXT("Max Mana");
        ManaAttr.attributeValue = StatsUpdate.manaMax;
        CharacterData.characterAttributes.attributesData.Add(TEXT("max_mana"), ManaAttr);
    }
    
    // Sync experience fields into character data
    if (StatsUpdate.experienceCurrent > 0 || StatsUpdate.experienceNextLevel > 0)
    {
        CharacterData.characterExperiencePoints = StatsUpdate.experienceCurrent;
        CharacterData.characterExpForLevelStart  = StatsUpdate.experienceLevelStart;
        CharacterData.characterExpForLevelEnd    = StatsUpdate.experienceNextLevel;
        CharacterData.characterExperienceDebt    = StatsUpdate.experienceDebt;
    }

    UE_LOG(LogNetPacket, Verbose, TEXT("PlayerAttributeParser: Updated character data for CharID: %d - Level: %d, HP: %d/%d, MP: %d/%d, XP: %d [%d-%d] Debt: %d"),
        CharacterData.characterId, CharacterData.characterLevel,
        CharacterData.characterCurrentHealth, StatsUpdate.healthMax,
        CharacterData.characterCurrentMana, StatsUpdate.manaMax,
        StatsUpdate.experienceCurrent, StatsUpdate.experienceLevelStart,
        StatsUpdate.experienceNextLevel, StatsUpdate.experienceDebt);
}