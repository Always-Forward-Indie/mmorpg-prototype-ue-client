#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Data/DataStructs.h"
#include "Data/EffectDefinitionTable.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "EffectSlotWidget.generated.h"

/**
 * Widget representing a single active buff / debuff slot in the effects bar.
 *
 * Blueprint setup (WBP_EffectSlot):
 *   Required:
 *     - Image        "Effect_Icon"       — slot icon
 *     - TextBlock    "Timer_Text"        — remaining seconds / "?"
 *     - Border       "Slot_Border"       — background, tinted by effect category
 *
 * Tooltip popup widget is created automatically when EffectTooltipClass is set.
 * The tooltip reads DisplayName + Description from the DataTable row.
 */
UCLASS(BlueprintType, Blueprintable)
class PROTOTYPING_API UEffectSlotWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    /**
     * Populate the slot with data from a server stats_update active effect entry.
     * Call this right after CreateWidget<UEffectSlotWidget>() inside UActiveEffectsWidget.
     *
     * @param InEffect     Effect data received from server.
     * @param InTable      Optional DataTable for icon / name / description lookup.
     */
    UFUNCTION(BlueprintCallable, Category = "Effect Slot")
    void SetupSlot(const FActiveEffectEntry& InEffect, UDataTable* InTable);

    /**
     * Grouped variant: one representative entry + all per-attribute modifiers.
     * Builds a multi-line attribute list in the tooltip.
     *
     * @param InEffect         Representative entry (supplies slug / type / expiresAt).
     * @param InModifiers      All raw entries that share the same slug.
     * @param InTable          Optional DataTable for icon / name / description lookup.
     */
    void SetupSlotGrouped(const FActiveEffectEntry& InEffect,
                          const TArray<FActiveEffectEntry>& InModifiers,
                          UDataTable* InTable);

    /** Update only the timer label (called every second by UActiveEffectsWidget). */
    UFUNCTION(BlueprintCallable, Category = "Effect Slot")
    void RefreshTimer();

    /** Return the effect slug this slot is currently displaying. */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Effect Slot")
    FString GetEffectSlug() const { return CachedEffect.slug; }

    // ?? DataTable reference ??????????????????????????????????????????????????
    /**
     * DataTable with FEffectDefinitionRow rows.
     * Row Name must match the effect slug coming from the server.
     * Assign in Blueprint defaults or set at runtime via SetEffectDefinitionTable().
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect Slot|Data")
    TObjectPtr<UDataTable> EffectDefinitionTable;

    UFUNCTION(BlueprintCallable, Category = "Effect Slot|Data")
    void SetEffectDefinitionTable(UDataTable* InTable) { EffectDefinitionTable = InTable; }

    // ?? Tooltip widget class ?????????????????????????????????????????????????
    /**
     * Optional custom tooltip widget class.
     * Must expose TextBlocks named "Tooltip_Title" and "Tooltip_Description".
     * If nullptr the built-in UE tooltip text is used instead.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect Slot|Tooltip")
    TSubclassOf<UUserWidget> EffectTooltipClass;

protected:
    // ?? Bound widgets ????????????????????????????????????????????????????????
    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "Effect Slot")
    TObjectPtr<UImage> Effect_Icon;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidget), Category = "Effect Slot")
    TObjectPtr<UTextBlock> Timer_Text;

    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "Effect Slot")
    TObjectPtr<UBorder> Slot_Border;

    // ?? Lifecycle ????????????????????????????????????????????????????????????
    virtual void NativeConstruct() override;

    // ?? Blueprint events ?????????????????????????????????????????????????????
    /** Called after slot data is applied — override in BP to animate icon / border. */
    UFUNCTION(BlueprintImplementableEvent, Category = "Effect Slot")
    void OnSlotSetup(const FActiveEffectEntry& Effect, const FEffectDefinitionRow& Definition, bool bFoundInTable);

private:
    // Snapshot of the effect this slot represents
    FActiveEffectEntry CachedEffect;

    // All per-attribute modifiers (populated by SetupSlotGrouped)
    TArray<FActiveEffectEntry> CachedModifiers;

    // Resolved definition (may be default if slug not in table)
    FEffectDefinitionRow CachedDefinition;

    // Whether a DataTable row was found for the current slug
    bool bHasDefinition = false;

    // ?? Helpers ??????????????????????????????????????????????????????????????
    FString BuildTimerString() const;
    FString BuildModifiersString() const;
    void ApplyIcon();
    void ApplyBorderTint();
    void BuildTooltip();
};
