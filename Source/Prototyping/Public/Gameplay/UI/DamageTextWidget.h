#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FloatingCombatTextManager.h"
#include "Components/TextBlock.h"
#include "Animation/WidgetAnimation.h"
#include "DamageTextWidget.generated.h"

class UTextBlock;
class UWidgetAnimation;

UCLASS()
class PROTOTYPING_API UDamageTextWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void Init(float Damage, bool bCrit, EDamageType Type);
	void SetPendingDamage(float Damage, bool bCrit, EDamageType Type);
	void PlayDamageAnimation();
	void SetOwningManager(UFloatingCombatTextManager* Manager);

protected:
	UFUNCTION()
	void OnAnimCompleted();

	UPROPERTY(Transient)
	bool bIsConstructed = false;

	bool bPendingInit = false;
	float PendingDamage = 0.f;
	bool PendingCrit = false;
	EDamageType PendingType = EDamageType::Physical;


	// constructor
	virtual void NativeConstruct() override;

	// destructor
	virtual void NativeDestruct() override;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* DamageText;

	UPROPERTY(meta = (BindWidgetAnim), Transient)
	UWidgetAnimation* ShowAnim;

private:
	UPROPERTY()
	UFloatingCombatTextManager* OwningManager;
	
	FTimerHandle TimerHandle;
	FWidgetAnimationDynamicEvent AnimationFinishedDelegate;

public:
	bool HasValidBindings() const
	{
		return IsValid(DamageText) && IsValid(ShowAnim);
	}
};