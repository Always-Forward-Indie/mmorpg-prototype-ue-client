#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Data/DataStructs.h"
#include "EmoteNetworkHandler.generated.h"

class UEmoteManager;
class UNetworkManager;
class UMyGameInstance;

/**
 * Subscribes to the chunk-server feed and routes emote packets to UEmoteManager.
 * Handles outbound useEmote requests.
 *
 * Handled inbound events:
 *   player_emotes  — unlocked emote list for the local character (on join)
 *   emoteAction    — broadcast: a character in the zone played an emote
 *
 * Outbound requests:
 *   useEmote       — request emote playback (server validates unlock)
 */
UCLASS()
class PROTOTYPING_API UEmoteNetworkHandler : public UObject
{
    GENERATED_BODY()

public:
    void Initialize(UEmoteManager* InManager, UNetworkManager* InNetworkManager, UMyGameInstance* InGameInstance);

    UFUNCTION(BlueprintCallable, Category = "Emotes Network")
    void SubscribeToNetworkEvents();

    UFUNCTION(BlueprintCallable, Category = "Emotes Network")
    void UnsubscribeFromNetworkEvents();

    /**
     * Send a useEmote request to the chunk server.
     * Server validates that the character owns this emote before broadcasting.
     */
    UFUNCTION(BlueprintCallable, Category = "Emotes Network")
    void RequestUseEmote(const FString& EmoteSlug);

private:
    UFUNCTION()
    void HandleChunkServerData(const FString& ReceivedData);

    void HandlePlayerEmotes(const TSharedPtr<FJsonObject>& Root);
    void HandleEmoteAction (const TSharedPtr<FJsonObject>& Root);

    static FEmoteDefinitionData ParseEmoteDefinition(const TSharedPtr<FJsonObject>& Obj);

    UPROPERTY()
    UEmoteManager* Manager = nullptr;

    UPROPERTY()
    UNetworkManager* NetworkManager = nullptr;

    UPROPERTY()
    UMyGameInstance* GameInstance = nullptr;

    int32 LocalCharacterId = 0;
    bool  bIsSubscribed    = false;

    double LastEmoteSendTime = 0.0;
    static constexpr double EmoteCooldownSeconds = 2.0;
};
