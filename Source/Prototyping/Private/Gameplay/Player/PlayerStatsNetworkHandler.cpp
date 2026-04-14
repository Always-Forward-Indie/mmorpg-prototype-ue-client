#include "Gameplay/Player/PlayerStatsNetworkHandler.h"
#include "Gameplay/Player/PlayerStatsManager.h"
#include "Networking/NetworkManager.h"
#include "MyGameInstance.h"
#include "Utils/JSONParser.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"

void UPlayerStatsNetworkHandler::Initialize(UPlayerStatsManager* InStatsManager, UNetworkManager* InNetworkManager, UMyGameInstance* InGameInstance)
{
    if (!InStatsManager || !InNetworkManager)
    {
        UE_LOG(LogTemp, Error, TEXT("PlayerStatsNetworkHandler: Initialize called with null parameters"));
        return;
    }
    StatsManager   = InStatsManager;
    NetworkManager = InNetworkManager;
    GameInstance   = InGameInstance;

    // Cache the local character ID once so callbacks never read ambient GameInstance state.
    LocalCharacterId = InGameInstance ? InGameInstance->GetCurrentCharacterID() : 0;
}

void UPlayerStatsNetworkHandler::SubscribeToNetworkEvents()
{
    if (!NetworkManager || !IsValid(NetworkManager) || bIsSubscribed) return;
    NetworkManager->OnChunkServerDataReceived.AddDynamic(this, &UPlayerStatsNetworkHandler::HandleChunkServerData);
    bIsSubscribed = true;
}

void UPlayerStatsNetworkHandler::UnsubscribeFromNetworkEvents()
{
    if (!NetworkManager || !IsValid(NetworkManager) || !bIsSubscribed) return;
    NetworkManager->OnChunkServerDataReceived.RemoveDynamic(this, &UPlayerStatsNetworkHandler::HandleChunkServerData);
    bIsSubscribed = false;
}

void UPlayerStatsNetworkHandler::HandleChunkServerData(const FString& ReceivedData)
{
    if (ReceivedData.IsEmpty()) return;

    if (!StatsManager)
    {
        UE_LOG(LogTemp, Error, TEXT("[EFFECTS] PlayerStatsNetworkHandler::HandleChunkServerData: StatsManager is NULL — handler is broken, check initialization"));
        return;
    }

    FMessageDataStruct Msg = JSONParser::DeserializeMessageData(ReceivedData);

    if (Msg.eventType == TEXT("stats_update"))
    {
        UE_LOG(LogTemp, Warning, TEXT("[LOADSEQ] PlayerStatsNetworkHandler: stats_update received (bFirstStatsDelivered=%d, localCharId=%d)"),
            bFirstStatsDelivered, LocalCharacterId);

        TSharedPtr<FJsonObject> Root;
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ReceivedData);
        if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()) return;

        const TSharedPtr<FJsonObject>* BodyPtr = nullptr;
        if (!Root->TryGetObjectField(TEXT("body"), BodyPtr)) return;

        FPlayerStatsUpdateStruct Stats = ParseStatsUpdate(*BodyPtr);

        UE_LOG(LogTemp, Warning, TEXT("[LOADSEQ] PlayerStatsNetworkHandler: stats_update charId=%d hp=%d/%d"),
            Stats.characterId, Stats.healthCurrent, Stats.healthMax);

        // Only apply stats that belong to our local character.
        if (LocalCharacterId > 0 && Stats.characterId > 0 &&
            Stats.characterId != LocalCharacterId)
        {
            UE_LOG(LogTemp, Warning, TEXT("[LOADSEQ] PlayerStatsNetworkHandler: charId mismatch (got %d, expected %d), skipping"),
                Stats.characterId, LocalCharacterId);
            return;
        }

        StatsManager->ApplyStatsUpdate(Stats);

        // Signal the loading screen gate when we have received enough full packets to
        // be confident that SET_PLAYER_ACTIVE_EFFECTS has already been processed on the
        // server and its stats_update is already in CachedStats.
        //
        // A "full" packet is any stats_update that carries attributes (as opposed to a
        // bare regen tick that only has health/mana fields).  At login the server always
        // sends at least two full packets:
        //   1. From handleSetPlayerInventoryEvent  (may have no active effects yet)
        //   2. From handleSetPlayerActiveEffectsEvent (always includes all effects)
        //
        // Waiting for the SECOND full packet therefore guarantees effects are present in
        // CachedStats before the loading-screen gate opens.
        // Characters who receive only one full packet here (unusual) still get the gate
        // fired by the BasicPlayer::ProcessStatsUpdate path, which calls
        // NotifyStatsReceived unconditionally every time it runs.
        if (!bFirstStatsDelivered && Stats.attributes.Num() > 0)
        {
            ++FullStatsPacketCount;
            if (FullStatsPacketCount >= 2 && GameInstance)
            {
                bFirstStatsDelivered = true;
                GameInstance->NotifyStatsReceived();
                UE_LOG(LogTemp, Warning, TEXT("[LOADSEQ] PlayerStatsNetworkHandler: NotifyStatsReceived() fired (2nd full packet, effects confirmed)"));
            }
        }
    }
    else if (Msg.eventType == TEXT("setPlayerActiveEffects"))
    {
        HandleSetPlayerActiveEffects(ReceivedData);
    }
    else if (Msg.eventType == TEXT("effectTick"))
    {
        // Parse and broadcast the DoT/HoT tick so game objects can update HP and show floating text
        FEffectTickData TickData = JSONParser::DeserializeEffectTick(ReceivedData);
        OnEffectTick.Broadcast(TickData);

        // Update the local player's cached HP/Mana immediately for our own character only.
        if (StatsManager && LocalCharacterId > 0 && TickData.characterId == LocalCharacterId)
        {
            FPlayerStatsUpdateStruct Updated = StatsManager->GetCachedStats();
            Updated.healthCurrent = TickData.newHealth;
            Updated.manaCurrent   = TickData.newMana;
            StatsManager->ApplyStatsUpdate(Updated);
        }
    }
}

