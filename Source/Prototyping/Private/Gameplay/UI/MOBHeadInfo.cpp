// Fill out your copyright notice in the Description page of Project Settings.

#include "Gameplay/UI/MOBHeadInfo.h"
#include "Gameplay/UI/W_MOBHeadInfoWidget.h"

// S5b: mob level chrome gathered via LOCTEXT.
#define LOCTEXT_NAMESPACE "MOBHeadInfo"

UW_MOBHeadInfoWidget* UMOBHeadInfo::GetHeadWidget()
{
	if (!CachedWidget)
	{
		CachedWidget = Cast<UW_MOBHeadInfoWidget>(GetUserWidgetObject());
	}
	return CachedWidget;
}

void UMOBHeadInfo::UpdateInfo(float CurrentHP, float MaxHP, const FString& MobName, int32 MobLevel, bool bMobAggressive)
{
	UW_MOBHeadInfoWidget* W = GetHeadWidget();
	if (!W) return;

	W->UpdateHealth(CurrentHP, MaxHP);
	W->UpdateNameAndLevel(MobName, MobLevel);
	W->SetAggressive(bMobAggressive);
}

void UMOBHeadInfo::UpdateHealth(float CurrentHP, float MaxHP)
{
	if (UW_MOBHeadInfoWidget* W = GetHeadWidget())
	{
		W->UpdateHealth(CurrentHP, MaxHP);
	}
}

void UMOBHeadInfo::UpdateMobName(const FString& MobName)
{
	if (UW_MOBHeadInfoWidget* W = GetHeadWidget())
	{
		W->UpdateNameAndLevel(MobName, -1);
	}
}

void UMOBHeadInfo::UpdateMobLevel(int32 MobLevel)
{
	if (UW_MOBHeadInfoWidget* W = GetHeadWidget())
	{
		if (W->MobLevelText)
		{
			W->MobLevelText->SetText(FText::Format(LOCTEXT("LevelValue", "LVL: {0}"), FText::AsNumber(MobLevel)));
		}
	}
}

void UMOBHeadInfo::UpdateMobAggressive(bool bMobAggressive)
{
	if (UW_MOBHeadInfoWidget* W = GetHeadWidget())
	{
		W->SetAggressive(bMobAggressive);
	}
}

void UMOBHeadInfo::ShowWidget(bool bShow)
{
	SetVisibility(bShow);
}

#undef LOCTEXT_NAMESPACE