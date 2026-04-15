// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/WidgetComponent.h"
#include "MOBHeadInfo.generated.h"

class UW_MOBHeadInfoWidget;

/**
 * WidgetComponent that owns and drives the MOB head-info widget.
 * Delegates all data updates to UW_MOBHeadInfoWidget via a lazy-resolved pointer.
 */
UCLASS()
class PROTOTYPING_API UMOBHeadInfo : public UWidgetComponent
{
	GENERATED_BODY()

public:
	/** Full update: HP bar, name, level and aggression colour. */
	void UpdateInfo(float CurrentHP, float MaxHP, const FString& MobName, int32 MobLevel, bool bMobAggressive);

	void UpdateHealth(float CurrentHP, float MaxHP);
	void UpdateMobName(const FString& MobName);
	void UpdateMobLevel(int32 MobLevel);
	void UpdateMobAggressive(bool bMobAggressive);

	void ShowWidget(bool bShow);

private:
	/** Resolved once, the first time any Update* method is called. */
	UPROPERTY()
	UW_MOBHeadInfoWidget* CachedWidget = nullptr;

	UW_MOBHeadInfoWidget* GetHeadWidget();
};
