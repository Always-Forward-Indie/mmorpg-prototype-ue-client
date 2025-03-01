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
struct FAttributeDataStruct {
	GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Attributes Struct")
    int attributeId = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Attributes Struct")
	FString attributeSlug = "";
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Attributes Struct")
    FString attributeName = "";
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Attributes Struct")
	int attributeValue = 0;
};


USTRUCT(BlueprintType)
struct FAttributesDataStruct {
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Attributes Struct")
    TMap<FString, FAttributeDataStruct> attributesData;
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
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Data Struct")
    FAttributesDataStruct characterAttributes;
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

USTRUCT(BlueprintType)
struct FSpawnZoneStruct
{
    GENERATED_BODY();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn Zone Struct")
    int zoneID = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn Zone Struct")
    FString zoneName = "";
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn Zone Struct")
    int MobIDToSpawn = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn Zone Struct")
    int currentMobsCount = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn Zone Struct")
    int MaxMobs = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn Zone Struct")
    int respawnTime = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn Zone Struct")
    bool bSpawningEnabled = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn Zone Struct")
    FVector spawnStartPos;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn Zone Struct")
    FVector spawnSize;
};

USTRUCT(BlueprintType)
struct FMOBStruct {
    GENERATED_BODY();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mob Data Struct")
    int mobID = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mob Data Struct")
    FString mobUniqueID = "";
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mob Data Struct")
    int mobZoneID = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mob Data Struct")
    FString mobName = "";
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mob Data Struct")
    FString mobRace = "";
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mob Data Struct")
    int mobLevel = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mob Data Struct")
    int mobCurrentHealth = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mob Data Struct")
    int mobCurrentMana = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mob Data Struct")
    FPositionDataStruct mobPosition;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mob Data Struct")
    bool bIsDead = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mob Data Struct")
    bool bIsAggressive = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mob Data Struct")
    FAttributesDataStruct mobAttributes;
};