#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "WIOChannelBarWidget.generated.h"

class UProgressBar;
class UTextBlock;

/**
 * UWIOChannelBarWidget
 *
 * Cast / channel progress bar widget for channeled WIO interactions.
 * Shows a filling progress bar and optional name/time text.
 *
 * Bind in your WBP:
 *   - ProgressBar "ChannelProgressBar"
 *   - TextBlock   "ChannelNameText"
 *   - TextBlock   "ChannelTimeText"
 */
UCLASS(BlueprintType, Blueprintable)
class PROTOTYPING_API UWIOChannelBarWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Begin showing the channel bar with the given name and total duration. */
	UFUNCTION(BlueprintCallable, Category = "WIO UI")
	void StartChannel(const FText& ChannelName, float DurationSeconds);

	/** Update the progress bar. Progress is [0..1]. */
	UFUNCTION(BlueprintCallable, Category = "WIO UI")
	void UpdateProgress(float Progress);

	/** Hide the channel bar (cancelled or completed). */
	UFUNCTION(BlueprintCallable, Category = "WIO UI")
	void StopChannel();

	/** Returns true if the bar is currently active. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "WIO UI")
	bool IsActive() const { return bIsActive; }

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** BP event when channel starts. */
	UFUNCTION(BlueprintImplementableEvent, Category = "WIO UI")
	void OnChannelStarted(const FText& ChannelName, float InDuration);

	/** BP event when channel completes (progress reaches 1.0). */
	UFUNCTION(BlueprintImplementableEvent, Category = "WIO UI")
	void OnChannelCompleted();

	/** BP event when channel is cancelled. */
	UFUNCTION(BlueprintImplementableEvent, Category = "WIO UI")
	void OnChannelCancelled();

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "WIO UI")
	UProgressBar* ChannelProgressBar = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "WIO UI")
	UTextBlock* ChannelNameText = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "WIO UI")
	UTextBlock* ChannelTimeText = nullptr;

private:
	bool    bIsActive       = false;
	float   Duration        = 0.f;
	double  StartTime       = 0.0;
};
