// Fill out your copyright notice in the Description page of Project Settings.


#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/DataStructs.h"
#include "NPCNameplateComponent.generated.h"

class UNameplateManager;


/**
 * Thin data-holder component on an NPC actor.
 *
 * On BeginPlay resolves UNameplateManager from UIManager and registers.
 * On EndPlay unregisters. All rendering is handled centrally by
 * UNameplateCanvasWidget � no WidgetComponent, no per-actor render target.
 */
UCLASS(ClassGroup = (UI), meta = (BlueprintSpawnableComponent))
class PROTOTYPING_API UNPCNameplateComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UNPCNameplateComponent();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
    /**
     * Populate nameplate data from a server NPC packet and register with
     * the central NameplateManager. Safe to call multiple times.
     */
    UFUNCTION(BlueprintCallable, Category = "NPC Nameplate")
    void InitialiseFromNPCData(const FNPCStruct& NPCData, bool bForceRefresh = false);

    // ------------------------------------------------------------------ //
    //  Per-actor configuration                                            //
    // ------------------------------------------------------------------ //

    /**
     * Extra Z margin (cm) above the top of the capsule where the nameplate anchors.
     * 0 = nameplate sits exactly at the crown of the capsule.
     * Leave at 0 to use the global default from NameplateCanvasWidget.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC Nameplate",
              meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "200.0"))
    float HeadOffsetZ = 0.0f;

    /**
     * Interact hint radius (cm).
     * 0 = use FNPCStruct.radius from the server packet.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NPC Nameplate")
    float InteractRadius = 0.0f;

private:
    bool       bInitialised   = false;
    bool       bRegistered    = false;
    FNPCStruct CachedNPCData;
    FTimerHandle RetryTimerHandle;

    void TryRegister();
    UNameplateManager* ResolveNameplateManager() const;

    UFUNCTION()
    void HandleLocaleChanged(const FString& NewLocale);
};

