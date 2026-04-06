#pragma once
#pragma once
#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "DataStructs.generated.h"

class UNiagaraSystem;

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
	int characterExpForLevelStart = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Data Struct")
	int characterExpForLevelEnd = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Data Struct")
    int characterExperienceDebt = 0;
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
    FVector spawnStartPos = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn Zone Struct")
    FVector spawnSize = FVector::ZeroVector;
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mob Data Struct")
	int32 mobCombatState = 0;

	// Inline velocity struct used by DevMode JSON loader
	struct FMobVelocityEntry
	{
		float dirX  = 0.f;
		float dirY  = 0.f;
		float speed = 0.f;
	} mobVelocity;
};

// Mob movement packet entry (server move broadcast)
USTRUCT(BlueprintType)
struct FMobMoveEntryStruct
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mob Move Entry")
	FPositionDataStruct position;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mob Move Entry")
	int32 combatState = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mob Move Entry")
	float velocityX = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mob Move Entry")
	float velocityY = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mob Move Entry")
	float speed = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mob Move Entry")
	int64 stepTimestampMs = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mob Move Entry")
	float waypointX = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mob Move Entry")
	float waypointY = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mob Move Entry")
	bool bHasWaypoint = false;

	FMobMoveEntryStruct() {}
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

// Integer values match the server protocol exactly.
// Protocol: INVALID=0 (unused), SELF=1, PLAYER=2, MOB=3, AREA=4, NONE=5
// NPC is a string-only type ("NPC"), mapped to 6 to avoid collision.
UENUM(BlueprintType)
enum class ECasterType : uint8
{
    Invalid = 0 UMETA(DisplayName = "Invalid", Hidden),
    Self    = 1 UMETA(DisplayName = "Self"),
    Player  = 2 UMETA(DisplayName = "Player"),
    Mob     = 3 UMETA(DisplayName = "Mob"),
    Area    = 4 UMETA(DisplayName = "Area"),
    None    = 5 UMETA(DisplayName = "None"),
    NPC     = 6 UMETA(DisplayName = "NPC")
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
    FString skillSlug = "";

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

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Initiation")
    int32 cooldownMs = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Initiation")
    int32 gcdMs = 0;

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
        cooldownMs = 0;
        gcdMs = 0;
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
    FString skillSlug = "";

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

    // Final caster mana after skill use
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Result")
    int32 finalCasterMana = 0;

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
        finalCasterMana = 0;
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

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TSoftObjectPtr<UTexture2D> Icon;

    // Height offset for combat hit effects (socket-based)
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float CombatHitHeight = 120.0f;

    // Niagara VFX spawned at death (optional)
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TSoftObjectPtr<UNiagaraSystem> DeathVFX;
};

// ============================================================
// Per-entity per-skill voice sound override.
// Row key format: "{audioProfileId}|{skillSlug}"
//   Examples: "warrior_m|fireball", "goblin_shaman|frostbolt", "warrior_m|basic_attack"
//
// Priority chain for cast-start voice:
//   P1: FSkillDefinitionData.castStartVoice  — same sound for ALL casters of this skill
//   P2: DT_EntitySkillVoiceOverrides["warrior_m|fireball"].CastStartVoice  — per-entity per-skill pool
//   P3: FEntityAudioProfile.VoiceCastStart[]  — per-entity generic fallback pool
// ============================================================
USTRUCT(BlueprintType)
struct PROTOTYPING_API FEntitySkillVoiceOverride : public FTableRowBase
{
    GENERATED_BODY()

    /** Random pool played at cast START (AnimNotify CastVoice). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voice Override")
    TArray<TSoftObjectPtr<USoundBase>> CastStartVoice;

    /** Random pool played at cast RELEASE (AnimNotify CastRelease). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voice Override")
    TArray<TSoftObjectPtr<USoundBase>> CastReleaseVoice;
};

// ============================================================
// Entity Audio Profile — shared between players and mobs
// Row key examples:
//   Players: "warrior_m", "warrior_f", "mage_m", "archer_f"
//   Mobs:    "wolf", "goblin_grunt", "goblin_shaman", "skeleton_warrior"
//   Shared:  "giant_humanoid" — multiple mob types, one sound set
// ============================================================
USTRUCT(BlueprintType)
struct FEntityAudioProfile : public FTableRowBase
{
    GENERATED_BODY()

    // ---- Voice ------------------------------------------------
    /** Random pool: melee swing cry ("hiya!", "ha!"). Nofity: VoiceAttack / Voice */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voice")
    TArray<TSoftObjectPtr<USoundBase>> VoiceAttack;

    /** Random pool: incantation / battle cry at cast START. Notify: CastVoice */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voice")
    TArray<TSoftObjectPtr<USoundBase>> VoiceCastStart;

    /** Random pool: shout / exhale at cast RELEASE. Notify: CastRelease (fallback) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voice")
    TArray<TSoftObjectPtr<USoundBase>> VoiceCastRelease;

    // ---- Combat -----------------------------------------------
    /** Weapon / limb whoosh during swing. Priority over SkillData.swingSound. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
    TSoftObjectPtr<USoundBase> SwingSound;

    /** Sound when this entity receives a hit. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
    TSoftObjectPtr<USoundBase> HitReceived;

    /** Death sound. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
    TSoftObjectPtr<USoundBase> Death;

    /** Revive / resurrection sound (players only; ignored on mobs). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
    TSoftObjectPtr<USoundBase> Revive;

    /** Aggro shout when mob detects a player target. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
    TSoftObjectPtr<USoundBase> Aggro;

    /** Generic attack sound (mob "Attack" slot; players typically leave this empty). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
    TSoftObjectPtr<USoundBase> AttackGeneric;

    // ---- Heal / Progression -----------------------------------
    /** Generic heal-received fallback (used when SkillData.healSound is empty). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Healing")
    TSoftObjectPtr<USoundBase> HealReceived;

    /** Level-up chime (players only). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Progression")
    TSoftObjectPtr<USoundBase> LevelUp;

    // ---- Movement / Ambient ----------------------------------
    /** Random pool: footstep sounds at walk speed. Notify: Walk */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
    TArray<TSoftObjectPtr<USoundBase>> FootstepsWalk;

    /** Random pool: footstep sounds at run speed. Notify: Run */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
    TArray<TSoftObjectPtr<USoundBase>> FootstepsRun;

    /** Random pool: ambient idle sounds (muttering, growls). Notify: Idle */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ambient")
    TArray<TSoftObjectPtr<USoundBase>> IdleAmbient;
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

