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

    // ------------------------------------------------------------------ //
    //  Per-actor configuration                                            //
    // ------------------------------------------------------------------ //

    /** Z-offset above actor root to the nameplate attach point (cm). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player Nameplate")
    float HeadOffsetZ = 210.0f;

private:
    bool bInitialised   = false;
    bool bIsLocalPlayer = false;
    bool bRegistered    = false;

    FCharacterDataStruct CachedCharData;
    FTimerHandle RetryTimerHandle;

    void TryRegister();
    UNameplateManager* ResolveNameplateManager() const;
};

