#include "Gameplay/Combat/SkillSystemManager.h"
#include "Gameplay/Combat/CombatSystemManager.h"
#include "Gameplay/Combat/ICombatable.h"
#include "Networking/NetworkManager.h"
#include "Engine/World.h"
#include "Services/TimeSyncService.h"
#include "MyGameInstance.h"
#include "Gameplay/Skills/PlayerSkillManager.h" // Add include for new system

USkillSystemManager::USkillSystemManager()
{
    CombatSystemManager = nullptr;
    NetworkManager = nullptr;
    PlayerSkillManager = nullptr; // Initialize new field
}

void USkillSystemManager::Initialize(UCombatSystemManager* CombatManager, UNetworkManager* InNetworkManager)
{
    CombatSystemManager = CombatManager;
    NetworkManager = InNetworkManager;

    // Register default skills
    FSkillData BasicAttack;
    BasicAttack.skillSlug = "basic_attack";
    BasicAttack.skillName = "Basic Attack";
    BasicAttack.animationName = "skill_basic_attack";
    BasicAttack.castTime = 0.0f;
    BasicAttack.cooldown = 1.0f;
    BasicAttack.manaCost = 0;
    BasicAttack.effectType = ESkillEffectType::Damage;
    BasicAttack.school = ESkillSchool::Physical;
    BasicAttack.range = 200.0f;
    RegisterSkill(BasicAttack.skillSlug, BasicAttack);

    UE_LOG(LogTemp, Warning, TEXT("SkillSystemManager: Initialized with default skills"));
}

// Add setter for PlayerSkillManager integration
void USkillSystemManager::SetPlayerSkillManager(UPlayerSkillManager* InPlayerSkillManager)
{
    PlayerSkillManager = InPlayerSkillManager;
    UE_LOG(LogTemp, Log, TEXT("SkillSystemManager: PlayerSkillManager integration enabled"));
}

void USkillSystemManager::RegisterSkill(const FString& SkillSlug, const FSkillData& SkillData)
{
    RegisteredSkills.Add(SkillSlug, SkillData);
    UE_LOG(LogTemp, Log, TEXT("SkillSystemManager: Registered skill %s (%s)"), 
        *SkillSlug, *SkillData.skillName);
}

bool USkillSystemManager::HasSkill(const FString& SkillSlug) const
{
    return RegisteredSkills.Contains(SkillSlug);
}

FSkillData USkillSystemManager::GetSkillData(const FString& SkillSlug) const
{
    if (const FSkillData* Found = RegisteredSkills.Find(SkillSlug))
    {
        return *Found;
    }
    return FSkillData(); // Return empty skill data
}

bool USkillSystemManager::CanCastSkill(int32 CasterId, const FString& SkillSlug) const
{
    // Check if skill exists
    if (!HasSkill(SkillSlug))
    {
        UE_LOG(LogTemp, Warning, TEXT("SkillSystemManager: Skill %s not found"), *SkillSlug);
        return false;
    }

    // Check cooldown
    if (IsSkillOnCooldown(CasterId, SkillSlug))
    {
        UE_LOG(LogTemp, Log, TEXT("SkillSystemManager: Skill %s is on cooldown for caster %d"), 
            *SkillSlug, CasterId);
        return false;
    }

    // Check mana
    if (!HasSufficientMana(CasterId, SkillSlug))
    {
        UE_LOG(LogTemp, Log, TEXT("SkillSystemManager: Insufficient mana for skill %s"), *SkillSlug);
        return false;
    }

    return true;
}

bool USkillSystemManager::CastSkill(int32 CasterId, int32 TargetId, const FString& SkillSlug, ECasterType TargetType)
{
    if (!CanCastSkill(CasterId, SkillSlug))
    {
        return false;
    }

    if (!ValidateSkillCast(CasterId, TargetId, SkillSlug, TargetType))
    {
        return false;
    }

    // Send skill request to server via combat system
    if (CombatSystemManager)
    {
        // basic_attack uses playerAttack event type; all other skills use skillUsage
        if (SkillSlug.Equals(TEXT("basic_attack"), ESearchCase::IgnoreCase))
        {
            CombatSystemManager->SendAttackRequest(CasterId, TargetId, SkillSlug, TargetType);
        }
        else
        {
            CombatSystemManager->SendSkillUsageRequest(TargetId, SkillSlug, TargetType);
        }
        UE_LOG(LogTemp, Log, TEXT("SkillSystemManager: Sent skill request %s from %d to %d (type=%d)"), 
            *SkillSlug, CasterId, TargetId, static_cast<int32>(TargetType));
        return true;
    }

    UE_LOG(LogTemp, Error, TEXT("SkillSystemManager: Combat system manager not available"));
    return false;
}

