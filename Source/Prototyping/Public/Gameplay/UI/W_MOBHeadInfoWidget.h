// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/ScaleBox.h"
#include "W_MOBHeadInfoWidget.generated.h"

/**
 * Widget displayed above a MOB's head showing HP bar, name and level.
 * All UPROPERTY(meta = (BindWidget)) fields must exist with matching names
 * in the Blueprint asset derived from this class.
 */
UCLASS()
class PROTOTYPING_API UW_MOBHeadInfoWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// ── Required Blueprint elements ─────────────────────────────────────────
	UPROPERTY(meta = (BindWidget))
	UProgressBar* HealthProgressBar;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* MobNameText;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* MobLevelText;

	UPROPERTY(meta = (BindWidget))
	UScaleBox* RootScaleBox;

	// ── Update API ───────────────────────────────────────────────────────────
	UFUNCTION(BlueprintCallable)
	void UpdateHealth(float CurrentHP, float MaxHP);

	UFUNCTION(BlueprintCallable)
	void UpdateNameAndLevel(const FString& Name, int32 Level);

	UFUNCTION(BlueprintCallable)
	void SetAggressive(bool bAggressive);

	UFUNCTION(BlueprintCallable)
	void SetWidgetScale(float Scale);
};
