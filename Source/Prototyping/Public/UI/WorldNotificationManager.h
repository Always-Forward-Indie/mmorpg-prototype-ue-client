#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Data/DataStructs.h"
#include "WorldNotificationManager.generated.h"

class UNetworkManager;
class UMyGameInstance;
class UBestiaryWidget;
class UBestiaryNetworkHandler;
class UNotificationToastWidget;
class UNotificationZoneBannerWidget;
class UNotificationScreenCenterWidget;
class UNotificationAtmosphereWidget;

/**
 * WorldNotificationManager
 *
 * Subscribes to chunk-server data and routes world_notification packets
 * to the correct UI widget based on the "channel" field:
 *
 *   channel            ? widget
 *   ?????????????????????????????????????????????????????
 *   "toast"            ? NotificationToastWidget
 *   "zone_banner"      ? NotificationZoneBannerWidget
 *   "screen_center"    ? NotificationScreenCenterWidget
 *   "atmosphere"       ? NotificationAtmosphereWidget
 *   "float_text"       ? reuses NotificationToastWidget (small)
 *   "chat_log"         ? logs only (no widget required)
 *
 * Also handles bestiary_tier_unlocked forwarding to BestiaryWidget for
 * cache invalidation while forwarding the event to BestiaryNetworkHandler.
 *
 * Usage:
 *   1. CreateSubobject / NewObject in UIManager.
 *   2. Call Initialize() with all widget and handler references.
 *   3. Call SubscribeToNetworkEvents() once NetworkManager is ready.
 */
UCLASS(BlueprintType, Blueprintable)
class PROTOTYPING_API UWorldNotificationManager : public UObject
{
    GENERATED_BODY()

public:
    UWorldNotificationManager();

    UFUNCTION(BlueprintCallable, Category = "World Notification")
    void Initialize(
        UMyGameInstance*                   InGameInstance,
        UNetworkManager*                   InNetworkManager,
        UBestiaryNetworkHandler*           InBestiaryHandler,
        UNotificationToastWidget*          InToastWidget,
        UNotificationZoneBannerWidget*     InZoneBannerWidget,
        UNotificationScreenCenterWidget*   InScreenCenterWidget,
        UNotificationAtmosphereWidget*     InAtmosphereWidget,
        UBestiaryWidget*                   InBestiaryWidget);

    UFUNCTION(BlueprintCallable, Category = "World Notification")
    void Shutdown();

    UFUNCTION(BlueprintCallable, Category = "World Notification")
    void SubscribeToNetworkEvents();

    /**
     * Subscribe to the network manager early (before Initialize is called).
     * Incoming world_notification packets are buffered until Initialize() is called.
     */
    void SubscribeToNetworkEvents_Early(UNetworkManager* InNetworkManager);

    UFUNCTION(BlueprintCallable, Category = "World Notification")
    void UnsubscribeFromNetworkEvents();

    /** Access the toast widget for direct enqueuing (e.g. item pickup notifications). */
    UNotificationToastWidget* GetToastWidget() const { return ToastWidget; }

private:
    UFUNCTION()
    void HandleChunkServerData(const FString& ReceivedData);

    void ProcessWorldNotification(const FString& JsonData);
    void ProcessDialogueActionNotification(const FString& JsonData, const FString& EventType);
    void DispatchNotification(const FWorldNotificationStruct& Notification);

    UPROPERTY()
    UMyGameInstance* GameInstance = nullptr;

    UPROPERTY()
    UNetworkManager* NetworkManager = nullptr;

    UPROPERTY()
    UBestiaryNetworkHandler* BestiaryHandler = nullptr;

    UPROPERTY()
    UNotificationToastWidget* ToastWidget = nullptr;

    UPROPERTY()
    UNotificationZoneBannerWidget* ZoneBannerWidget = nullptr;

    UPROPERTY()
    UNotificationScreenCenterWidget* ScreenCenterWidget = nullptr;

    UPROPERTY()
    UNotificationAtmosphereWidget* AtmosphereWidget = nullptr;

    UPROPERTY()
    UBestiaryWidget* BestiaryWidget = nullptr;

    // Deduplication: track last processed notificationId
    FString LastProcessedNotificationId;

    // Buffer for world_notification packets that arrive before Initialize() is called.
    // Flushed once all widget references are set.
    TArray<FString> PendingRawNotifications;
    bool bIsInitialized = false;

    bool bIsSubscribed = false;
};
