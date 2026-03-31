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
    
    UE_LOG(LogTemp, Log, TEXT("PlayerAttributeParser: Updated character data for CharID: %d - Level: %d, HP: %d/%d, MP: %d/%d"),
        CharacterData.characterId, CharacterData.characterLevel,
        CharacterData.characterCurrentHealth, StatsUpdate.healthMax,
        CharacterData.characterCurrentMana, StatsUpdate.manaMax);
}