#pragma once

#include "CoreMinimal.h"
#include "CombatLogRule.generated.h"

/** Combat-log channel. Bitmask values so the widget filter is a uint8 mask. */
UENUM(BlueprintType)
enum class ECombatLogCategory : uint8
{
	Damage     UMETA(DisplayName = "Damage"),
	Heal       UMETA(DisplayName = "Heal"),
	Loot       UMETA(DisplayName = "Loot"),
	Experience UMETA(DisplayName = "Experience"),
	System     UMETA(DisplayName = "System"),
};

/**
 * Pure combat-log policy (dependency-free: FString/FLinearColor/ints only).
 * Single owner of channel colors, line formats, capacity and filter-mask
 * ops used by UCombatLogManager (content) and UCombatLogWidget (display).
 */
struct PROTOTYPING_API FCombatLogRule
{
	/** Ring-buffer capacity (manager trims the oldest above this). */
	static constexpr int32 MaxEntries = 500;

	/** Widget-side line cap (manager keeps the full history for scrollback). */
	static constexpr int32 MaxWidgetLines = 200;

	static uint8 MaskFor(ECombatLogCategory Category) { return 1u << static_cast<uint8>(Category); }
	static constexpr uint8 AllMask() { return 0x1Fu; }

	static bool IsShown(uint8 Mask, ECombatLogCategory Category) { return (Mask & MaskFor(Category)) != 0; }

	static FLinearColor ColorFor(ECombatLogCategory Category)
	{
		switch (Category)
		{
		case ECombatLogCategory::Damage:     return FLinearColor(1.0f, 0.35f, 0.30f);
		case ECombatLogCategory::Heal:       return FLinearColor(0.35f, 1.0f, 0.45f);
		case ECombatLogCategory::Loot:       return FLinearColor(1.0f, 0.85f, 0.30f);
		case ECombatLogCategory::Experience: return FLinearColor(0.45f, 0.80f, 1.0f);
		case ECombatLogCategory::System:
		default:                             return FLinearColor(0.75f, 0.75f, 0.75f);
		}
	}

	static FString TargetDesc(int32 TargetId, const FString& TargetType)
	{
		return TargetType.IsEmpty()
			? FString::Printf(TEXT("#%d"), TargetId)
			: FString::Printf(TEXT("%s #%d"), *TargetType, TargetId);
	}

	/** Zero-effect results (clean miss, no death) stay on the FCT layer only. */
	static bool ShouldLogResult(int32 Damage, int32 Healing, int32 ManaHealing, bool bTargetDied)
	{
		return Damage > 0 || Healing > 0 || ManaHealing > 0 || bTargetDied;
	}

	static FString FormatDamage(const FString& Skill, const FString& Target, int32 Damage,
		bool bCrit, bool bBlocked, bool bDied, bool bOutgoing, bool bMissed)
	{
		if (bMissed && Damage <= 0)
			return FString::Printf(TEXT("%s %s — missed"), bOutgoing ? TEXT("You") : TEXT("Foe"), *Skill);
		const FString Who = bOutgoing ? TEXT("You") : TEXT("Foe");
		const FString Action = bOutgoing
			? FString::Printf(TEXT("%s hit %s with %s"), *Who, *Target, *Skill)
			: FString::Printf(TEXT("%s hit you with %s"), *Who, *Skill);
		FString Line = FString::Printf(TEXT("%s for %d"), *Action, Damage);
		if (bCrit)    Line += TEXT(" (crit!)");
		if (bBlocked) Line += TEXT(" (blocked)");
		if (bDied)    Line += TEXT(" — slain");
		return Line;
	}

	static FString FormatHeal(const FString& Skill, const FString& Target, int32 Healing, bool bOutgoing, bool bCrit)
	{
		FString Line = bOutgoing
			? FString::Printf(TEXT("Your %s healed %s for %d"), *Skill, *Target, Healing)
			: FString::Printf(TEXT("%s healed you for %d"), *Skill, Healing);
		if (bCrit) Line += TEXT(" (crit!)");
		return Line;
	}

	static FString FormatManaRestore(const FString& Skill, int32 Mana, bool bOutgoing)
	{
		return bOutgoing
			? FString::Printf(TEXT("Your %s restored %d mana"), *Skill, Mana)
			: FString::Printf(TEXT("%s restored %d mana to you"), *Skill, Mana);
	}

	static FString FormatLoot(const FString& Name, int32 Quantity)
	{
		return Quantity > 1
			? FString::Printf(TEXT("+%d x %s"), Quantity, *Name)
			: FString::Printf(TEXT("+ %s"), *Name);
	}

	static FString FormatExp(int32 Amount, const FString& Reason)
	{
		return Reason.IsEmpty()
			? FString::Printf(TEXT("+%d XP"), Amount)
			: FString::Printf(TEXT("+%d XP (%s)"), Amount, *Reason);
	}

	static FString FormatGold(int32 Amount, const FString& Source)
	{
		return Source.IsEmpty()
			? FString::Printf(TEXT("+%d gold"), Amount)
			: FString::Printf(TEXT("+%d gold (%s)"), Amount, *Source);
	}

	static FString FormatLevelUp(int32 OldLevel, int32 NewLevel)
	{
		return FString::Printf(TEXT("Level up! %d → %d"), OldLevel, NewLevel);
	}

	static FString FormatUsedItem(const FString& Name)
	{
		return FString::Printf(TEXT("Used %s"), *Name);
	}

	static FString FormatTradeComplete(int32 Gold, int32 ItemCount)
	{
		return FString::Printf(TEXT("Trade complete: +%d gold, %d item(s)"), Gold, ItemCount);
	}
};
