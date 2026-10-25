#include "Gameplay/Combat/CombatNetworkHandler.h"
#include "Prototyping.h"
#include "Gameplay/Combat/CombatSystemManager.h"
#include "Services/TimeSyncService.h"
#include "Networking/NetworkManager.h"
#include "Utils/JSONParser.h"

UCombatNetworkHandler::UCombatNetworkHandler()
{
    CombatSystemManager = nullptr;
    NetworkManager = nullptr;
}

void UCombatNetworkHandler::Initialize(UCombatSystemManager* CombatManager, UNetworkManager* InNetworkManager)
{
    CombatSystemManager = CombatManager;
    NetworkManager = InNetworkManager;

    UE_LOG(LogGameplay, Warning, TEXT("CombatNetworkHandler: Initialized"));
}

void UCombatNetworkHandler::SubscribeToNetworkManager()
{
    if (!NetworkManager)
    {
        UE_LOG(LogGameplay, Error, TEXT("CombatNetworkHandler: NetworkManager is null"));
        return;
    }

    if (!IsValid(NetworkManager))
    {
        UE_LOG(LogGameplay, Error, TEXT("CombatNetworkHandler: NetworkManager is not valid"));
        return;
    }

    // Subscribe to chunk server data for combat events.
    // Single-parse path (S05): dispatcher fallback delivers every parsed packet;
    // the entry below filters by eventType. Stale owners are skipped by weak
    // pointers, and this handler has no Unsubscribe counterpart by design.
    NetworkManager->DispatcherFor(EServerType::ChunkServer).RemoveFallbackHandler(this);
    NetworkManager->DispatcherFor(EServerType::ChunkServer).AddFallbackHandler(this, &UCombatNetworkHandler::HandleChunkPacket);

    UE_LOG(LogGameplay, Warning, TEXT("CombatNetworkHandler: Subscribed to network events"));
}

void UCombatNetworkHandler::HandleChunkPacket(const FParsedPacket& Packet)
{
    // Compat alias: sub-handlers below still take the raw string (S05b converts them).
    const FString& ReceivedData = Packet.Raw;

    // Single-parse path (S05): MessageData comes from the pre-parsed packet, no re-parse.
    FMessageDataStruct MessageData = JSONParser::DeserializeMessageData(Packet);
    
    LogNetworkEvent("Received", FString::Printf(TEXT("Event: %s, Status: %s"), 
        *MessageData.eventType, *MessageData.status));

    // Only process successful events (combat initiation/result packets always pass: failures carry body.errorCode)
    if (MessageData.status != TEXT("success") && MessageData.eventType != TEXT("combatInitiation") && MessageData.eventType != TEXT("healingInitiation") && MessageData.eventType != TEXT("buffInitiation") && MessageData.eventType != TEXT("debuffInitiation") && MessageData.eventType != TEXT("skillInitiation") && MessageData.eventType != TEXT("combatResult") && MessageData.eventType != TEXT("healingResult") && MessageData.eventType != TEXT("buffResult") && MessageData.eventType != TEXT("debuffResult") && MessageData.eventType != TEXT("skillResult") && MessageData.eventType != TEXT("skillExecutionResult") && MessageData.eventType != TEXT("combatAoeResult"))
    {
        UE_LOG(LogGameplay, Warning, TEXT("CombatNetworkHandler: Received non-success event: %s"), 
            *MessageData.status);
        return;
    }

    // Handle all skill initiation event types (damage, heal, buff, debuff, generic)
    if (MessageData.eventType == TEXT("combatInitiation")
        || MessageData.eventType == TEXT("healingInitiation")
        || MessageData.eventType == TEXT("buffInitiation")
        || MessageData.eventType == TEXT("debuffInitiation")
        || MessageData.eventType == TEXT("skillInitiation"))
    {
        HandleSkillInitiation(ReceivedData);
    }
    // Handle all skill result event types
    else if (MessageData.eventType == TEXT("combatResult")
        || MessageData.eventType == TEXT("healingResult")
        || MessageData.eventType == TEXT("buffResult")
        || MessageData.eventType == TEXT("debuffResult")
        || MessageData.eventType == TEXT("skillResult")
        || MessageData.eventType == TEXT("skillExecutionResult")
        || MessageData.eventType == TEXT("combatAoeResult"))
    {
        HandleSkillResult(ReceivedData);
    }
}

