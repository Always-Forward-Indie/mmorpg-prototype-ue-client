#include "Gameplay/Combat/CombatSystemManager.h"
#include "Gameplay/Combat/ISkillEffectHandler.h"
#include "Gameplay/Combat/ICombatable.h"
#include "Gameplay/UI/FloatingCombatTextManager.h"
#include "Gameplay/Skills/SkillDefinitionRepository.h"
#include "UI/UIManager.h"
#include "MyGameInstance.h"
#include "Gameplay/Skills/PlayerSkillManager.h"
#include "Networking/NetworkManager.h"
#include "Utils/JSONParser.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "CrashDiagnostics.h"

void UCombatSystemManager::SendAttackRequest(int32 AttackerId, int32 TargetId, const FString& SkillSlug, ECasterType TargetType)
{
    if (!GameInstance || !NetworkManager)
    {
        UE_LOG(LogTemp, Error, TEXT("CombatSystemManager: Cannot send attack request - missing dependencies"));
        return;
    }

    // Create JSON request
    TMap<FString, TSharedPtr<FJsonValue>> HeaderData;
    TMap<FString, TSharedPtr<FJsonValue>> BodyData;

    // Add client authentication
    HeaderData.Add(TEXT("clientId"), MakeShareable(new FJsonValueNumber(GameInstance->GetCurrentClientID())));
    HeaderData.Add(TEXT("hash"), MakeShareable(new FJsonValueString(GameInstance->GetCurrentClientHash())));

    // Add attack data
    BodyData.Add(TEXT("attackerId"), MakeShareable(new FJsonValueNumber(AttackerId)));
    BodyData.Add(TEXT("targetId"), MakeShareable(new FJsonValueNumber(TargetId)));
    BodyData.Add(TEXT("skillSlug"), MakeShareable(new FJsonValueString(SkillSlug)));
    BodyData.Add(TEXT("targetType"), MakeShareable(new FJsonValueNumber(static_cast<int32>(TargetType))));

    // Use TimeSyncService for automatic clientSendMs
    FString JsonString = JSONParser::SerializeJsonWithTimeSync(TEXT("playerAttack"), HeaderData, BodyData, GameInstance->GetTimeSyncService());
    NetworkManager->SendDataToChunkServer(JsonString);

    UE_LOG(LogTemp, Log, TEXT("CombatSystemManager: Sent attack request - Attacker: %d, Target: %d, Skill: %s"), 
        AttackerId, TargetId, *SkillSlug);
}

void UCombatSystemManager::SendSkillUsageRequest(int32 TargetId, const FString& SkillSlug, ECasterType TargetType)
{
    if (!GameInstance || !NetworkManager)
    {
        UE_LOG(LogTemp, Error, TEXT("CombatSystemManager: Cannot send skill usage request - missing dependencies"));
        return;
    }

    TMap<FString, TSharedPtr<FJsonValue>> HeaderData;
    TMap<FString, TSharedPtr<FJsonValue>> BodyData;

    // Add client authentication
    HeaderData.Add(TEXT("clientId"), MakeShareable(new FJsonValueNumber(GameInstance->GetCurrentClientID())));
    HeaderData.Add(TEXT("hash"), MakeShareable(new FJsonValueString(GameInstance->GetCurrentClientHash())));

    // skillUsage body: skillSlug, targetId, targetType (no attackerId — server knows from clientId)
    BodyData.Add(TEXT("skillSlug"), MakeShareable(new FJsonValueString(SkillSlug)));
    BodyData.Add(TEXT("targetId"), MakeShareable(new FJsonValueNumber(TargetId)));
    BodyData.Add(TEXT("targetType"), MakeShareable(new FJsonValueNumber(static_cast<int32>(TargetType))));

    FString JsonString = JSONParser::SerializeJsonWithTimeSync(TEXT("skillUsage"), HeaderData, BodyData, GameInstance->GetTimeSyncService());
    NetworkManager->SendDataToChunkServer(JsonString);

    UE_LOG(LogTemp, Log, TEXT("CombatSystemManager: Sent skillUsage request - Skill: %s, Target: %d, TargetType: %d"),
        *SkillSlug, TargetId, static_cast<int32>(TargetType));
}

