#include "Gameplay/Skills/PlayerSkillNetworkHandler.h"
#include "Prototyping.h"
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
        UE_LOG(LogGameplay, Error, TEXT("PlayerSkillNetworkHandler: Cannot initialize with null SkillManager"));
        return;
    }

    if (!InNetworkManager)
    {
        UE_LOG(LogGameplay, Error, TEXT("PlayerSkillNetworkHandler: Cannot initialize with null NetworkManager"));
        return;
    }

    SkillManager = InSkillManager;
    NetworkManager = InNetworkManager;

    UE_LOG(LogGameplay, Verbose, TEXT("PlayerSkillNetworkHandler: Initialized successfully"));
}

void UPlayerSkillNetworkHandler::SubscribeToNetworkEvents()
{
    if (!NetworkManager)
    {
        UE_LOG(LogGameplay, Error, TEXT("PlayerSkillNetworkHandler: Cannot subscribe - NetworkManager is null"));
        return;
    }

    if (!IsValid(NetworkManager))
    {
        UE_LOG(LogGameplay, Error, TEXT("PlayerSkillNetworkHandler: Cannot subscribe - NetworkManager is not valid"));
        return;
    }

    if (bIsSubscribed)
    {
        UE_LOG(LogGameplay, Warning, TEXT("PlayerSkillNetworkHandler: Already subscribed to network events"));
        return;
    }

    // Subscribe to chunk server data (single-parse dispatcher fallback)
    NetworkManager->DispatcherFor(EServerType::ChunkServer).RemoveFallbackHandler(this);
    NetworkManager->DispatcherFor(EServerType::ChunkServer).AddFallbackHandler(this, &UPlayerSkillNetworkHandler::HandleChunkPacket);

    bIsSubscribed = true;
    UE_LOG(LogGameplay, Verbose, TEXT("PlayerSkillNetworkHandler: Subscribed to network events"));
}

void UPlayerSkillNetworkHandler::UnsubscribeFromNetworkEvents()
{
    if (!NetworkManager || !bIsSubscribed)
    {
        return;
    }

    NetworkManager->DispatcherFor(EServerType::ChunkServer).RemoveFallbackHandler(this);
    
    bIsSubscribed = false;
    UE_LOG(LogGameplay, Verbose, TEXT("PlayerSkillNetworkHandler: Unsubscribed from network events"));
}

void UPlayerSkillNetworkHandler::HandleChunkPacket(const FParsedPacket& Packet)
{
    // Single-parse path (S05): EventType/Status/Body come from PollLink, no re-parse.
    if (!Packet.bValid) return;

    const FString& EventType = Packet.EventType;
    const FString& Status = Packet.Status;

    LogNetworkEvent("Received", FString::Printf(TEXT("Event: %s, Status: %s"),
        *EventType, *Status));

    // Only process successful events
    if (Status != TEXT("success"))
    {
        UE_LOG(LogGameplay, Warning, TEXT("PlayerSkillNetworkHandler: Received non-success event: %s"),
            *Status);
        return;
    }

    // Handle skill-related events
    if (EventType == TEXT("initializePlayerSkills"))
    {
        HandleInitializePlayerSkills(Packet);
    }
    else if (EventType == TEXT("setSkillCooldowns"))
    {
        HandleSetSkillCooldowns(Packet);
    }
    else if (EventType == TEXT("skillCooldownUpdate"))
    {
        HandleSkillCooldownUpdate(Packet);
    }
    else if (EventType == TEXT("skillLevelUpdate"))
    {
        HandleSkillLevelUpdate(Packet);
    }
    else if (EventType == TEXT("skillBarState"))
    {
        HandleSkillBarState(Packet);
    }
    else if (EventType == TEXT("skillBarSlotUpdated"))
    {
        HandleSkillBarSlotUpdated(Packet);
    }
}