void UCombatNetworkHandler::HandleSkillInitiation(const FString& JsonData)
{
    FSkillInitiationData SkillData = JSONParser::DeserializeSkillInitiation(JsonData);
    
    if (!SkillData.success)
    {
        UE_LOG(LogGameplay, Warning, TEXT("CombatNetworkHandler: Skill initiation failed for skill %s (caster=%d, reason: %s, code: %s)"), 
            *SkillData.skillName, SkillData.casterId, *SkillData.errorReason, *SkillData.errorCode);
if (CombatSystemManager)
{
const FString CcCode = !SkillData.errorCode.IsEmpty() ? SkillData.errorCode : SkillData.errorReason;
if (CcCode.Contains(TEXT("Stun"), ESearchCase::IgnoreCase) || CcCode.Contains(TEXT("Silence"), ESearchCase::IgnoreCase) || CcCode.Contains(TEXT("Root"), ESearchCase::IgnoreCase))
{
// CC rejection: no cast started (no cooldown/animation), but the player must see why.
// Forward as a failed result so effect handlers render floating text.
FSkillResultData CcResult;
CcResult.success = false;
CcResult.skillName = SkillData.skillName;
CcResult.skillSlug = SkillData.skillSlug;
CcResult.casterId = SkillData.casterId;
CcResult.targetId = SkillData.targetId;
CcResult.errorReason = CcCode;
CcResult.errorCode = CcCode;
CombatSystemManager->ProcessSkillResult(CcResult);
}
}
        return;
    }

    LogNetworkEvent("Skill Initiation", 
        FString::Printf(TEXT("Skill: %s, Caster: %d (%s), Target: %d (%s)"), 
            *SkillData.skillName, SkillData.casterId, *SkillData.casterTypeString,
            SkillData.targetId, *SkillData.targetTypeString));

    if (CombatSystemManager)
    {
        CombatSystemManager->ProcessSkillInitiation(SkillData);
    }
    else
    {
        UE_LOG(LogGameplay, Error, TEXT("CombatNetworkHandler: CombatSystemManager is null"));
    }
}

void UCombatNetworkHandler::HandleSkillResult(const FString& JsonData)
{
    FSkillResultData SkillResult = JSONParser::DeserializeSkillResult(JsonData);
    
    // Don't filter non-success results — they carry meaningful data (miss, out-of-range, etc.)
    // that must be shown to the player as floating text or handled by effect handlers.
    if (!SkillResult.success)
    {
        UE_LOG(LogGameplay, Warning, TEXT("CombatNetworkHandler: Skill result failed for skill %s (reason: %s)"), 
            *SkillResult.skillName, *SkillResult.errorReason);
        // Continue processing — fall through to ProcessSkillResult below
    }

    LogNetworkEvent("Skill Result", 
        FString::Printf(TEXT("Skill: %s, Target: %d (%s), Damage: %d, Healing: %d, Missed: %s, Critical: %s, Blocked: %s"), 
            *SkillResult.skillName, SkillResult.targetId, *SkillResult.targetTypeString,
            SkillResult.damage, SkillResult.healing, 
            SkillResult.isMissed ? TEXT("true") : TEXT("false"),
            SkillResult.isCritical ? TEXT("true") : TEXT("false"),
            SkillResult.isBlocked ? TEXT("true") : TEXT("false")));

    if (CombatSystemManager)
    {
        CombatSystemManager->ProcessSkillResult(SkillResult);
// combatAoeResult batch: fan out per-target entries (targets 2..N were dropped before).
for (const FAoeTargetResultData& AoeTarget : SkillResult.aoeTargets)
{
FSkillResultData PerTarget = SkillResult;
PerTarget.aoeTargets.Empty();
PerTarget.targetId = AoeTarget.targetId;
PerTarget.damage = AoeTarget.damage;
PerTarget.healing = AoeTarget.healing;
PerTarget.isCritical = AoeTarget.isCritical;
PerTarget.isBlocked = AoeTarget.isBlocked;
PerTarget.isMissed = AoeTarget.isMissed;
PerTarget.targetDied = AoeTarget.targetDied;
PerTarget.finalTargetHealth = AoeTarget.finalTargetHealth;
PerTarget.finalTargetMana = AoeTarget.finalTargetMana;
CombatSystemManager->ProcessSkillResult(PerTarget);
}
    }
    else
    {
        UE_LOG(LogGameplay, Error, TEXT("CombatNetworkHandler: CombatSystemManager is null"));
    }
}

void UCombatNetworkHandler::LogNetworkEvent(const FString& Event, const FString& Details)
{
    UE_LOG(LogGameplay, Verbose, TEXT("CombatNetworkHandler [%s]: %s"), *Event, *Details);
}
