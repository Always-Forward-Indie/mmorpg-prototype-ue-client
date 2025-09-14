#include "Gameplay/Combat/CombatSystemManager.h"
#include "Gameplay/Combat/ISkillEffectHandler.h"
#include "Gameplay/Combat/ICombatable.h"
#include "Gameplay/UI/FloatingCombatTextManager.h"
#include "UI/UIManager.h"
#include "MyGameInstance.h"
#include "Gameplay/Skills/PlayerSkillManager.h"
#include "Networking/NetworkManager.h"
#include "Utils/JSONParser.h"
#include "Engine/World.h"
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

    RegisteredCombatables.Add(Key, CombatableActor);
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

    // Check if this handler is already registered
    if (EffectHandlers.Contains(Handler))
    {
        UE_LOG(LogTemp, Warning, TEXT("CombatSystemManager: Effect handler already registered, skipping"));
        return;
    }

    UObject* HandlerObject = Handler.GetObject();
    ISkillEffectHandler* HandlerInterface = Handler.GetInterface();
    
    // Get handler priority for insertion order
    int32 HandlerPriority = ISkillEffectHandler::Execute_GetPriority(HandlerObject);
    
    // Insert handler in priority order (higher priority first)
    int32 InsertIndex = 0;
    for (int32 i = 0; i < EffectHandlers.Num(); i++)
    {
        const TScriptInterface<ISkillEffectHandler>& ExistingHandler = EffectHandlers[i];
        if (ExistingHandler.GetInterface() && ExistingHandler.GetObject() && IsValid(ExistingHandler.GetObject()))
        {
            int32 ExistingPriority = ISkillEffectHandler::Execute_GetPriority(ExistingHandler.GetObject());
            if (ExistingPriority >= HandlerPriority)
            {
                InsertIndex = i + 1;
            }
            else
            {
                break;
            }
        }
        else
        {
            // Remove invalid handler found during registration
            UE_LOG(LogTemp, Warning, TEXT("CombatSystemManager: Removing invalid handler during registration at index %d"), i);
            EffectHandlers.RemoveAt(i);
            i--; // Adjust index since we removed an element
        }
    }

    EffectHandlers.Insert(Handler, InsertIndex);
    UE_LOG(LogTemp, Log, TEXT("CombatSystemManager: Registered effect handler '%s' with priority %d at index %d"), 
        HandlerObject ? *HandlerObject->GetClass()->GetName() : TEXT("Unknown"), HandlerPriority, InsertIndex);
}

void UCombatSystemManager::UnregisterEffectHandler(const TScriptInterface<ISkillEffectHandler>& Handler)
{
    if (!Handler.GetInterface())
    {
        UE_LOG(LogTemp, Warning, TEXT("CombatSystemManager: Attempted to unregister null effect handler"));
        return;
    }

    int32 RemovedCount = EffectHandlers.Remove(Handler);
    if (RemovedCount > 0)
    {
        UE_LOG(LogTemp, Log, TEXT("CombatSystemManager: Unregistered effect handler (removed %d instances)"), RemovedCount);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("CombatSystemManager: Effect handler was not registered"));
    }
}

void UCombatSystemManager::CleanupInvalidHandlers()
{
    UE_LOG(LogTemp, Log, TEXT("CombatSystemManager: Cleaning up invalid effect handlers (current count: %d)"), EffectHandlers.Num());
    
    int32 InitialCount = EffectHandlers.Num();
    
    for (int32 i = EffectHandlers.Num() - 1; i >= 0; i--)
    {
        const TScriptInterface<ISkillEffectHandler>& Handler = EffectHandlers[i];
        
        if (!Handler.GetInterface() || !Handler.GetObject() || !IsValid(Handler.GetObject()))
        {
            UE_LOG(LogTemp, Warning, TEXT("CombatSystemManager: Removing invalid handler at index %d"), i);
            EffectHandlers.RemoveAt(i);
        }
    }
    
    int32 FinalCount = EffectHandlers.Num();
    int32 RemovedCount = InitialCount - FinalCount;
    
    if (RemovedCount > 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("CombatSystemManager: Removed %d invalid effect handlers (remaining: %d)"), RemovedCount, FinalCount);
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("CombatSystemManager: No invalid effect handlers found"));
    }

    // Also cleanup invalid combatables
    TArray<FString> KeysToRemove;
    for (const auto& Pair : RegisteredCombatables)
    {
        const TScriptInterface<ICombatable>& Combatable = Pair.Value;
        if (!Combatable.GetInterface() || !Combatable.GetObject() || !IsValid(Combatable.GetObject()))
        {
            KeysToRemove.Add(Pair.Key);
        }
    }
    
    for (const FString& Key : KeysToRemove)
    {
        RegisteredCombatables.Remove(Key);
        UE_LOG(LogTemp, Warning, TEXT("CombatSystemManager: Removed invalid combatable with key: %s"), *Key);
    }
}

