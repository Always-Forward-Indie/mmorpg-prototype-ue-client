// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Data/DataStructs.h"
#include "Data/ItemStruct.h"
#include "Data/WIODataStructs.h"
#include "Authentication/LoginFlowTypes.h"
#include "Networking/NetworkDispatcher.h"

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

    // Root-based twin (S05): same field reads on an already-parsed object, no re-parse.
    static FClientDataStruct DeserializeClientData(const TSharedPtr<FJsonObject>& Root);
    static FMessageDataStruct DeserializeMessageData(const FString& JsonString);

    // Root-based twin (S05): same field reads on an already-parsed packet, no re-parse.
    static FMessageDataStruct DeserializeMessageData(const FParsedPacket& Packet);

    // Single-parse entry point (S05): parses once, shares Root/Body with the
    // dispatcher and the time-sync path. Quiet defaults on garbage (same
    // contract as DeserializeMessageData — truncated packets yield invalid).
    static FParsedPacket ParsePacket(const FString& JsonString);
    static FString DeserializeEventTypeData(const FString& JsonString);

    // Central error-code resolution: prefers body.errorCode, falls back to header.message.
    // Returns "" on success packets. Never match logic on human-readable message text.
    static FString ResolveErrorCode(const FString& JsonString);
    static FString ResolveErrorCode(const TSharedPtr<FJsonObject>& Root);

    // Last-resort display text for a slug when the server omits the name and the
    // locale table has no row ("forest_wolf" -> "Forest wolf"). Never used for logic.
    static FString HumanizeSlugForDisplay(const FString& Slug);

    // New method to deserialize network header with time sync data
    static FNetworkHeaderStruct DeserializeNetworkHeader(const FString& JsonString);

    // Root-based twin (S05): same field reads on an already-parsed object, no re-parse.
    static FNetworkHeaderStruct DeserializeNetworkHeader(const TSharedPtr<FJsonObject>& Root);

    // Helper method to process time sync from network headers
    static void ProcessTimeSyncFromHeader(const FString& JsonString, UTimeSyncService* TimeSyncService);

    // Parsed-packet twin (S05): same logic on an already-parsed packet, no re-parse.
    static void ProcessTimeSyncFromPacket(const FParsedPacket& Packet, UTimeSyncService* TimeSyncService);

    static FPositionDataStruct DeserializePositionData(const TSharedPtr<FJsonObject>& PosObj);
    static TMap<FString, FAttributeDataStruct> DeserializeAttributesArray(const TArray<TSharedPtr<FJsonValue>>& JsonArray);

    static FCharacterDataStruct DeserializeCharacterData(const TSharedPtr<FJsonObject>& CD);
	static FCharacterDataStruct DeserializeCharacterData(const FString& JsonString);
	static TArray<FClientDataStruct> DeserializeCharactersList(const TSharedPtr<FJsonObject>& Body);

    static TArray<FLoginCharacterEntry> DeserializeLoginCharactersList(const FString& JsonString);

    static TArray<FSpawnZoneStruct> DeserializeSpawnZonesList(const TSharedPtr<FJsonObject>& Body);
    static FSpawnZoneStruct DeserializeSpawnZoneData(const TSharedPtr<FJsonObject>& Body);

    static FMOBStruct DeserializeMobData(const TSharedPtr<FJsonObject>& MobObject);

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

    // Parse a mobMoveUpdate packet � returns an array of lightweight move entries
    // paired with their mob UID. Body is the "body" JSON object.
    struct FMobMovePacketEntry
    {
        int32               uid          = 0;
        FMobMoveEntryStruct moveEntry;
    };
    static TArray<FMobMovePacketEntry> DeserializeMobMoveUpdate(const TSharedPtr<FJsonObject>& Body, int64 ServerSendMs);

    // Item data parsers
    static FItemAttributeStruct DeserializeItemAttribute(const TSharedPtr<FJsonObject>& AttributeObj);
    static TArray<FItemAttributeStruct> DeserializeItemAttributes(const TArray<TSharedPtr<FJsonValue>>& JsonArray);
    static FItemBaseStruct DeserializeItemData(const TSharedPtr<FJsonObject>& ItemObj);
    static FDroppedItemStruct DeserializeDroppedItem(const TSharedPtr<FJsonObject>& DroppedItemObj);
    static FItemDropResponseStruct DeserializeItemDropResponse(const TSharedPtr<FJsonObject>& Body);

    // Inventory data parsers
    static FInventoryItemStruct DeserializeInventoryItem(const TSharedPtr<FJsonObject>& ItemObj);
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

    // Player skills system parsers
    static FPlayerSkillNetworkData DeserializePlayerSkillNetworkData(const TSharedPtr<FJsonObject>& SkillObj);
    static TArray<FPlayerSkillNetworkData> DeserializePlayerSkillsArray(const TArray<TSharedPtr<FJsonValue>>& JsonArray);
    static FPlayerSkillsInitializationData DeserializePlayerSkillsInitialization(const TSharedPtr<FJsonObject>& Body);
    static FPlayerSkillsInitializationData DeserializePlayerSkillsInitialization(const FString& JsonString);

    // NPC system parsers
    static FNPCStatsStruct DeserializeNPCStats(const TSharedPtr<FJsonObject>& StatsObj);
    static FNPCHealthManaStruct DeserializeNPCHealthMana(const TSharedPtr<FJsonObject>& HealthManaObj);
    static FNPCStruct DeserializeNPCData(const TSharedPtr<FJsonObject>& NPCObj);
    static TArray<FNPCStruct> DeserializeNPCsList(const TArray<TSharedPtr<FJsonValue>>& JsonArray);
    static FNPCSpawnDataStruct DeserializeNPCSpawnData(const TSharedPtr<FJsonObject>& Body);
    static FNPCSpawnDataStruct DeserializeNPCSpawnData(const FString& JsonString);

    // Effect tick parser (DoT/HoT tick data)
    static FEffectTickData DeserializeEffectTick(const FString& JsonString);

    // Active effects packet parser
    static TArray<FActiveEffectEntry> DeserializeActiveEffectsPacket(const FString& JsonString);

    // World Interactive Objects parsers
    static FWorldObjectData DeserializeWorldObject(const TSharedPtr<FJsonObject>& ObjectObj);
    static TArray<FWorldObjectData> DeserializeWorldObjectsList(const TSharedPtr<FJsonObject>& Body);
    static TArray<FWorldObjectData> DeserializeWorldObjectsList(const FString& JsonString);
    static FWIOInteractResult DeserializeWIOInteractResult(const TSharedPtr<FJsonObject>& Body);
    static FWIOInteractResult DeserializeWIOInteractResult(const FString& JsonString);
    static FWIOStateUpdate DeserializeWIOStateUpdate(const TSharedPtr<FJsonObject>& Body);
    static FWIOStateUpdate DeserializeWIOStateUpdate(const FString& JsonString);
    static FWIOChannelCancelled DeserializeWIOChannelCancelled(const TSharedPtr<FJsonObject>& Body);
    static FWIOChannelCancelled DeserializeWIOChannelCancelled(const FString& JsonString);

    // ── Login Flow ───────────────────────────────────────────────────────────
    static FCharacterCreationOptions DeserializeCharacterCreationOptions(const FString& JsonString);
};
