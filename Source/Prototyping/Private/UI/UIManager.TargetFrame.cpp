#include "UI/UIManager.h"
#include "Gameplay/UI/PlayerInterfaceWidget.h"
#include "Gameplay/UI/MobTargetFrameWidget.h"

void UUIManager::ShowMobTargetFrame(const FString& MobSlug, const FString& MobName, int32 MobLevel, int32 CurrentHP, int32 MaxHP, bool bIsAggro, UTexture2D* MobIcon,
	int32 TargetUID, ECasterType TargetType, int32 ViewerLevel,
	const FString& RankCode, bool bIsChampion, const FString& ChampionKind, bool bIsDead)
{
	DisplayedTargetUID = TargetUID;
	DisplayedTargetType = TargetType;
	if (PlayerInterfaceWidget)
	{
		UMobTargetFrameWidget* TargetFrame = PlayerInterfaceWidget->GetMobTargetFrameWidget();
		if (TargetFrame)
		{
			TargetFrame->SetMobInfo(MobSlug, MobName, MobLevel, CurrentHP, MaxHP, bIsAggro, MobIcon);
			TargetFrame->SetRank(RankCode, bIsChampion, ChampionKind, ViewerLevel);
			TargetFrame->SetDead(bIsDead);
		}
	}
}

void UUIManager::SetMobTargetRank(const FString& RankCode, bool bIsChampion, const FString& ChampionKind, int32 ViewerLevel)
{
	if (PlayerInterfaceWidget)
	{
		if (UMobTargetFrameWidget* TargetFrame = PlayerInterfaceWidget->GetMobTargetFrameWidget())
		{
			TargetFrame->SetRank(RankCode, bIsChampion, ChampionKind, ViewerLevel);
		}
	}
}

void UUIManager::SetMobTargetDead(bool bIsDead)
{
	if (PlayerInterfaceWidget)
	{
		if (UMobTargetFrameWidget* TargetFrame = PlayerInterfaceWidget->GetMobTargetFrameWidget())
		{
			TargetFrame->SetDead(bIsDead);
		}
	}
}

void UUIManager::UpdateMobTargetFrameHP(int32 CurrentHP, int32 MaxHP)
{
	if (PlayerInterfaceWidget)
	{
		UMobTargetFrameWidget* TargetFrame = PlayerInterfaceWidget->GetMobTargetFrameWidget();
		if (TargetFrame)
		{
			TargetFrame->UpdateHP(CurrentHP, MaxHP);
		}
	}
}

void UUIManager::HideMobTargetFrame()
{
	DisplayedTargetUID = 0;
	DisplayedTargetType = ECasterType::None;
	if (PlayerInterfaceWidget)
	{
		UMobTargetFrameWidget* TargetFrame = PlayerInterfaceWidget->GetMobTargetFrameWidget();
		if (TargetFrame)
		{
			TargetFrame->ClearTarget();
		}
	}
}
