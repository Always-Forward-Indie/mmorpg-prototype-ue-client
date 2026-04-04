#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Image.h"
#include "CombatScreenFlashWidget.generated.h"

/**
 * Full-screen flash overlay used for damage (red) and heal (green) feedback.
 * The widget must contain a UImage named "FlashImage" bound via BindWidget,
 * OR it will create one dynamically at runtime.
 *
 * Usage:
 *   FlashWidget->PlayDamageFlash();   // red vignette
 *   FlashWidget->PlayHealFlash();     // green vignette
 */
UCLASS(BlueprintType, Blueprintable)
class PROTOTYPING_API UCombatScreenFlashWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    /** Trigger a red damage flash */
    UFUNCTION(BlueprintCallable, Category = "Combat Flash")
    void PlayDamageFlash();

    /** Trigger a green heal flash */
    UFUNCTION(BlueprintCallable, Category = "Combat Flash")
    void PlayHealFlash();

    /** Tick-driven fade; called automatically */
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

protected:
    virtual void NativeConstruct() override;

    /** Bound in Blueprint UMG. Falls back to dynamic creation if absent. */
    UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "Combat Flash")
    UImage* FlashImage = nullptr;

    /** Peak opacity of the flash (0..1) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Flash|Config")
    float PeakOpacity = 0.45f;

    /** How long the fade-out takes in seconds */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Flash|Config")
    float FadeOutDuration = 0.4f;

private:
    void StartFlash(FLinearColor Color);
    void EnsureFlashImage();

    float CurrentOpacity  = 0.0f;
    bool  bFading         = false;
    FLinearColor FlashColor = FLinearColor::Red;
};
