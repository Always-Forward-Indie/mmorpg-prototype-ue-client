#include "Gameplay/UI/TutorialHintManager.h"
#include "Prototyping.h"
#include "MyGameInstance.h"
#include "Gameplay/Players/BasicPlayer.h"
#include "UI/UIManager.h"
#include "UI/WorldNotificationManager.h"
#include "UI/NotificationStackWidget.h"
#include "Audio/AudioManager.h"

void UTutorialHintManager::Initialize(UMyGameInstance* InGameInstance)
{
	GameInstance = InGameInstance;
}

bool UTutorialHintManager::HasSeenHint(ETutorialHintKind Kind) const
{
	switch (Kind)
	{
	case ETutorialHintKind::PlayerMenu: return bSeenPlayerMenuHint;
	case ETutorialHintKind::Quickbar:   return bSeenQuickbarHint;
	case ETutorialHintKind::CombatLog:  return bSeenCombatLogHint;
	default:                            return true;
	}
}

void UTutorialHintManager::ResetSeenHints()
{
	bSeenPlayerMenuHint = false;
	bSeenQuickbarHint = false;
	bSeenCombatLogHint = false;
	SaveConfig();
}

void UTutorialHintManager::MaybeShowHint(ETutorialHintKind Kind)
{
	if (!GameInstance || HasSeenHint(Kind)) return;
	ShowHintToast(Kind);
	MarkSeen(Kind);
}

void UTutorialHintManager::MarkSeen(ETutorialHintKind Kind)
{
	switch (Kind)
	{
	case ETutorialHintKind::PlayerMenu: bSeenPlayerMenuHint = true; break;
	case ETutorialHintKind::Quickbar:   bSeenQuickbarHint = true;   break;
	case ETutorialHintKind::CombatLog:  bSeenCombatLogHint = true;  break;
	}
	SaveConfig();
}

void UTutorialHintManager::ShowHintToast(ETutorialHintKind Kind)
{
	if (!GameInstance) return;
	ABasicPlayer* Pawn = GameInstance->Player;
	UUIManager* UIMgr = Pawn ? Pawn->GetUIManager() : nullptr;
	UWorldNotificationManager* NotifMgr = UIMgr ? UIMgr->GetWorldNotificationManager() : nullptr;
	if (UNotificationStackWidget* Stack = NotifMgr ? NotifMgr->GetToastWidget() : nullptr)
	{
		TMap<FString, FString> Fields;
		Fields.Add(TEXT("title"), FTutorialHintRule::TitleFor(Kind));
		Fields.Add(TEXT("body"), FTutorialHintRule::BodyFor(Kind));
		Stack->EnqueueActionToast(TEXT("hint"), Fields);
	}
	if (GameInstance->AudioManager)
		GameInstance->AudioManager->PlayUISound(EUISoundEvent::Notification);
}
