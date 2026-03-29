#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "EffectDefinitionTable.generated.h"

/**
 * Row in the EffectDefinition data table.
 *
 * Row Name == effect slug (e.g. "poison", "haste", "regen_hp").
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

    // Human-readable name shown in tooltip header (e.g. "Poison", "Haste")
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect Definition")
    FText DisplayName;

    // Full description shown in tooltip body
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect Definition")
    FText Description;

    // Icon texture to display in the slot (soft reference to avoid cooking issues)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect Definition")
    TSoftObjectPtr<UTexture2D> Icon;

    // "buff", "debuff", "dot", "hot" — drives tint colour in the slot
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect Definition")
    FString EffectCategory = TEXT("buff");

    // Tint applied to the slot background to distinguish buff/debuff visually
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect Definition")
    FLinearColor SlotTintColor = FLinearColor::White;

    // Mark rows that represent passive skills so the slot displays "?" instead of a countdown
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect Definition")
    bool bIsPassive = false;

    FEffectDefinitionRow()
    {
        EffectCategory = TEXT("buff");
        SlotTintColor = FLinearColor::White;
        bIsPassive = false;
    }
};
