#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PointedProgressWidget.generated.h"

class UImage;
class UMaterialInstanceDynamic;
class UMaterialInterface;

/**
 * UPointedProgressWidget
 *
 * C++ base for the <=====> pointed bar. BP child provides only visuals:
 *   - Image "BarImage" with Brush Material = M_UI_ProgressPointed
 *   - recommended fixed aspect ~8:1 (e.g. 400x50) so Tip never stretches
 *
 * Progress lives only in the material (MID scalar "Progress"), never in a
 * UProgressBar (which clips a rectangle and kills the tips). Ghost is the
 * delayed damage trail, interpolated in NativeTick with early-out.
 */
UCLASS(BlueprintType, Blueprintable)
class PROTOTYPING_API UPointedProgressWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Set fill [0..1]. Ghost catches up from above via tick. */
	UFUNCTION(BlueprintCallable, Category = "Pointed Progress")
	void SetProgress(float InProgress);

	/** Snap ghost to progress (spawn, full heal, tab switch). */
	UFUNCTION(BlueprintCallable, Category = "Pointed Progress")
	void SnapGhost();

	/** Set fill color at runtime (HP green->red etc). */
	UFUNCTION(BlueprintCallable, Category = "Pointed Progress")
	void SetFillColor(const FLinearColor& InColor);

	UFUNCTION(BlueprintPure, Category = "Pointed Progress")
	float GetProgress() const { return Progress; }

	UFUNCTION(BlueprintPure, Category = "Pointed Progress")
	float GetGhost() const { return Ghost; }

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** Fallback master if the Image brush has no material yet. */
	UPROPERTY(EditDefaultsOnly, Category = "Pointed Progress")
	UMaterialInterface* SourceMaterial = nullptr;

	/** Ghost catch-up speed, 1/s. ~3 matches AAA damage trails. */
	UPROPERTY(EditDefaultsOnly, Category = "Pointed Progress", meta = (ClampMin = "0.1", ClampMax = "20.0"))
	float GhostInterpSpeed = 3.0f;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Pointed Progress")
	UImage* BarImage = nullptr;

private:
	void EnsureMID();
	void PushScalars() const;
	void PushColors() const;

	UPROPERTY(Transient)
	UMaterialInstanceDynamic* MID = nullptr;

	float Progress = 1.0f;
	float Ghost = 1.0f;
	FLinearColor FillColor = FLinearColor::White;

public:
	/** Material param names: single source of truth, no forked literals. */
	static const FName ParamProgress;
	static const FName ParamGhost;
	static const FName ParamFillColor;
};
