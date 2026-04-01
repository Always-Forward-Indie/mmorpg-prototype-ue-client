#include "Utils/PlayerAttributeParser.h"
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
        UE_LOG(LogTemp, Error, TEXT("PlayerAttributeParser: Failed to parse JSON string"));
        return StatsUpdate;
    }
    
    // Get the body object
    TSharedPtr<FJsonObject> Body = Root->GetObjectField(TEXT("body"));
    if (!Body.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("PlayerAttributeParser: No valid body found in JSON"));
        return StatsUpdate;
    }
    
    return DeserializePlayerStatsUpdate(Body);
}

FPlayerStatsUpdateStruct PlayerAttributeParser::DeserializePlayerStatsUpdate(const TSharedPtr<FJsonObject>& Body)
{
    FPlayerStatsUpdateStruct StatsUpdate;
    
    if (!Body.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("PlayerAttributeParser: Invalid body object"));
        return StatsUpdate;
    }
    
    // Parse character ID
    if (Body->HasField(TEXT("characterId")))
    {
        StatsUpdate.characterId = Body->GetIntegerField(TEXT("characterId"));
    }
    
    // Parse level
    if (Body->HasField(TEXT("level")))
    {
        StatsUpdate.level = Body->GetIntegerField(TEXT("level"));
    }
    
    // Parse health data
    if (Body->HasField(TEXT("health")))
    {
        TSharedPtr<FJsonObject> HealthObj = Body->GetObjectField(TEXT("health"));
        ParseHealthData(HealthObj, StatsUpdate.healthCurrent, StatsUpdate.healthMax);
    }
    
    // Parse mana data
    if (Body->HasField(TEXT("mana")))
    {
        TSharedPtr<FJsonObject> ManaObj = Body->GetObjectField(TEXT("mana"));
        ParseManaData(ManaObj, StatsUpdate.manaCurrent, StatsUpdate.manaMax);
    }

    // Parse experience data from "experience" sub-object (stats_update format)
    if (Body->HasField(TEXT("experience")))
    {
        TSharedPtr<FJsonObject> ExpObj = Body->GetObjectField(TEXT("experience"));
        if (ExpObj.IsValid())
        {
            if (ExpObj->HasField(TEXT("current")))
                StatsUpdate.experienceCurrent = ExpObj->GetIntegerField(TEXT("current"));
            if (ExpObj->HasField(TEXT("levelStart")))
                StatsUpdate.experienceLevelStart = ExpObj->GetIntegerField(TEXT("levelStart"));
            if (ExpObj->HasField(TEXT("nextLevel")))
                StatsUpdate.experienceNextLevel = ExpObj->GetIntegerField(TEXT("nextLevel"));
            if (ExpObj->HasField(TEXT("debt")))
                StatsUpdate.experienceDebt = ExpObj->GetIntegerField(TEXT("debt"));
        }
    }

    // Parse attributes array
    if (Body->HasField(TEXT("attributes")))
    {
        const TArray<TSharedPtr<FJsonValue>>* AttrsArray = nullptr;
        if (Body->TryGetArrayField(TEXT("attributes"), AttrsArray))
        {
            for (const TSharedPtr<FJsonValue>& AttrVal : *AttrsArray)
            {
                TSharedPtr<FJsonObject> AttrObj = AttrVal->AsObject();
                if (!AttrObj.IsValid()) continue;

                FStatAttributeEntry Entry;
                if (AttrObj->HasField(TEXT("slug")))
                    Entry.slug = AttrObj->GetStringField(TEXT("slug"));
                if (AttrObj->HasField(TEXT("name")))
                    Entry.name = AttrObj->GetStringField(TEXT("name"));
                if (AttrObj->HasField(TEXT("base")))
                    Entry.base = AttrObj->GetNumberField(TEXT("base"));
                if (AttrObj->HasField(TEXT("effective")))
                    Entry.effective = AttrObj->GetNumberField(TEXT("effective"));
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
                TSharedPtr<FJsonObject> EffObj = EffectVal->AsObject();
                if (!EffObj.IsValid()) continue;

                FActiveEffectEntry Effect;
                if (EffObj->HasField(TEXT("slug")))
                    Effect.slug = EffObj->GetStringField(TEXT("slug"));
                if (EffObj->HasField(TEXT("effectTypeSlug")))
                    Effect.effectTypeSlug = EffObj->GetStringField(TEXT("effectTypeSlug"));
                if (EffObj->HasField(TEXT("attributeSlug")))
                    Effect.attributeSlug = EffObj->GetStringField(TEXT("attributeSlug"));
                if (EffObj->HasField(TEXT("value")))
                    Effect.value = static_cast<float>(EffObj->GetNumberField(TEXT("value")));
                if (EffObj->HasField(TEXT("expiresAt")))
                    Effect.expiresAt = static_cast<int64>(EffObj->GetNumberField(TEXT("expiresAt")));
                StatsUpdate.activeEffects.Add(Effect);
            }
        }
    }

    // Parse weight data
    if (Body->HasField(TEXT("weight")))
    {
        TSharedPtr<FJsonObject> WeightObj = Body->GetObjectField(TEXT("weight"));
        if (WeightObj.IsValid())
        {
            if (WeightObj->HasField(TEXT("current")))
                StatsUpdate.weightCurrent = static_cast<float>(WeightObj->GetNumberField(TEXT("current")));
            if (WeightObj->HasField(TEXT("max")))
                StatsUpdate.weightMax = static_cast<float>(WeightObj->GetNumberField(TEXT("max")));
        }
    }

    // Validate the parsed data
    if (!ValidateStatsData(StatsUpdate))
    {
        UE_LOG(LogTemp, Warning, TEXT("PlayerAttributeParser: Validation failed for stats data"));
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("PlayerAttributeParser: Successfully parsed stats - CharID: %d, Level: %d, HP: %d/%d, MP: %d/%d"),
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
        UE_LOG(LogTemp, Warning, TEXT("PlayerAttributeParser: Invalid health object"));
        return;
    }
    
    if (HealthObj->HasField(TEXT("current")))
    {
        OutCurrent = HealthObj->GetIntegerField(TEXT("current"));
    }
    
    if (HealthObj->HasField(TEXT("max")))
    {
        OutMax = HealthObj->GetIntegerField(TEXT("max"));
    }
    
    UE_LOG(LogTemp, VeryVerbose, TEXT("PlayerAttributeParser: Parsed health - Current: %d, Max: %d"), OutCurrent, OutMax);
}