void UPlayerStatsNetworkHandler::HandleSetPlayerActiveEffects(const FString& ReceivedData) const
{
    if (!StatsManager) return;

    TArray<FActiveEffectEntry> Effects = JSONParser::DeserializeActiveEffectsPacket(ReceivedData);
    StatsManager->ApplyActiveEffects(Effects);
}

// ---------------------------------------------------------------------------
// Parser
// ---------------------------------------------------------------------------

FPlayerStatsUpdateStruct UPlayerStatsNetworkHandler::ParseStatsUpdate(const TSharedPtr<FJsonObject>& Body) const
{
    FPlayerStatsUpdateStruct S;

    Body->TryGetNumberField(TEXT("characterId"),    S.characterId);
    Body->TryGetNumberField(TEXT("level"),            S.level);
    Body->TryGetNumberField(TEXT("freeSkillPoints"),  S.freeSkillPoints);
    Body->TryGetStringField(TEXT("source"),           S.updateSource);

    // health
    const TSharedPtr<FJsonObject>* HealthObj = nullptr;
    if (Body->TryGetObjectField(TEXT("health"), HealthObj))
    {
        (*HealthObj)->TryGetNumberField(TEXT("current"), S.healthCurrent);
        (*HealthObj)->TryGetNumberField(TEXT("max"),     S.healthMax);
    }

    // mana
    const TSharedPtr<FJsonObject>* ManaObj = nullptr;
    if (Body->TryGetObjectField(TEXT("mana"), ManaObj))
    {
        (*ManaObj)->TryGetNumberField(TEXT("current"), S.manaCurrent);
        (*ManaObj)->TryGetNumberField(TEXT("max"),     S.manaMax);
    }

    // experience
    const TSharedPtr<FJsonObject>* ExpObj = nullptr;
    if (Body->TryGetObjectField(TEXT("experience"), ExpObj))
    {
        (*ExpObj)->TryGetNumberField(TEXT("current"),    S.experienceCurrent);
        (*ExpObj)->TryGetNumberField(TEXT("levelStart"), S.experienceLevelStart);
        (*ExpObj)->TryGetNumberField(TEXT("nextLevel"),  S.experienceNextLevel);
        (*ExpObj)->TryGetNumberField(TEXT("debt"),        S.experienceDebt);
    }

    // weight
    const TSharedPtr<FJsonObject>* WeightObj = nullptr;
    if (Body->TryGetObjectField(TEXT("weight"), WeightObj))
    {
        double Tmp = 0.0;
        if ((*WeightObj)->TryGetNumberField(TEXT("current"), Tmp)) S.weightCurrent = static_cast<float>(Tmp);
        if ((*WeightObj)->TryGetNumberField(TEXT("max"),     Tmp)) S.weightMax     = static_cast<float>(Tmp);
    }

    // attributes array
    const TArray<TSharedPtr<FJsonValue>>* AttrsArray = nullptr;
    if (Body->TryGetArrayField(TEXT("attributes"), AttrsArray))
    {
        for (const TSharedPtr<FJsonValue>& Val : *AttrsArray)
        {
            const TSharedPtr<FJsonObject>* ObjPtr = nullptr;
            if (!Val->TryGetObject(ObjPtr)) continue;

            FStatAttributeEntry Entry;
            (*ObjPtr)->TryGetStringField(TEXT("slug"),      Entry.slug);
            (*ObjPtr)->TryGetStringField(TEXT("name"),      Entry.name);
            (*ObjPtr)->TryGetNumberField(TEXT("base"),      Entry.base);
            (*ObjPtr)->TryGetNumberField(TEXT("effective"), Entry.effective);
            S.attributes.Add(Entry);
        }
    }

    // activeEffects array
    const TArray<TSharedPtr<FJsonValue>>* EffectsArray = nullptr;
    if (Body->TryGetArrayField(TEXT("activeEffects"), EffectsArray))
    {
        for (const TSharedPtr<FJsonValue>& Val : *EffectsArray)
        {
            const TSharedPtr<FJsonObject>* ObjPtr = nullptr;
            if (!Val->TryGetObject(ObjPtr)) continue;

            FActiveEffectEntry Entry;
            (*ObjPtr)->TryGetStringField(TEXT("slug"),           Entry.slug);
            (*ObjPtr)->TryGetStringField(TEXT("effectTypeSlug"), Entry.effectTypeSlug);
            (*ObjPtr)->TryGetStringField(TEXT("attributeSlug"),  Entry.attributeSlug);
            (*ObjPtr)->TryGetStringField(TEXT("sourceType"),     Entry.sourceType);
            double Tmp = 0.0;
            if ((*ObjPtr)->TryGetNumberField(TEXT("value"),     Tmp)) Entry.value     = static_cast<float>(Tmp);
            if ((*ObjPtr)->TryGetNumberField(TEXT("expiresAt"), Tmp)) Entry.expiresAt = static_cast<int64>(Tmp);
            S.activeEffects.Add(Entry);
        }
    }

    return S;
}