    /**
     * Pool of effort/grunt/roar/howl sounds played via AnimNotify_PlaySoundFromTable("Voice")
     * at the moment of the attack swing (e.g. wolf howl, goblin battle cry, skeleton rattle).
     * Place the notify on the attack montage at 20-40% of the animation.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<TSoftObjectPtr<USoundBase>> AttackVoiceSounds;

    /**
     * Weapon/limb swing whoosh specific to THIS mob type.
     * Played via AnimNotify_PlaySoundFromTable("Swing") or automatically during melee skill.
     * Priority 1 over FSkillDefinitionData::swingSound (which is the generic skill-level fallback).
     * Examples: wolf claw swipe, skeleton sword swish, goblin club whoosh, unarmed fist rush.
     * Leave empty to use the skill-defined swingSound instead.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TSoftObjectPtr<USoundBase> SwingSound;

    /**
     * Pool of voice sounds played at cast START for this mob type.
     * Used as fallback when the skill has no castStartVoice defined.
     * Played via AnimNotify_PlaySoundFromTable("CastVoice") or automatically at PlaySkillAnimation.
     * Examples: goblin chanting, skeleton jaw-rattle, wolf howl before breath attack.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<TSoftObjectPtr<USoundBase>> CastVoiceSounds;

    /**
     * Pool of voice sounds played at cast RELEASE (CastRelease notify) for this mob type.
     * Used as fallback when the skill has no castReleaseVoice defined.
     * Examples: goblin's final screech as fireball fires, golem stomp-release roar.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<TSoftObjectPtr<USoundBase>> ReleaseVoiceSounds;
};

USTRUCT(BlueprintType)
struct FMobDefinition : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName MobType;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FMobVisualData Visual;

    /**
     * Preferred: reference to a row in DT_EntityAudioProfiles.
     * When set, all audio for this mob is driven from that table row.
     * Multiple mob variants (e.g. wolf_pup, wolf_adult, alpha_wolf) can share
     * one profile row, so updating one row updates all of them.
     * Leave empty to fall back to the legacy inline Audio struct below.
     * Examples: "wolf", "goblin_shaman", "skeleton_warrior"
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
    FName AudioProfileId = NAME_None;

    /**
     * Legacy inline audio data. Used automatically when AudioProfileId is empty.
     * Migrate rows to AudioProfileId over time; do not add new fields here.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio|Legacy (prefer AudioProfileId)")
    FMobAudioData Audio;

    // Armor material type used for impact sound lookup (e.g. "leather", "plate")
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName ArmorMaterialType = NAME_None;
};

// ============================================================
// Item Attribute Entry (stat modifier from item)
// ============================================================

USTRUCT(BlueprintType)
struct FItemAttributeStruct
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Attribute")
    int32 id = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Attribute")
    FString name = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Attribute")
    FString slug = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Attribute")
    float value = 0.0f;

    // "equip" = passive bonus while worn, "use" = applied when item is used
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Attribute")
    FString apply_on = TEXT("equip");
};

// ============================================================
// Item Use Effect Entry (consumable effect descriptor)
// ============================================================

USTRUCT(BlueprintType)
struct PROTOTYPING_API FItemUseEffectEntry
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Use Effect")
    FString effectSlug = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Use Effect")
    FString attributeSlug = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Use Effect")
    float value = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Use Effect")
    int32 durationSeconds = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Use Effect")
    int32 tickMs = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Use Effect")
    int32 cooldownSeconds = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Use Effect")
    bool isInstant = false;

    FItemUseEffectEntry() {}
};

// Inventory item structure that matches server format
USTRUCT(BlueprintType)
struct FInventoryItemStruct
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Item")
	int32 id = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Item")
	int32 itemId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Item")
	int32 quantity = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Item")
	float weight = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Item")
	int32 stackSize = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Item")
	int32 maxQuantity = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Item")
	int32 inventorySlotId = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Item")
	int32 slotIndex = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Item")
	int32 equip_slot_id = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Item")
	int32 durabilityMax = 100;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Item")
	int32 durabilityMin = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Item")
	int32 durabilityCurrent = 100;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Item")
	int32 levelRequirement = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Item")
    int32 rarityId = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Item")
    int32 characterId = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Item")
    FString rarityName = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Item")
    FString raritySlug = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Item")
	FString itemSlug = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Item")
	FString itemType = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Item")
	FString itemTypeName = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Item")
    FString itemTypeSlug = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Item")
    int32 item_type_id = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Item")
	FString equipSlotSlug = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Item")
	FString masterySlug = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Item")
	int32 set_id = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Item")
	FString setSlug = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Item")
	bool isDurable = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Item")
	bool isDurabilityWarning = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Item")
	bool isTradable = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Item")
	bool isEquippable = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Item")
	bool is_equipped = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Item")
	bool isTwoHanded = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Item")
	int32 priceBuy = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Item")
	int32 priceSell = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Item")
    bool isContainer = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Item")
    bool isQuestItem = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Item")
    bool isUsable = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Item")
    bool isHarvestItem = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Item")
    bool addedToInventory = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Item")
    int32 killCount = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Item")
    TArray<FItemUseEffectEntry> useEffects;

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

	// Typed attribute array parsed from protocol (supports apply_on field)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory Item")
	TArray<FItemAttributeStruct> itemAttributes;

	FInventoryItemStruct()
	{
		id = 0;
		itemId = 0;
		quantity = 0;
		name = "";
        slug = "";
		itemSlug = "";
		description = "";
		type = "";
		rarity = "";
		rarityId = 0;
		rarityName = "";
		raritySlug = "";
		itemType = "";
		itemTypeName = "";
		itemTypeSlug = "";
        level_requirement = 0;
		levelRequirement = 0;
		weight = 0.0f;
		stackSize = 0;
		maxQuantity = 0;
		inventorySlotId = 0;
		durabilityMax = 100;
		durabilityCurrent = 100;
		durabilityMin = 0;
		priceBuy = 0;
		priceSell = 0;
		equipSlotSlug = "";
		masterySlug = "";
		setSlug = "";
		killCount = 0;

		isDurable = false;
		isDurabilityWarning = false;
		isTradable = true;
		isEquippable = false;
		is_equipped = false;
		isTwoHanded = false;
		isContainer = false;
		isQuestItem = false;
		isUsable = false;
		isHarvestItem = false;
		addedToInventory = false;

		attributes.Empty();
		itemAttributes.Empty();
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
	int32 gold = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character Inventory")
	TArray<FInventoryItemStruct> items;

	FCharacterInventoryStruct()
	{
		characterId = 0;
		gold = 0;
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
    Poison,
    Heal
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

// Mob health update (sent during RETURNING state for leash regen)
USTRUCT(BlueprintType)
struct FMobHealthUpdateStruct
{
    GENERATED_BODY()

    // Template/base mob ID (mobId from server)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mob Health Update")
    int32 mobId = 0;

    // Unique instance ID (mobUID from server) � use this to look up the actor
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mob Health Update")
    int32 mobUID = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mob Health Update")
    int32 currentHealth = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mob Health Update")
    int32 maxHealth = 0;

    FMobHealthUpdateStruct() {}
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

// Experience System Structures
USTRUCT(BlueprintType)
struct FExperienceUpdateStruct
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Experience Update")
    int32 characterId = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Experience Update")
    int32 oldLevel = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Experience Update")
    int32 newLevel = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Experience Update")
    int32 oldExperience = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Experience Update")
    int32 newExperience = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Experience Update")
    int32 experienceChange = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Experience Update")
    int32 expForCurrentLevel = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Experience Update")
    int32 expForNextLevel = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Experience Update")
    bool levelUp = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Experience Update")
    FString reason = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Experience Update")
    int32 sourceId = 0;

    FExperienceUpdateStruct()
    {
        characterId = 0;
        oldLevel = 1;
        newLevel = 1;
        oldExperience = 0;
        newExperience = 0;
        experienceChange = 0;
        expForCurrentLevel = 0;
        expForNextLevel = 0;
        levelUp = false;
        reason = "";
        sourceId = 0;
    }
};

// Player progression data structure for managing character advancement
USTRUCT(BlueprintType)
struct FPlayerProgressionStruct
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player Progression")
    int32 characterId = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player Progression")
    int32 currentLevel = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player Progression")
    int32 currentExperience = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player Progression")
    int32 totalExperience = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player Progression")
    int32 expForNextLevel = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player Progression")
    int32 expForCurrentLevel = 0;

    // Track level-ups that need to be shown to player
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player Progression")
    bool bHasPendingLevelUp = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player Progression")
    int32 pendingLevelGained = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player Progression")
    int32 experienceDebt = 0;

    FPlayerProgressionStruct()
    {
        characterId = 0;
        currentLevel = 1;
        currentExperience = 0;
        totalExperience = 0;
        expForNextLevel = 0;
        expForCurrentLevel = 0;
        bHasPendingLevelUp = false;
        pendingLevelGained = 0;
        experienceDebt = 0;
    }
};

// Experience gain reason enumeration for categorizing experience sources
UENUM(BlueprintType)
enum class EExperienceReason : uint8
{
    None = 0        UMETA(DisplayName = "None"),
    MobKill = 1     UMETA(DisplayName = "Mob Kill"),
    QuestComplete = 2  UMETA(DisplayName = "Quest Complete"),
    QuestTurnIn = 3    UMETA(DisplayName = "Quest Turn In"),
    Discovery = 4      UMETA(DisplayName = "Discovery"),
    Crafting = 5       UMETA(DisplayName = "Crafting"),
    Gathering = 6      UMETA(DisplayName = "Gathering"),
    PvPKill = 7        UMETA(DisplayName = "PvP Kill"),
    BossKill = 8       UMETA(DisplayName = "Boss Kill"),
    GroupBonus = 9     UMETA(DisplayName = "Group Bonus"),
    Event = 10         UMETA(DisplayName = "Event"),
    Admin = 99         UMETA(DisplayName = "Admin")
};

// Experience gain event structure for detailed logging and display
USTRUCT(BlueprintType)
struct FExperienceGainEventStruct
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Experience Gain Event")
    int32 experienceGained = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Experience Gain Event")
    EExperienceReason reason = EExperienceReason::None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Experience Gain Event")
    FString reasonText = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Experience Gain Event")
    int32 sourceId = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Experience Gain Event")
    FString sourceName = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Experience Gain Event")
    FDateTime timestamp;

    FExperienceGainEventStruct()
    {
        experienceGained = 0;
        reason = EExperienceReason::None;
        reasonText = "";
        sourceId = 0;
        sourceName = "";
        timestamp = FDateTime::Now();
    }
};

// ============================================================
// Stat Attribute Entry (used by PlayerStatsUpdateStruct)
// ============================================================

USTRUCT(BlueprintType)
struct PROTOTYPING_API FStatAttributeEntry
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stat Attribute")
    FString slug = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stat Attribute")
    FString name = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stat Attribute")
    float baseValue = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stat Attribute")
    float base = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stat Attribute")
    float totalValue = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stat Attribute")
    float effective = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stat Attribute")
    float bonus = 0.0f;

    FStatAttributeEntry() {}
};

// ============================================================
// Active Effect Entry (used by PlayerStatsUpdateStruct)
// ============================================================

USTRUCT(BlueprintType)
struct PROTOTYPING_API FActiveEffectEntry
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Active Effect")
    FString slug = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Active Effect")
    FString effectType = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Active Effect")
    FString effectTypeSlug = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Active Effect")
    FString attributeSlug = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Active Effect")
    float value = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Active Effect")
    bool bIsPercentage = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Active Effect")
    bool bIsPermanent = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Active Effect")
    int64 expiresAt = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Active Effect")
    FString sourceType = "";

    bool IsPassive() const { return effectTypeSlug.Equals(TEXT("passive"), ESearchCase::IgnoreCase) || bIsPermanent; }

    FActiveEffectEntry() {}
};

// Player Stats Update Structure - matches server format for stats_update event
USTRUCT(BlueprintType)
struct FPlayerStatsUpdateStruct
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player Stats Update")
    int32 characterId = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player Stats Update")
    int32 level = 1;

    // Health stats
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player Stats Update")
    int32 healthCurrent = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player Stats Update")
    int32 healthMax = 0;

    // Mana stats
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player Stats Update")
    int32 manaCurrent = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player Stats Update")
    int32 manaMax = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player Stats Update")
    TArray<FStatAttributeEntry> attributes;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player Stats Update")
    TArray<FActiveEffectEntry> activeEffects;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player Stats Update")
    float weightCurrent = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player Stats Update")
    float weightMax = 0.0f;

    // Free skill points
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player Stats Update")
    int32 freeSkillPoints = 0;

    // Experience fields
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player Stats Update")
    int32 experienceCurrent = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player Stats Update")
    int32 experienceLevelStart = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player Stats Update")
    int32 experienceNextLevel = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player Stats Update")
    int32 experienceDebt = 0;

    FPlayerStatsUpdateStruct()
    {
        characterId = 0;
        level = 1;
        healthCurrent = 0;
        healthMax = 0;
        manaCurrent = 0;
        manaMax = 0;
        freeSkillPoints = 0;
        experienceCurrent = 0;
        experienceLevelStart = 0;
        experienceNextLevel = 0;
        experienceDebt = 0;
    }
};

// Player Skills System Structures

// Server skill data from network
USTRUCT(BlueprintType)
struct FPlayerSkillNetworkData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player Skill Network")
    FString skillSlug = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player Skill Network")
    int32 skillLevel = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player Skill Network")
    int32 castMs = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player Skill Network")
    float coeff = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player Skill Network")
    int32 cooldownMs = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player Skill Network")
    int32 costMp = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player Skill Network")
    float flatAdd = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player Skill Network")
    int32 gcdMs = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player Skill Network")
    float maxRange = 0.0f;

    FPlayerSkillNetworkData()
    {
        skillSlug = "";
        skillLevel = 1;
        castMs = 0;
        coeff = 1.0f;
        cooldownMs = 0;
        costMp = 0;
        flatAdd = 0.0f;
        gcdMs = 0;
        maxRange = 0.0f;
    }
};

// Player skills initialization from server
USTRUCT(BlueprintType)
struct FPlayerSkillsInitializationData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player Skills Initialization")
    int32 characterId = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player Skills Initialization")
    TArray<FPlayerSkillNetworkData> skills;

    FPlayerSkillsInitializationData()
    {
        characterId = 0;
        skills.Empty();
    }
};

// Extended skill definition for client-side data (icons, descriptions, etc.)
USTRUCT(BlueprintType)
struct FSkillDefinitionData : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Definition")
    FString skillSlug = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Definition")
    FText displayName = FText::GetEmpty();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Definition")
    FText description = FText::GetEmpty();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Definition")
    TSoftObjectPtr<UTexture2D> skillIcon;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Definition")
    TSoftObjectPtr<USoundBase> castSound;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Definition")
    TSoftObjectPtr<USoundBase> hitSound;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Definition")
    FString animationName = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Definition")
    ESkillEffectType effectType = ESkillEffectType::None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Definition")
    ESkillSchool school = ESkillSchool::None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Definition")
    FLinearColor skillColor = FLinearColor::White;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Definition")
    bool bIsChanneled = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Definition")
    bool bIsTargeted = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Definition")
    bool bRequiresLineOfSight = true;

    // Niagara visual effects
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Definition|Niagara")
    TSoftObjectPtr<UNiagaraSystem> castEffectNiagara;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Definition|Niagara")
    TSoftObjectPtr<UNiagaraSystem> hitEffectNiagara;

    // VFX and sound spawned on the target when a heal or HoT tick lands.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Definition|Niagara")
    TSoftObjectPtr<UNiagaraSystem> healEffectNiagara;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Definition")
    TSoftObjectPtr<USoundBase> healSound;

    // Socket names for effect attachment
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Definition|Sockets")
    FName CastSocketName = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Definition|Sockets")
    FName HitSocketName = NAME_None;

    // Swing sound played during melee attacks before impact
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Definition")
    TSoftObjectPtr<USoundBase> swingSound;

    // Weapon impact type for impact sound lookup (e.g. "slash", "blunt")
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Definition")
    FName WeaponImpactType = NAME_None;

    // Sound played on a critical hit (layered on top of hitSound)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Definition")
    TSoftObjectPtr<USoundBase> critSound;

    // Sound played at the moment the spell/ability is released (use AnimNotify_PlayerCombatEvent with CastRelease slot).
    // Examples: fireball launch whoosh, arrow release twang, spell incantation finale.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Definition|Cast Release")
    TSoftObjectPtr<USoundBase> castEndSound;

    // Niagara VFX spawned at CastSocketName at the moment the spell is released.
    // Examples: muzzle flash on hands, departing glow orb, beam origin burst.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Definition|Cast Release")
    TSoftObjectPtr<UNiagaraSystem> castEndEffectNiagara;

    // Projectile actor class to spawn when the skill fires
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Definition")
    TSoftClassPtr<AActor> projectileClass;

    // ---------------------------------------------------------------
    // Voice sounds (who says what and when)
    // ---------------------------------------------------------------
    // Skill-specific battle cry / incantation played at cast START.
    // This is the SAME sound for any entity (player or mob) casting this skill.
    // Examples: "shimabalam!" for a fireball, "EGEY!" for a power strike.
    // Priority 1: this field. Priority 2: entity's own VoiceCastStart pool.
    // Leave empty to let each entity use their own voice pool instead.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Definition|Voice")
    TSoftObjectPtr<USoundBase> castStartVoice;

    // Skill-specific shout / incantation played at cast RELEASE moment.
    // Examples: "FIRE!" as the fireball launches, "TAKE THIS!" on a charge release.
    // Priority 1: this field. Priority 2: entity's own VoiceCastRelease pool.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Definition|Voice")
    TSoftObjectPtr<USoundBase> castReleaseVoice;

    FSkillDefinitionData()
    {
        skillSlug = "";
        displayName = FText::GetEmpty();
        description = FText::GetEmpty();
        animationName = "";
        effectType = ESkillEffectType::None;
        school = ESkillSchool::None;
        skillColor = FLinearColor::White;
        bIsChanneled = false;
        bIsTargeted = true;
        bRequiresLineOfSight = true;
    }
};

// Complete player skill data (network + definition)
USTRUCT(BlueprintType)
struct FPlayerSkillData
{
    GENERATED_BODY()

    // Network data from server
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player Skill")
    FPlayerSkillNetworkData networkData;

    // Definition data from data table
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player Skill")
    FSkillDefinitionData definitionData;

    // Runtime cooldown tracking
    UPROPERTY(BlueprintReadOnly, Category = "Player Skill")
    double cooldownEndTime = 0.0f;

    // Runtime state
    UPROPERTY(BlueprintReadOnly, Category = "Player Skill")
    bool bIsOnCooldown = false;

    UPROPERTY(BlueprintReadOnly, Category = "Player Skill")
    bool bIsReady = true;

    bool   bCooldownUsesServerClock = true;

    FPlayerSkillData()
    {
        networkData = FPlayerSkillNetworkData();
        definitionData = FSkillDefinitionData();
        cooldownEndTime = 0.0f;
        bIsOnCooldown = false;
        bIsReady = true;
    }
};

// Skill slot data for UI
USTRUCT(BlueprintType)
struct FSkillSlotData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Slot")
    int32 slotIndex = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Slot")
    FString skillSlug = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Slot")
    FKey boundKey;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Slot")
    bool bIsAssigned = false;

    FSkillSlotData()
    {
        slotIndex = 0;
        skillSlug = "";
        boundKey = FKey();
        bIsAssigned = false;
    }
};


//////////////////////////////////////////////////////////////////////////
// NPC System Structures
//////////////////////////////////////////////////////////////////////////

USTRUCT(BlueprintType)
struct PROTOTYPING_API FNPCStatsStruct
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC Stats")
    int32 current = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC Stats")
    int32 max = 0;

    FNPCStatsStruct() {}
};

USTRUCT(BlueprintType)
struct PROTOTYPING_API FNPCHealthManaStruct
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC Health Mana")
    FNPCStatsStruct health;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC Health Mana")
    FNPCStatsStruct mana;

    FNPCHealthManaStruct() {}
};

// Forward declaration needed by FNPCStruct::ComputeInteractionState
enum class ENPCInteractionState : uint8;

USTRUCT(BlueprintType)
struct PROTOTYPING_API FNPCQuestEntry
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC Quest Entry")
    FString slug = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC Quest Entry")
    FString status = "";

    FNPCQuestEntry() {}
};

USTRUCT(BlueprintType)
struct PROTOTYPING_API FNPCStruct
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC Data")
    int32 id = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC Data")
    FString name = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC Data")
    FString slug = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC Data")
    FString race = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC Data")
    int32 level = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC Data")
    FString npcType = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC Data")
    bool isInteractable = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC Data")
    FString dialogueId = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC Data")
    FString questId = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC Data")
    TArray<FNPCQuestEntry> quests;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC Data")
    FPositionDataStruct position;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC Data")
    TArray<FAttributeDataStruct> attributes;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC Data")
    FNPCHealthManaStruct stats;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC Data")
    int32 radius = 300;

    /** Compute the interaction state based on NPC data fields. */
    ENPCInteractionState ComputeInteractionState() const;

