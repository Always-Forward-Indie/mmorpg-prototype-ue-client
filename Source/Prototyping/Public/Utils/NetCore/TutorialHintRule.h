#pragma once

#include "CoreMinimal.h"
#include "TutorialHintRule.generated.h"

/** One-time tutorial hints (discoverability for menu / quickbar / log). */
UENUM(BlueprintType)
enum class ETutorialHintKind : uint8
{
	PlayerMenu UMETA(DisplayName = "Player Menu"),
	Quickbar   UMETA(DisplayName = "Quickbar"),
	CombatLog  UMETA(DisplayName = "Combat Log"),
};

/**
 * Pure hint content (dependency-free FStrings). Single owner of the
 * one-time tutorial copy shown by UTutorialHintManager.
 */
struct PROTOTYPING_API FTutorialHintRule
{
	static FString TitleFor(ETutorialHintKind Kind)
	{
		switch (Kind)
		{
		case ETutorialHintKind::PlayerMenu: return TEXT("Tip: player menu");
		case ETutorialHintKind::Quickbar:   return TEXT("Tip: quickbar");
		case ETutorialHintKind::CombatLog:  return TEXT("Tip: combat log");
		default:                            return TEXT("Tip");
		}
	}

	static FString BodyFor(ETutorialHintKind Kind)
	{
		switch (Kind)
		{
		case ETutorialHintKind::PlayerMenu:
			return TEXT("Right-click another player for Whisper, Trade and Inspect.");
		case ETutorialHintKind::Quickbar:
			return TEXT("Usable items can ride the quickbar (F6-F10): open the item menu and pick Quickbar.");
		case ETutorialHintKind::CombatLog:
			return TEXT("Damage, loot and XP history lives in the combat log, left side. Filters on top.");
		default:
			return TEXT("");
		}
	}
};
