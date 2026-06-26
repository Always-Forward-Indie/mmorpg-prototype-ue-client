#include "UI/HarvestProgressWidget.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Animation/WidgetAnimation.h"
#include "Engine/Engine.h"
#include "Gameplay/Items/HarvestManager.h"
#include "Framework/Application/SlateApplication.h"

void UHarvestProgressWidget::NativeConstruct()
{
	Super::NativeConstruct();

	CurrentProgress = 0.0f;
	bIsVisible = false;
	HarvestManager = nullptr;

	// Enable focus to handle keyboard input (ESC key)
	// Use the setter method instead of direct access
	SetIsFocusable(true);

	// Initialize widget but don't bind to HarvestManager yet
	// UIManager will handle the binding through SetHarvestManager
	InitializeWidget();
}

void UHarvestProgressWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Only handle visual updates here - no progress calculation
	// Progress calculation is handled by HarvestManager and received via delegate
	if (bIsVisible && TextBlock_Progress)
	{
		// Update progress text display (this could also be done in HandleHarvestProgress)
		FString ProgressText = FString::Printf(TEXT("%.0f%%"), CurrentProgress * 100.0f);
		TextBlock_Progress->SetText(FText::FromString(ProgressText));
	}
}

FReply UHarvestProgressWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	// Cancel harvest on ESC key
	if (InKeyEvent.GetKey() == EKeys::Escape && bIsVisible)
	{
		CancelHarvest();
		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UHarvestProgressWidget::SetHarvestManager(UHarvestManager* InHarvestManager)
{
	// Unbind from previous manager if any
	UnbindFromHarvestManager();

	HarvestManager = InHarvestManager;

	// Bind to new manager
	BindToHarvestManager();
}

void UHarvestProgressWidget::InitializeWidget()
{
	UE_LOG(LogTemp, Warning, TEXT("HarvestProgressWidget: InitializeWidget called"));

	// Set default text
	if (TextBlock_HarvestTitle)
	{
		TextBlock_HarvestTitle->SetText(FText::FromString(TEXT("Harvesting...")));
	}

	if (TextBlock_CancelHint)
	{
		TextBlock_CancelHint->SetText(FText::FromString(TEXT("Press ESC to cancel")));
	}

	// Initialize progress bar
	if (ProgressBar_Harvest)
	{
		ProgressBar_Harvest->SetPercent(0.0f);
	}

	// Hide widget initially
	HideWidget();
	
	UE_LOG(LogTemp, Warning, TEXT("HarvestProgressWidget: InitializeWidget completed"));
}

void UHarvestProgressWidget::UpdateProgress(float Progress)
{
	CurrentProgress = FMath::Clamp(Progress, 0.0f, 1.0f);

	if (ProgressBar_Harvest)
	{
		ProgressBar_Harvest->SetPercent(CurrentProgress);
	}

	// Change color based on progress
	if (ProgressBar_Harvest)
	{
		FLinearColor ProgressColor;
		if (CurrentProgress < 0.5f)
		{
			// Red to Yellow
			ProgressColor = FLinearColor::LerpUsingHSV(FLinearColor::Red, FLinearColor::Yellow, CurrentProgress * 2.0f);
		}
		else
		{
			// Yellow to Green
			ProgressColor = FLinearColor::LerpUsingHSV(FLinearColor::Yellow, FLinearColor::Green, (CurrentProgress - 0.5f) * 2.0f);
		}

		ProgressBar_Harvest->SetFillColorAndOpacity(ProgressColor);
	}
}

void UHarvestProgressWidget::SetHarvestText(const FString& Text)
{
	if (TextBlock_HarvestTitle)
	{
		TextBlock_HarvestTitle->SetText(FText::FromString(Text));
	}
}

void UHarvestProgressWidget::ShowWidget()
{
	if (!bIsVisible)
	{
		bIsVisible = true;
		SetVisibility(ESlateVisibility::Visible);

		// Play show animation if available
		if (ProgressAnimation)
		{
			PlayAnimation(ProgressAnimation);
		}

		// Set focus to handle input (now that bIsFocusable is true)
		SetUserFocus(GetOwningPlayer());

		UE_LOG(LogTemp, Warning, TEXT("HarvestProgressWidget: Widget shown - bIsVisible: %s, Visibility: Visible, CurrentProgress: %.2f%%"), 
			bIsVisible ? TEXT("true") : TEXT("false"), CurrentProgress * 100.0f);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("HarvestProgressWidget: ShowWidget called but widget already visible"));
	}
}