    FNPCStruct() {}
};

USTRUCT(BlueprintType)
struct PROTOTYPING_API FNPCSpawnDataStruct
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC Spawn Data")
    int32 npcCount = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC Spawn Data")
    float spawnRadius = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC Spawn Data")
    TArray<FNPCStruct> npcsSpawn;

    FNPCSpawnDataStruct() {}
};

//////////////////////////////////////////////////////////////////////////
// NPC Definition Structures (��� DataTable)
//////////////////////////////////////////////////////////////////////////

USTRUCT(BlueprintType)
struct PROTOTYPING_API FNPCVisualData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TSoftObjectPtr<USkeletalMesh> SkeletalMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TSoftClassPtr<UAnimInstance> AnimBPClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector ActorScale = FVector(1.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName NPCName;

    FNPCVisualData() {}
};

USTRUCT(BlueprintType)
struct PROTOTYPING_API FNPCAudioData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TSoftObjectPtr<USoundBase> GreetingSound;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TSoftObjectPtr<USoundBase> InteractSound;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TSoftObjectPtr<USoundBase> FarewellSound;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<TSoftObjectPtr<USoundBase>> IdleSounds;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<TSoftObjectPtr<USoundBase>> WalkSounds;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<TSoftObjectPtr<USoundBase>> RunSounds;

    FNPCAudioData() {}
};

