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

    UE_LOG(LogGameplay, Warning, TEXT("CombatSystemManager: Initialized with UE5 interface system"));
}

void UCombatSystemManager::SetWorldContext(UWorld* World)
{
    // Clear all pending safety-timeout timers before changing worlds.
    // If the old world is still valid, unregister them from its TimerManager.
    if (WorldContext && WorldContext != World)
    {
        for (auto& Pair : PendingResultTimerHandles)
        {
            WorldContext->GetTimerManager().ClearTimer(Pair.Value);
        }
    }
    PendingResultTimerHandles.Empty();

    WorldContext = World;
}

void UCombatSystemManager::RegisterCombatable(const TScriptInterface<ICombatable>& CombatableActor)
{
    if (!CombatableActor.GetInterface() || !CombatableActor.GetObject() || !IsValid(CombatableActor.GetObject()))
    {
        UE_LOG(LogGameplay, Error, TEXT("CombatSystemManager: Attempted to register invalid combatable actor"));
        return;
    }

    UObject* CombatableObject = CombatableActor.GetObject();

    int32 ActorId = ICombatable::Execute_GetActorId(CombatableObject);
    ECasterType ActorType = ICombatable::Execute_GetActorType(CombatableObject);

    if (ActorId <= 0 || ActorType == ECasterType::None)
    {
        UE_LOG(LogGameplay, Error, TEXT("CombatSystemManager: Invalid actor ID (%d) or type (%d)"), ActorId, (int32)ActorType);
        return;
    }

    FString Key = CreateCombatableKey(ActorId, ActorType);

    if (RegisteredCombatables.Contains(Key))
    {
        UE_LOG(LogGameplay, Warning, TEXT("CombatSystemManager: Actor with ID %d and type %s already registered"),
            ActorId, *ICombatable::Execute_GetActorTypeString(CombatableObject));
        return;
    }

    FCombatableEntry Entry;
    Entry.WeakObject = CombatableObject;
    Entry.Interface  = CombatableActor.GetInterface();
    RegisteredCombatables.Add(Key, Entry);
    UE_LOG(LogGameplay, Verbose, TEXT("CombatSystemManager: Registered combatable %d (%s)"),
        ActorId, *ICombatable::Execute_GetActorTypeString(CombatableObject));
}

void UCombatSystemManager::UnregisterCombatable(const TScriptInterface<ICombatable>& CombatableActor)
{
    if (!CombatableActor.GetInterface() || !CombatableActor.GetObject())
    {
        UE_LOG(LogGameplay, Warning, TEXT("CombatSystemManager: Attempted to unregister invalid combatable actor"));
        return;
    }

    UObject* CombatableObject = CombatableActor.GetObject();

    int32 ActorId = ICombatable::Execute_GetActorId(CombatableObject);
    ECasterType ActorType = ICombatable::Execute_GetActorType(CombatableObject);

    FString Key = CreateCombatableKey(ActorId, ActorType);

    if (RegisteredCombatables.Remove(Key) > 0)
    {
        UE_LOG(LogGameplay, Verbose, TEXT("CombatSystemManager: Unregistered combatable %d (%s)"),
            ActorId, *ICombatable::Execute_GetActorTypeString(CombatableObject));
    }
    else
    {
        UE_LOG(LogGameplay, Warning, TEXT("CombatSystemManager: Actor %d (%s) was not registered"),
            ActorId, *ICombatable::Execute_GetActorTypeString(CombatableObject));
    }
}

void UCombatSystemManager::RegisterEffectHandler(const TScriptInterface<ISkillEffectHandler>& Handler)
{
    if (!Handler.GetInterface() || !Handler.GetObject() || !IsValid(Handler.GetObject()))
    {
        UE_LOG(LogGameplay, Error, TEXT("CombatSystemManager: Attempted to register invalid effect handler"));
        return;
    }

    UObject* HandlerObject = Handler.GetObject();

    // Check if already registered via TWeakObjectPtr comparison
    for (const FEffectHandlerEntry& Existing : EffectHandlers)
    {
        if (Existing.WeakObject.IsValid() && Existing.WeakObject.Get() == HandlerObject)
        {
            UE_LOG(LogGameplay, Warning, TEXT("CombatSystemManager: Effect handler already registered, skipping"));
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
            UE_LOG(LogGameplay, Warning, TEXT("CombatSystemManager: Removing stale handler during registration at index %d"), i);
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

    UE_LOG(LogGameplay, Verbose, TEXT("CombatSystemManager: Registered effect handler '%s' with priority %d at index %d"),
        *HandlerObject->GetClass()->GetName(), HandlerPriority, InsertIndex);
}

void UCombatSystemManager::UnregisterEffectHandler(const TScriptInterface<ISkillEffectHandler>& Handler)
{
    if (!Handler.GetInterface() || !Handler.GetObject())
    {
        UE_LOG(LogGameplay, Warning, TEXT("CombatSystemManager: Attempted to unregister null effect handler"));
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
        UE_LOG(LogGameplay, Verbose, TEXT("CombatSystemManager: Unregistered effect handler (removed %d instances)"), RemovedCount);
    }
    else
    {
        UE_LOG(LogGameplay, Warning, TEXT("CombatSystemManager: Effect handler was not registered"));
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
        UE_LOG(LogGameplay, Warning, TEXT("CombatSystemManager: Removed %d stale effect handlers (remaining: %d)"),
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
        UE_LOG(LogGameplay, Warning, TEXT("CombatSystemManager: Removed stale combatable with key: %s"), *Key);
    }
}