void PlayerAttributeParser::ParseManaData(const TSharedPtr<FJsonObject>& ManaObj, int32& OutCurrent, int32& OutMax)
{
    if (!ManaObj.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("PlayerAttributeParser: Invalid mana object"));
        return;
    }
    
    if (ManaObj->HasField(TEXT("current")))
    {
        OutCurrent = ManaObj->GetIntegerField(TEXT("current"));
    }
    
    if (ManaObj->HasField(TEXT("max")))
    {
        OutMax = ManaObj->GetIntegerField(TEXT("max"));
    }
    
    UE_LOG(LogTemp, VeryVerbose, TEXT("PlayerAttributeParser: Parsed mana - Current: %d, Max: %d"), OutCurrent, OutMax);
}

bool PlayerAttributeParser::ValidateStatsData(const FPlayerStatsUpdateStruct& StatsData)
{
    // Check if character ID is valid
    if (StatsData.characterId <= 0)
    {
        UE_LOG(LogTemp, Error, TEXT("PlayerAttributeParser: Invalid character ID: %d"), StatsData.characterId);
        return false;
    }
    
    // Check if level is valid
    // Treat level == 0 as a warning but do not reject the packet: a death
    // stats_update is valid even if the server omits or zeroes the level field.
    if (StatsData.level <= 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("PlayerAttributeParser: Unexpected level value: %d (packet still processed)"), StatsData.level);
    }
    
    // Check if health values are valid
    if (StatsData.healthCurrent < 0 || StatsData.healthMax <= 0)
    {
        UE_LOG(LogTemp, Error, TEXT("PlayerAttributeParser: Invalid health values - Current: %d, Max: %d"), 
            StatsData.healthCurrent, StatsData.healthMax);
        return false;
    }
    
    if (StatsData.healthCurrent > StatsData.healthMax)
    {
        UE_LOG(LogTemp, Warning, TEXT("PlayerAttributeParser: Current health (%d) exceeds max health (%d)"), 
            StatsData.healthCurrent, StatsData.healthMax);
    }
    
    // Check if mana values are valid
    if (StatsData.manaCurrent < 0 || StatsData.manaMax < 0)
    {
        UE_LOG(LogTemp, Error, TEXT("PlayerAttributeParser: Invalid mana values - Current: %d, Max: %d"), 
            StatsData.manaCurrent, StatsData.manaMax);
        return false;
    }
    
    if (StatsData.manaCurrent > StatsData.manaMax)
    {
        UE_LOG(LogTemp, Warning, TEXT("PlayerAttributeParser: Current mana (%d) exceeds max mana (%d)"), 
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

    UE_LOG(LogTemp, Log, TEXT("PlayerAttributeParser: Updated character data for CharID: %d - Level: %d, HP: %d/%d, MP: %d/%d, XP: %d [%d-%d] Debt: %d"),
        CharacterData.characterId, CharacterData.characterLevel,
        CharacterData.characterCurrentHealth, StatsUpdate.healthMax,
        CharacterData.characterCurrentMana, StatsUpdate.manaMax,
        StatsUpdate.experienceCurrent, StatsUpdate.experienceLevelStart,
        StatsUpdate.experienceNextLevel, StatsUpdate.experienceDebt);
}