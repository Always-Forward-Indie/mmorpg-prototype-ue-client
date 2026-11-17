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
 *   - recommended fixed aspect ~8:1 (e.g. 400x50); the widget pushes its
 *     real size into the material so tips/border stay pixel-true at any size
 *
 * Progress lives only in the material (MID scalars "Progress"/"Ghost"),
 * never in a UProgressBar (which clips a rectangle and kills the tips).
 * Ghost is the delayed damage trail, interpolated in NativeTick with early-out.
 */
UCLASS(BlueprintType, Blueprintable)
class PROTOTYPING_API UPointedProgressWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Set fill target [0..1]. Displayed fill eases toward it (both directions);
	 * ghost trails behind the displayed value. */
	UFUNCTION(BlueprintCallable, Category = "Pointed Progress")
	void SetProgress(float InProgress);

	/** Snap ghost to progress (spawn, full heal, tab switch). */
	UFUNCTION(BlueprintCallable, Category = "Pointed Progress")
	void SnapGhost();

	/** Fill gradient (top -> bottom, e.g. HP #C96060 -> #782C37). */
	UFUNCTION(BlueprintCallable, Category = "Pointed Progress")
	void SetFillGradient(const FLinearColor& Top, const FLinearColor& Bottom);

	/** Ghost-trail gradient (same hue family as the fill, lighter). */
	UFUNCTION(BlueprintCallable, Category = "Pointed Progress")
	void SetGhostGradient(const FLinearColor& Top, const FLinearColor& Bottom);

	/** Track (background) gradient (e.g. #080E16 -> #1D2935). */
	UFUNCTION(BlueprintCallable, Category = "Pointed Progress")
	void SetTrackGradient(const FLinearColor& Top, const FLinearColor& Bottom);

	/** Track outline color (alpha drives ring translucency). */
	UFUNCTION(BlueprintCallable, Category = "Pointed Progress")
	void SetBorderColor(const FLinearColor& InColor);

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

	/** Displayed-fill catch-up speed, 1/s. Faster than GhostInterpSpeed so the
	 * trail stays visible behind the moving edge in both directions. */
	UPROPERTY(EditDefaultsOnly, Category = "Pointed Progress", meta = (ClampMin = "0.1", ClampMax = "20.0"))
	float FillInterpSpeed = 5.0f;

	/** Ghost catch-up speed, 1/s. ~3 matches AAA damage trails. */
	UPROPERTY(EditDefaultsOnly, Category = "Pointed Progress", meta = (ClampMin = "0.1", ClampMax = "20.0"))
	float GhostInterpSpeed = 3.0f;

	/** Tip length in px (blunt parabolic nose, default 18). */
	UPROPERTY(EditDefaultsOnly, Category = "Pointed Progress", meta = (ClampMin = "0.0", ClampMax = "64.0"))
	float TipLengthPx = 18.0f;

	/** Border ring thickness in px (default 2). */
	UPROPERTY(EditDefaultsOnly, Category = "Pointed Progress", meta = (ClampMin = "0.0", ClampMax = "8.0"))
	float BorderWidthPx = 2.0f;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Pointed Progress")
	UImage* BarImage = nullptr;

private:
	void EnsureMID();
	void PushScalars() const;
	void PushSize();

	UPROPERTY(Transient)
	UMaterialInstanceDynamic* MID = nullptr;

	float Progress = 1.0f;
	float TargetProgress = 1.0f;
	float Ghost = 1.0f;
	FVector2D LastPushedSize = FVector2D::ZeroVector;

public:
	/** Material param names: single source of truth, no forked literals. */
	static const FName ParamProgress;
	static const FName ParamGhost;
	static const FName ParamFillTop;
	static const FName ParamFillBottom;
	static const FName ParamGhostTop;
	static const FName ParamGhostBottom;
	static const FName ParamTrackTop;
	static const FName ParamTrackBottom;
	static const FName ParamBorderColor;
static const FName ParamBorderAlpha;
	static const FName ParamTipLenPx;
	static const FName ParamBorderPx;
	static const FName ParamBarWidthPx;
	static const FName ParamBarHeightPx;
};
