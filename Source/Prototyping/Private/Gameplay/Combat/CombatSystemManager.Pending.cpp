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

void UCombatSystemManager::FlushPendingResults(int32 CasterId, bool bSkipProjectileWaiters)
{
    TArray<FPendingResult>* Queue = PendingSkillResults.Find(CasterId);
    if (!Queue || Queue->Num() == 0)
    {
        UE_LOG(LogTemp, Log, TEXT("CombatSystemManager: No pending results for caster %d"), CasterId);
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
        UE_LOG(LogTemp, Log, TEXT("CombatSystemManager: All %d pending result(s) for caster %d are waiting for projectile impact - not flushing"),
            ResultsToRequeue.Num(), CasterId);
        return;
    }

    if (ResultsToApply.Num() > 1)
    {
        UE_LOG(LogTemp, Warning, TEXT("CombatSystemManager: Flushing %d pending results for caster %d - possible double-attack detected, applying all to avoid lost damage"),
            ResultsToApply.Num(), CasterId);
    }

    UE_LOG(LogTemp, Warning, TEXT("CombatSystemManager: Flushing %d pending result(s) for caster %d (%d requeued as projectile-waiters)"),
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
            UE_LOG(LogTemp, Warning, TEXT("CombatSystemManager: Could not find target %d (%s) for deferred result"),
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
                UE_LOG(LogTemp, Warning,
                    TEXT("CombatSystemManager: Safety timeout - flushing stale pending results for caster %d"),
                    CasterId);
                // bSkipProjectileWaiters = false: the timeout is the last resort, flush everything.
                WeakSelf->FlushPendingResults(CasterId, /*bSkipProjectileWaiters=*/false);
            }
        }
    }, static_cast<float>(PendingResultTimeoutSeconds), false);

    PendingResultTimerHandles.Add(CasterId, TimerHandle);
}