USTRUCT(BlueprintType)
struct PROTOTYPING_API FNPCDefinition : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName NPCType;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FNPCVisualData Visual;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FNPCAudioData Audio;

    FNPCDefinition() {}
};

// ============================================================
// NPC Interaction State
// ============================================================

UENUM(BlueprintType)
enum class ENPCInteractionState : uint8
{
    None            UMETA(DisplayName = "None"),
    NotInteractable UMETA(DisplayName = "Not Interactable"),
    QuestAvailable  UMETA(DisplayName = "Quest Available"),
    QuestInProgress UMETA(DisplayName = "Quest In Progress"),
    QuestComplete   UMETA(DisplayName = "Quest Complete"),
    Dialogue        UMETA(DisplayName = "Dialogue"),
    DialogueOnly    UMETA(DisplayName = "Dialogue Only"),
};

// ============================================================
// Effect Tick Data
// ============================================================

USTRUCT(BlueprintType)
struct PROTOTYPING_API FEffectTickData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect Tick")
    int32 characterId = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect Tick")
    FString effectSlug = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect Tick")
    FString effectTypeSlug = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect Tick")
    int32 value = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect Tick")
    bool bIsHeal = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect Tick")
    int32 newHealth = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect Tick")
    int32 newMana = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect Tick")
    bool targetDied = false;

    FEffectTickData() {}
};

