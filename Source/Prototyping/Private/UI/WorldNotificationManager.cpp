#include "UI/WorldNotificationManager.h"
#include "UI/BestiaryWidget.h"
#include "UI/NotificationToastWidget.h"
#include "UI/NotificationZoneBannerWidget.h"
#include "UI/NotificationScreenCenterWidget.h"
#include "UI/NotificationAtmosphereWidget.h"
#include "Gameplay/Bestiary/BestiaryNetworkHandler.h"
#include "Networking/NetworkManager.h"
#include "Utils/JSONParser.h"
#include "MyGameInstance.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"

UWorldNotificationManager::UWorldNotificationManager()
{
}

void UWorldNotificationManager::Initialize(
    UMyGameInstance*                   InGameInstance,
    UNetworkManager*                   InNetworkManager,
    UBestiaryNetworkHandler*           InBestiaryHandler,
    UNotificationToastWidget*          InToastWidget,
    UNotificationZoneBannerWidget*     InZoneBannerWidget,
    UNotificationScreenCenterWidget*   InScreenCenterWidget,
    UNotificationAtmosphereWidget*     InAtmosphereWidget,
    UBestiaryWidget*                   InBestiaryWidget)
{
    GameInstance       = InGameInstance;
    NetworkManager     = InNetworkManager;
    BestiaryHandler    = InBestiaryHandler;
    ToastWidget        = InToastWidget;
    ZoneBannerWidget   = InZoneBannerWidget;
    ScreenCenterWidget = InScreenCenterWidget;
    AtmosphereWidget   = InAtmosphereWidget;
    BestiaryWidget     = InBestiaryWidget;

    bIsInitialized = true;

    UE_LOG(LogTemp, Log, TEXT("WorldNotificationManager: Initialized"));

    // Flush notifications that arrived before the widgets were ready
    if (PendingRawNotifications.Num() > 0)
    {
        UE_LOG(LogTemp, Log, TEXT("WorldNotificationManager: Flushing %d pending notification(s)"),
            PendingRawNotifications.Num());

        // Copy and clear before processing to avoid re-entrant additions
        TArray<FString> Pending = MoveTemp(PendingRawNotifications);
        PendingRawNotifications.Empty();

        for (const FString& Raw : Pending)
        {
            FMessageDataStruct Msg = JSONParser::DeserializeMessageData(Raw);
            if (Msg.eventType == TEXT("world_notification"))
            {
                ProcessWorldNotification(Raw);
            }
            else
            {
                ProcessDialogueActionNotification(Raw, Msg.eventType);
            }
        }
    }
}

void UWorldNotificationManager::Shutdown()
{
    UnsubscribeFromNetworkEvents();
    GameInstance       = nullptr;
    NetworkManager     = nullptr;
    BestiaryHandler    = nullptr;
    ToastWidget        = nullptr;
    ZoneBannerWidget   = nullptr;
    ScreenCenterWidget = nullptr;
    AtmosphereWidget   = nullptr;
    BestiaryWidget     = nullptr;
    bIsInitialized     = false;
    PendingRawNotifications.Empty();
}

void UWorldNotificationManager::SubscribeToNetworkEvents()
{
    if (!NetworkManager || !IsValid(NetworkManager))
    {
        UE_LOG(LogTemp, Error, TEXT("WorldNotificationManager: Cannot subscribe — NetworkManager invalid"));
        return;
    }

    if (bIsSubscribed) return;

    NetworkManager->OnChunkServerDataReceived.RemoveDynamic(this, &UWorldNotificationManager::HandleChunkServerData);
    NetworkManager->OnChunkServerDataReceived.AddDynamic(this, &UWorldNotificationManager::HandleChunkServerData);
    bIsSubscribed = true;

    UE_LOG(LogTemp, Log, TEXT("WorldNotificationManager: Subscribed to network events"));
}

