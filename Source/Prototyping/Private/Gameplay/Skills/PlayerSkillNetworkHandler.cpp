#include "Gameplay/Skills/PlayerSkillNetworkHandler.h"
#include "Gameplay/Skills/PlayerSkillManager.h"
#include "Networking/NetworkManager.h"
#include "Utils/JSONParser.h"

UPlayerSkillNetworkHandler::UPlayerSkillNetworkHandler()
{
    SkillManager = nullptr;
    NetworkManager = nullptr;
    bIsSubscribed = false;
}

void UPlayerSkillNetworkHandler::Initialize(UPlayerSkillManager* InSkillManager, UNetworkManager* InNetworkManager)
{
    if (!InSkillManager)
    {
        UE_LOG(LogTemp, Error, TEXT("PlayerSkillNetworkHandler: Cannot initialize with null SkillManager"));
        return;
    }

    if (!InNetworkManager)
    {
        UE_LOG(LogTemp, Error, TEXT("PlayerSkillNetworkHandler: Cannot initialize with null NetworkManager"));
        return;
    }

    SkillManager = InSkillManager;
    NetworkManager = InNetworkManager;

    UE_LOG(LogTemp, Log, TEXT("PlayerSkillNetworkHandler: Initialized successfully"));
}

void UPlayerSkillNetworkHandler::SubscribeToNetworkEvents()
{
    if (!NetworkManager)
    {
        UE_LOG(LogTemp, Error, TEXT("PlayerSkillNetworkHandler: Cannot subscribe - NetworkManager is null"));
        return;
    }

    if (!IsValid(NetworkManager))
    {
        UE_LOG(LogTemp, Error, TEXT("PlayerSkillNetworkHandler: Cannot subscribe - NetworkManager is not valid"));
        return;
    }

    if (bIsSubscribed)
    {
        UE_LOG(LogTemp, Warning, TEXT("PlayerSkillNetworkHandler: Already subscribed to network events"));
        return;
    }

    // Subscribe to chunk server data
    NetworkManager->OnChunkServerDataReceived.RemoveDynamic(this, &UPlayerSkillNetworkHandler::HandleChunkServerData);
    NetworkManager->OnChunkServerDataReceived.AddDynamic(this, &UPlayerSkillNetworkHandler::HandleChunkServerData);

    bIsSubscribed = true;
    UE_LOG(LogTemp, Log, TEXT("PlayerSkillNetworkHandler: Subscribed to network events"));
}

void UPlayerSkillNetworkHandler::UnsubscribeFromNetworkEvents()
{
    if (!NetworkManager || !bIsSubscribed)
    {
        return;
    }

    NetworkManager->OnChunkServerDataReceived.RemoveDynamic(this, &UPlayerSkillNetworkHandler::HandleChunkServerData);
    
    bIsSubscribed = false;
    UE_LOG(LogTemp, Log, TEXT("PlayerSkillNetworkHandler: Unsubscribed from network events"));
}

void UPlayerSkillNetworkHandler::HandleChunkServerData(const FString& ReceivedData)
{
    FMessageDataStruct MessageData = JSONParser::DeserializeMessageData(ReceivedData);
    
    LogNetworkEvent("Received", FString::Printf(TEXT("Event: %s, Status: %s"), 
        *MessageData.eventType, *MessageData.status));

    // Only process successful events
    if (MessageData.status != TEXT("success"))
    {
        UE_LOG(LogTemp, Warning, TEXT("PlayerSkillNetworkHandler: Received non-success event: %s"), 
            *MessageData.status);
        return;
    }

    // Handle skill-related events
    if (MessageData.eventType == TEXT("initializePlayerSkills"))
    {
        HandleInitializePlayerSkills(ReceivedData);
    }
    else if (MessageData.eventType == TEXT("skillCooldownUpdate"))
    {
        HandleSkillCooldownUpdate(ReceivedData);
    }
    else if (MessageData.eventType == TEXT("skillLevelUpdate"))
    {
        HandleSkillLevelUpdate(ReceivedData);
    }
}

void UPlayerSkillNetworkHandler::HandleInitializePlayerSkills(const FString& JsonData)
{
    if (!SkillManager)
    {
        UE_LOG(LogTemp, Error, TEXT("PlayerSkillNetworkHandler: SkillManager is null"));
        return;
    }

    // Parse the JSON data for player skills initialization
    FPlayerSkillsInitializationData SkillsData = JSONParser::DeserializePlayerSkillsInitialization(JsonData);
    
    if (SkillsData.characterId <= 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("PlayerSkillNetworkHandler: Invalid character ID in skills initialization"));
        return;
    }

    LogNetworkEvent("Initialize Player Skills", 
        FString::Printf(TEXT("Character: %d, Skills: %d"), 
            SkillsData.characterId, SkillsData.skills.Num()));

    // Initialize skills in the manager
    SkillManager->InitializePlayerSkills(SkillsData);

    UE_LOG(LogTemp, Warning, TEXT("PlayerSkillNetworkHandler: Initialized %d skills for character %d"), 
        SkillsData.skills.Num(), SkillsData.characterId);
}

void UPlayerSkillNetworkHandler::HandleSkillCooldownUpdate(const FString& JsonData)
{
    // TODO: Implement skill cooldown updates from server
    // This would be used if server sends cooldown sync messages
    LogNetworkEvent("Skill Cooldown Update", TEXT("Processing cooldown update"));
}

void UPlayerSkillNetworkHandler::HandleSkillLevelUpdate(const FString& JsonData)
{
    // TODO: Implement skill level updates from server
    // This would be used when skills level up
    LogNetworkEvent("Skill Level Update", TEXT("Processing level update"));
}

void UPlayerSkillNetworkHandler::LogNetworkEvent(const FString& EventType, const FString& Details)
{
    UE_LOG(LogTemp, Log, TEXT("PlayerSkillNetworkHandler [%s]: %s"), *EventType, *Details);
}

bool UPlayerSkillNetworkHandler::ValidateEventData(const FMessageDataStruct& MessageData, const FString& ExpectedEventType)
{
    if (MessageData.eventType != ExpectedEventType)
    {
        UE_LOG(LogTemp, Warning, TEXT("PlayerSkillNetworkHandler: Expected event type %s but got %s"), 
            *ExpectedEventType, *MessageData.eventType);
        return false;
    }

    if (MessageData.status != TEXT("success"))
    {
        UE_LOG(LogTemp, Warning, TEXT("PlayerSkillNetworkHandler: Event %s failed with status: %s"), 
            *ExpectedEventType, *MessageData.status);
        return false;
    }

    return true;
}