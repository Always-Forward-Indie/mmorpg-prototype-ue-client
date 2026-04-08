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

UCombatSystemManager::UCombatSystemManager()
{
    WorldContext = nullptr;
    GameInstance = nullptr;
    NetworkManager = nullptr;
}

void UCombatSystemManager::Initialize(UMyGameInstance* InGameInstance, UNetworkManager* InNetworkManager)
{
    GameInstance = InGameInstance;
    NetworkManager = InNetworkManager;

    // Clean up any invalid effect handlers from previous sessions
    CleanupInvalidHandlers();

    UE_LOG(LogTemp, Warning, TEXT("CombatSystemManager: Initialized with UE5 interface system"));
}

void UCombatSystemManager::SetWorldContext(UWorld* World)
{
    WorldContext = World;
}

void UCombatSystemManager::RegisterCombatable(const TScriptInterface<ICombatable>& CombatableActor)
{
    if (!CombatableActor.GetInterface() || !CombatableActor.GetObject() || !IsValid(CombatableActor.GetObject()))
    {
        UE_LOG(LogTemp, Error, TEXT("CombatSystemManager: Attempted to register invalid combatable actor"));
        return;
    }

    UObject* CombatableObject = CombatableActor.GetObject();

    int32 ActorId = ICombatable::Execute_GetActorId(CombatableObject);
    ECasterType ActorType = ICombatable::Execute_GetActorType(CombatableObject);

    if (ActorId <= 0 || ActorType == ECasterType::None)
    {
        UE_LOG(LogTemp, Error, TEXT("CombatSystemManager: Invalid actor ID (%d) or type (%d)"), ActorId, (int32)ActorType);
        return;
    }

    FString Key = CreateCombatableKey(ActorId, ActorType);

    if (RegisteredCombatables.Contains(Key))
    {
        UE_LOG(LogTemp, Warning, TEXT("CombatSystemManager: Actor with ID %d and type %s already registered"),
            ActorId, *ICombatable::Execute_GetActorTypeString(CombatableObject));
        return;
    }

    FCombatableEntry Entry;
    Entry.WeakObject = CombatableObject;
    Entry.Interface  = CombatableActor.GetInterface();
    RegisteredCombatables.Add(Key, Entry);
    UE_LOG(LogTemp, Log, TEXT("CombatSystemManager: Registered combatable %d (%s)"),
        ActorId, *ICombatable::Execute_GetActorTypeString(CombatableObject));
}

void UCombatSystemManager::UnregisterCombatable(const TScriptInterface<ICombatable>& CombatableActor)
{
    if (!CombatableActor.GetInterface() || !CombatableActor.GetObject())
    {
        UE_LOG(LogTemp, Warning, TEXT("CombatSystemManager: Attempted to unregister invalid combatable actor"));
        return;
    }

    UObject* CombatableObject = CombatableActor.GetObject();

    int32 ActorId = ICombatable::Execute_GetActorId(CombatableObject);
    ECasterType ActorType = ICombatable::Execute_GetActorType(CombatableObject);

    FString Key = CreateCombatableKey(ActorId, ActorType);

    if (RegisteredCombatables.Remove(Key) > 0)
    {
        UE_LOG(LogTemp, Log, TEXT("CombatSystemManager: Unregistered combatable %d (%s)"),
            ActorId, *ICombatable::Execute_GetActorTypeString(CombatableObject));
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("CombatSystemManager: Actor %d (%s) was not registered"),
            ActorId, *ICombatable::Execute_GetActorTypeString(CombatableObject));
    }
}

