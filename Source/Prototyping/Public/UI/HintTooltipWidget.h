#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "HintTooltipWidget.generated.h"

/**
 * Simple hint tooltip shown when hovering over game bar buttons.
 * Uses UE's native SetToolTip mechanism — no manual tick/positioning.
 *
 * Blueprint subclass should contain:
 *   Hint_Text     (UTextBlock) — the hint label
 */
UCLASS(BlueprintType, Blueprintable)
class PROTOTYPING_API UHintTooltipWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetHintText(const FText& Text) { if (Hint_Text) Hint_Text->SetText(Text); }

protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	UTextBlock* Hint_Text;
};
