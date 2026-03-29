#include "Gameplay/Bestiary/BestiaryNetworkHandler.h"
#include "MyGameInstance.h"
#include "Networking/NetworkManager.h"
#include "Services/TimeSyncService.h"
#include "Utils/JSONParser.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"

UBestiaryNetworkHandler::UBestiaryNetworkHandler()
{
    GameInstance  = nullptr;
    NetworkManager = nullptr;
    bIsSubscribed = false;
}

void UBestiaryNetworkHandler::Initialize(UMyGameInstance* InGameInstance, UNetworkManager* InNetworkManager)
{
    if (!InGameInstance || !InNetworkManager)
    {
        UE_LOG(LogTemp, Error, TEXT("BestiaryNetworkHandler: Initialize called with null parameters"));
        return;
    }

    GameInstance   = InGameInstance;
    NetworkManager = InNetworkManager;

    UE_LOG(LogTemp, Log, TEXT("BestiaryNetworkHandler: Initialized"));
}

void UBestiaryNetworkHandler::Shutdown()
{
    UnsubscribeFromNetworkEvents();
    GameInstance   = nullptr;
    NetworkManager = nullptr;
    EntryCache.Empty();
}

void UBestiaryNetworkHandler::SubscribeToNetworkEvents()
{
    if (!NetworkManager || !IsValid(NetworkManager))
    {
        UE_LOG(LogTemp, Error, TEXT("BestiaryNetworkHandler: Cannot subscribe - NetworkManager invalid"));
        return;
    }

    if (bIsSubscribed)
        return;

    NetworkManager->OnChunkServerDataReceived.RemoveDynamic(this, &UBestiaryNetworkHandler::HandleChunkServerData);
    NetworkManager->OnChunkServerDataReceived.AddDynamic(this, &UBestiaryNetworkHandler::HandleChunkServerData);
    bIsSubscribed = true;

    UE_LOG(LogTemp, Log, TEXT("BestiaryNetworkHandler: Subscribed to network events"));
}

void UBestiaryNetworkHandler::UnsubscribeFromNetworkEvents()
{
    if (!NetworkManager || !IsValid(NetworkManager) || !bIsSubscribed)
        return;

    NetworkManager->OnChunkServerDataReceived.RemoveDynamic(this, &UBestiaryNetworkHandler::HandleChunkServerData);
    bIsSubscribed = false;
}

void UBestiaryNetworkHandler::RequestBestiaryOverview(int32 CharacterId)
{
    if (!NetworkManager || !GameInstance)
    {
        UE_LOG(LogTemp, Error, TEXT("BestiaryNetworkHandler: Cannot request overview - missing dependencies"));
        return;
    }

    TMap<FString, TSharedPtr<FJsonValue>> HeaderData;
    TMap<FString, TSharedPtr<FJsonValue>> BodyData;

    HeaderData.Add(TEXT("clientId"), MakeShareable(new FJsonValueNumber(GameInstance->GetCurrentClientID())));
    HeaderData.Add(TEXT("hash"),     MakeShareable(new FJsonValueString(GameInstance->GetCurrentClientHash())));

    BodyData.Add(TEXT("characterId"), MakeShareable(new FJsonValueNumber(CharacterId)));

    FString JsonString = JSONParser::SerializeJsonWithTimeSync(
        TEXT("getBestiaryOverview"), HeaderData, BodyData,
        GameInstance->GetTimeSyncService(), EServerType::ChunkServer);

    NetworkManager->SendDataToChunkServer(JsonString);

    UE_LOG(LogTemp, Log, TEXT("BestiaryNetworkHandler: Sent getBestiaryOverview request for character %d"), CharacterId);
}

