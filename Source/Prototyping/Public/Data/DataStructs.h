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
    
    // Time sync fields for lag compensation
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Packet Service Data Struct")
    int64 clientSendMs = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Packet Service Data Struct")
    int64 serverRecvMs = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Packet Service Data Struct")
    int64 serverSendMs = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Packet Service Data Struct")
    int64 clientSendMsEcho = 0;
};

// New unified network header structure for all client-server communication
USTRUCT(BlueprintType)
struct FNetworkHeaderStruct
{
    GENERATED_BODY()

    // Event type identifier
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Network Header")
    FString eventType = "";

    // Response status (for server responses)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Network Header")
    FString status = "";

    // Client authentication
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Network Header")
    int32 clientId = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Network Header")
    FString hash = "";

    // Request ID for matching request-response pairs
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Network Header")
    FString requestId = "";

    // Time sync for lag compensation
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Network Header")
    int64 clientSendMs = 0;

    // Server response time fields (only in server responses)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Network Header")
    int64 serverRecvMs = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Network Header")
    int64 serverSendMs = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Network Header")
    int64 clientSendMsEcho = 0;

    // Optional message for errors or additional info
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Network Header")
    FString message = "";

    FNetworkHeaderStruct()
    {
        eventType = "";
        status = "";
        clientId = 0;
        hash = "";
        requestId = "";
        clientSendMs = 0;
        serverRecvMs = 0;
        serverSendMs = 0;
        clientSendMsEcho = 0;
        message = "";
    }
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mob Data Struct")
	int32 mobTargetId = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mob Data Struct")
	FString mobTargetType = "";

	// Harvest state
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mob Data Struct")
	bool bHasBeenHarvested = false;
};

// New Combat System Data Structures for the new server format

UENUM(BlueprintType)
enum class ESkillEffectType : uint8
{
    None        UMETA(DisplayName = "None"),
    Damage      UMETA(DisplayName = "Damage"),
    Healing     UMETA(DisplayName = "Healing"), 
    Buff        UMETA(DisplayName = "Buff"),
    Debuff      UMETA(DisplayName = "Debuff"),
    Resource    UMETA(DisplayName = "Resource") // for mana/energy effects
};

UENUM(BlueprintType)
enum class ESkillSchool : uint8
{
    None        UMETA(DisplayName = "None"),
    Physical    UMETA(DisplayName = "Physical"),
    Fire        UMETA(DisplayName = "Fire"),
    Ice         UMETA(DisplayName = "Ice"),
    Nature      UMETA(DisplayName = "Nature"),
    Arcane      UMETA(DisplayName = "Arcane"),
    Shadow      UMETA(DisplayName = "Shadow"),
    Holy        UMETA(DisplayName = "Holy")
};

UENUM(BlueprintType)
enum class ECasterType : uint8
{
    None = 0 UMETA(DisplayName = "None"),
    Self = 1 UMETA(DisplayName = "Self"),
    Player = 2 UMETA(DisplayName = "Player"),
    Mob = 3 UMETA(DisplayName = "Mob"),
    NPC = 4 UMETA(DisplayName = "NPC")
};

// Applied Effect Data - for buffs/debuffs
USTRUCT(BlueprintType)
struct FAppliedEffectData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Applied Effect")
    FString effectName = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Applied Effect")
    float duration = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Applied Effect")
    int32 value = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Applied Effect")
    FString effectType = ""; // "buff" or "debuff"

    FAppliedEffectData()
    {
        effectName = "";
        duration = 0.0f;
        value = 0;
        effectType = "";
    }
};

// Skill Initiation Data - corresponds to "combatInitiation" event
USTRUCT(BlueprintType)
struct FSkillInitiationData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Initiation")
    FString skillName = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Initiation")
    FString animationName = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Initiation")
    float animationDuration = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Initiation")
    float castTime = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Initiation")
    int32 casterId = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Initiation")
    int32 casterType = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Initiation")
    FString casterTypeString = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Initiation")
    int32 targetId = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Initiation")
    int32 targetType = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Initiation")
    FString targetTypeString = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Initiation")
    ESkillEffectType skillEffectType = ESkillEffectType::None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Initiation")
    ESkillSchool skillSchool = ESkillSchool::None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Initiation")
    bool success = false;

    FSkillInitiationData()
    {
        skillName = "";
        animationName = "";
        animationDuration = 0.0f;
        castTime = 0.0f;
        casterId = 0;
        casterType = 0;
        casterTypeString = "";
        targetId = 0;
        targetType = 0;
        targetTypeString = "";
        skillEffectType = ESkillEffectType::None;
        skillSchool = ESkillSchool::None;
        success = false;
    }
};

