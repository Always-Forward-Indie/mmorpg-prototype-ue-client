#include "UI/PointedProgressWidget.h"
#include "Components/Image.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Utils/NetCore/PointedProgressCore.h"

const FName UPointedProgressWidget::ParamProgress(TEXT("Progress"));
const FName UPointedProgressWidget::ParamGhost(TEXT("Ghost"));
const FName UPointedProgressWidget::ParamFillTop(TEXT("FillTop"));
const FName UPointedProgressWidget::ParamFillBottom(TEXT("FillBottom"));
const FName UPointedProgressWidget::ParamGhostTop(TEXT("GhostTop"));
const FName UPointedProgressWidget::ParamGhostBottom(TEXT("GhostBottom"));
const FName UPointedProgressWidget::ParamTrackTop(TEXT("TrackTop"));
const FName UPointedProgressWidget::ParamTrackBottom(TEXT("TrackBottom"));
const FName UPointedProgressWidget::ParamBorderColor(TEXT("BorderColor"));
const FName UPointedProgressWidget::ParamBorderAlpha(TEXT("BorderAlpha"));
const FName UPointedProgressWidget::ParamTipLenPx(TEXT("TipLenPx"));
const FName UPointedProgressWidget::ParamBorderPx(TEXT("BorderPx"));
const FName UPointedProgressWidget::ParamBarWidthPx(TEXT("BarWidthPx"));
const FName UPointedProgressWidget::ParamBarHeightPx(TEXT("BarHeightPx"));
const FName UPointedProgressWidget::ParamFlashStart(TEXT("FlashStart"));
const FName UPointedProgressWidget::ParamFlashEnd(TEXT("FlashEnd"));
const FName UPointedProgressWidget::ParamFlashColor(TEXT("FlashColor"));
const FName UPointedProgressWidget::ParamFlashStrength(TEXT("FlashStrength"));

void UPointedProgressWidget::NativeConstruct()
{
	Super::NativeConstruct();
	LastPushedSize = FVector2D::ZeroVector;
	EnsureMID();
	PushScalars();
}

void UPointedProgressWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	PushSize();

	bool bMoved = false;
	if (!FMath::IsNearlyEqual(Progress, TargetProgress, 1e-4f))
	{
		Progress = FMath::FInterpConstantTo(Progress, TargetProgress, InDeltaTime, FillInterpSpeed);
		bMoved = true;
	}
	// Ghost trails the displayed edge in both directions (damage and heal).
	const float NewGhost = FPointedProgressCore::StepGhost(Ghost, Progress, InDeltaTime, GhostInterpSpeed);
	if (!FMath::IsNearlyEqual(NewGhost, Ghost, 1e-5f))
	{
		Ghost = NewGhost;
		bMoved = true;
	}
	if (bMoved)
	{
		PushScalars();
	}
}

void UPointedProgressWidget::SetProgress(float InProgress)
{
	TargetProgress = FPointedProgressCore::Clamp01(InProgress);
}

void UPointedProgressWidget::SnapGhost()
{
	Progress = TargetProgress;
	Ghost = TargetProgress;
	PushScalars();
}

void UPointedProgressWidget::SetFillGradient(const FLinearColor& Top, const FLinearColor& Bottom)
{
	if (MID)
	{
		MID->SetVectorParameterValue(ParamFillTop, Top);
		MID->SetVectorParameterValue(ParamFillBottom, Bottom);
	}
}

void UPointedProgressWidget::SetGhostGradient(const FLinearColor& Top, const FLinearColor& Bottom)
{
	if (MID)
	{
		MID->SetVectorParameterValue(ParamGhostTop, Top);
		MID->SetVectorParameterValue(ParamGhostBottom, Bottom);
	}
}

void UPointedProgressWidget::SetTrackGradient(const FLinearColor& Top, const FLinearColor& Bottom)
{
	if (MID)
	{
		MID->SetVectorParameterValue(ParamTrackTop, Top);
		MID->SetVectorParameterValue(ParamTrackBottom, Bottom);
	}
}

void UPointedProgressWidget::SetBorderColor(const FLinearColor& InColor)
{
	if (MID)
	{
		FLinearColor RGB = InColor;
		RGB.A = 1.0f;
		MID->SetVectorParameterValue(ParamBorderColor, RGB);
		MID->SetScalarParameterValue(ParamBorderAlpha, InColor.A);
	}
}

void UPointedProgressWidget::SetHealFlash(float From, float To, const FLinearColor& Color, float Strength)
{
	if (!MID)
	{
		return;
	}
	MID->SetScalarParameterValue(ParamFlashStart, From);
	MID->SetScalarParameterValue(ParamFlashEnd, To);
	MID->SetVectorParameterValue(ParamFlashColor, Color);
	MID->SetScalarParameterValue(ParamFlashStrength, Strength);
}

void UPointedProgressWidget::EnsureMID()
{
	if (MID || !BarImage)
	{
		return;
	}
	if (UMaterialInterface* BrushMat = BarImage->GetDynamicMaterial())
	{
		MID = Cast<UMaterialInstanceDynamic>(BrushMat);
	}
	if (!MID)
	{
		UMaterialInterface* Base = nullptr;
		if (const FSlateBrush& Brush = BarImage->GetBrush(); Brush.GetResourceObject())
		{
			Base = Cast<UMaterialInterface>(Brush.GetResourceObject());
		}
		if (!Base)
		{
			Base = SourceMaterial;
		}
		if (Base)
		{
			MID = UMaterialInstanceDynamic::Create(Base, this);
			BarImage->SetBrushFromMaterial(MID);
		}
	}
}

void UPointedProgressWidget::PushScalars() const
{
	if (!MID)
	{
		return;
	}
	MID->SetScalarParameterValue(ParamProgress, Progress);
	MID->SetScalarParameterValue(ParamGhost, Ghost);
	MID->SetScalarParameterValue(ParamTipLenPx, TipLengthPx);
	MID->SetScalarParameterValue(ParamBorderPx, BorderWidthPx);
}

void UPointedProgressWidget::PushSize()
{
	if (!MID)
	{
		return;
	}
	const FVector2D Size = GetCachedGeometry().GetLocalSize();
	if (Size.X > 0.0f && Size.Y > 0.0f && !Size.Equals(LastPushedSize, 0.5f))
	{
		LastPushedSize = Size;
		MID->SetScalarParameterValue(ParamBarWidthPx, Size.X);
		MID->SetScalarParameterValue(ParamBarHeightPx, Size.Y);
	}
}