void UBestiaryNetworkHandler::RequestBestiaryEntry(int32 CharacterId, const FString& MobSlug)
{
    if (!NetworkManager || !GameInstance)
    {
        UE_LOG(LogTemp, Error, TEXT("BestiaryNetworkHandler: Cannot request entry - missing dependencies"));
        return;
    }

    TMap<FString, TSharedPtr<FJsonValue>> HeaderData;
    TMap<FString, TSharedPtr<FJsonValue>> BodyData;

    HeaderData.Add(TEXT("clientId"), MakeShareable(new FJsonValueNumber(GameInstance->GetCurrentClientID())));
    HeaderData.Add(TEXT("hash"),     MakeShareable(new FJsonValueString(GameInstance->GetCurrentClientHash())));

    BodyData.Add(TEXT("characterId"), MakeShareable(new FJsonValueNumber(CharacterId)));
    BodyData.Add(TEXT("mobSlug"),     MakeShareable(new FJsonValueString(MobSlug)));

    FString JsonString = JSONParser::SerializeJsonWithTimeSync(
        TEXT("getBestiaryEntry"), HeaderData, BodyData,
        GameInstance->GetTimeSyncService(), EServerType::ChunkServer);

    NetworkManager->SendDataToChunkServer(JsonString);

    UE_LOG(LogTemp, Log, TEXT("BestiaryNetworkHandler: Sent getBestiaryEntry request for mob slug '%s'"), *MobSlug);
}

bool UBestiaryNetworkHandler::HasCachedEntry(const FString& MobSlug) const
{
    return EntryCache.Contains(MobSlug);
}

FBestiaryEntryStruct UBestiaryNetworkHandler::GetCachedEntry(const FString& MobSlug) const
{
    const FBestiaryEntryStruct* Found = EntryCache.Find(MobSlug);
    return Found ? *Found : FBestiaryEntryStruct();
}

void UBestiaryNetworkHandler::InvalidateCacheEntry(const FString& MobSlug)
{
    EntryCache.Remove(MobSlug);
}

TArray<FBestiaryOverviewEntryStruct> UBestiaryNetworkHandler::ConsumePendingOverview()
{
    TArray<FBestiaryOverviewEntryStruct> Result = MoveTemp(PendingOverview);
    bHasPendingOverview = false;
    return Result;
}


void UBestiaryNetworkHandler::HandleChunkServerData(const FString& ReceivedData)
{
    if (ReceivedData.IsEmpty())
        return;

    FMessageDataStruct MessageData = JSONParser::DeserializeMessageData(ReceivedData);

    if (MessageData.eventType == TEXT("getBestiaryOverview") && MessageData.status == TEXT("success"))
    {
        ProcessBestiaryOverviewResponse(ReceivedData);
    }
    else if (MessageData.eventType == TEXT("getBestiaryEntry") && MessageData.status == TEXT("success"))
    {
        ProcessBestiaryEntryResponse(ReceivedData);
    }
    else if (MessageData.eventType == TEXT("world_notification"))
    {
        // Parse just enough to detect bestiary_tier_unlocked
        TSharedPtr<FJsonObject> Root;
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ReceivedData);
        if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
            return;

        const TSharedPtr<FJsonObject>* BodyPtr = nullptr;
        if (!Root->TryGetObjectField(TEXT("body"), BodyPtr) || !(*BodyPtr).IsValid())
            return;

        FString NotifType;
        (*BodyPtr)->TryGetStringField(TEXT("notificationType"), NotifType);

        if (NotifType == TEXT("bestiary_tier_unlocked") || NotifType == TEXT("bestiary_kill_update"))
        {
            FWorldNotificationStruct Notif;
            (*BodyPtr)->TryGetNumberField(TEXT("characterId"),      Notif.characterId);
            (*BodyPtr)->TryGetStringField(TEXT("notificationId"),   Notif.notificationId);
            Notif.notificationType = NotifType;
            (*BodyPtr)->TryGetStringField(TEXT("priority"),         Notif.priority);
            (*BodyPtr)->TryGetStringField(TEXT("channel"),          Notif.channel);
            (*BodyPtr)->TryGetStringField(TEXT("text"), Notif.text);

            const TSharedPtr<FJsonObject>* DataPtr = nullptr;
            if ((*BodyPtr)->TryGetObjectField(TEXT("data"), DataPtr) && (*DataPtr).IsValid())
            {
                for (const auto& Pair : (*DataPtr)->Values)
                {
                    FString Val;
                    if (Pair.Value->Type == EJson::String)
                        Val = Pair.Value->AsString();
                    else if (Pair.Value->Type == EJson::Number)
                    {
                        double N = 0.0;
                        Pair.Value->TryGetNumber(N);
                        Val = FString::SanitizeFloat(N);
                    }
                    Notif.dataFields.Add(Pair.Key, Val);
                }
            }

            if (NotifType == TEXT("bestiary_kill_update"))
                ProcessBestiaryKillUpdate(Notif);
            else
                ProcessBestiaryTierUnlocked(Notif);
        }
    }
}

