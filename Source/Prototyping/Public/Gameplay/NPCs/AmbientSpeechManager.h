#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Data/DataStructs.h"
#include "AmbientSpeechManager.generated.h"

class UMyGameInstance;

/**
 * AmbientSpeechManager
 *
 * Stores the ambient speech pool definitions received from the server
 * (NPC_AMBIENT_POOLS event).  Once received, per-NPC components query
 * this manager to retrieve their pool data.
 *
 * Owned by UMyGameInstance.
 */
UCLASS(BlueprintType)
class PROTOTYPING_API UAmbientSpeechManager : public UObject
{
    GENERATED_BODY()

public:
    /** Called once from MyGameInstance::InitGameSystems after allocation. */
    void Initialize(UMyGameInstance* InGameInstance);

    /**
     * Store (or replace) the ambient speech pool definition for one NPC.
     * Called by UAmbientSpeechNetworkHandler when it parses NPC_AMBIENT_POOLS.
     */
    void SetAmbientSpeechPools(int32 NpcId, const FAmbientSpeechNPCData& Data);

    /**
     * Retrieve ambient data for a given NPC id.
     * Returns true and fills OutData when found.
     */
    UFUNCTION(BlueprintCallable, Category = "Ambient Speech")
    bool GetNPCAmbientData(int32 NpcId, FAmbientSpeechNPCData& OutData) const;

    /** Returns true if any pool data is registered for NpcId. */
    UFUNCTION(BlueprintCallable, Category = "Ambient Speech")
    bool HasNPCAmbientData(int32 NpcId) const;

    /** Clear all stored pool definitions (e.g. on disconnect). */
    UFUNCTION(BlueprintCallable, Category = "Ambient Speech")
    void ClearAll();

private:
    UPROPERTY()
    UMyGameInstance* GameInstance = nullptr;

    /** NpcId -> pool definition */
    TMap<int32, FAmbientSpeechNPCData> AmbientData;
};