void UPlayerSkillNetworkHandler::HandleInitializePlayerSkills(const FParsedPacket& Packet)
{
    if (!SkillManager)
    {
        UE_LOG(LogGameplay, Error, TEXT("PlayerSkillNetworkHandler: SkillManager is null"));
        return;
    }

    // Parse the JSON data for player skills initialization (Body overload — no re-parse)
    FPlayerSkillsInitializationData SkillsData;
    if (Packet.Body.IsValid())
    {
        SkillsData = JSONParser::DeserializePlayerSkillsInitialization(Packet.Body);
    }
    
    if (SkillsData.characterId <= 0)
    {
        UE_LOG(LogGameplay, Warning, TEXT("PlayerSkillNetworkHandler: Invalid character ID in skills initialization"));
        return;
    }

    LogNetworkEvent("Initialize Player Skills", 
        FString::Printf(TEXT("Character: %d, Skills: %d"), 
            SkillsData.characterId, SkillsData.skills.Num()));

    // Initialize skills in the manager
    SkillManager->InitializePlayerSkills(SkillsData);

    UE_LOG(LogGameplay, Warning, TEXT("PlayerSkillNetworkHandler: Initialized %d skills for character %d"), 
        SkillsData.skills.Num(), SkillsData.characterId);
}

// ---------------------------------------------------------------------------
// Inbound: setSkillCooldowns — active cooldowns restored on login
// Server sends this after initializePlayerSkills when the character has
// skills still on cooldown from their previous session.
// Each entry: { skillSlug, remainingMs } — time LEFT, not full duration.
// ---------------------------------------------------------------------------
void UPlayerSkillNetworkHandler::HandleSetSkillCooldowns(const FParsedPacket& Packet)
{
    if (!SkillManager)
    {
        UE_LOG(LogGameplay, Error, TEXT("PlayerSkillNetworkHandler: HandleSetSkillCooldowns - SkillManager is null"));
        return;
    }

    const TSharedPtr<FJsonObject>& Root = Packet.Root;
    if (!Root.IsValid())
    {
        UE_LOG(LogNetPacket, Error, TEXT("PlayerSkillNetworkHandler: HandleSetSkillCooldowns - failed to parse JSON"));
        return;
    }

    const TSharedPtr<FJsonObject>* BodyPtr = nullptr;
    if (!Root->TryGetObjectField(TEXT("body"), BodyPtr))
    {
        UE_LOG(LogNetPacket, Warning, TEXT("PlayerSkillNetworkHandler: HandleSetSkillCooldowns - missing 'body' field"));
        return;
    }

    const TArray<TSharedPtr<FJsonValue>>* CooldownsArr = nullptr;
    if (!(*BodyPtr)->TryGetArrayField(TEXT("cooldowns"), CooldownsArr) || CooldownsArr->IsEmpty())
    {
        // Empty array is valid — no active cooldowns to restore
        UE_LOG(LogGameplay, Verbose, TEXT("PlayerSkillNetworkHandler: HandleSetSkillCooldowns - no cooldowns to restore"));
        return;
    }

    TArray<FSkillCooldownEntry> Cooldowns;
    Cooldowns.Reserve(CooldownsArr->Num());

    for (const TSharedPtr<FJsonValue>& Val : *CooldownsArr)
    {
        const TSharedPtr<FJsonObject>* EntryObj = nullptr;
        if (!Val->TryGetObject(EntryObj)) continue;

        FSkillCooldownEntry Entry;
        if (!(*EntryObj)->TryGetStringField(TEXT("skillSlug"), Entry.skillSlug)) continue;
        if (!(*EntryObj)->TryGetNumberField(TEXT("remainingMs"), Entry.remainingMs)) continue;
        if (Entry.skillSlug.IsEmpty() || Entry.remainingMs <= 0) continue;

        Cooldowns.Add(Entry);
    }

    UE_LOG(LogGameplay, Verbose,
        TEXT("PlayerSkillNetworkHandler: HandleSetSkillCooldowns - restoring %d cooldowns"),
        Cooldowns.Num());

    SkillManager->ApplyServerCooldowns(Cooldowns);
}

void UPlayerSkillNetworkHandler::HandleSkillCooldownUpdate(const FParsedPacket& Packet)
{
    // TODO: Implement skill cooldown updates from server
    // This would be used if server sends cooldown sync messages
    LogNetworkEvent("Skill Cooldown Update", TEXT("Processing cooldown update"));
}