void UCombatSystemManager::RegisterEffectHandler(const TScriptInterface<ISkillEffectHandler>& Handler)
{
    if (!Handler.GetInterface() || !Handler.GetObject() || !IsValid(Handler.GetObject()))
    {
        UE_LOG(LogTemp, Error, TEXT("CombatSystemManager: Attempted to register invalid effect handler"));
        return;
    }

    UObject* HandlerObject = Handler.GetObject();

    // Check if already registered via TWeakObjectPtr comparison
    for (const FEffectHandlerEntry& Existing : EffectHandlers)
    {
        if (Existing.WeakObject.IsValid() && Existing.WeakObject.Get() == HandlerObject)
        {
            UE_LOG(LogTemp, Warning, TEXT("CombatSystemManager: Effect handler already registered, skipping"));
            return;
        }
    }

    // Get handler priority for insertion order
    int32 HandlerPriority = ISkillEffectHandler::Execute_GetPriority(HandlerObject);

    // Insert in priority order (higher priority first), purging stale entries along the way
    int32 InsertIndex = 0;
    for (int32 i = 0; i < EffectHandlers.Num(); i++)
    {
        FEffectHandlerEntry& ExistingEntry = EffectHandlers[i];
        if (!ExistingEntry.WeakObject.IsValid())
        {
            UE_LOG(LogTemp, Warning, TEXT("CombatSystemManager: Removing stale handler during registration at index %d"), i);
            EffectHandlers.RemoveAt(i);
            i--;
            continue;
        }
        int32 ExistingPriority = ISkillEffectHandler::Execute_GetPriority(ExistingEntry.WeakObject.Get());
        if (ExistingPriority >= HandlerPriority)
        {
            InsertIndex = i + 1;
        }
        else
        {
            break;
        }
    }

    FEffectHandlerEntry NewEntry;
    NewEntry.WeakObject = HandlerObject;
    NewEntry.Interface  = Handler.GetInterface();
    EffectHandlers.Insert(NewEntry, InsertIndex);

    // Keep a strong UPROPERTY reference so the handler is not garbage-collected.
    EffectHandlerStrongRefs.AddUnique(HandlerObject);

    UE_LOG(LogTemp, Log, TEXT("CombatSystemManager: Registered effect handler '%s' with priority %d at index %d"),
        *HandlerObject->GetClass()->GetName(), HandlerPriority, InsertIndex);
}

void UCombatSystemManager::UnregisterEffectHandler(const TScriptInterface<ISkillEffectHandler>& Handler)
{
    if (!Handler.GetInterface() || !Handler.GetObject())
    {
        UE_LOG(LogTemp, Warning, TEXT("CombatSystemManager: Attempted to unregister null effect handler"));
        return;
    }

    UObject* HandlerObject = Handler.GetObject();
    int32 RemovedCount = EffectHandlers.RemoveAll([HandlerObject](const FEffectHandlerEntry& Entry)
    {
        return !Entry.WeakObject.IsValid() || Entry.WeakObject.Get() == HandlerObject;
    });
    if (RemovedCount > 0)
    {
        EffectHandlerStrongRefs.Remove(HandlerObject);
        UE_LOG(LogTemp, Log, TEXT("CombatSystemManager: Unregistered effect handler (removed %d instances)"), RemovedCount);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("CombatSystemManager: Effect handler was not registered"));
    }
}

void UCombatSystemManager::CleanupInvalidHandlers()
{
    int32 RemovedHandlers = EffectHandlers.RemoveAll([](const FEffectHandlerEntry& Entry)
    {
        return !Entry.WeakObject.IsValid();
    });
    if (RemovedHandlers > 0)
    {
        // Sync strong-ref array: remove entries whose UObject is no longer valid
        EffectHandlerStrongRefs.RemoveAll([](const TObjectPtr<UObject>& Obj)
        {
            return !IsValid(Obj);
        });
        UE_LOG(LogTemp, Warning, TEXT("CombatSystemManager: Removed %d stale effect handlers (remaining: %d)"),
            RemovedHandlers, EffectHandlers.Num());
    }

    // Cleanup stale combatables
    TArray<FString> KeysToRemove;
    for (const auto& Pair : RegisteredCombatables)
    {
        if (!Pair.Value.WeakObject.IsValid())
        {
            KeysToRemove.Add(Pair.Key);
        }
    }
    for (const FString& Key : KeysToRemove)
    {
        RegisteredCombatables.Remove(Key);
        UE_LOG(LogTemp, Warning, TEXT("CombatSystemManager: Removed stale combatable with key: %s"), *Key);
    }
}