TScriptInterface<ISkillEffectHandler> UCombatSystemManager::FindEffectHandler(ESkillEffectType EffectType)
{
    // Iterate forward: handlers are stored highest-priority-first (index 0 = highest)
    for (int32 i = 0; i < EffectHandlers.Num(); i++)
    {
        FEffectHandlerEntry& Entry = EffectHandlers[i];

        if (!Entry.WeakObject.IsValid())
        {
            UE_LOG(LogTemp, Warning, TEXT("CombatSystemManager: Removing stale handler at index %d during search"), i);
            EffectHandlers.RemoveAt(i);
            i--;
            continue;
        }

        UObject* HandlerObject = Entry.WeakObject.Get();

        if (ISkillEffectHandler::Execute_CanHandle(HandlerObject, EffectType))
        {
            TScriptInterface<ISkillEffectHandler> Result;
            Result.SetObject(HandlerObject);
            Result.SetInterface(Entry.Interface);
            return Result;
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("CombatSystemManager: No valid handler found for effect type: %s"),
        *UEnum::GetValueAsString(EffectType));
    return TScriptInterface<ISkillEffectHandler>();
}

void UCombatSystemManager::ApplySkillEffects(const FSkillResultData& SkillResult, const TScriptInterface<ICombatable>& Target)
{
    if (!Target.GetInterface() || !Target.GetObject() || !IsValid(Target.GetObject()))
    {
        UE_LOG(LogTemp, Error, TEXT("CombatSystemManager: Cannot apply skill effects to invalid target"));
        return;
    }

    // ����� � ������� ��������������� �������
    TScriptInterface<ISkillEffectHandler> Handler = FindEffectHandler(SkillResult.skillEffectType);
    if (Handler.GetInterface() && Handler.GetObject() && IsValid(Handler.GetObject()))
    {
        UObject* HandlerObject = Handler.GetObject();

        // ������� ��������� �������� �� ���������� ��������
        ISkillEffectHandler::Execute_ProcessSkillResult(HandlerObject, SkillResult, Target);

        UE_LOG(LogTemp, Verbose, TEXT("CombatSystemManager: Successfully processed skill effect %s via handler"),
            *UEnum::GetValueAsString(SkillResult.skillEffectType));
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("CombatSystemManager: No handler found for effect type: %s"),
            *UEnum::GetValueAsString(SkillResult.skillEffectType));
    }
}


void UCombatSystemManager::LogCombatEvent(const FString& Event, const FString& Details)
{
    UE_LOG(LogTemp, Warning, TEXT("CombatSystemManager [%s]: %s"), *Event, *Details);
}

FString UCombatSystemManager::CreateCombatableKey(int32 ActorId, ECasterType ActorType) const
{
    return FString::Printf(TEXT("%d_%d"), ActorId, static_cast<int32>(ActorType));
}

void UCombatSystemManager::NotifyHitPoint(int32 CasterId)
{
    UE_LOG(LogTemp, Warning, TEXT("CombatSystemManager: HitPoint notify from caster %d (animation path - skipping projectile waiters)"), CasterId);
    // bSkipProjectileWaiters = true: do not flush results that are waiting for a
    // projectile to physically land.  Prevents a parallel auto-attack's hit-point
    // from consuming the pending projectile result early.
    FlushPendingResults(CasterId, /*bSkipProjectileWaiters=*/true);
}

void UCombatSystemManager::NotifyProjectileImpact(int32 CasterId)
{
    UE_LOG(LogTemp, Warning, TEXT("CombatSystemManager: Projectile impact for caster %d - flushing all pending results"), CasterId);

    // Check BEFORE flushing whether a projectile-waiter result is already queued.
    // We need this to decide whether to store an early-impact marker after the flush.
    // We cannot check AFTER the flush because FlushPendingResults empties the queue
    // even when it successfully applies results — causing a spurious marker.
    bool bHadProjectileWaiter = false;
    if (TArray<FPendingResult>* Queue = PendingSkillResults.Find(CasterId))
    {
        for (const FPendingResult& R : *Queue)
        {
            if (R.bWaitsForProjectile)
            {
                bHadProjectileWaiter = true;
                break;
            }
        }
    }

    // bSkipProjectileWaiters = false: the projectile has arrived, apply everything.
    FlushPendingResults(CasterId, /*bSkipProjectileWaiters=*/false);

    // If no projectile-waiter was queued, combatResult hasn't arrived yet.
    // Record the impact time so ProcessSkillResult can apply it immediately
    // when it arrives, instead of waiting 12 seconds for the safety timeout.
    if (!bHadProjectileWaiter)
    {
        double ImpactTime = WorldContext ? WorldContext->GetTimeSeconds() : 0.0;
        EarlyProjectileImpacts.Add(CasterId, ImpactTime);
        UE_LOG(LogTemp, Warning,
            TEXT("CombatSystemManager: Projectile hit but no pending result yet for caster %d - stored early-impact marker (t=%.3f)"),
            CasterId, ImpactTime);
    }
}

