#pragma once

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Data/DataStructs.h"
#include "Gameplay/UI/EffectSlotWidget.h"
#include "Components/HorizontalBox.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "ActiveEffectsWidget.generated.h"

/**
 * HUD widget that displays a row of active buff/debuff icons with countdown timers.
 *
 * Blueprint setup:
 *   - Add a HorizontalBox named "Effects_Container" to the widget.
 *   - Assign "EffectSlotClass" � a small UUserWidget that has:
 *       - An Image named  "Effect_Icon"   (optional, shows slug-based icon)
 *       - A TextBlock named "Timer_Text"  (shows remaining seconds or "permanent")
 *       - A TextBlock named "Name_Text"   (shows effect slug)
 *
 * Call RefreshEffects() whenever a new stats_update arrives.
 * The widget ticks itself every second to update countdown timers.
 */
UCLASS(BlueprintType, Blueprintable)
class PROTOTYPING_API UActiveEffectsWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    // Feed the latest activeEffects array from a stats_update packet.
    UFUNCTION(BlueprintCallable, Category = "Active Effects")
    void RefreshEffects(const TArray<FActiveEffectEntry>& Effects);

    // Clear all displayed effects (e.g. on death).
    UFUNCTION(BlueprintCallable, Category = "Active Effects")
    void ClearEffects();

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct()  override;

    // Container that holds one slot widget per active effect.
    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional))
    UHorizontalBox* Effects_Container = nullptr;

    // Widget class used for each effect slot (should be based on UEffectSlotWidget).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Active Effects|Classes")
    TSubclassOf<UEffectSlotWidget> EffectSlotClass;

    /**
     * DataTable (row struct: FEffectDefinitionRow) that maps effect slugs to
     * display name, description, icon and slot tint colour.
     * Assign in Blueprint defaults. Propagated to each created EffectSlotWidget.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Active Effects|Data")
    TObjectPtr<UDataTable> EffectDefinitionTable;

private:
    // Cached snapshot used for per-second timer tick.
    TArray<FActiveEffectEntry> CachedEffects;

    // Timer that refreshes countdown labels every second.
    FTimerHandle TickTimerHandle;

    UFUNCTION()
    void OnSecondTick();

    // Group raw server entries by slug: one slot per unique effect name.
    static void GroupEffects(const TArray<FActiveEffectEntry>& InEffects,
                             TArray<FActiveEffectEntry>& OutRepresentatives,
                             TMap<FString, TArray<FActiveEffectEntry>>& OutGrouped);

    // Rebuild the slot widgets from CachedEffects.
    void RebuildSlots();
};
