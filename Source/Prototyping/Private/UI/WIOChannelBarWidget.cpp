#include "UI/WIOChannelBarWidget.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"

void UWIOChannelBarWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::Collapsed);
}

void UWIOChannelBarWidget::StartChannel(const FText& ChannelName, float DurationSeconds)
{
	bIsActive = true;
	Duration  = DurationSeconds;
	StartTime = FPlatformTime::Seconds();

	SetVisibility(ESlateVisibility::HitTestInvisible);

	if (ChannelProgressBar)
	{
		ChannelProgressBar->SetPercent(0.f);
	}
	if (ChannelNameText)
	{
		ChannelNameText->SetText(ChannelName);
	}
	if (ChannelTimeText)
	{
		ChannelTimeText->SetText(FText::FromString(FString::Printf(TEXT("%.1fs"), DurationSeconds)));
	}

	OnChannelStarted(ChannelName, DurationSeconds);
}

void UWIOChannelBarWidget::UpdateProgress(float Progress)
{
	if (ChannelProgressBar)
	{
		ChannelProgressBar->SetPercent(FMath::Clamp(Progress, 0.f, 1.f));
	}

	// Update remaining time text
	if (ChannelTimeText && Duration > 0.f)
	{
		const float Remaining = Duration * (1.f - FMath::Clamp(Progress, 0.f, 1.f));
		ChannelTimeText->SetText(FText::FromString(FString::Printf(TEXT("%.1fs"), Remaining)));
	}
}

void UWIOChannelBarWidget::StopChannel()
{
	const bool bWasActive = bIsActive;
	bIsActive = false;

	SetVisibility(ESlateVisibility::Collapsed);

	if (bWasActive)
	{
		// Check if it was a natural completion vs cancellation
		if (Duration > 0.f)
		{
			const double Elapsed = FPlatformTime::Seconds() - StartTime;
			if (Elapsed >= Duration * 0.95) // Allow small tolerance
			{
				OnChannelCompleted();
			}
			else
			{
				OnChannelCancelled();
			}
		}
		else
		{
			OnChannelCancelled();
		}
	}

	Duration  = 0.f;
	StartTime = 0.0;
}

void UWIOChannelBarWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!bIsActive || Duration <= 0.f)
	{
		return;
	}

	const double Elapsed = FPlatformTime::Seconds() - StartTime;
	const float Progress = FMath::Clamp(static_cast<float>(Elapsed / Duration), 0.f, 1.f);

	UpdateProgress(Progress);
}