void UBestiaryNetworkHandler::ProcessBestiaryOverviewResponse(const FString& JsonData)
{
    TSharedPtr<FJsonObject> Root;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonData);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("BestiaryNetworkHandler: Failed to deserialize getBestiaryOverview response"));
        return;
    }

    const TSharedPtr<FJsonObject>* BodyPtr = nullptr;
    if (!Root->TryGetObjectField(TEXT("body"), BodyPtr) || !(*BodyPtr).IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("BestiaryNetworkHandler: getBestiaryOverview response missing body"));
        return;
    }

    const TArray<TSharedPtr<FJsonValue>>* EntriesArray = nullptr;
    TArray<FBestiaryOverviewEntryStruct> Overview;

    if ((*BodyPtr)->TryGetArrayField(TEXT("entries"), EntriesArray) && EntriesArray)
    {
        for (const TSharedPtr<FJsonValue>& EntryVal : *EntriesArray)
        {
            TSharedPtr<FJsonObject> EntryObj = EntryVal->AsObject();
            if (!EntryObj.IsValid())
                continue;

            FBestiaryOverviewEntryStruct OverviewEntry;
            EntryObj->TryGetStringField(TEXT("mobSlug"),   OverviewEntry.mobSlug);
            EntryObj->TryGetNumberField(TEXT("killCount"), OverviewEntry.killCount);
            Overview.Add(OverviewEntry);
        }
    }

    UE_LOG(LogTemp, Log, TEXT("BestiaryNetworkHandler: Received overview with %d mob(s)"), Overview.Num());

    // Cache in case BestiaryWidget binds after this broadcast
    PendingOverview = Overview;
    bHasPendingOverview = true;

    OnBestiaryOverviewReceived.Broadcast(Overview);
}

void UBestiaryNetworkHandler::ProcessBestiaryEntryResponse(const FString& JsonData)
{
    TSharedPtr<FJsonObject> Root;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonData);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("BestiaryNetworkHandler: Failed to deserialize getBestiaryEntry response"));
        return;
    }

    const TSharedPtr<FJsonObject>* BodyPtr = nullptr;
    if (!Root->TryGetObjectField(TEXT("body"), BodyPtr) || !(*BodyPtr).IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("BestiaryNetworkHandler: getBestiaryEntry response missing body"));
        return;
    }

    const TSharedPtr<FJsonObject>* EntryObjPtr = nullptr;
    if (!(*BodyPtr)->TryGetObjectField(TEXT("entry"), EntryObjPtr) || !(*EntryObjPtr).IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("BestiaryNetworkHandler: getBestiaryEntry response missing entry"));
        return;
    }

    FBestiaryEntryStruct Entry;
    (*EntryObjPtr)->TryGetStringField(TEXT("mobSlug"),   Entry.mobSlug);
    (*EntryObjPtr)->TryGetNumberField(TEXT("killCount"), Entry.killCount);

    const TArray<TSharedPtr<FJsonValue>>* TiersArray = nullptr;
    if ((*EntryObjPtr)->TryGetArrayField(TEXT("tiers"), TiersArray) && TiersArray)
    {
        for (const TSharedPtr<FJsonValue>& TierVal : *TiersArray)
        {
            TSharedPtr<FJsonObject> TierObj = TierVal->AsObject();
            if (TierObj.IsValid())
                Entry.tiers.Add(ParseTier(TierObj));
        }
    }

    // Update cache (v1.2: keyed by mobSlug)
    EntryCache.Add(Entry.mobSlug, Entry);

    UE_LOG(LogTemp, Log, TEXT("BestiaryNetworkHandler: Received entry for mob '%s', killCount=%d, tiers=%d"),
        *Entry.mobSlug, Entry.killCount, Entry.tiers.Num());

    OnBestiaryEntryReceived.Broadcast(Entry);
}