void UCombatSystemManager::ProcessSkillInitiation(const FSkillInitiationData& SkillData)
{
    LogCombatEvent("Skill Initiation", 
        FString::Printf(TEXT("Skill: %s, Caster: %d (%s), Target: %d (%s), cooldownMs: %d, gcdMs: %d"), 
            *SkillData.skillName, SkillData.casterId, *SkillData.casterTypeString,
            SkillData.targetId, *SkillData.targetTypeString,
            SkillData.cooldownMs, SkillData.gcdMs));

    // Handle skill initiation in PlayerSkillManager for cooldown + GCD management
    if (GameInstance)
    {
        UPlayerSkillManager* PlayerSkillManager = GameInstance->GetPlayerSkillManager();
        if (PlayerSkillManager)
        {
            PlayerSkillManager->HandleSkillInitiation(SkillData.skillSlug, SkillData.casterId,
                SkillData.cooldownMs, SkillData.gcdMs);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("CombatSystemManager: PlayerSkillManager not available"));
        }
    }

    // Find caster
    ECasterType CasterType = MapNetCasterType(SkillData.casterType, SkillData.casterTypeString);
    TScriptInterface<ICombatable> Caster = FindCombatableById(SkillData.casterId, CasterType);

    UE_LOG(LogTemp, Warning, TEXT("INIT: casterId=%d netType=%d(%s) mapped=%d key=%s"),
        SkillData.casterId, SkillData.casterType, *SkillData.casterTypeString,
        (int32)CasterType, *CreateCombatableKey(SkillData.casterId, CasterType));

    if (Caster.GetInterface() && Caster.GetObject() && IsValid(Caster.GetObject()))
    {
        UObject* CasterObject = Caster.GetObject();

        // ShowCastBar must run BEFORE PlaySkillAnimation so that BasicPlayer::CurrentCastTime
        // is set when StartAttack() reads it to choose the phase-based or classic anim path.
        if (SkillData.castTime > 0.0f)
        {
            ICombatable::Execute_ShowCastBar(CasterObject, SkillData.castTime, SkillData.skillName);
        }

        // Play animation on caster (animationDuration drives PlayRate)
        ICombatable::Execute_PlaySkillAnimation(CasterObject, SkillData.animationName, SkillData.skillSlug, SkillData.animationDuration);

		// Set target if specified
		if (SkillData.targetId > 0)
		{
			ECasterType TargetType = MapNetCasterType(SkillData.targetType, SkillData.targetTypeString);
			ICombatable::Execute_SetTarget(CasterObject, SkillData.targetId, TargetType);
			ICombatable::Execute_SetIsAggressiveState(CasterObject, SkillData.targetId, TargetType, SkillData.skillEffectType == ESkillEffectType::Damage);
		}
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("CombatSystemManager: Could not find caster %d (%s) for skill initiation"),
            SkillData.casterId, *SkillData.casterTypeString);
    }

    // Broadcast event
    OnSkillInitiated.Broadcast(SkillData);
}

ECasterType UCombatSystemManager::MapNetCasterType(int32 Net, const FString& Str)
{
    // String-based mapping takes priority (always present in server packets)
    if (Str.Equals(TEXT("PLAYER"), ESearchCase::IgnoreCase)) return ECasterType::Player;
    if (Str.Equals(TEXT("MOB"),    ESearchCase::IgnoreCase)) return ECasterType::Mob;
    if (Str.Equals(TEXT("NPC"),    ESearchCase::IgnoreCase)) return ECasterType::NPC;
    if (Str.Equals(TEXT("SELF"),   ESearchCase::IgnoreCase)) return ECasterType::Self;
    if (Str.Equals(TEXT("AREA"),   ESearchCase::IgnoreCase)) return ECasterType::Area;

    // Fallback: enum values now match the server protocol directly (SELF=1..NPC=6)
    if (Net >= 1 && Net <= 6)
        return static_cast<ECasterType>(Net);

    return ECasterType::None;
}

