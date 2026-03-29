#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Data/DataStructs.h"
#include "ChatManager.generated.h"

// Forward declarations
class UNetworkManager;
class UMyGameInstance;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnChatMessageReceived, const FChatMessageStruct&, Message);

/**
 * ChatManager
 *
 * Owns the chat message log and exposes SendChatMessage() for the UI.
 * Inbound messages arrive via UChatNetworkHandler and are forwarded here.
 */
UCLASS(BlueprintType)
class PROTOTYPING_API UChatManager : public UObject
{
    GENERATED_BODY()

public:
    UChatManager();

    UFUNCTION(BlueprintCallable, Category = "Chat")
    void Initialize(UMyGameInstance* InGameInstance, UNetworkManager* InNetworkManager);

    // Called by the UI to send a message
    UFUNCTION(BlueprintCallable, Category = "Chat")
    void SendChatMessage(const FString& Channel, const FString& Text, const FString& TargetName = TEXT(""));

    // Called by ChatNetworkHandler on receiving a broadcast or error
    void OnMessageReceived(const FChatMessageStruct& Message);

    // Broadcast to all UI listeners
    UPROPERTY(BlueprintAssignable, Category = "Chat")
    FOnChatMessageReceived OnChatMessageReceived;

    // Read-only access to the message log
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Chat")
    const TArray<FChatMessageStruct>& GetMessageLog() const { return MessageLog; }

    // Maximum number of messages kept in memory
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chat")
    int32 MaxLogSize = 200;

private:
    UPROPERTY()
    UMyGameInstance* GameInstance = nullptr;

    UPROPERTY()
    UNetworkManager* NetworkManager = nullptr;

    TArray<FChatMessageStruct> MessageLog;
};