FBestiaryTierStruct UBestiaryNetworkHandler::ParseTier(const TSharedPtr<FJsonObject>& TierObj)
{
    FBestiaryTierStruct Tier;
    TierObj->TryGetNumberField(TEXT("tier"),             Tier.tier);
    TierObj->TryGetStringField(TEXT("categorySlug"),     Tier.categorySlug);
    TierObj->TryGetNumberField(TEXT("requiredKills"),    Tier.requiredKills);
    TierObj->TryGetBoolField(TEXT("unlocked"),           Tier.unlocked);
    TierObj->TryGetNumberField(TEXT("requiredKillsLeft"), Tier.requiredKillsLeft);

    if (!Tier.unlocked)
        return Tier;

    const TSharedPtr<FJsonObject>* DataPtr = nullptr;
    if (!TierObj->TryGetObjectField(TEXT("data"), DataPtr) || !(*DataPtr).IsValid())
        return Tier;

    const TSharedPtr<FJsonObject>& Data = *DataPtr;

    if (Tier.categorySlug == TEXT("basic_info"))
    {
        Data->TryGetNumberField(TEXT("level"),   Tier.level);
        Data->TryGetStringField(TEXT("rank"),    Tier.rank);
        Data->TryGetNumberField(TEXT("hpMin"),   Tier.hpMin);
        Data->TryGetNumberField(TEXT("hpMax"),   Tier.hpMax);
        Data->TryGetStringField(TEXT("type"),    Tier.mobType);
        Data->TryGetStringField(TEXT("biomeSlug"), Tier.biomeSlug);
    }
    else if (Tier.categorySlug == TEXT("lore"))
    {
        Data->TryGetStringField(TEXT("loreKey"), Tier.loreKey);
    }
    else if (Tier.categorySlug == TEXT("combat_info"))
    {
        const TArray<TSharedPtr<FJsonValue>>* WeaknessArr = nullptr;
        if (Data->TryGetArrayField(TEXT("weaknesses"), WeaknessArr) && WeaknessArr)
        {
            for (const TSharedPtr<FJsonValue>& V : *WeaknessArr)
                Tier.weaknesses.Add(V->AsString());
        }

        const TArray<TSharedPtr<FJsonValue>>* ResistArr = nullptr;
        if (Data->TryGetArrayField(TEXT("resistances"), ResistArr) && ResistArr)
        {
            for (const TSharedPtr<FJsonValue>& V : *ResistArr)
                Tier.resistances.Add(V->AsString());
        }

        const TArray<TSharedPtr<FJsonValue>>* AbilitiesArr = nullptr;
        if (Data->TryGetArrayField(TEXT("abilities"), AbilitiesArr) && AbilitiesArr)
        {
            for (const TSharedPtr<FJsonValue>& V : *AbilitiesArr)
                Tier.abilities.Add(V->AsString());
        }
    }
    else if (Tier.categorySlug == TEXT("loot_table"))
    {
        const TArray<TSharedPtr<FJsonValue>>* ItemsArr = nullptr;
        if (Data->TryGetArrayField(TEXT("items"), ItemsArr) && ItemsArr)
        {
            for (const TSharedPtr<FJsonValue>& V : *ItemsArr)
                Tier.lootItems.Add(V->AsString());
        }
    }
    else if (Tier.categorySlug == TEXT("drop_rates"))
    {
        const TArray<TSharedPtr<FJsonValue>>* LootArr = nullptr;
        if (Data->TryGetArrayField(TEXT("loot"), LootArr) && LootArr)
        {
            for (const TSharedPtr<FJsonValue>& LootVal : *LootArr)
            {
                TSharedPtr<FJsonObject> LootObj = LootVal->AsObject();
                if (!LootObj.IsValid()) continue;

                FBestiaryLootEntryStruct LootEntry;
                LootObj->TryGetStringField(TEXT("itemSlug"), LootEntry.itemSlug);
                double Chance = 0.0;
                LootObj->TryGetNumberField(TEXT("chance"), Chance);
                LootEntry.chance = static_cast<float>(Chance);
                Tier.loot.Add(LootEntry);
            }
        }
    }
    else if (Tier.categorySlug == TEXT("hunter_mastery"))
    {
        Data->TryGetStringField(TEXT("titleSlug"),       Tier.titleSlug);
        Data->TryGetStringField(TEXT("achievementSlug"), Tier.achievementSlug);
    }

    return Tier;
}

