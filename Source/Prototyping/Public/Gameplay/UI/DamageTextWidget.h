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
	void Init(float Damage, bool bCrit, EDamageType Type, bool bIsSelfTarget = false);
	void InitSpecialText(const FString& Text, FLinearColor Color);
	void SetPendingDamage(float Damage, bool bCrit, EDamageType Type, bool bIsSelfTarget = false);
	void SetPendingSpecialText(const FString& Text, FLinearColor Color);
	void PlayDamageAnimation();
	void SetOwningManager(UFloatingCombatTextManager* Manager);

	// Check if widget has been constructed
	bool IsConstructed() const { return bIsConstructed; }

	// Function to clean up widget state when returning to pool
	void ResetWidgetState();

protected:
	UFUNCTION()
	void OnAnimCompleted();

	UPROPERTY(Transient)
	bool bIsConstructed = false;

	bool bPendingInit = false;
	float PendingDamage = 0.f;
	bool PendingCrit = false;
	bool PendingSelfTarget = false;
	EDamageType PendingType = EDamageType::Physical;

	// For special text (MISSED, BLOCKED, etc.)
	bool bIsSpecialText = false;
	FString PendingSpecialText = "";
	FLinearColor PendingSpecialColor = FLinearColor::White;

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

	// Friend class to allow access to private members for cleanup
	friend class UFloatingCombatTextManager;
};