// Fill out your copyright notice in the Description page of Project Settings.


#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/DataStructs.h"
#include "PlayerNameplateComponent.generated.h"

class UNameplateManager;


/**
 * Thin data-holder component on a remote-player actor.
 *
 * Registers with UNameplateManager on BeginPlay / InitialiseFromCharacterData.
 * All rendering is handled centrally by UNameplateCanvasWidget.
 */
UCLASS(ClassGroup = (UI), meta = (BlueprintSpawnableComponent))
class PROTOTYPING_API UPlayerNameplateComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UPlayerNameplateComponent();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
    UFUNCTION(BlueprintCallable, Category = "Player Nameplate")
    void InitialiseFromCharacterData(const FCharacterDataStruct& CharData,
                                     bool bIsLocal      = false,
                                     bool bForceRefresh = false);

    UFUNCTION(BlueprintCallable, Category = "Player Nameplate")
    void UpdateHealth(int32 CurrentHP, int32 MaxHP);

    UFUNCTION(BlueprintCallable, Category = "Player Nameplate")
    void SetDeadState(bool bNewDead);

    UFUNCTION(BlueprintCallable, Category = "Player Nameplate")
    void UpdateLevel(int32 NewLevel);

    /**
     * Push the equipped title display name to the nameplate.
     * Call this whenever UTitleManager::OnTitlesUpdated fires for the local player,
     * or when another player's title is received from the server.
     *
     * @param InTitle   FTitleEntry::displayName of the equipped title, empty string = no title.
     */
    UFUNCTION(BlueprintCallable, Category = "Player Nameplate")
    void UpdateTitle(const FText& InTitle);

    /**
     * Show a chat speech bubble on this actor's nameplate for the given duration.
     * Silently ignored for the local player (no nameplate entry).
     *
     * @param Text      Message text to display.
     * @param Duration  Seconds until the bubble auto-hides.
     */
    UFUNCTION(BlueprintCallable, Category = "Player Nameplate")
    void ShowChatBubble(const FString& Text, float Duration = 5.0f);

    // ------------------------------------------------------------------ //
    //  Per-actor configuration                                            //
    // ------------------------------------------------------------------ //

    /**
     * Extra Z margin (cm) above the top of the capsule where the nameplate anchors.
     * 0 = nameplate sits exactly at the crown of the capsule.
     * Leave at 0 to use the global default from NameplateCanvasWidget.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player Nameplate",
              meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "200.0"))
    float HeadOffsetZ = 0.0f;

private:
    bool bInitialised   = false;
    bool bIsLocalPlayer = false;
    bool bRegistered    = false;

    FCharacterDataStruct CachedCharData;
    FTimerHandle RetryTimerHandle;

    // Cached so it can be applied after a deferred TryRegister() completes.
    FText PendingTitle;

    void TryRegister();
    UNameplateManager* ResolveNameplateManager() const;
};