// ============================================================
// Equipment Structures
// ============================================================

USTRUCT(BlueprintType)
struct PROTOTYPING_API FEquipmentSlotData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment Slot")
    FString slotSlug = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment Slot")
    int32 inventoryItemId = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment Slot")
    int32 itemId = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment Slot")
    FString itemSlug = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment Slot")
    FString itemName = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment Slot")
    int32 durability = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment Slot")
    int32 maxDurability = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment Slot")
    int32 durabilityCurrent = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment Slot")
    int32 durabilityMax = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment Slot")
    bool bIsOccupied = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment Slot")
    bool blockedByTwoHanded = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment Slot")
    bool isDurabilityWarning = false;

    FEquipmentSlotData() {}
};

USTRUCT(BlueprintType)
struct PROTOTYPING_API FEquipmentStateData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment State")
    int32 characterId = 0;

    // keyed by slotSlug ("main_hand", "head", etc.)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment State")
    TMap<FString, FEquipmentSlotData> slots;

    FEquipmentStateData() {}
};

USTRUCT(BlueprintType)
struct PROTOTYPING_API FEquipResultData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equip Result")
    int32 characterId = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equip Result")
    bool bSuccess = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equip Result")
    FString errorCode = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equip Result")
    FString slotSlug = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equip Result")
    FString equipSlotSlug = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equip Result")
    FString action = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equip Result")
    int32 inventoryItemId = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equip Result")
    int32 swappedOutInventoryItemId = 0;

    FEquipResultData() {}
};

USTRUCT(BlueprintType)
struct PROTOTYPING_API FWeightStatusData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weight Status")
    int32 characterId = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weight Status")
    float currentWeight = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weight Status")
    float maxWeight = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weight Status")
    float weightLimit = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weight Status")
    float weightCurrent = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weight Status")
    bool isOverweight = false;

    FWeightStatusData() {}
};

// ============================================================
// Chat
// ============================================================

USTRUCT(BlueprintType)
struct PROTOTYPING_API FChatMessageStruct
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chat Message")
    FString channel = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chat Message")
    FString senderName = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chat Message")
    int32 senderId = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chat Message")
    FString text = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chat Message")
    int64 timestamp = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chat Message")
    bool bIsError = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chat Message")
    FString errorMessage = "";

    FChatMessageStruct() {}
};

// ============================================================
// Trade
// ============================================================

USTRUCT(BlueprintType)
struct PROTOTYPING_API FTradeOfferItem
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trade Offer Item")
    int32 inventoryItemId = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trade Offer Item")
    int32 itemId = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trade Offer Item")
    FString itemName = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trade Offer Item")
    FString itemSlug = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trade Offer Item")
    int32 quantity = 1;

    FTradeOfferItem() {}
};

USTRUCT(BlueprintType)
struct PROTOTYPING_API FTradeInviteData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trade Invite")
    FString sessionId = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trade Invite")
    int32 initiatorId = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trade Invite")
    FString initiatorName = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trade Invite")
    FString fromCharacterName = "";

    FTradeInviteData() {}
};

USTRUCT(BlueprintType)
struct PROTOTYPING_API FTradeStateData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trade State")
    FString sessionId = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trade State")
    int32 myCharacterId = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trade State")
    int32 partnerCharacterId = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trade State")
    FString partnerName = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trade State")
    TArray<FInventoryItemStruct> myItems;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trade State")
    TArray<FTradeOfferItem> partnerItems;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trade State")
    TArray<FInventoryItemStruct> theirItems;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trade State")
    int32 myGold = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trade State")
    int32 partnerGold = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trade State")
    int32 theirGold = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trade State")
    int32 myGoldBalance = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trade State")
    bool bMyConfirmed = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trade State")
    bool bPartnerConfirmed = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trade State")
    bool myConfirmed = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trade State")
    bool theirConfirmed = false;

    FTradeStateData() {}
};

