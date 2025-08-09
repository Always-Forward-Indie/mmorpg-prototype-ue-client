#pragma once
#include "CoreMinimal.h"
#include "Engine/DataTable.h"
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
	int characterExpForNextLevel = 0;
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
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Data Struct")
	bool bIsDead = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Data Struct")
	bool bIsMoving = false;

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
    FString mobSlug = "";
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
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mob Data Struct")
	bool bIsMoving = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mob Data Struct")
    bool bIsGotDamage = false;
};

USTRUCT(BlueprintType)
struct FCombatAnimationData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Animation Data Struct")
    FString AnimationName;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Animation Data Struct")
    int32 CharacterId = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Animation Data Struct")
    float Duration = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Animation Data Struct")
    bool bIsLooping = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Animation Data Struct")
    FPositionDataStruct Position;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Animation Data Struct")
    FPositionDataStruct TargetPosition;
};

USTRUCT(BlueprintType)
struct FCombatActionData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Action Data Struct")
    int32 ActionId = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Action Data Struct")
    FString ActionName;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Action Data Struct")
    int32 ActionType = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Action Data Struct")
    FString AnimationName;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Action Data Struct")
    float CastTime = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Action Data Struct")
    int32 CasterId = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Action Data Struct")
    int32 Damage = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Action Data Struct")
    float Range = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Action Data Struct")
    int32 State = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Action Data Struct")
    int32 TargetId = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Action Data Struct")
    FPositionDataStruct TargetPosition;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Action Data Struct")
    int32 TargetType = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Action Data Struct")
    FString TargetTypeString = "";
};

USTRUCT(BlueprintType)
struct FCombatResultData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Result Data Struct")
    int32 ActionId = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Result Data Struct")
    int32 CasterId = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Result Data Struct")
    int32 DamageDealt = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Result Data Struct")
    int32 HealingDone = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Result Data Struct")
    bool bIsBlocked = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Result Data Struct")
    bool bIsCritical = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Result Data Struct")
    bool bIsDodged = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Result Data Struct")
	bool bIsResisted = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Result Data Struct")
    int32 RemainingHealth = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Result Data Struct")
    int32 RemainingMana = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Result Data Struct")
    bool bTargetDied = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Result Data Struct")
	bool bIsDamaged = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Result Data Struct")
    int32 TargetId = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Action Data Struct")
    int32 TargetType = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Action Data Struct")
    FString TargetTypeString = "";
};


USTRUCT(BlueprintType)
struct FMobVisualData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TSoftObjectPtr<USkeletalMesh> SkeletalMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TSoftClassPtr<UAnimInstance> AnimBPClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector ActorScale = FVector(1.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName MobName;
};

USTRUCT(BlueprintType)
struct FMobAudioData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TSoftObjectPtr<USoundBase> AggroSound;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TSoftObjectPtr<USoundBase> AttackSound;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TSoftObjectPtr<USoundBase> HitSound;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TSoftObjectPtr<USoundBase> DeathSound;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<TSoftObjectPtr<USoundBase>> IdleSounds;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<TSoftObjectPtr<USoundBase>> WalkSounds;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<TSoftObjectPtr<USoundBase>> RunSounds;
};


USTRUCT(BlueprintType)
struct FMobDefinition : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName MobType;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FMobVisualData Visual;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FMobAudioData Audio;

    // Will be other data
};


UENUM(BlueprintType)
enum class EDamageType : uint8
{
    Physical,
    Fire,
    Ice,
    Poison
};