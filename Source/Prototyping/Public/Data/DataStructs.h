#pragma once
#include "CoreMinimal.h"
#include "DataStructs.generated.h"

USTRUCT(BlueprintType)
struct FPositionDataStruct
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Position Struct")
    double positionX = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Position Struct")
    double positionY = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Position Struct")
    double positionZ = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Position Struct")
    double rotationZ = 0;
};

USTRUCT(BlueprintType)
struct FCharacterDataStruct
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Data Struct")
    int characterId = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Data Struct")
    int characterLevel = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Data Struct")
    int characterExperiencePoints = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Data Struct")
    int characterCurrentHealth = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Data Struct")
    int characterCurrentMana = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Data Struct")
    FString characterName = "";
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Data Struct")
    FString characterClass = "";
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Data Struct")
    FString characterRace = "";
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Data Struct")
    FPositionDataStruct characterPosition;
};

USTRUCT(BlueprintType)
struct FClientDataStruct
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Client Data Struct")
    int clientId = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Client Data Struct")
    bool isOtherClient = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Client Data Struct")
    FString clientLogin = "";
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Client Data Struct")
    FString hash = "";
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Client Data Struct")
    FCharacterDataStruct characterData;
};

USTRUCT(BlueprintType)
struct FMessageDataStruct
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Packet Service Data Struct")
    FString eventType;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Packet Service Data Struct")
    FString status;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Packet Service Data Struct")
    FString message;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Packet Service Data Struct")
	FString timestamp = "";
};