void UCombatSystemManager::ProcessSkillInitiation(const FSkillInitiationData& SkillData)
{
    LogCombatEvent("Skill Initiation", 
        FString::Printf(TEXT("Skill: %s, Caster: %d (%s), Target: %d (%s)"), 
            *SkillData.skillName, SkillData.casterId, *SkillData.casterTypeString,
            SkillData.targetId, *SkillData.targetTypeString));

    // NEW: Handle skill initiation in PlayerSkillManager for cooldown management
    if (GameInstance)
    {
        UPlayerSkillManager* PlayerSkillManager = GameInstance->GetPlayerSkillManager();
        if (PlayerSkillManager)
        {
            FString SkillSlug = "";

			SkillSlug = SkillData.skillSlug;
            
            PlayerSkillManager->HandleSkillInitiation(SkillSlug, SkillData.casterId);
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
        
        // Play animation on caster
        ICombatable::Execute_PlaySkillAnimation(CasterObject, SkillData.animationName, SkillData.animationDuration);
        
        // Set target if specified
        if (SkillData.targetId > 0)
        {
            ECasterType TargetType = static_cast<ECasterType>(SkillData.targetType);
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
    if (Str.Equals(TEXT("PLAYER"), ESearchCase::IgnoreCase)) return ECasterType::Player;
    if (Str.Equals(TEXT("MOB"), ESearchCase::IgnoreCase)) return ECasterType::Mob;
    if (Str.Equals(TEXT("NPC"), ESearchCase::IgnoreCase)) return ECasterType::NPC;
    if (Str.Equals(TEXT("SELF"), ESearchCase::IgnoreCase)) return ECasterType::Self;

    // на случай, если строки нет — хардкод по текущему серверному контракту
    switch (Net) { case 1: return ECasterType::Player; case 3: return ECasterType::Mob; case 4: return ECasterType::NPC; }
                         return ECasterType::None;
}

void UCombatSystemManager::ProcessSkillResult(const FSkillResultData& SkillResult)
{
    LogCombatEvent("Skill Result", 
        FString::Printf(TEXT("Skill: %s, Target: %d (%s), Effect: %s, Missed: %s, Blocked: %s, Critical: %s, Damage: %d"), 
            *SkillResult.skillName, SkillResult.targetId, *SkillResult.targetTypeString,
            *UEnum::GetValueAsString(SkillResult.skillEffectType),
            SkillResult.isMissed ? TEXT("true") : TEXT("false"),
            SkillResult.isBlocked ? TEXT("true") : TEXT("false"),
            SkillResult.isCritical ? TEXT("true") : TEXT("false"),
            SkillResult.damage));

    // Find target
    ECasterType TargetType = static_cast<ECasterType>(SkillResult.targetType);
    TScriptInterface<ICombatable> Target = FindCombatableById(SkillResult.targetId, TargetType);
    
    if (Target.GetInterface() && Target.GetObject() && IsValid(Target.GetObject()))
    {
        UObject* TargetObject = Target.GetObject();

            ApplySkillEffects(SkillResult, Target);
            
            
            // Check for death only if effects were applied
            if (SkillResult.targetDied || ICombatable::Execute_IsDead(TargetObject))
            {
                OnActorDied.Broadcast(ICombatable::Execute_GetActorId(TargetObject));
            }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("CombatSystemManager: Could not find target %d (%s) for skill result"), 
            SkillResult.targetId, *SkillResult.targetTypeString);
    }

    // Broadcast event
    OnSkillCompleted.Broadcast(SkillResult);
}

TScriptInterface<ICombatable> UCombatSystemManager::FindCombatableById(int32 ActorId, ECasterType ActorType)
{
    if (ActorId <= 0 || ActorType == ECasterType::None)
    {
        UE_LOG(LogTemp, Warning, TEXT("CombatSystemManager: Invalid search parameters - ActorId: %d, ActorType: %d"), ActorId, (int32)ActorType);
        return TScriptInterface<ICombatable>();
    }

    FString Key = CreateCombatableKey(ActorId, ActorType);

    if (TScriptInterface<ICombatable>* Found = RegisteredCombatables.Find(Key))
    {
        const TScriptInterface<ICombatable>& Combatable = *Found;
        
        // Validate the found combatable
        if (!Combatable.GetInterface() || !Combatable.GetObject() || !IsValid(Combatable.GetObject()))
        {
            UE_LOG(LogTemp, Warning, TEXT("CombatSystemManager: Found invalid combatable for ID %d, removing from registry"), ActorId);
            RegisteredCombatables.Remove(Key);
            return TScriptInterface<ICombatable>();
        }

        // Validate the actor ID matches
        UObject* CombatableObject = Combatable.GetObject();
        if (ICombatable::Execute_GetActorId(CombatableObject) != ActorId)
        {
            UE_LOG(LogTemp, Error, TEXT("CombatSystemManager: Actor ID mismatch - expected %d, got %d"), 
                ActorId, ICombatable::Execute_GetActorId(CombatableObject));
            RegisteredCombatables.Remove(Key);
            return TScriptInterface<ICombatable>();
        }

        return Combatable;
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
    // Clean up invalid handlers first
    for (int32 i = EffectHandlers.Num() - 1; i >= 0; i--)
    {
        const TScriptInterface<ISkillEffectHandler>& Handler = EffectHandlers[i];
        
        if (!Handler.GetInterface() || !Handler.GetObject() || !IsValid(Handler.GetObject()))
        {
            UE_LOG(LogTemp, Warning, TEXT("CombatSystemManager: Removing invalid handler at index %d during search"), i);
            EffectHandlers.RemoveAt(i);
            continue;
        }

        UObject* HandlerObject = Handler.GetObject();
        
        // Check if handler can handle this effect type
        if (ISkillEffectHandler::Execute_CanHandle(HandlerObject, EffectType))
        {
            UE_LOG(LogTemp, Verbose, TEXT("CombatSystemManager: Found handler for effect type %s"), 
                *UEnum::GetValueAsString(EffectType));
            return Handler;
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

    // Найти и вызвать соответствующий хендлер
    TScriptInterface<ISkillEffectHandler> Handler = FindEffectHandler(SkillResult.skillEffectType);
    if (Handler.GetInterface() && Handler.GetObject() && IsValid(Handler.GetObject()))
    {
        UObject* HandlerObject = Handler.GetObject();

        // Хендлер полностью отвечает за применение эффектов
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