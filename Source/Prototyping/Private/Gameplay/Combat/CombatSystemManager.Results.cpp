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

void UCombatSystemManager::ProcessSkillResult(const FSkillResultData& SkillResult)
{
    LogCombatEvent("Skill Result (DEFERRED)", 
        FString::Printf(TEXT("Skill: %s, Caster: %d, Target: %d (%s), Effect: %s, Damage: %d, Success: %s"), 
            *SkillResult.skillName, SkillResult.casterId, SkillResult.targetId, *SkillResult.targetTypeString,
            *UEnum::GetValueAsString(SkillResult.skillEffectType),
            SkillResult.damage,
            SkillResult.success ? TEXT("true") : TEXT("false")));

    // Treat failed results with "out of range" as a miss — the projectile hit
    // but the target was too far at execution time. Show "Miss" floating text.
    FSkillResultData Result = SkillResult;
    if (!Result.success && Result.errorReason.Contains(TEXT("out of range")))
    {
        Result.isMissed = true;
        UE_LOG(LogTemp, Warning, TEXT("CombatSystemManager: Skill %s failed (out of range) — treating as miss"), *Result.skillName);
    }

    // --- Update caster mana immediately (UI responsiveness) ---
    if (Result.finalCasterMana > 0 && GameInstance)
    {
        ECasterType CasterCombatType = MapNetCasterType(Result.casterType, Result.casterTypeString);
        TScriptInterface<ICombatable> Caster = FindCombatableById(Result.casterId, CasterCombatType);
        if (Caster.GetInterface() && Caster.GetObject() && IsValid(Caster.GetObject()))
        {
            ICombatable::Execute_SetCurrentMana(Caster.GetObject(), Result.finalCasterMana);
        }
    }

    // --- Heal results from item use (potions/elixirs) have no attack animation to sync with.
    // Apply them immediately so the floating text appears right away instead of after the
    // 12-second safety timeout or the next swing's hit-point notify.
    if (Result.skillEffectType == ESkillEffectType::Healing && GameInstance)
    {
        if (USkillDefinitionRepository* Repo = GameInstance->GetSkillDefinitionRepository())
        {
            const FString& LookupKey = Result.skillSlug.IsEmpty() ? Result.skillName : Result.skillSlug;
            if (!Repo->HasDefinition(LookupKey))
            {
                // Slug not in the skill DataTable → this is an item-based heal, not a cast spell.
                // Find the target and apply effects without queueing.
                ECasterType TargetCombatType = MapNetCasterType(Result.targetType, Result.targetTypeString);
                TScriptInterface<ICombatable> Target = FindCombatableById(Result.targetId, TargetCombatType);
                if (Target.GetInterface() && Target.GetObject() && IsValid(Target.GetObject()))
                {
                    UE_LOG(LogTemp, Log, TEXT("CombatSystemManager: Applying item heal for '%s' immediately (no anim sync needed)"), *LookupKey);
                    ApplySkillEffects(Result, Target);
                    OnSkillCompleted.Broadcast(Result);
                }
                return;
            }
        }
    }

    // --- Store the result for deferred application at the animation hit-point ---
    FPendingResult Pending;
    Pending.ResultData         = Result;
    Pending.StoredAtWorldTime  = WorldContext ? WorldContext->GetTimeSeconds() : 0.0;

    // Mark results for projectile skills so that ordinary animation hit-point notifies
    // (e.g. from a parallel auto-attack) cannot flush them prematurely.
    // Only the projectile actor itself (via NotifyProjectileImpact) may flush these.
    if (GameInstance)
    {
        if (USkillDefinitionRepository* Repo = GameInstance->GetSkillDefinitionRepository())
        {
            const FString& LookupKey = Result.skillSlug.IsEmpty() ? Result.skillName : Result.skillSlug;
            Pending.bWaitsForProjectile = !Repo->GetDefinition(LookupKey).projectileClass.IsNull();
            if (Pending.bWaitsForProjectile)
            {
                UE_LOG(LogTemp, Warning,
                    TEXT("CombatSystemManager: Marking result for caster %d skill '%s' as bWaitsForProjectile"),
                    Result.casterId, *LookupKey);
            }
        }
    }

    TArray<FPendingResult>& Queue = PendingSkillResults.FindOrAdd(Result.casterId);
    Queue.Add(MoveTemp(Pending));

    UE_LOG(LogTemp, Log, TEXT("CombatSystemManager: Queued pending result for caster %d (%d in queue)"),
        Result.casterId, Queue.Num());

    // Check if the projectile already landed before this result arrived (server was slow).
    // In that case apply immediately and clear the early-impact marker.
    if (Pending.bWaitsForProjectile)
    {
        if (const double* ImpactTime = EarlyProjectileImpacts.Find(Result.casterId))
        {
            const double NowTime = WorldContext ? WorldContext->GetTimeSeconds() : 0.0;
            if ((NowTime - *ImpactTime) < EarlyImpactWindowSeconds)
            {
                UE_LOG(LogTemp, Warning,
                    TEXT("CombatSystemManager: Early-impact marker found for caster %d (%.3fs ago) - flushing immediately"),
                    Result.casterId, NowTime - *ImpactTime);
                EarlyProjectileImpacts.Remove(Result.casterId);
                FlushPendingResults(Result.casterId, /*bSkipProjectileWaiters=*/false);
                return;
            }
            else
            {
                // Stale marker (shouldn't happen in practice), clean it up
                UE_LOG(LogTemp, Warning,
                    TEXT("CombatSystemManager: Stale early-impact marker for caster %d (%.3fs ago) - ignoring"),
                    Result.casterId, NowTime - *ImpactTime);
                EarlyProjectileImpacts.Remove(Result.casterId);
            }
        }
    }

    // Start a safety-timeout so results are applied even if the anim notify never fires
    StartPendingResultTimeout(Result.casterId);
}

