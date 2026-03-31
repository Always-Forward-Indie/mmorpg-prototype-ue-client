#include "Gameplay/NPCs/NPCNetworkHandler.h"
#include "Gameplay/NPCs/NPCManager.h"
#include "Networking/NetworkManager.h"
#include "Utils/JSONParser.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"

UNPCNetworkHandler::UNPCNetworkHandler()
{
    NPCManager = nullptr;
    NetworkManager = nullptr;
    bDebugLogging = true;
    bIsSubscribed = false;
}

void UNPCNetworkHandler::Initialize(UNPCManager* InNPCManager, UNetworkManager* InNetworkManager)
{
    if (!InNPCManager || !InNetworkManager)
    {
        UE_LOG(LogTemp, Error, TEXT("NPCNetworkHandler: Cannot initialize with null parameters"));
        return;
    }

    NPCManager = InNPCManager;
    NetworkManager = InNetworkManager;

    LogNetworkEvent(TEXT("Initialization"), TEXT("NPCNetworkHandler initialized successfully"));
}

void UNPCNetworkHandler::SubscribeToNetworkEvents()
{
    if (!NetworkManager || !IsValid(NetworkManager))
    {
        UE_LOG(LogTemp, Error, TEXT("NPCNetworkHandler: Cannot subscribe - NetworkManager is null or invalid"));
        return;
    }

    if (bIsSubscribed)
    {
        UE_LOG(LogTemp, Warning, TEXT("NPCNetworkHandler: Already subscribed to network events"));
        return;
    }

    // Subscribe to chunk server data (where NPC events come from)
    NetworkManager->OnChunkServerDataReceived.RemoveDynamic(this, &UNPCNetworkHandler::HandleChunkServerData);
    NetworkManager->OnChunkServerDataReceived.AddDynamic(this, &UNPCNetworkHandler::HandleChunkServerData);

    bIsSubscribed = true;
    LogNetworkEvent(TEXT("Subscription"), TEXT("Subscribed to network events"));
}

void UNPCNetworkHandler::UnsubscribeFromNetworkEvents()
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
    NetworkManager->OnChunkServerDataReceived.RemoveDynamic(this, &UNPCNetworkHandler::HandleChunkServerData);

    bIsSubscribed = false;
    LogNetworkEvent(TEXT("Unsubscription"), TEXT("Unsubscribed from network events"));
}

bool UNPCNetworkHandler::IsNPCEvent(const FString& EventType) const
{
    // Define NPC-related events
    return EventType == TEXT("spawnNPCs") ||
           EventType == TEXT("updateNPC") ||
           EventType == TEXT("removeNPC") ||
           EventType == TEXT("npcInteraction") ||
           EventType == TEXT("npcDialogue") ||
           EventType == TEXT("npcQuest");
}

void UNPCNetworkHandler::HandleChunkServerData(const FString& ReceivedData)
{
    if (ReceivedData.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("NPCNetworkHandler: Received empty data"));
        return;
    }

    // Parse message data to get event type
    FMessageDataStruct MessageData = JSONParser::DeserializeMessageData(ReceivedData);
    
    if (MessageData.eventType.IsEmpty())
    {
        return; // Not a valid message, ignore
    }

    // Check if this is an NPC-related event
    if (!IsNPCEvent(MessageData.eventType))
    {
        return; // Not an NPC event, ignore
    }

    LogNetworkEvent(TEXT("Event Received"), 
        FString::Printf(TEXT("Processing %s event"), *MessageData.eventType));

    // Validate event data before processing
    if (!ValidateNPCEventData(ReceivedData))
    {
        UE_LOG(LogTemp, Warning, TEXT("NPCNetworkHandler: Invalid NPC event data"));
        return;
    }

    // Process the specific NPC event
    if (MessageData.eventType == TEXT("spawnNPCs"))
    {
        HandleNPCSpawn(ReceivedData);
    }
    else if (MessageData.eventType == TEXT("updateNPC"))
    {
        HandleNPCUpdate(ReceivedData);
    }
    else if (MessageData.eventType == TEXT("removeNPC"))
    {
        HandleNPCRemove(ReceivedData);
    }
    else if (MessageData.eventType == TEXT("npcInteraction"))
    {
        HandleNPCInteraction(ReceivedData);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("NPCNetworkHandler: Unknown NPC event type: %s"), *MessageData.eventType);
    }
}

void UNPCNetworkHandler::HandleNPCSpawn(const FString& JsonData)
{
    if (!NPCManager || !IsValid(NPCManager))
    {
        UE_LOG(LogTemp, Error, TEXT("NPCNetworkHandler: NPCManager is null or invalid"));
        return;
    }

    // Parse the NPC spawn data from JSON
    FNPCSpawnDataStruct NPCSpawnData = JSONParser::DeserializeNPCSpawnData(JsonData);
    
    if (NPCSpawnData.npcsSpawn.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("NPCNetworkHandler: No NPCs to spawn"));
        return;
    }

    LogNetworkEvent(TEXT("NPC Spawn"), 
        FString::Printf(TEXT("Spawning %d NPCs"), NPCSpawnData.npcsSpawn.Num()));

    // Forward to NPC manager
    NPCManager->SpawnNPCs(NPCSpawnData.npcsSpawn);

    UE_LOG(LogTemp, Warning, TEXT("NPCNetworkHandler: Successfully processed spawn for %d NPCs"), 
        NPCSpawnData.npcsSpawn.Num());
}