void UWorldNotificationManager::SubscribeToNetworkEvents_Early(UNetworkManager* InNetworkManager)
{
    if (!InNetworkManager || !IsValid(InNetworkManager))
    {
        UE_LOG(LogTemp, Error, TEXT("WorldNotificationManager: Early subscribe — NetworkManager invalid"));
        return;
    }

    if (bIsSubscribed) return;

    NetworkManager = InNetworkManager;
    NetworkManager->OnChunkServerDataReceived.RemoveDynamic(this, &UWorldNotificationManager::HandleChunkServerData);
    NetworkManager->OnChunkServerDataReceived.AddDynamic(this, &UWorldNotificationManager::HandleChunkServerData);
    bIsSubscribed = true;

    UE_LOG(LogTemp, Log, TEXT("WorldNotificationManager: Subscribed early to network events (buffering until Initialize)"));
}

void UWorldNotificationManager::UnsubscribeFromNetworkEvents()
{
    if (!NetworkManager || !IsValid(NetworkManager) || !bIsSubscribed) return;
    NetworkManager->OnChunkServerDataReceived.RemoveDynamic(this, &UWorldNotificationManager::HandleChunkServerData);
    bIsSubscribed = false;
}

void UWorldNotificationManager::HandleChunkServerData(const FString& ReceivedData)
{
    if (ReceivedData.IsEmpty()) return;

    FMessageDataStruct MessageData = JSONParser::DeserializeMessageData(ReceivedData);

    if (MessageData.eventType == TEXT("world_notification"))
    {
        if (!bIsInitialized)
        {
            // Widgets not ready yet — buffer for replay after Initialize()
            PendingRawNotifications.Add(ReceivedData);
            return;
        }
        ProcessWorldNotification(ReceivedData);
        return;
    }

    // Dialogue action notifications arrive with their own eventType.
    // Route them to the toast widget as visual feedback.
    static const TSet<FString> DialogueActionTypes = {
        TEXT("quest_offered"),
        TEXT("quest_turned_in"),
        TEXT("quest_failed"),
        TEXT("item_received"),
        TEXT("exp_received"),
        TEXT("gold_received"),
        TEXT("skill_learned"),
        TEXT("learn_skill_failed"),
        TEXT("reputationChanged")
    };

    if (DialogueActionTypes.Contains(MessageData.eventType))
    {
        if (!bIsInitialized)
        {
            PendingRawNotifications.Add(ReceivedData);
            return;
        }
        ProcessDialogueActionNotification(ReceivedData, MessageData.eventType);
    }
}

void UWorldNotificationManager::ProcessWorldNotification(const FString& JsonData)
{
    TSharedPtr<FJsonObject> Root;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonData);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()) return;

    const TSharedPtr<FJsonObject>* BodyPtr = nullptr;
    if (!Root->TryGetObjectField(TEXT("body"), BodyPtr) || !(*BodyPtr).IsValid()) return;

    FWorldNotificationStruct Notif;
    (*BodyPtr)->TryGetNumberField(TEXT("characterId"),      Notif.characterId);
    (*BodyPtr)->TryGetStringField(TEXT("notificationId"),   Notif.notificationId);
    (*BodyPtr)->TryGetStringField(TEXT("notificationType"), Notif.notificationType);
    (*BodyPtr)->TryGetStringField(TEXT("priority"),         Notif.priority);
    (*BodyPtr)->TryGetStringField(TEXT("channel"),          Notif.channel);
    (*BodyPtr)->TryGetStringField(TEXT("text"),             Notif.text);

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
                // Preserve integer look for integer values
                Val = FString::Printf(TEXT("%g"), N);
            }
            else if (Pair.Value->Type == EJson::Boolean)
            {
                Val = Pair.Value->AsBool() ? TEXT("true") : TEXT("false");
            }
            Notif.dataFields.Add(Pair.Key, Val);
        }
    }

    // Deduplicate by notificationId
    if (!Notif.notificationId.IsEmpty() && Notif.notificationId == LastProcessedNotificationId)
        return;
    if (!Notif.notificationId.IsEmpty())
        LastProcessedNotificationId = Notif.notificationId;

    DispatchNotification(Notif);
}

