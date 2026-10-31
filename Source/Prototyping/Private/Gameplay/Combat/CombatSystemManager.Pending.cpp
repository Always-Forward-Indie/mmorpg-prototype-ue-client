#include "Gameplay/Combat/CombatSystemManager.h"
#include "Prototyping.h"
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

void UCombatSystemManager::FlushAllPendingForCaster(int32 CasterId)
{
    FlushPendingResults(CasterId, false);
}

bool UCombatSystemManager::HasPendingResults(int32 CasterId) const
{
    if (const TArray<FPendingResult>* Queue = PendingSkillResults.Find(CasterId))
    {
        return Queue->Num() > 0;
    }
    return false;
}

bool UCombatSystemManager::HoldMobHPForImpact(int32 MobUID, int32 HP, UObject* MobObject)
{
    if (MobUID <= 0 || !IsValid(MobObject)) return false;
    if (!WorldContext || !WorldContext->IsValidLowLevel()) return false;

    FHeldMobHP Held;
    Held.Mob = MobObject;
    Held.HP = HP;
    HeldMobHP.Add(MobUID, Held);

    // (Re)arm the TTL: unclaimed updates (regen, foreign hits without a
    // result, spawn sync) apply here, at most 0.5 s late.
    if (FTimerHandle* Existing = HeldMobHPTimers.Find(MobUID))
    {
        WorldContext->GetTimerManager().ClearTimer(*Existing);
    }
    FTimerHandle TimerHandle;
    TWeakObjectPtr<UCombatSystemManager> WeakSelf(this);
    WorldContext->GetTimerManager().SetTimer(TimerHandle, [WeakSelf, MobUID]()
    {
        if (WeakSelf.IsValid())
        {
            WeakSelf->FlushHeldMobHP(MobUID);
        }
    }, static_cast<float>(HeldMobHPWindowSeconds), false);
    HeldMobHPTimers.Add(MobUID, TimerHandle);
    return true;
}

void UCombatSystemManager::ClaimHeldMobHP(int32 MobUID)
{
    // The hit-point flush carries the same absolute HP from combatResult,
    // so the held copy is simply dropped (and its TTL disarmed).
    HeldMobHP.Remove(MobUID);
    if (FTimerHandle* Handle = HeldMobHPTimers.Find(MobUID))
    {
        if (WorldContext && WorldContext->IsValidLowLevel())
        {
            WorldContext->GetTimerManager().ClearTimer(*Handle);
        }
        HeldMobHPTimers.Remove(MobUID);
    }
}

void UCombatSystemManager::FlushHeldMobHP(int32 MobUID)
{
    FHeldMobHP Held;
    if (!HeldMobHP.RemoveAndCopyValue(MobUID, Held))
    {
        return;
    }
    HeldMobHPTimers.Remove(MobUID);

    UObject* TargetObject = Held.Mob.Get();
    if (!IsValid(TargetObject)) return;
    // Never resurrect: a stale held HP must not raise a corpse
    // (spawn/revive owns revival; Die() pins 0).
    if (ICombatable::Execute_IsDead(TargetObject) && Held.HP > 0) return;
    ICombatable::Execute_SetCurrentHealth(TargetObject, Held.HP);
}