void UNPCNetworkHandler::HandleNPCUpdate(const FString& JsonData)
{
    // Parse JSON to get NPC update data
    TSharedPtr<FJsonObject> Root;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonData);
    
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("NPCNetworkHandler: Failed to parse NPC update JSON"));
        return;
    }

    // Get body object
    TSharedPtr<FJsonObject> Body;
    if (!Root->HasField(TEXT("body")))
    {
        UE_LOG(LogTemp, Error, TEXT("NPCNetworkHandler: No body in NPC update JSON"));
        return;
    }
    
    Body = Root->GetObjectField(TEXT("body"));
    if (!Body.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("NPCNetworkHandler: Invalid body object in NPC update"));
        return;
    }

    // Parse NPC ID and updated data
    int32 NPCId = 0;
    Body->TryGetNumberField(TEXT("npcId"), NPCId);

    if (NPCId <= 0)
    {
        UE_LOG(LogTemp, Error, TEXT("NPCNetworkHandler: Invalid NPC ID in update: %d"), NPCId);
        return;
    }

    LogNetworkEvent(TEXT("NPC Update"), 
        FString::Printf(TEXT("Updating NPC ID: %d"), NPCId));

    // Get the NPC and update it
    if (NPCManager && IsValid(NPCManager))
    {
        ABasicNPC* NPC = NPCManager->GetNPCById(NPCId);
        if (NPC)
        {
            // Parse updated NPC data from body
            FNPCStruct UpdatedData = JSONParser::DeserializeNPCData(Body);
            if (UpdatedData.id == NPCId)
            {
                NPC->SetNPCData(UpdatedData);
                UE_LOG(LogTemp, Log, TEXT("NPCNetworkHandler: Updated NPC %s (ID: %d)"), 
                    *UpdatedData.name, NPCId);
            }
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("NPCNetworkHandler: NPC with ID %d not found for update"), NPCId);
        }
    }
}

void UNPCNetworkHandler::HandleNPCRemove(const FString& JsonData)
{
    // Parse JSON to get NPC removal data
    TSharedPtr<FJsonObject> Root;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonData);
    
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("NPCNetworkHandler: Failed to parse NPC remove JSON"));
        return;
    }

    // Get body object
    TSharedPtr<FJsonObject> Body = Root->GetObjectField(TEXT("body"));
    if (!Body.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("NPCNetworkHandler: Invalid body object in NPC remove"));
        return;
    }

    // Parse NPC ID
    int32 NPCId = 0;
    Body->TryGetNumberField(TEXT("npcId"), NPCId);

    if (NPCId <= 0)
    {
        UE_LOG(LogTemp, Error, TEXT("NPCNetworkHandler: Invalid NPC ID in remove: %d"), NPCId);
        return;
    }

    LogNetworkEvent(TEXT("NPC Remove"), 
        FString::Printf(TEXT("Removing NPC ID: %d"), NPCId));

    // Remove the NPC
    if (NPCManager && IsValid(NPCManager))
    {
        NPCManager->RemoveNPC(NPCId);
    }
}

void UNPCNetworkHandler::HandleNPCInteraction(const FString& JsonData)
{
    LogNetworkEvent(TEXT("NPC Interaction"), TEXT("Processing NPC interaction event"));
    
    // Parse JSON to get interaction data
    TSharedPtr<FJsonObject> Root;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonData);
    
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("NPCNetworkHandler: Failed to parse NPC interaction JSON"));
        return;
    }

    // Get body object
    TSharedPtr<FJsonObject> Body = Root->GetObjectField(TEXT("body"));
    if (!Body.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("NPCNetworkHandler: Invalid body object in NPC interaction"));
        return;
    }

    // Parse interaction data
    int32 NPCId = 0;
    int32 PlayerId = 0;
    FString InteractionType;
    
    Body->TryGetNumberField(TEXT("npcId"), NPCId);
    Body->TryGetNumberField(TEXT("playerId"), PlayerId);
    Body->TryGetStringField(TEXT("interactionType"), InteractionType);

    UE_LOG(LogTemp, Log, TEXT("NPCNetworkHandler: NPC interaction - NPC: %d, Player: %d, Type: %s"), 
        NPCId, PlayerId, *InteractionType);

    // TODO: Handle specific interaction types (dialogue, quest, trade, etc.)
}

bool UNPCNetworkHandler::ValidateNPCEventData(const FString& JsonData) const
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

    // Get header and validate
    TSharedPtr<FJsonObject> Header = Root->GetObjectField(TEXT("header"));
    if (Header.IsValid())
    {
        // Check message field (server uses "message" not "status")
        FString Message;
        if (Header->TryGetStringField(TEXT("message"), Message))
        {
            // Reject explicit error messages
            if (Message == TEXT("error"))
            {
                return false;
            }
        }
    }

    return true;
}

void UNPCNetworkHandler::LogNetworkEvent(const FString& Event, const FString& Details) const
{
    if (bDebugLogging)
    {
        UE_LOG(LogTemp, Warning, TEXT("NPCNetworkHandler [%s]: %s"), *Event, *Details);
    }
}