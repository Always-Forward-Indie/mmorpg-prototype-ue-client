#include "Gameplay/Combat/CombatNetworkHandler.h"
#include "Gameplay/Combat/CombatSystemManager.h"
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

    UE_LOG(LogTemp, Warning, TEXT("CombatNetworkHandler: Initialized"));
}

void UCombatNetworkHandler::SubscribeToNetworkManager()
{
    if (!NetworkManager)
    {
        UE_LOG(LogTemp, Error, TEXT("CombatNetworkHandler: NetworkManager is null"));
        return;
    }

    if (!IsValid(NetworkManager))
    {
        UE_LOG(LogTemp, Error, TEXT("CombatNetworkHandler: NetworkManager is not valid"));
        return;
    }

    // Subscribe to chunk server data for combat events
    NetworkManager->OnChunkServerDataReceived.RemoveDynamic(this, &UCombatNetworkHandler::ProcessChunkServerData);
    NetworkManager->OnChunkServerDataReceived.AddDynamic(this, &UCombatNetworkHandler::ProcessChunkServerData);

    UE_LOG(LogTemp, Warning, TEXT("CombatNetworkHandler: Subscribed to network events"));
}

void UCombatNetworkHandler::ProcessChunkServerData(const FString& ReceivedData)
{
    FMessageDataStruct MessageData = JSONParser::DeserializeMessageData(ReceivedData);
    
    LogNetworkEvent("Received", FString::Printf(TEXT("Event: %s, Status: %s"), 
        *MessageData.eventType, *MessageData.status));

    // Only process successful events
    if (MessageData.status != TEXT("success"))
    {
        UE_LOG(LogTemp, Warning, TEXT("CombatNetworkHandler: Received non-success event: %s"), 
            *MessageData.status);
        return;
    }

    // Handle new combat system events
    if (MessageData.eventType == TEXT("combatInitiation"))
    {
        HandleSkillInitiation(ReceivedData);
    }
    else if (MessageData.eventType == TEXT("combatResult"))
    {
        HandleSkillResult(ReceivedData);
    }
}

void UCombatNetworkHandler::HandleSkillInitiation(const FString& JsonData)
{
    FSkillInitiationData SkillData = JSONParser::DeserializeSkillInitiation(JsonData);
    
    if (!SkillData.success)
    {
        UE_LOG(LogTemp, Warning, TEXT("CombatNetworkHandler: Skill initiation failed for skill %s"), 
            *SkillData.skillName);
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
        UE_LOG(LogTemp, Error, TEXT("CombatNetworkHandler: CombatSystemManager is null"));
    }
}

void UCombatNetworkHandler::HandleSkillResult(const FString& JsonData)
{
    FSkillResultData SkillResult = JSONParser::DeserializeSkillResult(JsonData);
    
    if (!SkillResult.success)
    {
        UE_LOG(LogTemp, Warning, TEXT("CombatNetworkHandler: Skill result failed for skill %s"), 
            *SkillResult.skillName);
        return;
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
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("CombatNetworkHandler: CombatSystemManager is null"));
    }
}

void UCombatNetworkHandler::LogNetworkEvent(const FString& Event, const FString& Details)
{
    UE_LOG(LogTemp, Log, TEXT("CombatNetworkHandler [%s]: %s"), *Event, *Details);
}