void UCombatSystemManager::FlushPendingResults(int32 CasterId, bool bSkipProjectileWaiters)
{
    TArray<FPendingResult>* Queue = PendingSkillResults.Find(CasterId);
    if (!Queue || Queue->Num() == 0)
    {
        UE_LOG(LogGameplay, Verbose, TEXT("CombatSystemManager: No pending results for caster %d"), CasterId);
        return;
    }

    // Separate results that should be applied now from those that must wait for the projectile.
    TArray<FPendingResult> ResultsToApply;
    TArray<FPendingResult> ResultsToRequeue;

    for (FPendingResult& Pending : *Queue)
    {
        if (bSkipProjectileWaiters && Pending.bWaitsForProjectile)
        {
            ResultsToRequeue.Add(Pending);
        }
        else
        {
            ResultsToApply.Add(Pending);
        }
    }

    // Replace (or remove) the queue before processing to prevent re-entrancy issues.
    PendingSkillResults.Remove(CasterId);
    if (ResultsToRequeue.Num() > 0)
    {
        PendingSkillResults.Add(CasterId, ResultsToRequeue);
    }
    else
    {
        // Queue fully drained — cancel the safety timer so it cannot accidentally
        // flush results from a future attack cycle that queues under the same CasterId.
        if (FTimerHandle* Handle = PendingResultTimerHandles.Find(CasterId))
        {
            if (WorldContext)
            {
                WorldContext->GetTimerManager().ClearTimer(*Handle);
            }
            PendingResultTimerHandles.Remove(CasterId);
        }
    }

    if (ResultsToApply.Num() == 0)
    {
        UE_LOG(LogGameplay, Verbose, TEXT("CombatSystemManager: All %d pending result(s) for caster %d are waiting for projectile impact - not flushing"),
            ResultsToRequeue.Num(), CasterId);
        return;
    }

    if (ResultsToApply.Num() > 1)
    {
        UE_LOG(LogGameplay, Warning, TEXT("CombatSystemManager: Flushing %d pending results for caster %d - possible double-attack detected, applying all to avoid lost damage"),
            ResultsToApply.Num(), CasterId);
    }

    UE_LOG(LogGameplay, Warning, TEXT("CombatSystemManager: Flushing %d pending result(s) for caster %d (%d requeued as projectile-waiters)"),
        ResultsToApply.Num(), CasterId, ResultsToRequeue.Num());

    for (const FPendingResult& Pending : ResultsToApply)
    {
        const FSkillResultData& SkillResult = Pending.ResultData;

        // Find target � use MapNetCasterType for robust string-first mapping
        ECasterType TargetType = MapNetCasterType(SkillResult.targetType, SkillResult.targetTypeString);
        TScriptInterface<ICombatable> Target = FindCombatableById(SkillResult.targetId, TargetType);

        if (Target.GetInterface() && Target.GetObject() && IsValid(Target.GetObject()))
        {
            UObject* TargetObject = Target.GetObject();

            ApplySkillEffects(SkillResult, Target);

            // Check for death
            if (SkillResult.targetDied || ICombatable::Execute_IsDead(TargetObject))
            {
                OnActorDied.Broadcast(ICombatable::Execute_GetActorId(TargetObject));
            }
        }
        else
        {
            UE_LOG(LogGameplay, Warning, TEXT("CombatSystemManager: Could not find target %d (%s) for deferred result"),
                SkillResult.targetId, *SkillResult.targetTypeString);
        }

        // Broadcast event
        OnSkillCompleted.Broadcast(SkillResult);
    }
}

void UCombatSystemManager::StartPendingResultTimeout(int32 CasterId)
{
    // Prefer the stored WorldContext; fall back to GEngine->GetWorldContexts() if stale.
    UWorld* World = WorldContext;
    if (!World || !World->IsValidLowLevel())
    {
        if (GEngine && GEngine->GetWorldContexts().Num() > 0)
        {
            for (const FWorldContext& Ctx : GEngine->GetWorldContexts())
            {
                if (Ctx.World() && Ctx.WorldType == EWorldType::Game)
                {
                    World = Ctx.World();
                    WorldContext = World;
                    break;
                }
            }
        }
    }
    if (!World) return;

    // Cancel any existing safety timer for this caster to prevent stale timers
    // from accidentally flushing results of a future attack cycle.
    if (FTimerHandle* ExistingHandle = PendingResultTimerHandles.Find(CasterId))
    {
        World->GetTimerManager().ClearTimer(*ExistingHandle);
    }

    FTimerHandle TimerHandle;
    TWeakObjectPtr<UCombatSystemManager> WeakSelf(this);
    World->GetTimerManager().SetTimer(TimerHandle, [WeakSelf, CasterId]()
    {
        CRASH_GUARD("CombatSystem::TimeoutResult");

        if (WeakSelf.IsValid())
        {
            // Clean up the stored handle
            WeakSelf->PendingResultTimerHandles.Remove(CasterId);

            // Only flush if results are still pending (hit-point may have already fired)
            if (WeakSelf->PendingSkillResults.Contains(CasterId))
            {
                UE_LOG(LogGameplay, Warning,
                    TEXT("CombatSystemManager: Safety timeout - flushing stale pending results for caster %d"),
                    CasterId);
                // bSkipProjectileWaiters = false: the timeout is the last resort, flush everything.
                WeakSelf->FlushPendingResults(CasterId, /*bSkipProjectileWaiters=*/false);
            }
        }
    }, static_cast<float>(PendingResultTimeoutSeconds), false);

    PendingResultTimerHandles.Add(CasterId, TimerHandle);
}
