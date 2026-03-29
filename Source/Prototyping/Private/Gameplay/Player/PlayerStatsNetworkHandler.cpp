#include "Gameplay/Player/PlayerStatsNetworkHandler.h"
#include "Gameplay/Player/PlayerStatsManager.h"
#include "Networking/NetworkManager.h"
#include "Utils/JSONParser.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"

void UPlayerStatsNetworkHandler::Initialize(UPlayerStatsManager* InStatsManager, UNetworkManager* InNetworkManager)
{
    if (!InStatsManager || !InNetworkManager)
    {
        UE_LOG(LogTemp, Error, TEXT("PlayerStatsNetworkHandler: Initialize called with null parameters"));
        return;
    }
    StatsManager  = InStatsManager;
    NetworkManager = InNetworkManager;
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
    if (ReceivedData.IsEmpty() || !StatsManager) return;

    FMessageDataStruct Msg = JSONParser::DeserializeMessageData(ReceivedData);

    if (Msg.eventType == TEXT("stats_update"))
    {
        TSharedPtr<FJsonObject> Root;
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ReceivedData);
        if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()) return;

        const TSharedPtr<FJsonObject>* BodyPtr = nullptr;
        if (!Root->TryGetObjectField(TEXT("body"), BodyPtr)) return;

        FPlayerStatsUpdateStruct Stats = ParseStatsUpdate(*BodyPtr);
        StatsManager->ApplyStatsUpdate(Stats);
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

        // §9.1: Update the local player's cached HP/Mana immediately so the HUD
        // reflects HoT/DoT ticks without waiting for the next stats_update.
        if (StatsManager && TickData.characterId == StatsManager->GetCachedStats().characterId)
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

    Body->TryGetNumberField(TEXT("characterId"), S.characterId);
    Body->TryGetNumberField(TEXT("level"),       S.level);

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
