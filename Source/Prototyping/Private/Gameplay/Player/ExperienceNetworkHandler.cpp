#include "Gameplay/Player/ExperienceNetworkHandler.h"
#include "Gameplay/Player/ExperienceManager.h"
#include "MyGameInstance.h"
#include "Networking/NetworkManager.h"
#include "Utils/JSONParser.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"

UExperienceNetworkHandler::UExperienceNetworkHandler()
{
    ExperienceManager = nullptr;
    GameInstance = nullptr;
    NetworkManager = nullptr;
    bDebugLogging = true;
    bIsSubscribed = false;
}

void UExperienceNetworkHandler::Initialize(UExperienceManager* InExperienceManager, UMyGameInstance* InGameInstance, UNetworkManager* InNetworkManager)
{
    if (!InExperienceManager || !InGameInstance || !InNetworkManager)
    {
        UE_LOG(LogTemp, Error, TEXT("ExperienceNetworkHandler: Cannot initialize with null parameters"));
        return;
    }

    ExperienceManager = InExperienceManager;
    GameInstance = InGameInstance;
    NetworkManager = InNetworkManager;

    LogNetworkEvent(TEXT("Initialization"), TEXT("ExperienceNetworkHandler initialized successfully"));
}

void UExperienceNetworkHandler::Shutdown()
{
    // Unsubscribe from network events
    UnsubscribeFromNetworkEvents();

    // Clear references
    ExperienceManager = nullptr;
    GameInstance = nullptr;
    NetworkManager = nullptr;

    LogNetworkEvent(TEXT("Shutdown"), TEXT("ExperienceNetworkHandler shutdown completed"));
}

void UExperienceNetworkHandler::SubscribeToNetworkEvents()
{
    if (!NetworkManager || !IsValid(NetworkManager))
    {
        UE_LOG(LogTemp, Error, TEXT("ExperienceNetworkHandler: Cannot subscribe - NetworkManager is null or invalid"));
        return;
    }

    if (bIsSubscribed)
    {
        UE_LOG(LogTemp, Warning, TEXT("ExperienceNetworkHandler: Already subscribed to network events"));
        return;
    }

    // Subscribe to chunk server data (where experience updates come from)
    NetworkManager->OnChunkServerDataReceived.RemoveDynamic(this, &UExperienceNetworkHandler::HandleChunkServerData);
    NetworkManager->OnChunkServerDataReceived.AddDynamic(this, &UExperienceNetworkHandler::HandleChunkServerData);

    bIsSubscribed = true;
    LogNetworkEvent(TEXT("Subscription"), TEXT("Subscribed to network events"));
}

void UExperienceNetworkHandler::UnsubscribeFromNetworkEvents()
{
    if (!NetworkManager || !IsValid(NetworkManager))
    {
        return;
    }

    if (!bIsSubscribed)
    {
        return;
    }

    // Unsubscribe from chunk server data
    NetworkManager->OnChunkServerDataReceived.RemoveDynamic(this, &UExperienceNetworkHandler::HandleChunkServerData);

    bIsSubscribed = false;
    LogNetworkEvent(TEXT("Unsubscription"), TEXT("Unsubscribed from network events"));
}

void UExperienceNetworkHandler::HandleChunkServerData(const FString& ReceivedData)
{
    if (ReceivedData.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("ExperienceNetworkHandler: Received empty data"));
        return;
    }

    // Parse message data to get event type
    FMessageDataStruct MessageData = JSONParser::DeserializeMessageData(ReceivedData);
    
    if (MessageData.eventType.IsEmpty())
    {
        return; // Not a valid message, ignore
    }

    // Check if this is an experience-related event
    if (!IsExperienceEvent(MessageData.eventType))
    {
        return; // Not an experience event, ignore
    }

    LogNetworkEvent(TEXT("Event Received"), 
        FString::Printf(TEXT("Processing %s event"), *MessageData.eventType));

    // Validate event data before processing
    if (!ValidateExperienceEventData(ReceivedData))
    {
        UE_LOG(LogTemp, Warning, TEXT("ExperienceNetworkHandler: Invalid experience event data"));
        return;
    }

    // Process the specific experience event
    if (MessageData.eventType == TEXT("experience_update"))
    {
        ProcessExperienceUpdateEvent(ReceivedData);
    }
    else if (MessageData.eventType == TEXT("level_up"))
    {
        ProcessLevelUpEvent(ReceivedData);
    }
    else if (MessageData.eventType == TEXT("progression_update"))
    {
        ProcessProgressionUpdateEvent(ReceivedData);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("ExperienceNetworkHandler: Unknown experience event type: %s"), *MessageData.eventType);
    }
}

bool UExperienceNetworkHandler::IsExperienceEvent(const FString& EventType) const
{
    // Define experience-related events
    return EventType == TEXT("experience_update") ||
           EventType == TEXT("level_up") ||
           EventType == TEXT("progression_update") ||
           EventType == TEXT("experience_gain");
}