void UPlayerSkillNetworkHandler::HandleSkillLevelUpdate(const FParsedPacket& Packet)
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
void UPlayerSkillNetworkHandler::HandleSkillBarState(const FParsedPacket& Packet)
{
    if (!SkillManager)
    {
        UE_LOG(LogGameplay, Error, TEXT("PlayerSkillNetworkHandler: HandleSkillBarState - SkillManager is null"));
        return;
    }

    const TSharedPtr<FJsonObject>& Root = Packet.Root;
    if (!Root.IsValid())
    {
        UE_LOG(LogNetPacket, Error, TEXT("PlayerSkillNetworkHandler: HandleSkillBarState - failed to parse JSON"));
        return;
    }

    const TSharedPtr<FJsonObject>* BodyPtr = nullptr;
    if (!Root->TryGetObjectField(TEXT("body"), BodyPtr))
    {
        UE_LOG(LogNetPacket, Warning, TEXT("PlayerSkillNetworkHandler: HandleSkillBarState - missing 'body' field"));
        return;
    }

    const TArray<TSharedPtr<FJsonValue>>* SlotsArr = nullptr;
    if (!(*BodyPtr)->TryGetArrayField(TEXT("slots"), SlotsArr))
    {
        // Empty bar is valid
        UE_LOG(LogGameplay, Verbose, TEXT("PlayerSkillNetworkHandler: HandleSkillBarState - empty skill bar"));
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

    UE_LOG(LogGameplay, Verbose, TEXT("PlayerSkillNetworkHandler: HandleSkillBarState - loaded %d slots"), LoadedSlots);
}

// ---------------------------------------------------------------------------
// Inbound: skillBarSlotUpdated — ACK from server after setSkillBarSlot
// ---------------------------------------------------------------------------
void UPlayerSkillNetworkHandler::HandleSkillBarSlotUpdated(const FParsedPacket& Packet)
{
    const TSharedPtr<FJsonObject>& Root = Packet.Root;
    if (!Root.IsValid()) return;

    const TSharedPtr<FJsonObject>* BodyPtr = nullptr;
    if (!Root->TryGetObjectField(TEXT("body"), BodyPtr)) return;

    int32 SlotIndex = -1;
    FString SkillSlug;
    (*BodyPtr)->TryGetNumberField(TEXT("slotIndex"), SlotIndex);
    (*BodyPtr)->TryGetStringField(TEXT("skillSlug"), SkillSlug);

    UE_LOG(LogGameplay, Verbose, TEXT("PlayerSkillNetworkHandler: skillBarSlotUpdated ACK - slot=%d skill='%s'"),
        SlotIndex, *SkillSlug);
}

// ---------------------------------------------------------------------------
// Outbound: setSkillBarSlot — player-initiated slot assignment or clear
// ---------------------------------------------------------------------------
void UPlayerSkillNetworkHandler::SendSetSkillBarSlot(int32 SlotIndex, const FString& SkillSlug, int32 CharacterId)
{
    if (!NetworkManager || !GameInstance)
    {
        UE_LOG(LogNetPacket, Warning, TEXT("PlayerSkillNetworkHandler: SendSetSkillBarSlot - missing NetworkManager or GameInstance"));
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

    UE_LOG(LogGameplay, Verbose, TEXT("PlayerSkillNetworkHandler: Sent setSkillBarSlot - slot=%d skill='%s' char=%d"),
        SlotIndex, *SkillSlug, CharacterId);
}

void UPlayerSkillNetworkHandler::LogNetworkEvent(const FString& EventType, const FString& Details)
{
    UE_LOG(LogGameplay, Verbose, TEXT("PlayerSkillNetworkHandler [%s]: %s"), *EventType, *Details);
}

bool UPlayerSkillNetworkHandler::ValidateEventData(const FMessageDataStruct& MessageData, const FString& ExpectedEventType)
{
    if (MessageData.eventType != ExpectedEventType)
    {
        UE_LOG(LogGameplay, Warning, TEXT("PlayerSkillNetworkHandler: Expected event type %s but got %s"), 
            *ExpectedEventType, *MessageData.eventType);
        return false;
    }

    if (MessageData.status != TEXT("success"))
    {
        UE_LOG(LogGameplay, Warning, TEXT("PlayerSkillNetworkHandler: Event %s failed with status: %s"), 
            *ExpectedEventType, *MessageData.status);
        return false;
    }

    return true;
}