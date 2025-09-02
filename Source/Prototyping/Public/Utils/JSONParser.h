// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Data/DataStructs.h"
#include "Data/ItemStruct.h"

// Forward declaration
class UTimeSyncService;
enum class EServerType : uint8;

/**
 * 
 */
class PROTOTYPING_API JSONParser
{
public:
    static FString SerializeJson(const FString& EventType, const TMap<FString, TSharedPtr<FJsonValue>>& HeaderData, const TMap<FString, TSharedPtr<FJsonValue>>& BodyData);
    
    // New method with explicit TimeSyncService parameter
    static FString SerializeJsonWithTimeSync(const FString& EventType, const TMap<FString, TSharedPtr<FJsonValue>>& HeaderData, const TMap<FString, TSharedPtr<FJsonValue>>& BodyData, UTimeSyncService* TimeSyncService);

    // New method with explicit TimeSyncService parameter and server type
    static FString SerializeJsonWithTimeSync(const FString& EventType, const TMap<FString, TSharedPtr<FJsonValue>>& HeaderData, const TMap<FString, TSharedPtr<FJsonValue>>& BodyData, UTimeSyncService* TimeSyncService, EServerType ServerType);

    // Helper method to get TimeSyncService instance
    static UTimeSyncService* GetTimeSyncService();

    static FClientDataStruct DeserializeClientData(const FString& JsonString);
    static FMessageDataStruct DeserializeMessageData(const FString& JsonString);
    static FString DeserializeEventTypeData(const FString& JsonString);

    // New method to deserialize network header with time sync data
    static FNetworkHeaderStruct DeserializeNetworkHeader(const FString& JsonString);

    // Helper method to process time sync from network headers
    static void ProcessTimeSyncFromHeader(const FString& JsonString, UTimeSyncService* TimeSyncService);

    static FPositionDataStruct DeserializePositionData(const TSharedPtr<FJsonObject>& PosObj);
    static TMap<FString, FAttributeDataStruct> DeserializeAttributesArray(const TArray<TSharedPtr<FJsonValue>>& JsonArray);

    static FCharacterDataStruct DeserializeCharacterData(const TSharedPtr<FJsonObject>& CD);
	static FCharacterDataStruct DeserializeCharacterData(const FString& JsonString);
	static TArray<FClientDataStruct> DeserializeCharactersList(const TSharedPtr<FJsonObject>& Body);

    static TArray<FCharacterDataStruct>DeserializeLoginCharactersList(const FString& JsonString);

    static TArray<FSpawnZoneStruct> DeserializeSpawnZonesList(const TSharedPtr<FJsonObject>& Body);
    static FSpawnZoneStruct DeserializeSpawnZoneData(const TSharedPtr<FJsonObject>& Body);

    static FMOBStruct DeserializeMobData(const TSharedPtr<FJsonObject>& MobObject);
    
    // Legacy combat data parsers (for backward compatibility)
    static FCombatAnimationData DeserializeCombatAnimation(const TSharedPtr<FJsonObject>& AnimationObj);
    static FCombatActionData DeserializeCombatAction(const TSharedPtr<FJsonObject>& ActionObj);
    static FCombatResultData DeserializeCombatResult(const TSharedPtr<FJsonObject>& ResultObj);

    // New combat system parsers
    static ESkillEffectType ParseSkillEffectType(const FString& EffectTypeString);
    static ESkillSchool ParseSkillSchool(const FString& SchoolString);
    static ECasterType ParseCasterType(const FString& CasterTypeString);
    static FAppliedEffectData DeserializeAppliedEffect(const TSharedPtr<FJsonObject>& EffectObj);
    static TArray<FAppliedEffectData> DeserializeAppliedEffects(const TArray<TSharedPtr<FJsonValue>>& JsonArray);
    static FSkillInitiationData DeserializeSkillInitiation(const TSharedPtr<FJsonObject>& InitiationObj);
    static FSkillInitiationData DeserializeSkillInitiation(const FString& JsonString);
    static FSkillResultData DeserializeSkillResult(const TSharedPtr<FJsonObject>& ResultObj);
    static FSkillResultData DeserializeSkillResult(const FString& JsonString);

    static TArray<FMOBStruct> DeserializeMobsList(const TSharedPtr<FJsonObject>& Body);