void UWorldNotificationManager::ProcessDialogueActionNotification(const FString& JsonData, const FString& EventType)
{
    TSharedPtr<FJsonObject> Root;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonData);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()) return;

    const TSharedPtr<FJsonObject>* BodyPtr = nullptr;
    if (!Root->TryGetObjectField(TEXT("body"), BodyPtr) || !(*BodyPtr).IsValid()) return;

    // Build a lightweight FWorldNotificationStruct from the dialogue action body
    FWorldNotificationStruct Notif;
    Notif.notificationType = EventType;
    Notif.channel          = TEXT("toast");
    Notif.priority         = TEXT("medium");

    // Extract all body fields into dataFields (flat key-value)
    for (const auto& Pair : (*BodyPtr)->Values)
    {
        FString Val;
        if (Pair.Value->Type == EJson::String)
            Val = Pair.Value->AsString();
        else if (Pair.Value->Type == EJson::Number)
        {
            double N = 0.0;
            Pair.Value->TryGetNumber(N);
            Val = FString::Printf(TEXT("%g"), N);
        }
        else if (Pair.Value->Type == EJson::Boolean)
        {
            Val = Pair.Value->AsBool() ? TEXT("true") : TEXT("false");
        }
        else
        {
            continue; // skip arrays/objects (e.g. items array in openVendorShop)
        }
        Notif.dataFields.Add(Pair.Key, Val);
    }

    // Route to toast widget
    if (ToastWidget)
    {
        ToastWidget->ShowNotification(Notif);
    }
}

void UWorldNotificationManager::DispatchNotification(const FWorldNotificationStruct& Notification)
{
    const FString& Channel = Notification.channel;
    const FString& Type    = Notification.notificationType;

    // --- bestiary_kill_update: silent counter update, no UI shown ---
    if (Type == TEXT("bestiary_kill_update"))
    {
        if (BestiaryWidget)
        {
            const FString MobSlug      = Notification.dataFields.FindRef(TEXT("mobSlug"));
            const FString KillCountStr = Notification.dataFields.FindRef(TEXT("killCount"));
            const int32   KillCount    = KillCountStr.IsEmpty() ? 0 : FCString::Atoi(*KillCountStr);
            BestiaryWidget->AddOrUpdateMobEntry(MobSlug, KillCount);
        }
        // channel == "silent" ? no widget to show, early return
        return;
    }

    // --- bestiary_tier_unlocked: forward to BestiaryWidget for list update
    if (Type == TEXT("bestiary_tier_unlocked") && BestiaryWidget)
    {
        const FString MobSlug      = Notification.dataFields.FindRef(TEXT("mobSlug"));
        const FString KillCountStr = Notification.dataFields.FindRef(TEXT("killCount"));
        const int32   KillCount    = KillCountStr.IsEmpty() ? 0 : FCString::Atoi(*KillCountStr);
        // BestiaryNetworkHandler also subscribes and invalidates its cache — no duplicate needed here.
        BestiaryWidget->AddOrUpdateMobEntry(MobSlug, KillCount);
    }

    // --- Route by channel ---
    if (Channel == TEXT("toast") || Channel == TEXT("float_text"))
    {
        if (ToastWidget)
            ToastWidget->ShowNotification(Notification);
    }
    else if (Channel == TEXT("zone_banner") || Channel == TEXT("banner"))
    {
        if (ZoneBannerWidget)
            ZoneBannerWidget->ShowZoneBanner(Notification);
    }
    else if (Channel == TEXT("screen_center"))
    {
        if (ScreenCenterWidget)
            ScreenCenterWidget->ShowScreenCenter(Notification);
    }
    else if (Channel == TEXT("atmosphere"))
    {
        if (AtmosphereWidget)
            AtmosphereWidget->ShowAtmosphere(Notification);
    }
    else if (Channel == TEXT("chat_log"))
    {
        UE_LOG(LogTemp, Log, TEXT("WorldNotification [chat_log] %s"), *Type);
    }
    else if (Channel == TEXT("silent"))
    {
        // Silent notifications update data only — no visual widget
        UE_LOG(LogTemp, Verbose, TEXT("WorldNotification [silent] %s"), *Type);
    }
    else
    {
        // Unknown channel — graceful fallback to toast if widget available
        UE_LOG(LogTemp, Verbose, TEXT("WorldNotificationManager: Unknown channel '%s' for type '%s', fallback to toast"), *Channel, *Type);
        if (ToastWidget)
            ToastWidget->ShowNotification(Notification);
    }
}
