#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Data/DataStructs.h"
#include "SkillSystemManager.generated.h"

// Forward declarations
class UCombatSystemManager;
class UNetworkManager;
class UTimeSyncService;

// Skill data structure
USTRUCT(BlueprintType)
struct FSkillData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
    FString skillSlug = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
    FString skillName = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
    FString animationName = "";

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
    float castTime = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
    float cooldown = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
    int32 manaCost = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
    ESkillEffectType effectType = ESkillEffectType::None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
    ESkillSchool school = ESkillSchool::None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
    float range = 0.0f;

    FSkillData()
    {
        skillSlug = "";
        skillName = "";
        animationName = "";
        castTime = 0.0f;
        cooldown = 0.0f;
        manaCost = 0;
        effectType = ESkillEffectType::None;
        school = ESkillSchool::None;
        range = 0.0f;
    }
};

/**
 * Manages skill execution, cooldowns, and casting
 * Separate from combat system to handle skill-specific logic
 * Uses TimeSyncService for accurate cooldown calculations
 */
UCLASS(BlueprintType)
class PROTOTYPING_API USkillSystemManager : public UObject
{
    GENERATED_BODY()

public:
    USkillSystemManager();

    // Initialization
    UFUNCTION(BlueprintCallable, Category = "Skill System")
    void Initialize(UCombatSystemManager* CombatManager, UNetworkManager* NetworkManager);

    // Skill management
    UFUNCTION(BlueprintCallable, Category = "Skill System")
    void RegisterSkill(const FString& SkillSlug, const FSkillData& SkillData);

    UFUNCTION(BlueprintCallable, Category = "Skill System")
    bool HasSkill(const FString& SkillSlug) const;

    UFUNCTION(BlueprintCallable, Category = "Skill System")
    FSkillData GetSkillData(const FString& SkillSlug) const;

    // Skill execution
    UFUNCTION(BlueprintCallable, Category = "Skill System")
    bool CanCastSkill(int32 CasterId, const FString& SkillSlug) const;

    UFUNCTION(BlueprintCallable, Category = "Skill System")
    bool CastSkill(int32 CasterId, int32 TargetId, const FString& SkillSlug, ECasterType TargetType);

    // Cooldown management
    UFUNCTION(BlueprintCallable, Category = "Skill System")
    bool IsSkillOnCooldown(int32 CasterId, const FString& SkillSlug) const;

    UFUNCTION(BlueprintCallable, Category = "Skill System")
    float GetSkillCooldownRemaining(int32 CasterId, const FString& SkillSlug) const;

    UFUNCTION(BlueprintCallable, Category = "Skill System")
    void StartSkillCooldown(int32 CasterId, const FString& SkillSlug);

    // Resource checking
    UFUNCTION(BlueprintCallable, Category = "Skill System")
    bool HasSufficientMana(int32 CasterId, const FString& SkillSlug) const;

protected:
    // Validate skill casting conditions
    bool ValidateSkillCast(int32 CasterId, int32 TargetId, const FString& SkillSlug, ECasterType TargetType);

private:
    UPROPERTY()
    UCombatSystemManager* CombatSystemManager;

    UPROPERTY()
    UNetworkManager* NetworkManager;

    // Skill database
    UPROPERTY()
    TMap<FString, FSkillData> RegisteredSkills;

    // Cooldown tracking - maps CasterId -> SkillSlug -> EndTime
    TMap<int32, TMap<FString, float>> SkillCooldowns;

    // Helper to get current world time
    float GetWorldTime() const;

    // Helper to get server-synchronized time for accurate cooldown calculations
    float GetSynchronizedWorldTime() const;

    // Helper to get TimeSyncService instance
    UTimeSyncService* GetTimeSyncService() const;
};