TScriptInterface<ICombatable> UCombatSystemManager::FindCombatableById(int32 ActorId, ECasterType ActorType)
{
    if (ActorId <= 0 || ActorType == ECasterType::None)
    {
        UE_LOG(LogTemp, Warning, TEXT("CombatSystemManager: Invalid search parameters - ActorId: %d, ActorType: %d"), ActorId, (int32)ActorType);
        return TScriptInterface<ICombatable>();
    }

    // Helper lambda that looks up a single key and returns a valid interface or empty.
    auto TryFindByKey = [this, ActorId](const FString& Key) -> TScriptInterface<ICombatable>
    {
        if (FCombatableEntry* Found = RegisteredCombatables.Find(Key))
        {
            if (!Found->WeakObject.IsValid())
            {
                UE_LOG(LogTemp, Warning, TEXT("CombatSystemManager: Stale combatable for ID %d, removing"), ActorId);
                RegisteredCombatables.Remove(Key);
                return TScriptInterface<ICombatable>();
            }

            UObject* CombatableObject = Found->WeakObject.Get();

            if (ICombatable::Execute_GetActorId(CombatableObject) != ActorId)
            {
                UE_LOG(LogTemp, Error, TEXT("CombatSystemManager: Actor ID mismatch - expected %d, got %d"),
                    ActorId, ICombatable::Execute_GetActorId(CombatableObject));
                RegisteredCombatables.Remove(Key);
                return TScriptInterface<ICombatable>();
            }

            TScriptInterface<ICombatable> Result;
            Result.SetObject(CombatableObject);
            Result.SetInterface(Found->Interface);
            return Result;
        }
        return TScriptInterface<ICombatable>();
    };

    // Primary lookup
    FString Key = CreateCombatableKey(ActorId, ActorType);
    TScriptInterface<ICombatable> Result = TryFindByKey(Key);
    if (Result.GetInterface())
    {
        return Result;
    }

    // Fallback: the server sometimes sends a mismatched targetTypeString (e.g.
    // "PLAYER" for a MOB target). Try the other common registration types so
    // combat still works even when the string field is wrong.
    static const ECasterType FallbackTypes[] = { ECasterType::Mob, ECasterType::Player, ECasterType::Self };
    for (ECasterType Fallback : FallbackTypes)
    {
        if (Fallback == ActorType) continue; // already tried
        FString FallbackKey = CreateCombatableKey(ActorId, Fallback);
        Result = TryFindByKey(FallbackKey);
        if (Result.GetInterface())
        {
            UE_LOG(LogTemp, Warning, TEXT("CombatSystemManager: Found actor %d via fallback type %d (requested %d)"),
                ActorId, (int32)Fallback, (int32)ActorType);
            return Result;
        }
    }

    return TScriptInterface<ICombatable>();
}

