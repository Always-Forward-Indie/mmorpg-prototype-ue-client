#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Image.h"
#include "CombatScreenFlashWidget.generated.h"

/**
 * Full-screen flash overlay used for damage (red) and heal (green) feedback,
 * plus a persistent low-health warning (pulsing red border).
 *
 * Usage:
 *   FlashWidget->PlayDamageFlash();          // brief red vignette
 *   FlashWidget->PlayHealFlash();            // brief green vignette
 *   FlashWidget->SetLowHealthWarning(true);  // persistent pulsing red border
 *   FlashWidget->SetLowHealthWarning(false); // disable warning
 */
UCLASS(BlueprintType, Blueprintable)
class PROTOTYPING_API UCombatScreenFlashWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Combat Flash")
	void PlayDamageFlash();

	UFUNCTION(BlueprintCallable, Category = "Combat Flash")
	void PlayHealFlash();

	UFUNCTION(BlueprintCallable, Category = "Combat Flash")
	void SetLowHealthWarning(bool bActive);

	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

protected:
	virtual void NativeConstruct() override;

	/** Bound in Blueprint UMG. Falls back to dynamic creation if absent. */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "Combat Flash")
	UImage* FlashImage = nullptr;

	/** Optional second image for low-health border (pulsing vignette). */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidgetOptional), Category = "Combat Flash")
	UImage* LowHealthBorder = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Flash|Config")
	float PeakOpacity = 0.45f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Flash|Config")
	float FadeOutDuration = 0.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Flash|LowHealth")
	float LowHealthOpacity = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Flash|LowHealth")
	float LowHealthPulseSpeed = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Flash|LowHealth")
	float LowHealthThreshold = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Flash|LowHealth")
	FLinearColor LowHealthColor = FLinearColor(0.85f, 0.05f, 0.05f, 1.0f);

private:
	void StartFlash(FLinearColor Color);
	void EnsureFlashImage();
	void EnsureLowHealthBorder();

	float CurrentOpacity  = 0.0f;
	bool  bFading         = false;
	FLinearColor FlashColor = FLinearColor::Red;

	bool  bLowHealthWarning = false;
	float LowHealthPulseTime = 0.0f;
};
