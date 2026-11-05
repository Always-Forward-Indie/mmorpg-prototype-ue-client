#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Utils/NetCore/TutorialHintRule.h"
#include "TutorialHintManager.generated.h"

class UMyGameInstance;

/**
 * UTutorialHintManager
 *
 * One-time tutorial toasts (discoverability). Seen-flags persist in
 * Game.ini (config=Game) so each hint shows once ever, not every login.
 * Read-only: never sends packets, only reads GameInstance + UI singletons.
 */
UCLASS(config=Game)
class PROTOTYPING_API UTutorialHintManager : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(UMyGameInstance* InGameInstance);

	/** Show the hint unless already seen (persists the flag only when shown —
	 *  a dropped toast (UI not ready) is retried on the next trigger, not lost). */
	UFUNCTION(BlueprintCallable, Category = "Tutorial")
	void MaybeShowHint(ETutorialHintKind Kind);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Tutorial")
	bool HasSeenHint(ETutorialHintKind Kind) const;

	/** Test/dev reset (does not touch other state). */
	UFUNCTION(BlueprintCallable, Category = "Tutorial")
	void ResetSeenHints();

private:
	/** Enqueues the toast + plays the earcon. Returns true only when the toast
	 *  actually reached the stack (no visual → no sound, never silent). */
	bool ShowHintToast(ETutorialHintKind Kind);
	void MarkSeen(ETutorialHintKind Kind);

	UPROPERTY()
	UMyGameInstance* GameInstance = nullptr;

	UPROPERTY(config)
	bool bSeenPlayerMenuHint = false;

	UPROPERTY(config)
	bool bSeenQuickbarHint = false;

	UPROPERTY(config)
	bool bSeenCombatLogHint = false;
};