USTRUCT(BlueprintType)
struct PROTOTYPING_API FTradeDeclinedData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trade Declined")
    FString sessionId = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trade Declined")
    int32 decliningCharacterId = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trade Declined")
    FString byCharacterName = "";

    FTradeDeclinedData() {}
};

USTRUCT(BlueprintType)
struct PROTOTYPING_API FTradeCancelledData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trade Cancelled")
    FString sessionId = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trade Cancelled")
    FString reason = "";

    FTradeCancelledData() {}
};

USTRUCT(BlueprintType)
struct PROTOTYPING_API FTradeReceivedItem
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trade Received Item")
    int32 itemId = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trade Received Item")
    FString slug = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trade Received Item")
    int32 quantity = 0;

    FTradeReceivedItem() {}
};

USTRUCT(BlueprintType)
struct PROTOTYPING_API FTradeCompleteData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trade Complete")
    FString sessionId = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trade Complete")
    bool bSuccess = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trade Complete")
    int32 receivedGold = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trade Complete")
    int32 newGoldBalance = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trade Complete")
    TArray<FTradeReceivedItem> receivedItems;

    FTradeCompleteData() {}
};

// ============================================================
// Vendor / Shop
// ============================================================

USTRUCT(BlueprintType)
struct PROTOTYPING_API FVendorShopItemData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vendor Shop Item")
    int32 id = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vendor Shop Item")
    int32 itemId = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vendor Shop Item")
    FString itemSlug = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vendor Shop Item")
    FString slug = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vendor Shop Item")
    FString itemName = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vendor Shop Item")
    FString itemTypeName = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vendor Shop Item")
    FString rarityName = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vendor Shop Item")
    int32 itemType = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vendor Shop Item")
    int32 rarityId = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vendor Shop Item")
    FString raritySlug = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vendor Shop Item")
    FString description = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vendor Shop Item")
    int32 equipSlot = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vendor Shop Item")
    FString equipSlotSlug = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vendor Shop Item")
    FString itemTypeSlug = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vendor Shop Item")
    TArray<FItemUseEffectEntry> useEffects;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vendor Shop Item")
    FString masterySlug = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vendor Shop Item")
    int32 killCount = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vendor Shop Item")
    int32 price = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vendor Shop Item")
    int32 priceBuy = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vendor Shop Item")
    int32 priceSell = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vendor Shop Item")
    int32 sellPrice = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vendor Shop Item")
    int32 stock = -1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vendor Shop Item")
    int32 stockCurrent = -1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vendor Shop Item")
    int32 stockMax = -1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vendor Shop Item")
    int32 stackMax = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vendor Shop Item")
    int32 levelRequirement = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vendor Shop Item")
    int32 setId = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vendor Shop Item")
    FString setSlug = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vendor Shop Item")
    float weight = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vendor Shop Item")
    int32 durabilityMax = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vendor Shop Item")
    bool isEquippable = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vendor Shop Item")
    bool isUsable = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vendor Shop Item")
    bool isTwoHanded = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vendor Shop Item")
    bool isQuestItem = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vendor Shop Item")
    bool isContainer = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vendor Shop Item")
    bool isHarvest = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vendor Shop Item")
    bool isTradable = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vendor Shop Item")
    bool isDurable = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vendor Shop Item")
    TArray<int32> allowedClassIds;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vendor Shop Item")
    TMap<FString, FString> attributes;

    FVendorShopItemData() {}
};

USTRUCT(BlueprintType)
struct PROTOTYPING_API FVendorCartEntry
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vendor Cart Entry")
    int32 itemId = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vendor Cart Entry")
    int32 inventoryItemId = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vendor Cart Entry")
    FString itemSlug = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vendor Cart Entry")
    FString itemName = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vendor Cart Entry")
    FString slug = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vendor Cart Entry")
    int32 quantity = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vendor Cart Entry")
    int32 maxQuantity = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vendor Cart Entry")
    int32 pricePerUnit = 0;

    FVendorCartEntry() {}
};

USTRUCT(BlueprintType)
struct PROTOTYPING_API FVendorShopData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vendor Shop")
    int32 npcId = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vendor Shop")
    FString npcName = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vendor Shop")
    FString npcSlug = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vendor Shop")
    int32 goldBalance = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vendor Shop")
    TArray<FVendorShopItemData> items;

    FVendorShopData() {}
};

USTRUCT(BlueprintType)
struct PROTOTYPING_API FBuyItemResultData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Buy Item Result")
    bool bSuccess = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Buy Item Result")
    int32 npcId = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Buy Item Result")
    int32 itemId = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Buy Item Result")
    int32 quantity = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Buy Item Result")
    int32 goldSpent = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Buy Item Result")
    int32 totalPrice = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Buy Item Result")
    int32 newGoldBalance = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Buy Item Result")
    FString errorCode = "";

    FBuyItemResultData() {}
};

USTRUCT(BlueprintType)
struct PROTOTYPING_API FSellItemResultData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sell Item Result")
    bool bSuccess = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sell Item Result")
    int32 npcId = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sell Item Result")
    int32 inventoryItemId = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sell Item Result")
    int32 inventorySlotId = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sell Item Result")
    int32 quantity = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sell Item Result")
    int32 goldReceived = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sell Item Result")
    int32 newGoldBalance = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sell Item Result")
    FString errorCode = "";

    FSellItemResultData() {}
};

USTRUCT(BlueprintType)
struct PROTOTYPING_API FBuyBatchItemResult
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Buy Batch Item Result")
    int32 itemId = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Buy Batch Item Result")
    int32 quantity = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Buy Batch Item Result")
    int32 totalPrice = 0;

    FBuyBatchItemResult() {}
};

USTRUCT(BlueprintType)
struct PROTOTYPING_API FBuyItemBatchResultData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Buy Batch Result")
    bool bSuccess = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Buy Batch Result")
    int32 totalGoldSpent = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Buy Batch Result")
    int32 newGoldBalance = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Buy Batch Result")
    TArray<FBuyBatchItemResult> items;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Buy Batch Result")
    FString errorCode = "";

    FBuyItemBatchResultData() {}
};

USTRUCT(BlueprintType)
struct PROTOTYPING_API FSellBatchItemResult
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sell Batch Item Result")
    int32 inventoryItemId = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sell Batch Item Result")
    int32 itemId = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sell Batch Item Result")
    int32 quantity = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sell Batch Item Result")
    int32 goldReceived = 0;

    FSellBatchItemResult() {}
};

USTRUCT(BlueprintType)
struct PROTOTYPING_API FSellItemBatchResultData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sell Batch Result")
    bool bSuccess = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sell Batch Result")
    int32 totalGoldReceived = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sell Batch Result")
    TArray<FSellBatchItemResult> items;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sell Batch Result")
    FString errorCode = "";

    FSellItemBatchResultData() {}
};

