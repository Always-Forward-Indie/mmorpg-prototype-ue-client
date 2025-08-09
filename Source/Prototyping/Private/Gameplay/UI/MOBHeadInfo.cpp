// Fill out your copyright notice in the Description page of Project Settings.


#include "Gameplay/UI/MOBHeadInfo.h"

void UMOBHeadInfo::BeginPlay()
{
	Super::BeginPlay();

	// Получаем виджет
	MobWidget = GetWidget();

	if (MobWidget)
	{
		// Ищем ProgressBar'ы и TextBlock'и в UI
		HealthBar = Cast<UProgressBar>(MobWidget->GetWidgetFromName(TEXT("HealthProgressBar")));
		ManaBar = Cast<UProgressBar>(MobWidget->GetWidgetFromName(TEXT("ManaProgressBar")));
		MobNameText = Cast<UTextBlock>(MobWidget->GetWidgetFromName(TEXT("MobNameText")));
		MobLevelText = Cast<UTextBlock>(MobWidget->GetWidgetFromName(TEXT("MobLevelText")));
	}

	// По умолчанию скрываем
	SetVisibility(false);
}

void UMOBHeadInfo::UpdateInfo(float CurrentHP, float MaxHP, float CurrentMP, float MaxMP, const FString& MobName, int MobLevel, bool isMobAggressive)
{
	if (!MobWidget) return;

	if (isMobAggressive)
	{
		bIsAggressive = true;
	}

	// Обновляем HP
	if (HealthBar && MaxHP > 0.0f)
	{
		float NewPercent = CurrentHP / MaxHP;
		HealthBar->SetPercent(NewPercent);
		
		// Добавляем логи для отладки
		UE_LOG(LogTemp, Warning, TEXT("MOB HP Updated: %f/%f = %f%%"), CurrentHP, MaxHP, NewPercent * 100.0f);
	}

	// Обновляем MP
	if (ManaBar && MaxMP > 0.0f)
	{
		float NewPercent = CurrentMP / MaxMP;
		ManaBar->SetPercent(NewPercent);
	}

	// Обновляем имя моба
	if (MobNameText)
	{
		MobNameText->SetText(FText::FromString(MobName));

		//if mob is aggressive, change color
		if (bIsAggressive)
		{
			MobNameText->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.0f, 0.0f, 1.0f)));
		}
	}

	// Обновляем уровень
	if (MobLevelText)
	{
		MobLevelText->SetText(FText::FromString(FString::Printf(TEXT("LVL: %d"), MobLevel)));
	}
}

void UMOBHeadInfo::UpdateHealth(float CurrentHP, float MaxHP)
{
	if (!MobWidget) return;
	// Обновляем HP
	if (HealthBar)
	{
		HealthBar->SetPercent(CurrentHP / MaxHP);
	}
}

void UMOBHeadInfo::UpdateMana(float CurrentMP, float MaxMP)
{
	if (!MobWidget) return;
	// Обновляем MP
	if (ManaBar)
	{
		ManaBar->SetPercent(CurrentMP / MaxMP);
	}
}

void UMOBHeadInfo::UpdateMobName(const FString& MobName)
{
	if (!MobWidget) return;
	// Обновляем имя моба
	if (MobNameText)
	{
		MobNameText->SetText(FText::FromString(MobName));
	}
}

void UMOBHeadInfo::UpdateMobLevel(int MobLevel)
{
	if (!MobWidget) return;
	// Обновляем уровень
	if (MobLevelText)
	{
		MobLevelText->SetText(FText::FromString(FString::Printf(TEXT("LVL: %d"), MobLevel)));
	}
}


void UMOBHeadInfo::UpdateMobAggressive(bool isMobAggressive)
{
	if (!MobWidget) return;
	//if mob is aggressive, change color
	if (isMobAggressive)
	{
		MobNameText->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.0f, 0.0f, 1.0f)));
	}
}

void UMOBHeadInfo::ShowWidget(bool bShow)
{
	SetVisibility(bShow);
}