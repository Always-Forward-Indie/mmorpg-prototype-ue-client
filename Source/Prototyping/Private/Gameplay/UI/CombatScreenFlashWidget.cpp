#include "Gameplay/UI/CombatScreenFlashWidget.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/CanvasPanel.h"
#include "Engine/Texture2D.h"

void UCombatScreenFlashWidget::NativeConstruct()
{
	Super::NativeConstruct();
	EnsureFlashImage();
	EnsureLowHealthBorder();
}

void UCombatScreenFlashWidget::EnsureFlashImage()
{
	if (FlashImage)
	{
		FlashImage->SetVisibility(ESlateVisibility::HitTestInvisible);
		FlashImage->SetColorAndOpacity(FLinearColor(FlashColor.R, FlashColor.G, FlashColor.B, 0.0f));
		return;
	}

	UCanvasPanel* Root = Cast<UCanvasPanel>(GetRootWidget());
	if (!Root)
	{
		UE_LOG(LogTemp, Warning, TEXT("CombatScreenFlashWidget: Root widget is not a CanvasPanel, flash will not display."));
		return;
	}

	FlashImage = NewObject<UImage>(this, TEXT("FlashImage"));
	FlashImage->SetColorAndOpacity(FLinearColor(1.f, 0.f, 0.f, 0.0f));
	FlashImage->SetVisibility(ESlateVisibility::HitTestInvisible);

	UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(FlashImage);
	if (PanelSlot)
	{
		FAnchors FullAnchors(0.f, 0.f, 1.f, 1.f);
		PanelSlot->SetAnchors(FullAnchors);
		PanelSlot->SetOffsets(FMargin(0.f, 0.f, 0.f, 0.f));
		PanelSlot->SetAlignment(FVector2D(0.f, 0.f));
		PanelSlot->SetAutoSize(false);
		PanelSlot->SetZOrder(9999);
	}
}

void UCombatScreenFlashWidget::EnsureLowHealthBorder()
{
	if (LowHealthBorder)
	{
		LowHealthBorder->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	UCanvasPanel* Root = Cast<UCanvasPanel>(GetRootWidget());
	if (!Root) return;

	LowHealthBorder = NewObject<UImage>(this, TEXT("LowHealthBorder"));
	LowHealthBorder->SetColorAndOpacity(FLinearColor(LowHealthColor.R, LowHealthColor.G, LowHealthColor.B, 0.0f));
	LowHealthBorder->SetVisibility(ESlateVisibility::Collapsed);

	UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(LowHealthBorder);
	if (PanelSlot)
	{
		FAnchors FullAnchors(0.f, 0.f, 1.f, 1.f);
		PanelSlot->SetAnchors(FullAnchors);
		PanelSlot->SetOffsets(FMargin(0.f, 0.f, 0.f, 0.f));
		PanelSlot->SetAlignment(FVector2D(0.f, 0.f));
		PanelSlot->SetAutoSize(false);
		PanelSlot->SetZOrder(9998);
	}
}

void UCombatScreenFlashWidget::PlayDamageFlash()
{
	StartFlash(FLinearColor(1.0f, 0.05f, 0.05f, 1.0f));
}

void UCombatScreenFlashWidget::PlayHealFlash()
{
	StartFlash(FLinearColor(0.1f, 0.9f, 0.2f, 1.0f));
}

void UCombatScreenFlashWidget::SetLowHealthWarning(bool bActive)
{
	bLowHealthWarning = bActive;
	if (!bActive)
	{
		LowHealthPulseTime = 0.0f;
		if (LowHealthBorder)
			LowHealthBorder->SetVisibility(ESlateVisibility::Collapsed);
	}
	else
	{
		EnsureLowHealthBorder();
	}
}

void UCombatScreenFlashWidget::StartFlash(FLinearColor Color)
{
	if (!FlashImage)
	{
		EnsureFlashImage();
		if (!FlashImage) return;
	}

	FlashColor      = Color;
	CurrentOpacity  = PeakOpacity;
	bFading         = true;

	FlashImage->SetColorAndOpacity(FLinearColor(Color.R, Color.G, Color.B, CurrentOpacity));
	FlashImage->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UCombatScreenFlashWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (bFading && FlashImage)
	{
		const float FadeStep = (FadeOutDuration > 0.f) ? (InDeltaTime / FadeOutDuration) : 1.0f;
		CurrentOpacity -= FadeStep * PeakOpacity;

		if (CurrentOpacity <= 0.0f)
		{
			CurrentOpacity = 0.0f;
			bFading        = false;
			FlashImage->SetVisibility(ESlateVisibility::Collapsed);
		}

		FlashImage->SetColorAndOpacity(FLinearColor(FlashColor.R, FlashColor.G, FlashColor.B, CurrentOpacity));
	}

	if (bLowHealthWarning && LowHealthBorder)
	{
		LowHealthPulseTime += InDeltaTime;
		const float Pulse = FMath::Abs(FMath::Sin(LowHealthPulseTime * LowHealthPulseSpeed));
		const float Alpha = LowHealthOpacity * (0.35f + 0.65f * Pulse);

		LowHealthBorder->SetColorAndOpacity(
			FLinearColor(LowHealthColor.R, LowHealthColor.G, LowHealthColor.B, Alpha));
		LowHealthBorder->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}
