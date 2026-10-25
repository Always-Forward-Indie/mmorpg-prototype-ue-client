#include "Gameplay/Skills/PlayerSkillManager.h"
#include "Prototyping.h"
#include "Gameplay/Combat/SkillSystemManager.h"
#include "Gameplay/Skills/SkillDefinitionRepository.h"
#include "Services/TimeSyncService.h"
#include "Utils/NetCore/CooldownFormat.h"
#include "Utils/NetCore/SkillSlotRule.h"

void UPlayerSkillManager::UpdateCooldowns(float DeltaTime)
{
    // Detect time-sync loss: when IsTimeSyncValid() transitions from true to false,
    // server-clock cooldowns (endTime ~1.75B) would be compared against world-time
    // (~70) causing permanent cooldown. Convert them to world-time immediately.
    const bool bTimeSyncNowValid = TimeSyncService && TimeSyncService->IsTimeSyncValid();
    if (bWasTimeSyncValid && !bTimeSyncNowValid)
    {
        ConvertServerClockCooldownsToWorldTime();
    }
    bWasTimeSyncValid = bTimeSyncNowValid;

    for (auto& Pair : PlayerSkills) {
        FPlayerSkillData& S = Pair.Value;
        if (!S.bIsOnCooldown) continue;
        const double now = NowFor(&S);
        if (now >= S.cooldownEndTime) {
            S.bIsOnCooldown = false;
            S.bIsReady = true;
            S.cooldownEndTime = 0.0;
            OnSkillReady.Broadcast(Pair.Key);
        }
    }
}

FPlayerSkillData UPlayerSkillManager::CreatePlayerSkillData(const FPlayerSkillNetworkData& NetworkData) const
{
    FPlayerSkillData SkillData;
    SkillData.networkData = NetworkData;
    
    UE_LOG(LogGameplay, Warning, TEXT("PlayerSkillManager: Creating skill data for %s"), *NetworkData.skillSlug);
    
    // Get definition from repository
    if (DefinitionRepository)
    {
        UE_LOG(LogGameplay, Warning, TEXT("PlayerSkillManager: DefinitionRepository available, getting definition"));
        SkillData.definitionData = DefinitionRepository->GetDefinition(NetworkData.skillSlug);
        
        // Debug the loaded definition
        UE_LOG(LogGameplay, Warning, TEXT("PlayerSkillManager: Loaded definition - DisplayName: %s, Icon IsValid: %s"), 
            *SkillData.definitionData.displayName.ToString(),
            SkillData.definitionData.skillIcon.IsValid() ? TEXT("true") : TEXT("false"));
            
        if (SkillData.definitionData.skillIcon.IsValid())
        {
            UE_LOG(LogGameplay, Warning, TEXT("PlayerSkillManager: Icon path: %s"), 
                *SkillData.definitionData.skillIcon.ToSoftObjectPath().ToString());
        }
    }
    else
    {
        UE_LOG(LogGameplay, Error, TEXT("PlayerSkillManager: DefinitionRepository is NULL"));
        
        // Create basic definition if repository is not available
        SkillData.definitionData.displayName = FText::FromString(NetworkData.skillSlug);
    }

    return SkillData;
}

float UPlayerSkillManager::GetCurrentTime() const
{
    // Use the same time system as SkillSystemManager for consistency
    if (SkillSystemManager)
    {
        return SkillSystemManager->GetSynchronizedWorldTime();
    }
    
    // Fallback to synchronized time if SkillSystemManager is not available
    if (TimeSyncService && TimeSyncService->IsTimeSyncValid())
    {
        int64 ServerTimeMs = TimeSyncService->GetEstimatedServerTimeMs();
        return static_cast<float>(ServerTimeMs) / 1000.0f;
    }
    
    // Last resort fallback to world time
    if (SkillSystemManager)
    {
        return SkillSystemManager->GetWorldTime();
    }
    
    return 0.0f;
}

UWorld* UPlayerSkillManager::GetWorld() const
{
    // Try to get world through SkillSystemManager
    if (SkillSystemManager)
    {
        // We can't directly access private members, so let's try through outer
        if (UObject* Outer = GetOuter())
        {
            return Outer->GetWorld();
        }
    }
    
    return nullptr;
}

void UPlayerSkillManager::UpdateSkillCooldownState(FPlayerSkillData& SkillData)
{
    float CurrentTime = GetCurrentTime();
    // Predicate lives in FCooldownState (pinned by MMO.Skills.CooldownState).
    SkillData.bIsOnCooldown = FCooldownState::IsOnCooldown(CurrentTime, SkillData.cooldownEndTime);
    SkillData.bIsReady = !SkillData.bIsOnCooldown;
}

bool UPlayerSkillManager::ValidateSkillSlot(int32 SlotIndex) const
{
    // Predicate lives in FSkillSlotRule (pinned by MMO.Skills.SlotRule).
    return FSkillSlotRule::IsValidSlot(SlotIndex, MaxSkillSlots);
}