void UHarvestProgressWidget::HideWidget()
{
	bIsVisible = false;
	SetVisibility(ESlateVisibility::Hidden);

	// Reset progress
	CurrentProgress = 0.0f;
	if (ProgressBar_Harvest)
	{
		ProgressBar_Harvest->SetPercent(0.0f);
	}

	// Clear focus when hiding - use the proper UMG method
	if (HasUserFocus(GetOwningPlayer()))
	{
		FSlateApplication::Get().ClearKeyboardFocus(EFocusCause::Cleared);
	}
	FSlateApplication::Get().SetAllUserFocusToGameViewport();

	UE_LOG(LogTemp, Warning, TEXT("HarvestProgressWidget: Widget hidden - bIsVisible: %s, Visibility: Hidden"), 
		bIsVisible ? TEXT("true") : TEXT("false"));
}

void UHarvestProgressWidget::BindToHarvestManager()
{
	if (HarvestManager)
	{
		UE_LOG(LogTemp, Warning, TEXT("HarvestProgressWidget: Binding to HarvestManager"));
		
		HarvestManager->OnHarvestStarted.AddDynamic(this, &UHarvestProgressWidget::HandleHarvestStarted);
		HarvestManager->OnHarvestProgressUpdate.AddDynamic(this, &UHarvestProgressWidget::HandleHarvestProgress);
		HarvestManager->OnHarvestCompleted.AddDynamic(this, &UHarvestProgressWidget::HandleHarvestCompleted);
		HarvestManager->OnHarvestError.AddDynamic(this, &UHarvestProgressWidget::HandleHarvestError);
		
		UE_LOG(LogTemp, Warning, TEXT("HarvestProgressWidget: Successfully bound to HarvestManager delegates"));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("HarvestProgressWidget: Cannot bind to HarvestManager - manager is null"));
	}
}

void UHarvestProgressWidget::UnbindFromHarvestManager()
{
	if (HarvestManager)
	{
		UE_LOG(LogTemp, Warning, TEXT("HarvestProgressWidget: Unbinding from HarvestManager"));
		
		HarvestManager->OnHarvestStarted.RemoveDynamic(this, &UHarvestProgressWidget::HandleHarvestStarted);
		HarvestManager->OnHarvestProgressUpdate.RemoveDynamic(this, &UHarvestProgressWidget::HandleHarvestProgress);
		HarvestManager->OnHarvestCompleted.RemoveDynamic(this, &UHarvestProgressWidget::HandleHarvestCompleted);
		HarvestManager->OnHarvestError.RemoveDynamic(this, &UHarvestProgressWidget::HandleHarvestError);
		
		UE_LOG(LogTemp, Warning, TEXT("HarvestProgressWidget: Successfully unbound from HarvestManager delegates"));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("HarvestProgressWidget: Cannot unbind from HarvestManager - manager is null"));
	}
}

void UHarvestProgressWidget::HandleHarvestStarted(const FHarvestStartedStruct& HarvestData)
{
	SetHarvestText(TEXT("Harvesting..."));
	
	// Reset progress to 0 when starting harvest
	UpdateProgress(0.0f);
	
	ShowWidget();
	
	UE_LOG(LogTemp, Warning, TEXT("HarvestProgressWidget: Harvest started, progress reset to 0. Expected duration: %.2fs"), 
		HarvestData.duration / 1000.0f);
}

void UHarvestProgressWidget::HandleHarvestProgress(float Progress)
{
	UpdateProgress(Progress);
	
	// Log progress updates to verify delegate is working
	UE_LOG(LogTemp, Warning, TEXT("HarvestProgressWidget: Progress updated to %.2f%%"), Progress * 100.0f);
	
	// Update the progress text immediately to ensure synchronization
	if (TextBlock_Progress)
	{
		FString ProgressText = FString::Printf(TEXT("%.0f%%"), Progress * 100.0f);
		TextBlock_Progress->SetText(FText::FromString(ProgressText));
	}
}

void UHarvestProgressWidget::HandleHarvestCompleted(const FHarvestCompleteStruct& HarvestData)
{
	HideWidget();
}

void UHarvestProgressWidget::HandleHarvestError(const FHarvestErrorStruct& ErrorData)
{
	HideWidget();
}

void UHarvestProgressWidget::CancelHarvest()
{
	UE_LOG(LogTemp, Warning, TEXT("HarvestProgressWidget: Cancel harvest requested"));

	if (HarvestManager)
	{
		HarvestManager->CancelHarvest();
	}
	else
	{
		HideWidget();
	}

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Orange, TEXT("Harvest cancelled"));
	}
}

void UHarvestProgressWidget::NativeDestruct()
{
	UnbindFromHarvestManager();
	Super::NativeDestruct();
}