void UExperienceNetworkHandler::ProcessExperienceUpdateEvent(const FString& JsonData)
{
    if (!ExperienceManager || !IsValid(ExperienceManager))
    {
        UE_LOG(LogTemp, Error, TEXT("ExperienceNetworkHandler: ExperienceManager is null or invalid"));
        return;
    }

    // Parse the experience update from JSON
    FExperienceUpdateStruct ExperienceUpdate = ParseExperienceUpdateFromJson(JsonData);
    
    if (ExperienceUpdate.characterId <= 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("ExperienceNetworkHandler: Invalid character ID in experience update"));
        return;
    }

    LogNetworkEvent(TEXT("Experience Update"), 
        FString::Printf(TEXT("Character %d gained %d XP (%s), Level %d->%d"), 
            ExperienceUpdate.characterId, ExperienceUpdate.experienceChange,
            *ExperienceUpdate.reason, ExperienceUpdate.oldLevel, ExperienceUpdate.newLevel));

    // Forward to experience manager
    ExperienceManager->ProcessExperienceUpdate(ExperienceUpdate);
}

void UExperienceNetworkHandler::ProcessLevelUpEvent(const FString& JsonData)
{
    // Level up events are typically handled as part of experience updates
    // But this method allows for separate level up notifications if needed
    ProcessExperienceUpdateEvent(JsonData);
}

void UExperienceNetworkHandler::ProcessProgressionUpdateEvent(const FString& JsonData)
{
    if (!ExperienceManager || !IsValid(ExperienceManager))
    {
        UE_LOG(LogTemp, Error, TEXT("ExperienceNetworkHandler: ExperienceManager is null or invalid"));
        return;
    }

    // Parse progression update (simplified version of experience update)
    FExperienceUpdateStruct ExperienceUpdate = ParseExperienceUpdateFromJson(JsonData);
    
    if (ExperienceUpdate.characterId <= 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("ExperienceNetworkHandler: Invalid character ID in progression update"));
        return;
    }

    // Convert to progression struct and update
    FPlayerProgressionStruct Progression;
    Progression.characterId = ExperienceUpdate.characterId;
    Progression.currentLevel = ExperienceUpdate.newLevel;
    Progression.currentExperience = ExperienceUpdate.newExperience;
    Progression.totalExperience = ExperienceUpdate.newExperience;
    Progression.expForCurrentLevel = ExperienceUpdate.expForCurrentLevel;
    Progression.expForNextLevel = ExperienceUpdate.expForNextLevel;

    ExperienceManager->UpdateCharacterProgression(ExperienceUpdate.characterId, Progression);
}

FExperienceUpdateStruct UExperienceNetworkHandler::ParseExperienceUpdateFromJson(const FString& JsonData) const
{
    FExperienceUpdateStruct ExperienceUpdate;

    // Parse JSON
    TSharedPtr<FJsonObject> Root;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonData);
    
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("ExperienceNetworkHandler: Failed to parse JSON"));
        return ExperienceUpdate;
    }

    // Get body object
    TSharedPtr<FJsonObject> Body;
    if (!Root->HasField(TEXT("body")))
    {
        UE_LOG(LogTemp, Error, TEXT("ExperienceNetworkHandler: No valid body in JSON"));
        return ExperienceUpdate;
    }
    
    Body = Root->GetObjectField(TEXT("body"));
    if (!Body.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("ExperienceNetworkHandler: Invalid body object"));
        return ExperienceUpdate;
    }

    // Parse experience update fields from body
    Body->TryGetNumberField(TEXT("characterId"), ExperienceUpdate.characterId);
    Body->TryGetNumberField(TEXT("oldLevel"), ExperienceUpdate.oldLevel);
    Body->TryGetNumberField(TEXT("newLevel"), ExperienceUpdate.newLevel);
    Body->TryGetNumberField(TEXT("oldExperience"), ExperienceUpdate.oldExperience);
    Body->TryGetNumberField(TEXT("newExperience"), ExperienceUpdate.newExperience);
    Body->TryGetNumberField(TEXT("experienceChange"), ExperienceUpdate.experienceChange);
    Body->TryGetNumberField(TEXT("expForCurrentLevel"), ExperienceUpdate.expForCurrentLevel);
    Body->TryGetNumberField(TEXT("expForNextLevel"), ExperienceUpdate.expForNextLevel);
    Body->TryGetBoolField(TEXT("levelUp"), ExperienceUpdate.levelUp);
    Body->TryGetStringField(TEXT("reason"), ExperienceUpdate.reason);
    Body->TryGetNumberField(TEXT("sourceId"), ExperienceUpdate.sourceId);

    return ExperienceUpdate;
}

bool UExperienceNetworkHandler::ValidateExperienceEventData(const FString& JsonData) const
{
    if (JsonData.IsEmpty())
    {
        return false;
    }

    // Parse JSON to validate structure
    TSharedPtr<FJsonObject> Root;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonData);
    
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
    {
        return false;
    }

    // Check for required fields
    if (!Root->HasField(TEXT("header")) || !Root->HasField(TEXT("body")))
    {
        return false;
    }

    // Get header and validate status
    TSharedPtr<FJsonObject> Header;
    if (Root->HasField(TEXT("header")))
    {
        Header = Root->GetObjectField(TEXT("header"));
        if (Header.IsValid())
        {
            FString Status;
            if (Header->TryGetStringField(TEXT("status"), Status))
            {
                // Only process successful events
                return Status == TEXT("success");
            }
        }
    }

    return false;
}

void UExperienceNetworkHandler::LogNetworkEvent(const FString& Event, const FString& Details) const
{
    if (bDebugLogging)
    {
        UE_LOG(LogTemp, Warning, TEXT("ExperienceNetworkHandler [%s]: %s"), *Event, *Details);
    }
}