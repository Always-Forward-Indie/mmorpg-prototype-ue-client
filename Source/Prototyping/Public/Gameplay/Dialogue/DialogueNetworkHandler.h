#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Data/DataStructs.h"
#include "Networking/NetworkDispatcher.h"
#include "DialogueNetworkHandler.generated.h"

// Forward declarations
class UDialogueManager;
class UNetworkManager;

/**
 * DialogueNetworkHandler
 *
 * Subscribes to OnChunkServerDataReceived and routes:
 *   DIALOGUE_NODE    ? DialogueManager::OnNodeReceived
 *   DIALOGUE_CLOSE   ? DialogueManager::OnSessionClosed
 *   dialogueError    ? DialogueManager::OnErrorReceived
 */
UCLASS(BlueprintType)
class PROTOTYPING_API UDialogueNetworkHandler : public UObject
{
    GENERATED_BODY()

public:
    UDialogueNetworkHandler();

    UFUNCTION(BlueprintCallable, Category = "Dialogue Network")
    void Initialize(UDialogueManager* InDialogueManager, UNetworkManager* InNetworkManager);

    UFUNCTION(BlueprintCallable, Category = "Dialogue Network")
    void SubscribeToNetworkEvents();

    UFUNCTION(BlueprintCallable, Category = "Dialogue Network")
    void UnsubscribeFromNetworkEvents();

private:
    // Packet entry (S05): single-parse path via the per-eventType dispatcher.
    // Replaces HandleChunkServerData (which re-parsed every packet to filter).
    void HandleChunkPacket(const FParsedPacket& Packet);

    // EventTypes owned by this handler (single list drives dispatcher registration).
    static const TArray<FString>& HandledEvents();

    // Parsers
    FDialogueNodeData  ParseDialogueNode(const TSharedPtr<FJsonObject>& Root)       const;
    FDialogueErrorData ParseDialogueError(const TSharedPtr<FJsonObject>& Root)      const;
    FQuestPreviewData  ParseQuestPreview(const TSharedPtr<FJsonObject>& Obj)        const;

    UPROPERTY()
    UDialogueManager* DialogueManager = nullptr;

    UPROPERTY()
    UNetworkManager* NetworkManager = nullptr;

    bool bIsSubscribed = false;
};