void UCombatSystemManager::ProcessSkillResult(const FSkillResultData& SkillResult)
{
    LogCombatEvent("Skill Result (DEFERRED)", 
        FString::Printf(TEXT("Skill: %s, Caster: %d, Target: %d (%s), Effect: %s, Damage: %d"), 
            *SkillResult.skillName, SkillResult.casterId, SkillResult.targetId, *SkillResult.targetTypeString,
            *UEnum::GetValueAsString(SkillResult.skillEffectType),
            SkillResult.damage));

    // --- Update caster mana immediately (UI responsiveness) ---
    if (SkillResult.finalCasterMana >= 0 && GameInstance)
    {
        ECasterType CasterCombatType = MapNetCasterType(SkillResult.casterType, SkillResult.casterTypeString);
        TScriptInterface<ICombatable> Caster = FindCombatableById(SkillResult.casterId, CasterCombatType);
        if (Caster.GetInterface() && Caster.GetObject() && IsValid(Caster.GetObject()))
        {
            ICombatable::Execute_SetCurrentMana(Caster.GetObject(), SkillResult.finalCasterMana);
        }
    }

    // --- Store the result for deferred application at the animation hit-point ---
    FPendingResult Pending;
    Pending.ResultData         = SkillResult;
    Pending.StoredAtWorldTime  = WorldContext ? WorldContext->GetTimeSeconds() : 0.0;

    // Mark results for projectile skills so that ordinary animation hit-point notifies
    // (e.g. from a parallel auto-attack) cannot flush them prematurely.
    // Only the projectile actor itself (via NotifyProjectileImpact) may flush these.
    if (GameInstance)
    {
        if (USkillDefinitionRepository* Repo = GameInstance->GetSkillDefinitionRepository())
        {
            const FString& LookupKey = SkillResult.skillSlug.IsEmpty() ? SkillResult.skillName : SkillResult.skillSlug;
            Pending.bWaitsForProjectile = !Repo->GetDefinition(LookupKey).projectileClass.IsNull();
            if (Pending.bWaitsForProjectile)
            {
                UE_LOG(LogTemp, Warning,
                    TEXT("CombatSystemManager: Marking result for caster %d skill '%s' as bWaitsForProjectile"),
                    SkillResult.casterId, *LookupKey);
            }
        }
    }

    TArray<FPendingResult>& Queue = PendingSkillResults.FindOrAdd(SkillResult.casterId);
    Queue.Add(MoveTemp(Pending));

    UE_LOG(LogTemp, Log, TEXT("CombatSystemManager: Queued pending result for caster %d (%d in queue)"),
        SkillResult.casterId, Queue.Num());

    // Check if the projectile already landed before this result arrived (server was slow).
    // In that case apply immediately and clear the early-impact marker.
    if (Pending.bWaitsForProjectile)
    {
        if (const double* ImpactTime = EarlyProjectileImpacts.Find(SkillResult.casterId))
        {
            const double NowTime = WorldContext ? WorldContext->GetTimeSeconds() : 0.0;
            if ((NowTime - *ImpactTime) < EarlyImpactWindowSeconds)
            {
                UE_LOG(LogTemp, Warning,
                    TEXT("CombatSystemManager: Early-impact marker found for caster %d (%.3fs ago) - flushing immediately"),
                    SkillResult.casterId, NowTime - *ImpactTime);
                EarlyProjectileImpacts.Remove(SkillResult.casterId);
                FlushPendingResults(SkillResult.casterId, /*bSkipProjectileWaiters=*/false);
                return;
            }
            else
            {
                // Stale marker (shouldn't happen in practice), clean it up
                UE_LOG(LogTemp, Warning,
                    TEXT("CombatSystemManager: Stale early-impact marker for caster %d (%.3fs ago) - ignoring"),
                    SkillResult.casterId, NowTime - *ImpactTime);
                EarlyProjectileImpacts.Remove(SkillResult.casterId);
            }
        }
    }

    // Start a safety-timeout so results are applied even if the anim notify never fires
    StartPendingResultTimeout(SkillResult.casterId);
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

    if (ResultsToApply.Num() == 0)
    {
        UE_LOG(LogTemp, Log, TEXT("CombatSystemManager: All %d pending result(s) for caster %d are waiting for projectile impact - not flushing"),
            ResultsToRequeue.Num(), CasterId);
        return;
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

    FTimerHandle TimerHandle;
    TWeakObjectPtr<UCombatSystemManager> WeakSelf(this);
    World->GetTimerManager().SetTimer(TimerHandle, [WeakSelf, CasterId]()
    {
        if (WeakSelf.IsValid())
        {
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
}