// Skill Result Data - corresponds to "combatResult" event  
USTRUCT(BlueprintType)
struct FSkillResultData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Result")
    FString skillName = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Result")
    int32 casterId = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Result")
    int32 casterType = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Result")
    FString casterTypeString = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Result")
    int32 targetId = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Result")
    int32 targetType = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Result")
    FString targetTypeString = "";

    // Damage/Healing values
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Result")
    int32 damage = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Result")
    int32 healing = 0;

    // Final target stats
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Result")
    int32 finalTargetHealth = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Result")
    int32 finalTargetMana = 0;

    // Combat result flags
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Result")
    bool isCritical = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Result")
    bool isBlocked = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Result")
    bool isMissed = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Result")
    bool targetDied = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Result")
    bool success = false;

    // Skill type and school
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Result")
    ESkillEffectType skillEffectType = ESkillEffectType::None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Result")
    ESkillSchool skillSchool = ESkillSchool::None;

    // Applied effects (buffs/debuffs)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Result")
    TArray<FAppliedEffectData> appliedEffects;

    FSkillResultData()
    {
        skillName = "";
        casterId = 0;
        casterType = 0;
        casterTypeString = "";
        targetId = 0;
        targetType = 0;
        targetTypeString = "";
        damage = 0;
        healing = 0;
        finalTargetHealth = 0;
        finalTargetMana = 0;
        isCritical = false;
        isBlocked = false;
        isMissed = false;
        targetDied = false;
        success = false;
        skillEffectType = ESkillEffectType::None;
        skillSchool = ESkillSchool::None;
    }
};

// Legacy structs (keeping for backward compatibility)
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
    int32 CasterId = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Action Data Struct")
    int32 TargetId = 0;
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

// Inventory item structure that matches server format
USTRUCT(BlueprintType)
struct FInventoryItemStruct
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Item")
	int32 itemId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Item")
	int32 quantity = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Item")
	float weight = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Item")
	int32 stackSize = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Item")
	int32 durability_max = 100;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Item")
	int32 durability_current = 100;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Item")
	bool is_durable = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Item")
	bool is_tradable = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Item")
	bool is_equippable = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Item")
	int32 vendor_price_buy = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Item")
    int32 vendor_price_sell = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Item")
    bool is_container = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Item")
    bool is_quest_item = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Item")
    FString slug = "";

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Item")
	FString name = "";

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Item")
	FString description = "";

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Item")
	FString type = "";

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Item")
	FString rarity = "";

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Item")
	int32 level_requirement = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Item")
	TMap<FString, FString> attributes;

	FInventoryItemStruct()
	{
		itemId = 0;
		quantity = 0;
		name = "";
        slug = "";
		description = "";
		type = "";
		rarity = "";
        level_requirement = 0;
		weight = 0.0f;
		stackSize = 0;
		durability_max = 100;
		durability_current = 100;
		vendor_price_buy = 0;
		vendor_price_sell = 0;

        is_durable = false;
        is_tradable = true;
        is_equippable = false;
		is_container = false;
		is_quest_item = false;

		attributes.Empty();
	}
};

// Character inventory structure
USTRUCT(BlueprintType)
struct FCharacterInventoryStruct
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Inventory")
	int32 characterId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Inventory")
	TArray<FInventoryItemStruct> items;

	FCharacterInventoryStruct()
	{
		characterId = 0;
		items.Empty();
	}
};

// Inventory update structure for when items are added/removed
USTRUCT(BlueprintType)
struct FInventoryUpdateStruct
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Update")
	FString eventType = "";

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Update")
	int32 characterId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Update")
	FCharacterInventoryStruct data;

	FInventoryUpdateStruct()
	{
		eventType = "";
		characterId = 0;
		data = FCharacterInventoryStruct();
	}
};

UENUM(BlueprintType)
enum class EDamageType : uint8
{
    Physical,
    Fire,
    Ice,
    Poison
};

USTRUCT(BlueprintType)
struct FMobTargetLostStruct
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mob Target Lost")
    int32 lostTargetPlayerId = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mob Target Lost")
    int32 mobId = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mob Target Lost")
    int32 mobUID = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mob Target Lost")
    FPositionDataStruct position;

    FMobTargetLostStruct()
    {
        lostTargetPlayerId = 0;
        mobId = 0;
        mobUID = 0;
        position = FPositionDataStruct();
    }
};

