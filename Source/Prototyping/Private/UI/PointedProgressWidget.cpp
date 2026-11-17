#include "UI/PointedProgressWidget.h"
#include "Components/Image.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Utils/NetCore/PointedProgressCore.h"

const FName UPointedProgressWidget::ParamProgress(TEXT("Progress"));
const FName UPointedProgressWidget::ParamGhost(TEXT("Ghost"));
const FName UPointedProgressWidget::ParamFillColor(TEXT("FillColor"));

void UPointedProgressWidget::NativeConstruct()
{
	Super::NativeConstruct();
	EnsureMID();
	PushScalars();
	PushColors();
}

void UPointedProgressWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (FMath::IsNearlyEqual(Ghost, Progress, 1e-4f))
	{
		return;
	}
	Ghost = FPointedProgressCore::StepGhost(Ghost, Progress, InDeltaTime, GhostInterpSpeed);
	// Ghost only falls toward Progress; a heal snaps it up instantly.
	if (Ghost < Progress)
	{
		Ghost = Progress;
	}
	if (MID)
	{
		MID->SetScalarParameterValue(ParamGhost, Ghost);
	}
}

void UPointedProgressWidget::SetProgress(float InProgress)
{
	Progress = FPointedProgressCore::Clamp01(InProgress);
	if (Ghost < Progress)
	{
		Ghost = Progress;
	}
	PushScalars();
}

void UPointedProgressWidget::SnapGhost()
{
	Ghost = Progress;
	PushScalars();
}

void UPointedProgressWidget::SetFillColor(const FLinearColor& InColor)
{
	FillColor = InColor;
	PushColors();
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
}

void UPointedProgressWidget::PushColors() const
{
	if (!MID)
	{
		return;
	}
	MID->SetVectorParameterValue(ParamFillColor, FillColor);
}
