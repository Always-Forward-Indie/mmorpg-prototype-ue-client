#pragma once

#include "CoreMinimal.h"
#include "Data/DataStructs.h"

/**
 * JSON Parser specifically for player attributes and stats-related data
 * This class handles parsing of all player attribute updates from server
 */
class PROTOTYPING_API PlayerAttributeParser
{
public:
    // Parse player stats update from JSON string
    static FPlayerStatsUpdateStruct DeserializePlayerStatsUpdate(const FString& JsonString);
    
    // Parse player stats update from JSON object
    static FPlayerStatsUpdateStruct DeserializePlayerStatsUpdate(const TSharedPtr<FJsonObject>& Body);
    
    // Parse individual health data from JSON object
    static void ParseHealthData(const TSharedPtr<FJsonObject>& HealthObj, int32& OutCurrent, int32& OutMax);
    
    // Parse individual mana data from JSON object
    static void ParseManaData(const TSharedPtr<FJsonObject>& ManaObj, int32& OutCurrent, int32& OutMax);
    
    // Helper function to validate stats data
    static bool ValidateStatsData(const FPlayerStatsUpdateStruct& StatsData);
    
    // Parse player attributes for character data struct
    static void UpdateCharacterDataFromStatsUpdate(FCharacterDataStruct& CharacterData, const FPlayerStatsUpdateStruct& StatsUpdate);
};