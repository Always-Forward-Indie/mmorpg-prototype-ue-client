#pragma once

#include "CoreMinimal.h"
#include "Data/DataStructs.h"
#include "StatsUpdateCounterHelper.generated.h"

/** Test-only listener that counts OnStatsUpdated broadcasts (HUD-bus pin). */
UCLASS()
class UStatsUpdateCounter : public UObject
{
    GENERATED_BODY()

public:
    int32 Count = 0;
    int32 LastEffects = 0;

    UFUNCTION()
    void OnUpdate(const FPlayerStatsUpdateStruct& S)
    {
        ++Count;
        LastEffects = S.activeEffects.Num();
    }
};
