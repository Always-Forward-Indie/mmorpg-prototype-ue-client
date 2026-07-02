#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "EffectDefinitionTable.generated.h"

class UNiagaraSystem;
class USoundBase;

/**
 * Row in the EffectDefinition data table.
 *
 * Row Name == effect slug (e.g. "poison", "haste", "regen_hp").
 * Display text comes from LocalizationSubsystem / DT_EffectLocale.
 *
 * How to set up in Editor:
 *   1. Create DataTable asset with row struct FEffectDefinitionRow.
 *   2. Fill in one row per effect slug that the server can send.
 *   3. Assign the DataTable to UEffectSlotWidget::EffectDefinitionTable.
 */
USTRUCT(BlueprintType)
struct FEffectDefinitionRow : public FTableRowBase
{
    GENERATED_BODY()

    // Icon texture to display in the slot (soft reference to avoid cooking issues)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect Definition")
    TSoftObjectPtr<UTexture2D> Icon;

    // Tint applied to the slot background to distinguish buff/debuff visually
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect Definition")
    FLinearColor SlotTintColor = FLinearColor::White;

    // Sound played on the target when this effect is applied
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect Definition|Audio")
    TSoftObjectPtr<USoundBase> ApplySound;

    // Niagara VFX spawned on the target when this effect is applied
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect Definition|VFX")
    TSoftObjectPtr<UNiagaraSystem> ApplyVFX;
};
