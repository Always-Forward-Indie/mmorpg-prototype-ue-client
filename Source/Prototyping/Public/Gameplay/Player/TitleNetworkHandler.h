#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Data/DataStructs.h"
#include "TitleNetworkHandler.generated.h"

class UTitleManager;
class UNetworkManager;
class UMyGameInstance;

/**
 * Subscribes to the chunk-server feed and routes title packets to UTitleManager.
 * Also provides outgoing request methods (getTitles, equipTitle).
 *
 * Handled inbound events:
 *   player_titles_update  — full state snapshot (response to getTitles /
 *                           setPlayerTitlesData / successful equipTitle)
 *   PLAYER_TITLE_CHANGED  — broadcast when another player equips/removes a title;
 *                           updates their nameplate without touching TitleManager.
 *
 * Outbound requests:
 *   getTitles   — request full title list from server
 *   equipTitle  — equip (or remove) a title by slug
 */
UCLASS()
class PROTOTYPING_API UTitleNetworkHandler : public UObject
{
    GENERATED_BODY()

public:
    void Initialize(UTitleManager* InManager, UNetworkManager* InNetworkManager, UMyGameInstance* InGameInstance);

    UFUNCTION(BlueprintCallable, Category = "Titles Network")
    void SubscribeToNetworkEvents();

    UFUNCTION(BlueprintCallable, Category = "Titles Network")
    void UnsubscribeFromNetworkEvents();

    /** Request the full title list from the server. */
    UFUNCTION(BlueprintCallable, Category = "Titles Network")
    void RequestGetTitles(int32 CharacterId);

    /**
     * Equip the given title slug (or pass "" to remove the current title).
     * The server responds with player_titles_update on success, or equipTitle error on failure.
     */
    UFUNCTION(BlueprintCallable, Category = "Titles Network")
    void RequestEquipTitle(int32 CharacterId, const FString& TitleSlug);

private:
    UFUNCTION()
    void HandleChunkServerData(const FString& ReceivedData);

    /** Handle player_titles_update — own character title state from server. */
    void HandleOwnTitlesUpdate(const TSharedPtr<FJsonObject>& Root);

    /** Handle PLAYER_TITLE_CHANGED — update a remote player's nameplate title. */
    void HandleRemoteTitleChanged(const TSharedPtr<FJsonObject>& Root);

    FPlayerTitlesState ParseTitlesState(const TSharedPtr<FJsonObject>& Body) const;
    static FTitleEntry ParseTitleEntry (const TSharedPtr<FJsonObject>& Obj);

    UPROPERTY()
    UTitleManager* Manager = nullptr;

    UPROPERTY()
    UNetworkManager* NetworkManager = nullptr;

    UPROPERTY()
    UMyGameInstance* GameInstance = nullptr;

    int32 LocalCharacterId = 0;
    bool  bIsSubscribed    = false;
};
