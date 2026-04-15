// Fill out your copyright notice in the Description page of Project Settings.


#include "Gameplay/UI/W_MOBHeadInfoWidget.h"

void UW_MOBHeadInfoWidget::UpdateHealth(float CurrentHP, float MaxHP)
{
	if (!HealthProgressBar || MaxHP <= 0.f) return;
	HealthProgressBar->SetPercent(CurrentHP / MaxHP);
}

void UW_MOBHeadInfoWidget::UpdateNameAndLevel(const FString& Name, int32 Level)
{
	if (MobNameText)
	{
		MobNameText->SetText(FText::FromString(Name));
	}

	if (MobLevelText)
	{
		MobLevelText->SetText(FText::FromString(FString::Printf(TEXT("LVL: %d"), Level)));
	}
}

void UW_MOBHeadInfoWidget::SetAggressive(bool bAggressive)
{
	if (!MobNameText) return;

	const FLinearColor Color = bAggressive
		? FLinearColor(1.f, 0.f, 0.f, 1.f)   // Red — aggressive
		: FLinearColor(1.f, 1.f, 0.f, 1.f);  // Yellow — neutral

	MobNameText->SetColorAndOpacity(FSlateColor(Color));
}

void UW_MOBHeadInfoWidget::SetWidgetScale(float Scale)
{
	FWidgetTransform Transform;
	Transform.Scale = FVector2D(Scale, Scale);
	SetRenderTransform(Transform);
}