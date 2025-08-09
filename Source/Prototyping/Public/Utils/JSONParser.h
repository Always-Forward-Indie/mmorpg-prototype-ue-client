// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Data/DataStructs.h"

/**
 * 
 */
class PROTOTYPING_API JSONParser
{
public:
    static FString SerializeJson(const FString& EventType, const TMap<FString, TSharedPtr<FJsonValue>>& HeaderData, const TMap<FString, TSharedPtr<FJsonValue>>& BodyData);

    static FClientDataStruct DeserializeClientData(const FString& JsonString);
    static FMessageDataStruct DeserializeMessageData(const FString& JsonString);
    static FString DeserializeEventTypeData(const FString& JsonString);

    static FPositionDataStruct DeserializePositionData(const TSharedPtr<FJsonObject>& PosObj);
    static TMap<FString, FAttributeDataStruct> DeserializeAttributesArray(const TArray<TSharedPtr<FJsonValue>>& JsonArray);

    static FCharacterDataStruct DeserializeCharacterData(const TSharedPtr<FJsonObject>& CD);
	static FCharacterDataStruct DeserializeCharacterData(const FString& JsonString);
	static TArray<FClientDataStruct> DeserializeCharactersList(const TSharedPtr<FJsonObject>& Body);

    static TArray<FCharacterDataStruct>DeserializeLoginCharactersList(const FString& JsonString);

    static TArray<FSpawnZoneStruct> DeserializeSpawnZonesList(const TSharedPtr<FJsonObject>& Body);
    static FSpawnZoneStruct DeserializeSpawnZoneData(const TSharedPtr<FJsonObject>& Body);

    static FMOBStruct DeserializeMobData(const TSharedPtr<FJsonObject>& MobObject);
    // Combat data parsers
    static FCombatAnimationData DeserializeCombatAnimation(const TSharedPtr<FJsonObject>& AnimationObj);
    static FCombatActionData DeserializeCombatAction(const TSharedPtr<FJsonObject>& ActionObj);
    static FCombatResultData DeserializeCombatResult(const TSharedPtr<FJsonObject>& ResultObj);

    static TArray<FMOBStruct> DeserializeMobsList(const TSharedPtr<FJsonObject>& Body);
};
