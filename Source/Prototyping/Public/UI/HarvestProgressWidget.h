#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "HarvestProgressWidget.generated.h"

/**
 * Widget to display harvest progress
 */
UCLASS()
class PROTOTYPING_API UHarvestProgressWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Initialize the widget
	UFUNCTION(BlueprintCallable, Category = "Harvest Progress")
	void InitializeWidget();

	// Update progress (0.0 to 1.0)
	UFUNCTION(BlueprintCallable, Category = "Harvest Progress")
	void UpdateProgress(float Progress);

	// Set the harvest item name
	UFUNCTION(BlueprintCallable, Category = "Harvest Progress")
	void SetHarvestText(const FString& Text);

	// Show/Hide the widget
	UFUNCTION(BlueprintCallable, Category = "Harvest Progress")
	void ShowWidget();

	UFUNCTION(BlueprintCallable, Category = "Harvest Progress")
	void HideWidget();

	// Cancel harvest (called by cancel button or ESC key)
	UFUNCTION(BlueprintCallable, Category = "Harvest Progress")
	void CancelHarvest();

	// Set harvest manager (called by UIManager)
	UFUNCTION(BlueprintCallable, Category = "Harvest Progress")
	void SetHarvestManager(class UHarvestManager* InHarvestManager);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void NativeDestruct() override;

	// Handle input for canceling harvest
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

protected:
	// UI Components (bind these in Blueprint)
	UPROPERTY(meta = (BindWidget))
	class UProgressBar* ProgressBar_Harvest;

	UPROPERTY(meta = (BindWidget))
	class UTextBlock* TextBlock_HarvestTitle;

	UPROPERTY(meta = (BindWidget))
	class UTextBlock* TextBlock_Progress;

	UPROPERTY(meta = (BindWidget))
	class UTextBlock* TextBlock_CancelHint;

	// Animation for progress bar
	UPROPERTY(Transient, meta = (BindWidgetAnim))
	class UWidgetAnimation* ProgressAnimation;

private:
	// Current progress value
	float CurrentProgress;

	// Is widget currently visible
	bool bIsVisible;

	// Reference to HarvestManager
	class UHarvestManager* HarvestManager;

	// Bind/unbind to HarvestManager events
	void BindToHarvestManager();
	void UnbindFromHarvestManager();

	// Event handlers
	UFUNCTION()
	void HandleHarvestStarted(const struct FHarvestStartedStruct& HarvestData);
	UFUNCTION()
	void HandleHarvestProgress(float Progress);
	UFUNCTION()
	void HandleHarvestCompleted(const struct FHarvestCompleteStruct& HarvestData);
	UFUNCTION()
	void HandleHarvestError(const struct FHarvestErrorStruct& ErrorData);
};