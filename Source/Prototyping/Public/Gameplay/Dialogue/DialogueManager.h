#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Data/DataStructs.h"
#include "DialogueManager.generated.h"

// Forward declarations
class UNetworkManager;
class UMyGameInstance;
class UDialogueWidget;

// Fired when a new dialogue node arrives from the server
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDialogueNodeReceived, const FDialogueNodeData&, NodeData);
// Fired when the dialogue session closes (end-node, player close, TTL)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDialogueSessionClosed, const FString&, SessionId);
// Fired when the server returns a dialogue error
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDialogueError, const FDialogueErrorData&, ErrorData);

/**
 * DialogueManager
 *
 * Owns the active dialogue session state and exposes:
 *   - OpenDialogue()      — sends npcInteract to Chunk Server
 *   - SendChoice()        — sends dialogueChoice to Chunk Server
 *   - CloseDialogue()     — sends dialogueClose to Chunk Server
 *
 * Receives DIALOGUE_NODE / DIALOGUE_CLOSE / dialogueError via
 * DialogueNetworkHandler and broadcasts events to UI.
 */
UCLASS(BlueprintType)
class PROTOTYPING_API UDialogueManager : public UObject
{
    GENERATED_BODY()

public:
    UDialogueManager();

    // Initialization (called from MyGameInstance)
    UFUNCTION(BlueprintCallable, Category = "Dialogue")
    void Initialize(UNetworkManager* InNetworkManager, UMyGameInstance* InGameInstance);

    // --- Client ? Server requests ---

    // Send npcInteract packet: clientId taken from GameInstance
    UFUNCTION(BlueprintCallable, Category = "Dialogue")
    void OpenDialogue(int32 NpcId);

    // Send dialogueChoice packet with the selected edgeId
    UFUNCTION(BlueprintCallable, Category = "Dialogue")
    void SendChoice(int32 EdgeId);

    // Send dialogueClose packet and clear local session
    UFUNCTION(BlueprintCallable, Category = "Dialogue")
    void CloseDialogue();

    // --- Called by DialogueNetworkHandler ---

    void OnNodeReceived(const FDialogueNodeData& NodeData);
    void OnSessionClosed(const FString& SessionId);
    void OnErrorReceived(const FDialogueErrorData& ErrorData);

    // --- Queries ---

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Dialogue")
    bool IsDialogueActive() const { return bSessionActive; }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Dialogue")
    FString GetCurrentSessionId() const { return CurrentSessionId; }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Dialogue")
    int32 GetCurrentNpcId() const { return CurrentNpcId; }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Dialogue")
    FDialogueNodeData GetCurrentNode() const { return CurrentNode; }

    // --- Events for Blueprint / UI ---

    UPROPERTY(BlueprintAssignable, Category = "Dialogue Events")
    FOnDialogueNodeReceived OnDialogueNodeReceived;

    UPROPERTY(BlueprintAssignable, Category = "Dialogue Events")
    FOnDialogueSessionClosed OnDialogueSessionClosed;

    UPROPERTY(BlueprintAssignable, Category = "Dialogue Events")
    FOnDialogueError OnDialogueError;

private:
    UPROPERTY()
    UNetworkManager* NetworkManager = nullptr;

    UPROPERTY()
    UMyGameInstance* GameInstance = nullptr;

    bool     bSessionActive   = false;
    FString  CurrentSessionId = "";
    int32    CurrentNpcId     = 0;
    FDialogueNodeData CurrentNode;
};
