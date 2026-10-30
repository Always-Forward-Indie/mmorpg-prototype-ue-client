#pragma once

#include "CoreMinimal.h"
#include "ShopStatusStyle.generated.h"

/**
 * Severity of a shop result line (vendor / repair / skill shop).
 *
 * Single source of truth so success and failure never look identical:
 * green receipt, neutral info, red error. Pinned by MMO.Shop.StatusStyle.
 */
UENUM(BlueprintType)
enum class EShopStatusSeverity : uint8
{
	/** Action completed (bought / sold / repaired / learned). Green. */
	Success UMETA(DisplayName = "Success"),
	/** Neutral note (nothing to do). Default text color. */
	Info    UMETA(DisplayName = "Info"),
	/** Action failed (not enough gold, out of range, ...). Red + error sound. */
	Error   UMETA(DisplayName = "Error")
};

/**
 * Pure color mapping for EShopStatusSeverity (no world, no widgets).
 * Callers apply the result to their Status_Text via SetColorAndOpacity.
 */
struct PROTOTYPING_API FShopStatusStyle
{
	static FLinearColor ColorFor(EShopStatusSeverity Severity)
	{
		switch (Severity)
		{
		case EShopStatusSeverity::Success:
			return FLinearColor(0.35f, 1.0f, 0.45f);
		case EShopStatusSeverity::Error:
			return FLinearColor(1.0f, 0.35f, 0.35f);
		case EShopStatusSeverity::Info:
		default:
			return FLinearColor::White;
		}
	}
};
