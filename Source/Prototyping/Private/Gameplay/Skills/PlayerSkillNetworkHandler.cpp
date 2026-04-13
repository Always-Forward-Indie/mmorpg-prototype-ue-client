#include "Gameplay/Skills/PlayerSkillNetworkHandler.h"
#include "Gameplay/Skills/PlayerSkillManager.h"
#include "Networking/NetworkManager.h"
#include "Utils/JSONParser.h"
#include "MyGameInstance.h"
#include "Services/TimeSyncService.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"

UPlayerSkillNetworkHandler::UPlayerSkillNetworkHandler()
{
    SkillManager = nullptr;
    NetworkManager = nullptr;
    GameInstance = nullptr;
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
    else if (MessageData.eventType == TEXT("skillBarState"))
    {
        HandleSkillBarState(ReceivedData);
    }
    else if (MessageData.eventType == TEXT("skillBarSlotUpdated"))
    {
        HandleSkillBarSlotUpdated(ReceivedData);
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

void UPlayerSkillNetworkHandler::SetGameInstance(UMyGameInstance* InGameInstance)
{
    GameInstance = InGameInstance;
}

// ---------------------------------------------------------------------------
// Inbound: skillBarState — full bar snapshot sent on login
// Server guarantees initializePlayerSkills arrives BEFORE this packet,
// so SkillManager already knows all learned skills.
// ---------------------------------------------------------------------------
void UPlayerSkillNetworkHandler::HandleSkillBarState(const FString& JsonData)
{
    if (!SkillManager)
    {
        UE_LOG(LogTemp, Error, TEXT("PlayerSkillNetworkHandler: HandleSkillBarState - SkillManager is null"));
        return;
    }

    TSharedPtr<FJsonObject> Root;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonData);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("PlayerSkillNetworkHandler: HandleSkillBarState - failed to parse JSON"));
        return;
    }

    const TSharedPtr<FJsonObject>* BodyPtr = nullptr;
    if (!Root->TryGetObjectField(TEXT("body"), BodyPtr))
    {
        UE_LOG(LogTemp, Warning, TEXT("PlayerSkillNetworkHandler: HandleSkillBarState - missing 'body' field"));
        return;
    }

    const TArray<TSharedPtr<FJsonValue>>* SlotsArr = nullptr;
    if (!(*BodyPtr)->TryGetArrayField(TEXT("slots"), SlotsArr))
    {
        // Empty bar is valid
        UE_LOG(LogTemp, Log, TEXT("PlayerSkillNetworkHandler: HandleSkillBarState - empty skill bar"));
        return;
    }

    int32 LoadedSlots = 0;
    for (const TSharedPtr<FJsonValue>& Val : *SlotsArr)
    {
        const TSharedPtr<FJsonObject>* SlotObj = nullptr;
        if (!Val->TryGetObject(SlotObj)) continue;

        int32 SlotIndex = 0;
        FString SkillSlug;
        if (!(*SlotObj)->TryGetNumberField(TEXT("slotIndex"), SlotIndex)) continue;
        if (!(*SlotObj)->TryGetStringField(TEXT("skillSlug"), SkillSlug)) continue;
        if (SkillSlug.IsEmpty()) continue;

        // Server-authoritative: apply directly. No outbound packet; this IS the server state.
        SkillManager->SetSkillSlot(SlotIndex, SkillSlug, FKey());
        ++LoadedSlots;
    }

    UE_LOG(LogTemp, Log, TEXT("PlayerSkillNetworkHandler: HandleSkillBarState - loaded %d slots"), LoadedSlots);
}

// ---------------------------------------------------------------------------
// Inbound: skillBarSlotUpdated — ACK from server after setSkillBarSlot
// ---------------------------------------------------------------------------
void UPlayerSkillNetworkHandler::HandleSkillBarSlotUpdated(const FString& JsonData)
{
    TSharedPtr<FJsonObject> Root;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonData);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()) return;

    const TSharedPtr<FJsonObject>* BodyPtr = nullptr;
    if (!Root->TryGetObjectField(TEXT("body"), BodyPtr)) return;

    int32 SlotIndex = -1;
    FString SkillSlug;
    (*BodyPtr)->TryGetNumberField(TEXT("slotIndex"), SlotIndex);
    (*BodyPtr)->TryGetStringField(TEXT("skillSlug"), SkillSlug);

    UE_LOG(LogTemp, Log, TEXT("PlayerSkillNetworkHandler: skillBarSlotUpdated ACK - slot=%d skill='%s'"),
        SlotIndex, *SkillSlug);
}

// ---------------------------------------------------------------------------
// Outbound: setSkillBarSlot — player-initiated slot assignment or clear
// ---------------------------------------------------------------------------
void UPlayerSkillNetworkHandler::SendSetSkillBarSlot(int32 SlotIndex, const FString& SkillSlug, int32 CharacterId)
{
    if (!NetworkManager || !GameInstance)
    {
        UE_LOG(LogTemp, Warning, TEXT("PlayerSkillNetworkHandler: SendSetSkillBarSlot - missing NetworkManager or GameInstance"));
        return;
    }

    TMap<FString, TSharedPtr<FJsonValue>> HeaderData;
    TMap<FString, TSharedPtr<FJsonValue>> BodyData;

    HeaderData.Add(TEXT("clientId"), MakeShareable(new FJsonValueNumber(GameInstance->GetCurrentClientID())));
    HeaderData.Add(TEXT("hash"),     MakeShareable(new FJsonValueString(GameInstance->GetCurrentClientHash())));

    BodyData.Add(TEXT("slotIndex"),   MakeShareable(new FJsonValueNumber(SlotIndex)));
    BodyData.Add(TEXT("skillSlug"),   MakeShareable(new FJsonValueString(SkillSlug)));
    BodyData.Add(TEXT("characterId"), MakeShareable(new FJsonValueNumber(CharacterId)));

    FString JsonString = JSONParser::SerializeJsonWithTimeSync(
        TEXT("setSkillBarSlot"), HeaderData, BodyData,
        GameInstance->GetTimeSyncService(), EServerType::ChunkServer);

    NetworkManager->SendDataToChunkServer(JsonString);

    UE_LOG(LogTemp, Log, TEXT("PlayerSkillNetworkHandler: Sent setSkillBarSlot - slot=%d skill='%s' char=%d"),
        SlotIndex, *SkillSlug, CharacterId);
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