bool USkillSystemManager::IsSkillOnCooldown(int32 CasterId, const FString& SkillSlug) const
{
    if (const TMap<FString, float>* CasterCooldowns = SkillCooldowns.Find(CasterId))
    {
        if (const float* EndTime = CasterCooldowns->Find(SkillSlug))
        {
            // Use server-synchronized time for more accurate cooldown checking
            float CurrentTime = GetSynchronizedWorldTime();
            return CurrentTime < *EndTime;
        }
    }
    return false;
}

float USkillSystemManager::GetSkillCooldownRemaining(int32 CasterId, const FString& SkillSlug) const
{
    if (const TMap<FString, float>* CasterCooldowns = SkillCooldowns.Find(CasterId))
    {
        if (const float* EndTime = CasterCooldowns->Find(SkillSlug))
        {
            // Use server-synchronized time for accurate remaining time calculation
            float CurrentTime = GetSynchronizedWorldTime();
            float Remaining = *EndTime - CurrentTime;
            return FMath::Max(0.0f, Remaining);
        }
    }
    return 0.0f;
}

void USkillSystemManager::StartSkillCooldown(int32 CasterId, const FString& SkillSlug)
{
    const FSkillData SkillData = GetSkillData(SkillSlug);
    if (SkillData.cooldown > 0.0f)
    {
        // Use server-synchronized time for cooldown start
        float CurrentTime = GetSynchronizedWorldTime();
        float EndTime = CurrentTime + SkillData.cooldown;
        
        if (!SkillCooldowns.Contains(CasterId))
        {
            SkillCooldowns.Add(CasterId, TMap<FString, float>());
        }
        
        SkillCooldowns[CasterId].Add(SkillSlug, EndTime);
        
        UE_LOG(LogTemp, Log, TEXT("SkillSystemManager: Started cooldown for %s (%.1fs) at synchronized time %.3f"), 
            *SkillSlug, SkillData.cooldown, CurrentTime);
    }
}

bool USkillSystemManager::HasSufficientMana(int32 CasterId, const FString& SkillSlug) const
{
    const FSkillData SkillData = GetSkillData(SkillSlug);
    
    if (SkillData.manaCost <= 0)
    {
        return true; // No mana cost
    }

    // Would need to check caster's mana through combat system
    // For now, assume sufficient mana
    return true;
}

bool USkillSystemManager::ValidateSkillCast(int32 CasterId, int32 TargetId, const FString& SkillSlug, ECasterType TargetType)
{
    // Basic validation - could be expanded with range checks, line of sight, etc.
    if (CasterId <= 0 || TargetId <= 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("SkillSystemManager: Invalid caster or target ID"));
        return false;
    }

    // Additional validation could include:
    // - Range checking
    // - Line of sight
    // - Friendly fire rules
    // - Target validity (alive, not immune, etc.)

    return true;
}

float USkillSystemManager::GetWorldTime() const
{
    if (CombatSystemManager && CombatSystemManager->GetWorld())
    {
        return CombatSystemManager->GetWorld()->GetTimeSeconds();
    }
    return 0.0f;
}

float USkillSystemManager::GetSynchronizedWorldTime() const
{
    // Try to get synchronized server time for more accurate calculations
    UTimeSyncService* TimeSyncService = GetTimeSyncService();
    if (TimeSyncService && TimeSyncService->IsTimeSyncValid())
    {
        // Convert server time to local time equivalent for consistency with existing cooldown system
        int64 ServerTimeMs = TimeSyncService->GetEstimatedServerTimeMs();
        return static_cast<float>(ServerTimeMs) / 1000.0f;
    }
    
    // Fallback to local world time if sync service is not available
    return GetWorldTime();
}

UTimeSyncService* USkillSystemManager::GetTimeSyncService() const
{
    if (CombatSystemManager && CombatSystemManager->GetWorld())
    {
        UMyGameInstance* GameInstance = Cast<UMyGameInstance>(CombatSystemManager->GetWorld()->GetGameInstance());
        if (GameInstance)
        {
            return GameInstance->GetTimeSyncService();
        }
    }
    return nullptr;
}