// ============================================================
// Repair Shop
// ============================================================

USTRUCT(BlueprintType)
struct PROTOTYPING_API FRepairShopItemData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Repair Shop Item")
    int32 inventoryItemId = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Repair Shop Item")
    int32 itemId = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Repair Shop Item")
    FString itemName = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Repair Shop Item")
    FString slug = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Repair Shop Item")
    int32 durabilityCurrent = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Repair Shop Item")
    int32 durabilityMax = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Repair Shop Item")
    int32 repairCost = 0;

    FRepairShopItemData() {}
};

USTRUCT(BlueprintType)
struct PROTOTYPING_API FRepairedItemEntry
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Repaired Item Entry")
    int32 inventoryItemId = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Repaired Item Entry")
    int32 itemId = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Repaired Item Entry")
    int32 durabilityMax = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Repaired Item Entry")
    int32 goldSpent = 0;

    FRepairedItemEntry() {}
};

USTRUCT(BlueprintType)
struct PROTOTYPING_API FRepairShopData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Repair Shop")
    int32 npcId = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Repair Shop")
    FString npcName = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Repair Shop")
    int32 totalRepairCost = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Repair Shop")
    int32 repairAllCost = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Repair Shop")
    TArray<FRepairShopItemData> items;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Repair Shop")
    TArray<FEquipmentSlotData> repairableItems;

    FRepairShopData() {}
};

USTRUCT(BlueprintType)
struct PROTOTYPING_API FRepairItemResultData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Repair Item Result")
    bool bSuccess = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Repair Item Result")
    int32 inventoryItemId = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Repair Item Result")
    int32 itemId = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Repair Item Result")
    int32 goldSpent = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Repair Item Result")
    int32 durabilityMax = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Repair Item Result")
    int32 newGoldBalance = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Repair Item Result")
    FString errorCode = "";

    FRepairItemResultData() {}
};

USTRUCT(BlueprintType)
struct PROTOTYPING_API FRepairAllResultData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Repair All Result")
    bool bSuccess = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Repair All Result")
    int32 totalGoldSpent = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Repair All Result")
    int32 goldSpent = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Repair All Result")
    int32 newGoldBalance = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Repair All Result")
    TArray<FRepairedItemEntry> repairedItems;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Repair All Result")
    FString errorCode = "";

    FRepairAllResultData() {}
};

// ============================================================
// Skill Shop (NPC Trainer)
// ============================================================

USTRUCT(BlueprintType)
struct PROTOTYPING_API FSkillShopSkillData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Shop")
    int32 skillId = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Shop")
    FString skillSlug = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Shop")
    FString skillName = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Shop")
    FString description = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Shop")
    bool isPassive = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Shop")
    int32 requiredLevel = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Shop")
    int32 spCost = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Shop")
    int32 goldCost = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Shop")
    bool requiresBook = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Shop")
    int32 bookItemId = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Shop")
    FString prerequisiteSkillSlug = "";

    // Server-evaluated affordability flags
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Shop")
    bool isLearned = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Shop")
    bool canLearn = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Shop")
    bool prereqMet = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Shop")
    bool levelMet = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Shop")
    bool spMet = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Shop")
    bool goldMet = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Shop")
    bool bookMet = true;

    FSkillShopSkillData() {}
};

USTRUCT(BlueprintType)
struct PROTOTYPING_API FSkillShopData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Shop")
    int32 npcId = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Shop")
    FString npcSlug = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Shop")
    int32 freeSkillPoints = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Shop")
    int32 goldBalance = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill Shop")
    TArray<FSkillShopSkillData> skills;

    FSkillShopData() {}
};

USTRUCT(BlueprintType)
struct PROTOTYPING_API FLearnSkillResultData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Learn Skill Result")
    bool bSuccess = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Learn Skill Result")
    FString skillSlug = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Learn Skill Result")
    FString skillName = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Learn Skill Result")
    bool isPassive = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Learn Skill Result")
    int32 newFreeSkillPoints = 0;

    // On success: full skill data to register in PlayerSkillManager
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Learn Skill Result")
    FPlayerSkillNetworkData skillData;

    // On failure: reason code
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Learn Skill Result")
    FString failReason = "";

    FLearnSkillResultData() {}
};

// ============================================================
// Quest
// ============================================================

USTRUCT(BlueprintType)
struct PROTOTYPING_API FQuestProgressData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest Progress")
    FString questSlug = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest Progress")
    FString questTitle = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest Progress")
    FString status = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest Progress")
    FString currentStep = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest Progress")
    FString clientStepKey = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest Progress")
    int32 stepIndex = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest Progress")
    int32 totalSteps = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest Progress")
    TArray<FString> completedSteps;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest Progress")
    TMap<FString, int32> objectiveCounts;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest Progress")
    int32 questId = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest Progress")
    FString clientQuestKey = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest Progress")
    FString questState = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest Progress")
    FString state = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest Progress")
    FString currentStepKey = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest Progress")
    FString stepType = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest Progress")
    int32 stepCurrentCount = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest Progress")
    int32 stepRequiredCount = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest Progress")
    int32 progressCurrent = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest Progress")
    int32 progressRequired = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest Progress")
    FString progressJson = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest Progress")
    FString requiredJson = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest Progress")
    FString completionMode = "";

    FQuestProgressData() {}
};

USTRUCT(BlueprintType)
struct PROTOTYPING_API FQuestOfferedData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest Offered")
    int32 questId = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest Offered")
    FString clientQuestKey = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest Offered")
    int32 npcId = 0;

    FQuestOfferedData() {}
};

USTRUCT(BlueprintType)
struct PROTOTYPING_API FQuestTurnedInData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest Turned In")
    int32 questId = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest Turned In")
    FString clientQuestKey = "";

    FQuestTurnedInData() {}
};

USTRUCT(BlueprintType)
struct PROTOTYPING_API FQuestFailedData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest Failed")
    int32 questId = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest Failed")
    FString clientQuestKey = "";

    FQuestFailedData() {}
};

USTRUCT(BlueprintType)
struct PROTOTYPING_API FReputationChangedData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reputation")
    FString faction = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reputation")
    int32 delta = 0;

    FReputationChangedData() {}
};

USTRUCT(BlueprintType)
struct PROTOTYPING_API FExpReceivedData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Exp Received")
    int32 amount = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Exp Received")
    FString source = "";

    FExpReceivedData() {}
};

USTRUCT(BlueprintType)
struct PROTOTYPING_API FItemReceivedData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Received")
    int32 itemId = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Received")
    FString itemSlug = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Received")
    FString itemName = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Received")
    int32 quantity = 1;

    FItemReceivedData() {}
};

USTRUCT(BlueprintType)
struct PROTOTYPING_API FGoldReceivedData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gold Received")
    int32 amount = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gold Received")
    FString source = "";

    FGoldReceivedData() {}
};

// ============================================================
// Dialogue
// ============================================================