void UBestiaryNetworkHandler::ProcessBestiaryTierUnlocked(const FWorldNotificationStruct& Notification)
{
    FString MobSlug;
    FString UnlockedTierStr;
    FString CategorySlug;

    Notification.dataFields.Contains(TEXT("mobSlug"))      ? MobSlug        = Notification.dataFields[TEXT("mobSlug")]      : MobSlug        = TEXT("");
    Notification.dataFields.Contains(TEXT("unlockedTier")) ? UnlockedTierStr = Notification.dataFields[TEXT("unlockedTier")] : UnlockedTierStr = TEXT("0");
    Notification.dataFields.Contains(TEXT("categorySlug")) ? CategorySlug    = Notification.dataFields[TEXT("categorySlug")] : CategorySlug    = TEXT("");

    const int32 UnlockedTier = FCString::Atoi(*UnlockedTierStr);

    // Update killCount in the cached entry (server is source of truth)
    const FString KillCountStr = Notification.dataFields.FindRef(TEXT("killCount"));
    const int32 KillCount = KillCountStr.IsEmpty() ? 0 : FCString::Atoi(*KillCountStr);
    if (FBestiaryEntryStruct* Cached = EntryCache.Find(MobSlug))
        Cached->killCount = KillCount;

    UE_LOG(LogTemp, Log, TEXT("BestiaryNetworkHandler: bestiary_tier_unlocked mob='%s' tier=%d (%s) killCount=%d"),
        *MobSlug, UnlockedTier, *CategorySlug, KillCount);

    // Invalidate cache so next UI open triggers a fresh request
    InvalidateCacheEntry(MobSlug);

    OnBestiaryTierUnlocked.Broadcast(MobSlug, UnlockedTier, CategorySlug);

    // Also fire the kill-count update delegate so the overview list stays in sync
    OnBestiaryKillCountUpdated.Broadcast(MobSlug, KillCount);
}

void UBestiaryNetworkHandler::ProcessBestiaryKillUpdate(const FWorldNotificationStruct& Notification)
{
    const FString MobSlug      = Notification.dataFields.FindRef(TEXT("mobSlug"));
    const FString KillCountStr = Notification.dataFields.FindRef(TEXT("killCount"));
    const int32   KillCount    = KillCountStr.IsEmpty() ? 0 : FCString::Atoi(*KillCountStr);

    if (MobSlug.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("BestiaryNetworkHandler: bestiary_kill_update missing mobSlug"));
        return;
    }

    // Update killCount in the cached full entry if present (no cache invalidation needed)
    if (FBestiaryEntryStruct* Cached = EntryCache.Find(MobSlug))
        Cached->killCount = KillCount;

    UE_LOG(LogTemp, Verbose, TEXT("BestiaryNetworkHandler: bestiary_kill_update mob='%s' killCount=%d"),
        *MobSlug, KillCount);

    OnBestiaryKillCountUpdated.Broadcast(MobSlug, KillCount);
}
