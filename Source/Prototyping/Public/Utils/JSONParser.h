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
    static FCharacterDataStruct DeserializeCharacterData(const FString& JsonString);
    static FClientDataStruct DeserializeClientData(const FString& JsonString);
    static FMessageDataStruct DeserializeMessageData(const FString& JsonString);
    static FString DeserializeEventTypeData(const FString& JsonString);
    static FPositionDataStruct DeserializePositionData(const FString& JsonString);
    static TArray<FClientDataStruct>DeserializeCharactersList(const FString& JsonString);
    static TArray<FCharacterDataStruct>DeserializeLoginCharactersList(const FString& JsonString);
};