USTRUCT(BlueprintType)
struct PROTOTYPING_API FDialogueChoice
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue Choice")
    int32 edgeId = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue Choice")
    FString clientChoiceKey = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue Choice")
    FString displayText = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue Choice")
    bool conditionMet = true;

    FDialogueChoice() {}
};

USTRUCT(BlueprintType)
struct PROTOTYPING_API FDialogueNodeData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue Node")
    FString sessionId = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue Node")
    int32 npcId = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue Node")
    int32 nodeId = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue Node")
    int32 speakerNpcId = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue Node")
    FString clientNodeKey = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue Node")
    FString type = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue Node")
    FString npcText = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue Node")
    TArray<FDialogueChoice> choices;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue Node")
    bool bIsEndNode = false;

    FDialogueNodeData() {}
};

USTRUCT(BlueprintType)
struct PROTOTYPING_API FDialogueErrorData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue Error")
    FString sessionId = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue Error")
    FString errorCode = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue Error")
    FString message = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue Error")
    FString factionSlug = "";

    FDialogueErrorData() {}
};

// ============================================================
// World Notifications
// ============================================================

USTRUCT(BlueprintType)
struct PROTOTYPING_API FWorldNotificationStruct
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Notification")
    FString notificationType = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Notification")
    FString notificationId = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Notification")
    FString channel = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Notification")
    FString priority = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Notification")
    FString text = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Notification")
    int32 characterId = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Notification")
    FString mobSlug = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Notification")
    int32 unlockedTier = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Notification")
    FString categorySlug = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Notification")
    FString zoneSlug = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Notification")
    FString extraData = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Notification")
    TMap<FString, FString> dataFields;

    FWorldNotificationStruct() {}
};

// ============================================================
// Bestiary
// ============================================================

USTRUCT(BlueprintType)
struct PROTOTYPING_API FBestiaryLootEntryStruct
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bestiary Loot")
    FString itemSlug = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bestiary Loot")
    float chance = 0.0f;

    FBestiaryLootEntryStruct() {}
};

USTRUCT(BlueprintType)
struct PROTOTYPING_API FBestiaryTierStruct
{
    GENERATED_BODY()

    // Matches server field "tier"
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bestiary Tier")
    int32 tier = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bestiary Tier")
    FString categorySlug = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bestiary Tier")
    bool unlocked = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bestiary Tier")
    int32 requiredKills = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bestiary Tier")
    int32 requiredKillsLeft = 0;

    // basic_info fields
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bestiary Tier")
    int32 level = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bestiary Tier")
    FString rank = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bestiary Tier")
    int32 hpMin = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bestiary Tier")
    int32 hpMax = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bestiary Tier")
    FString mobType = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bestiary Tier")
    FString biomeSlug = "";

    // lore fields
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bestiary Tier")
    FString loreKey = "";

    // combat_info fields
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bestiary Tier")
    TArray<FString> weaknesses;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bestiary Tier")
    TArray<FString> resistances;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bestiary Tier")
    TArray<FString> abilities;

    // loot_table / drop_rates fields
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bestiary Tier")
    TArray<FString> lootItems;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bestiary Tier")
    TArray<FBestiaryLootEntryStruct> loot;

    // hunter_mastery fields
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bestiary Tier")
    FString titleSlug = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bestiary Tier")
    FString achievementSlug = "";

    FBestiaryTierStruct() {}
};

USTRUCT(BlueprintType)
struct PROTOTYPING_API FBestiaryEntryStruct
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bestiary Entry")
    FString mobSlug = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bestiary Entry")
    int32 killCount = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bestiary Entry")
    TArray<FBestiaryTierStruct> tiers;

    FBestiaryEntryStruct() {}
};

USTRUCT(BlueprintType)
struct PROTOTYPING_API FBestiaryOverviewEntryStruct
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bestiary Overview Entry")
    FString mobSlug = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bestiary Overview Entry")
    int32 killCount = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bestiary Overview Entry")
    int32 highestUnlockedTier = 0;

    FBestiaryOverviewEntryStruct() {}
};

// ============================================================
// Localization DataTable Row Structs
// ============================================================

USTRUCT(BlueprintType)
struct PROTOTYPING_API FQuestDefinition : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest Definition")
    FText displayName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest Definition")
    FText description;

    FQuestDefinition() {}
};

USTRUCT(BlueprintType)
struct PROTOTYPING_API FQuestStepDefinition : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest Step Definition")
    FText description;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest Step Definition")
    FText hint;

    FQuestStepDefinition() {}
};

USTRUCT(BlueprintType)
struct PROTOTYPING_API FDialogueNodeDefinition : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue Node Definition")
    FText nodeText;

    FDialogueNodeDefinition() {}
};

USTRUCT(BlueprintType)
struct PROTOTYPING_API FDialogueChoiceDefinition : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue Choice Definition")
    FText choiceText;

    FDialogueChoiceDefinition() {}
};

USTRUCT(BlueprintType)
struct PROTOTYPING_API FItemLocaleDefinition : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Locale")
    FText displayName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Locale")
    FText description;

    FItemLocaleDefinition() {}
};

USTRUCT(BlueprintType)
struct PROTOTYPING_API FMobLocaleDefinition : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mob Locale")
    FText displayName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mob Locale")
    FText description;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mob Locale")
    FText loreText;

    FMobLocaleDefinition() {}
};

USTRUCT(BlueprintType)
struct PROTOTYPING_API FNPCLocaleDefinition : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC Locale")
    FText displayName;

    FNPCLocaleDefinition() {}
};

USTRUCT(BlueprintType)
struct PROTOTYPING_API FBestiaryCategoryDefinition : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bestiary Category")
    FText categoryTitle;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bestiary Category")
    FText lockedHint;

    FBestiaryCategoryDefinition() {}
};

USTRUCT(BlueprintType)
struct PROTOTYPING_API FZoneLocaleDefinition : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zone Locale")
    FText displayName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zone Locale")
    FText description;

    FZoneLocaleDefinition() {}
};

USTRUCT(BlueprintType)
struct PROTOTYPING_API FNotificationLocaleDefinition : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Notification Locale")
    FText textTemplate;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Notification Locale")
    FText title;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Notification Locale")
    FString iconPath = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Notification Locale")
    FString soundPath = "";

    // Icon soft reference for optional icon display
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Notification Locale")
    TSoftObjectPtr<UTexture2D> Icon;

    FNotificationLocaleDefinition() {}
};

// ============================================================
// Impact Sound Data (for weapon impact ? armor material lookup)
// ============================================================

USTRUCT(BlueprintType)
struct PROTOTYPING_API FImpactSoundData : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact Sound")
    TArray<TSoftObjectPtr<USoundBase>> ImpactSounds;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact Sound")
    TSoftObjectPtr<UNiagaraSystem> ImpactVFX;

    FImpactSoundData() {}
};