// Harvest system structures
USTRUCT(BlueprintType)
struct FHarvestItemStruct
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harvest Item")
    int32 itemId = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harvest Item")
    FString itemSlug = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harvest Item")
    int32 quantity = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harvest Item")
    FString name = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harvest Item")
    FString description = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harvest Item")
    int32 rarityId = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harvest Item")
    FString rarityName = "Common";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harvest Item")
    FString itemType = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harvest Item")
    float weight = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harvest Item")
    bool addedToInventory = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harvest Item")
    bool isHarvestItem = true;

    FHarvestItemStruct()
    {
        itemId = 0;
        itemSlug = "";
        quantity = 0;
        name = "";
        description = "";
        rarityId = 1;
        rarityName = "Common";
        itemType = "";
        weight = 0.0f;
        addedToInventory = false;
        isHarvestItem = true;
    }
};

USTRUCT(BlueprintType)
struct FHarvestStartedStruct
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harvest Started")
    FString type = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harvest Started")
    int32 clientId = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harvest Started")
    int32 playerId = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harvest Started")
    int32 corpseId = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harvest Started")
    int32 duration = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harvest Started")
    int64 startTime = 0;

    FHarvestStartedStruct()
    {
        type = "";
        clientId = 0;
        playerId = 0;
        corpseId = 0;
        duration = 0;
        startTime = 0;
    }
};

USTRUCT(BlueprintType)
struct FHarvestCompleteStruct
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harvest Complete")
    FString type = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harvest Complete")
    int32 clientId = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harvest Complete")
    int32 playerId = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harvest Complete")
    int32 corpseId = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harvest Complete")
    bool success = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harvest Complete")
    int32 totalItems = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harvest Complete")
    TArray<FHarvestItemStruct> availableLoot;

    FHarvestCompleteStruct()
    {
        type = "";
        clientId = 0;
        playerId = 0;
        corpseId = 0;
        success = false;
        totalItems = 0;
        availableLoot.Empty();
    }
};

USTRUCT(BlueprintType)
struct FHarvestErrorStruct
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harvest Error")
    FString type = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harvest Error")
    int32 clientId = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harvest Error")
    int32 playerId = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harvest Error")
    int32 corpseId = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harvest Error")
    FString errorCode = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harvest Error")
    FString message = "";

    FHarvestErrorStruct()
    {
        type = "";
        clientId = 0;
        playerId = 0;
        corpseId = 0;
        errorCode = "";
        message = "";
    }
};

USTRUCT(BlueprintType)
struct FCorpseLootPickupRequestItem
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Corpse Loot Pickup")
    int32 itemId = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Corpse Loot Pickup")
    int32 quantity = 0;

    FCorpseLootPickupRequestItem()
    {
        itemId = 0;
        quantity = 0;
    }
};

USTRUCT(BlueprintType)
struct FCorpseLootPickupResponseStruct
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Corpse Loot Pickup Response")
    bool success = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Corpse Loot Pickup Response")
    int32 corpseUID = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Corpse Loot Pickup Response")
    TArray<FHarvestItemStruct> pickedUpItems;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Corpse Loot Pickup Response")
    TArray<FHarvestItemStruct> remainingLoot;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Corpse Loot Pickup Response")
    int32 itemsPickedUp = 0;

    FCorpseLootPickupResponseStruct()
    {
        success = false;
        corpseUID = 0;
        pickedUpItems.Empty();
        remainingLoot.Empty();
        itemsPickedUp = 0;
    }
};

USTRUCT(BlueprintType)
struct FCorpseLootPickupErrorStruct
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Corpse Loot Pickup Error")
    bool success = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Corpse Loot Pickup Error")
    FString errorCode = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Corpse Loot Pickup Error")
    int32 corpseUID = 0;

    FCorpseLootPickupErrorStruct()
    {
        success = false;
        errorCode = "";
        corpseUID = 0;
    }
};

// Corpse loot inspection structures
USTRUCT(BlueprintType)
struct FCorpseLootInspectResponseStruct
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Corpse Loot Inspect Response")
    bool success = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Corpse Loot Inspect Response")
    int32 corpseUID = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Corpse Loot Inspect Response")
    FString type = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Corpse Loot Inspect Response")
    int32 totalItems = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Corpse Loot Inspect Response")
    TArray<FHarvestItemStruct> availableLoot;

    FCorpseLootInspectResponseStruct()
    {
        success = false;
        corpseUID = 0;
        type = "";
        totalItems = 0;
        availableLoot.Empty();
    }
};

USTRUCT(BlueprintType)
struct FCorpseLootInspectErrorStruct
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Corpse Loot Inspect Error")
    bool success = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Corpse Loot Inspect Error")
    FString errorCode = "";

    FCorpseLootInspectErrorStruct()
    {
        success = false;
        errorCode = "";
    }
};