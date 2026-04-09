// Fill out your copyright notice in the Description page of Project Settings.


#include "Gameplay/UI/MOBHeadInfo.h"

void UMOBHeadInfo::BeginPlay()
{
	Super::BeginPlay();

	// �������� ������
	MobWidget = GetWidget();

	if (MobWidget)
	{
		// ���� ProgressBar'� � TextBlock'� � UI
		HealthBar = Cast<UProgressBar>(MobWidget->GetWidgetFromName(TEXT("HealthProgressBar")));
		ManaBar = Cast<UProgressBar>(MobWidget->GetWidgetFromName(TEXT("ManaProgressBar")));
		MobNameText = Cast<UTextBlock>(MobWidget->GetWidgetFromName(TEXT("MobNameText")));
		MobLevelText = Cast<UTextBlock>(MobWidget->GetWidgetFromName(TEXT("MobLevelText")));
	}

	// �� ��������� ��������
	SetVisibility(false);
}

void UMOBHeadInfo::UpdateInfo(float CurrentHP, float MaxHP, float CurrentMP, float MaxMP, const FString& MobName, int MobLevel, bool isMobAggressive)
{
	if (!MobWidget) return;

	// Bidirectional assignment — was a one-way latch (bIsAggressive could never go back to false)
	bIsAggressive = isMobAggressive;

	// Update HP
	if (HealthBar && MaxHP > 0.0f)
	{
		float NewPercent = CurrentHP / MaxHP;
		HealthBar->SetPercent(NewPercent);
		
		// ��������� ���� ��� �������
		UE_LOG(LogTemp, Warning, TEXT("MOB HP Updated: %f/%f = %f%%"), CurrentHP, MaxHP, NewPercent * 100.0f);
	}

	// ��������� MP
	if (ManaBar && MaxMP > 0.0f)
	{
		float NewPercent = CurrentMP / MaxMP;
		ManaBar->SetPercent(NewPercent);
	}

	// ��������� ��� ����
	if (MobNameText)
	{
		MobNameText->SetText(FText::FromString(MobName));

		if (bIsAggressive)
		{
			MobNameText->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.0f, 0.0f, 1.0f)));
		}
		else
		{
			MobNameText->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 1.0f, 0.0f, 1.0f)));
		}
	}

	// ��������� �������
	if (MobLevelText)
	{
		MobLevelText->SetText(FText::FromString(FString::Printf(TEXT("LVL: %d"), MobLevel)));
	}
}

void UMOBHeadInfo::UpdateHealth(float CurrentHP, float MaxHP)
{
	if (!MobWidget) return;
	// ��������� HP
	if (HealthBar)
	{
		HealthBar->SetPercent(CurrentHP / MaxHP);
	}
}

void UMOBHeadInfo::UpdateMana(float CurrentMP, float MaxMP)
{
	if (!MobWidget) return;
	// ��������� MP
	if (ManaBar)
	{
		ManaBar->SetPercent(CurrentMP / MaxMP);
	}
}

void UMOBHeadInfo::UpdateMobName(const FString& MobName)
{
	if (!MobWidget) return;
	// ��������� ��� ����
	if (MobNameText)
	{
		MobNameText->SetText(FText::FromString(MobName));
	}
}

void UMOBHeadInfo::UpdateMobLevel(int MobLevel)
{
	if (!MobWidget) return;
	// ��������� �������
	if (MobLevelText)
	{
		MobLevelText->SetText(FText::FromString(FString::Printf(TEXT("LVL: %d"), MobLevel)));
	}
}


void UMOBHeadInfo::UpdateMobAggressive(bool isMobAggressive)
{
	if (!MobWidget) return;
	if (isMobAggressive)
	{
		MobNameText->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.0f, 0.0f, 1.0f)));
	}
	else
	{
		// Reset to default neutral (yellow) color when mob becomes passive again
		MobNameText->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 1.0f, 0.0f, 1.0f)));
	}
}

void UMOBHeadInfo::ShowWidget(bool bShow)
{
	SetVisibility(bShow);
}