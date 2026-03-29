#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Data/DataStructs.h"
#include "ChatNetworkHandler.generated.h"

// Forward declarations
class UChatManager;
class UNetworkManager;

/**
 * ChatNetworkHandler
 *
 * Subscribes to OnChunkServerDataReceived and routes chatMessage packets
 * (both broadcast messages and whisper-error responses) to UChatManager.
 */
UCLASS(BlueprintType)
class PROTOTYPING_API UChatNetworkHandler : public UObject
{
    GENERATED_BODY()

public:
    UChatNetworkHandler();

    UFUNCTION(BlueprintCallable, Category = "Chat Network")
    void Initialize(UChatManager* InChatManager, UNetworkManager* InNetworkManager);

    UFUNCTION(BlueprintCallable, Category = "Chat Network")
    void SubscribeToNetworkEvents();

    UFUNCTION(BlueprintCallable, Category = "Chat Network")
    void UnsubscribeFromNetworkEvents();

private:
    UFUNCTION()
    void HandleChunkServerData(const FString& ReceivedData);

    FChatMessageStruct ParseChatMessage(const TSharedPtr<FJsonObject>& Root) const;

    UPROPERTY()
    UChatManager* ChatManager = nullptr;

    UPROPERTY()
    UNetworkManager* NetworkManager = nullptr;

    bool bIsSubscribed = false;
};
