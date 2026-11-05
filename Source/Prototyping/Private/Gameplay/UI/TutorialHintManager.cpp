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
	// Persist the flag only when the toast actually showed: a dropped toast
	// (UI not ready yet) is retried on the next trigger instead of being lost.
	if (ShowHintToast(Kind))
	{
		MarkSeen(Kind);
	}
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

bool UTutorialHintManager::ShowHintToast(ETutorialHintKind Kind)
{
	if (!GameInstance) return false;
	ABasicPlayer* Pawn = GameInstance->Player;
	UUIManager* UIMgr = Pawn ? Pawn->GetUIManager() : nullptr;
	UWorldNotificationManager* NotifMgr = UIMgr ? UIMgr->GetWorldNotificationManager() : nullptr;
	if (UNotificationStackWidget* Stack = NotifMgr ? NotifMgr->GetToastWidget() : nullptr)
	{
		TMap<FString, FString> Fields;
		Fields.Add(TEXT("title"), FTutorialHintRule::TitleFor(Kind));
		Fields.Add(TEXT("body"), FTutorialHintRule::BodyFor(Kind));
		Stack->EnqueueActionToast(TEXT("hint"), Fields);

		// Earcon only for a toast the player can actually see — never sound-only.
		if (GameInstance->AudioManager)
			GameInstance->AudioManager->PlayUISound(EUISoundEvent::Notification);
		return true;
	}
	UE_LOG(LogGameplay, Verbose, TEXT("TutorialHintManager: toast stack not ready, hint %d deferred"),
		static_cast<int32>(Kind));
	return false;
}
