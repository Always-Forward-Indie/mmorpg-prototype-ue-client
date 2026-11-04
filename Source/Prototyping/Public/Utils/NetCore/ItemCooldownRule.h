#pragma once

#include "CoreMinimal.h"
#include "Utils/NetCore/CooldownFormat.h"

/**
 * Item consumable cooldown rule (pure logic, no UObject).
 *
 * Canonical owner of the predictive item-cooldown overlay math shared by
 * UInventorySlotWidget and UItemQuickSlotWidget: the server enforces the real
 * cooldown, the client only displays remaining/total/ratio from the catalog
 * useEffects[0].cooldownSeconds echo (FOnItemCooldown).
 * Pinned by MMO.Items.CooldownRule.
 */
struct PROTOTYPING_API FItemCooldownRule
{
	static float Remaining(float NowSec, float EndSec)
	{
		const float Rem = EndSec - NowSec;
		return Rem > 0.0f ? Rem : 0.0f;
	}

	static float Ratio(float RemainingSec, float TotalSec)
	{
		if (TotalSec <= 0.0f) return 0.0f;
		const float R = RemainingSec / TotalSec;
		return FMath::Clamp(R, 0.0f, 1.0f);
	}

	static bool IsActive(float NowSec, float EndSec)
	{
		return FCooldownState::IsOnCooldown(NowSec, EndSec);
	}

	static FString Format(float RemainingSec)
	{
		return FCooldownFormat::Format(RemainingSec);
	}
};