    // Item data parsers
    static FItemAttributeStruct DeserializeItemAttribute(const TSharedPtr<FJsonObject>& AttributeObj);
    static TArray<FItemAttributeStruct> DeserializeItemAttributes(const TArray<TSharedPtr<FJsonValue>>& JsonArray);
    static FItemBaseStruct DeserializeItemData(const TSharedPtr<FJsonObject>& ItemObj);
    static FDroppedItemStruct DeserializeDroppedItem(const TSharedPtr<FJsonObject>& DroppedItemObj);
    static FItemDropResponseStruct DeserializeItemDropResponse(const TSharedPtr<FJsonObject>& Body);
    static FItemDropResponseStruct DeserializeItemDropResponse(const FString& JsonString);

    // Inventory data parsers
    static FInventoryItemStruct DeserializeInventoryItem(const TSharedPtr<FJsonObject>& ItemObj);
    static TArray<FInventoryItemStruct> DeserializeInventoryItems(const TArray<TSharedPtr<FJsonValue>>& JsonArray);
    static FCharacterInventoryStruct DeserializeCharacterInventory(const TSharedPtr<FJsonObject>& InventoryObj);
    static FCharacterInventoryStruct DeserializeCharacterInventory(const FString& JsonString);
    static FInventoryUpdateStruct DeserializeInventoryUpdate(const TSharedPtr<FJsonObject>& UpdateObj);
    static FInventoryUpdateStruct DeserializeInventoryUpdate(const FString& JsonString);

    // Parse mob target lost data from JSON object
    static FMobTargetLostStruct DeserializeMobTargetLost(const TSharedPtr<FJsonObject>& Body);
    // Parse mob target lost data from JSON string
    static FMobTargetLostStruct DeserializeMobTargetLost(const FString& JsonString);

    // Harvest system parsers
    static FHarvestItemStruct DeserializeHarvestItem(const TSharedPtr<FJsonObject>& ItemObj);
    static FHarvestStartedStruct DeserializeHarvestStarted(const TSharedPtr<FJsonObject>& Body);
    static FHarvestStartedStruct DeserializeHarvestStarted(const FString& JsonString);
    static FHarvestCompleteStruct DeserializeHarvestComplete(const TSharedPtr<FJsonObject>& Body);
    static FHarvestCompleteStruct DeserializeHarvestComplete(const FString& JsonString);
    static FHarvestErrorStruct DeserializeHarvestError(const TSharedPtr<FJsonObject>& Body);
    static FHarvestErrorStruct DeserializeHarvestError(const FString& JsonString);
    static FCorpseLootPickupResponseStruct DeserializeCorpseLootPickupResponse(const TSharedPtr<FJsonObject>& Body);
    static FCorpseLootPickupResponseStruct DeserializeCorpseLootPickupResponse(const FString& JsonString);
    static FCorpseLootPickupErrorStruct DeserializeCorpseLootPickupError(const TSharedPtr<FJsonObject>& Body);
    static FCorpseLootPickupErrorStruct DeserializeCorpseLootPickupError(const FString& JsonString);
    static FCorpseLootInspectResponseStruct DeserializeCorpseLootInspectResponse(const TSharedPtr<FJsonObject>& Body);
    static FCorpseLootInspectResponseStruct DeserializeCorpseLootInspectResponse(const FString& JsonString);
    static FCorpseLootInspectErrorStruct DeserializeCorpseLootInspectError(const TSharedPtr<FJsonObject>& Body);
    static FCorpseLootInspectErrorStruct DeserializeCorpseLootInspectError(const FString& JsonString);

    // Experience system parsers
    static FExperienceUpdateStruct DeserializeExperienceUpdate(const TSharedPtr<FJsonObject>& Body);
    static FExperienceUpdateStruct DeserializeExperienceUpdate(const FString& JsonString);
    static FPlayerProgressionStruct DeserializePlayerProgression(const TSharedPtr<FJsonObject>& ProgressionObj);
    static FPlayerProgressionStruct DeserializePlayerProgression(const FString& JsonString);
    static FExperienceGainEventStruct DeserializeExperienceGainEvent(const TSharedPtr<FJsonObject>& EventObj);
    static FExperienceGainEventStruct DeserializeExperienceGainEvent(const FString& JsonString);

    // Player stats system parsers
    static FPlayerStatsUpdateStruct DeserializePlayerStatsUpdate(const TSharedPtr<FJsonObject>& Body);
    static FPlayerStatsUpdateStruct DeserializePlayerStatsUpdate(const FString& JsonString);
};
