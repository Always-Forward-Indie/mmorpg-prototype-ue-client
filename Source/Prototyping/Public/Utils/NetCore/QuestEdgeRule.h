#pragma once

#include "CoreMinimal.h"

/**
 * Dialogue-choice edge picking for quest accept / turn-in / advance (pure logic).
 *
 * Canonical owner of the predicates used by UDevModeBotBrain::FindGoalEdge /
 * FindAdvanceEdge. First matching edge wins, 0 means "no edge". Goal edges
 * have priority in the CALLER (it tries FindGoalEdge first), so
 * FindAdvanceEdge may return a goal edge when one is listed first — that is
 * pinned behaviour, not a bug.
 */
struct PROTOTYPING_API FQuestEdgeRule
{
	/** Minimal choice snapshot the predicates need (mapped 1:1 from FDialogueChoice). */
	struct FChoice
	{
		int32 EdgeId = 0;
		bool bHasQuestPreview = false;
		bool bHasTurnInPreview = false;
		bool bConditionMet = false;
		FString ChoiceKey;
	};

	/** First edge carrying a quest offer (or a turn-in when bWantTurnin) whose condition is met; 0 = none. */
	static int32 FindGoalEdge(const TArray<FChoice>& Choices, bool bWantTurnin)
	{
		for (const FChoice& C : Choices)
		{
			const bool bGoal = bWantTurnin ? C.bHasTurnInPreview : C.bHasQuestPreview;
			if (bGoal && C.bConditionMet)
			{
				return C.EdgeId;
			}
		}
		return 0;
	}

	/** First condition-met, non-farewell edge (neutral continue/back); 0 = none. */
	static int32 FindAdvanceEdge(const TArray<FChoice>& Choices)
	{
		for (const FChoice& C : Choices)
		{
			if (!C.bConditionMet || IsFarewellKey(C.ChoiceKey))
			{
				continue;
			}
			return C.EdgeId;
		}
		return 0;
	}

	/** Farewell-class keys never advance a quest leg (leave/back/close...). Case-insensitive. */
	static bool IsFarewellKey(const FString& Key)
	{
		const FString Lower = Key.ToLower();
		return Lower.Contains(TEXT("farewell")) || Lower.Contains(TEXT("bye")) || Lower.Contains(TEXT("leave"))
			|| Lower.Contains(TEXT("goodbye")) || Lower.Contains(TEXT("close")) || Lower.Contains(TEXT("exit"))
			|| Lower.Contains(TEXT("cancel"));
	}
};
