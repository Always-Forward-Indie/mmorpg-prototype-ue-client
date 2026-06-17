#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "IdleWarningWidget.generated.h"

class UTextBlock;

UCLASS(Abstract, BlueprintType, Blueprintable)
class PROTOTYPING_API UIdleWarningWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Idle Warning")
	void ShowIdleWarning(int32 TotalSecondsRemaining);

	UFUNCTION(BlueprintCallable, Category = "Idle Warning")
	void HideIdleWarning();

	UFUNCTION(BlueprintCallable, Category = "Idle Warning")
	void UpdateCountdown(int32 SecondsRemaining);

protected:
	virtual void NativeConstruct() override;

	UFUNCTION(BlueprintNativeEvent, Category = "Idle Warning")
	void PlayShowAnimation();
	virtual void PlayShowAnimation_Implementation() {}

	UFUNCTION(BlueprintNativeEvent, Category = "Idle Warning")
	void PlayHideAnimation();
	virtual void PlayHideAnimation_Implementation() { SetVisibility(ESlateVisibility::Collapsed); }

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Idle Warning")
	UTextBlock* TitleText = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Idle Warning")
	UTextBlock* CountdownText = nullptr;
};
