#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Data/DataStructs.h"
#include "Utils/NetCore/CombatLogRule.h"
#include "CombatLogManager.generated.h"

class UMyGameInstance;

/** One retained combat-log line (data only; color comes from FCombatLogRule). */
USTRUCT(BlueprintType)
struct PROTOTYPING_API FCombatLogEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Log")
	ECombatLogCategory Category = ECombatLogCategory::System;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Log")
	FString Text;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat Log")
	FDateTime Timestamp = FDateTime::Now();

	FCombatLogEntry() {}
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCombatLogEntryAdded, const FCombatLogEntry&, Entry);

/**
 * UCombatLogManager
 *
 * Read-only sink for authoritative server facts: combat results, XP,
 * loot pickups, quest rewards, trade receipts, chat errors. Owns a capped
 * ring buffer (FCombatLogRule::MaxEntries) and broadcasts each line for
 * the widget. Never sends packets, never mutates game state.
 */
UCLASS(BlueprintType)
class PROTOTYPING_API UCombatLogManager : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(UMyGameInstance* InGameInstance);

	/** Append a preformatted line (used by tests, bots and edge callers). */
	UFUNCTION(BlueprintCallable, Category = "Combat Log")
	void AddEntry(ECombatLogCategory Category, const FString& Text);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Combat Log")
	const TArray<FCombatLogEntry>& GetEntries() const { return Entries; }

	UFUNCTION(BlueprintCallable, Category = "Combat Log")
	void ClearLog();

	UPROPERTY(BlueprintAssignable, Category = "Combat Log")
	FOnCombatLogEntryAdded OnLogEntryAdded;

protected:
	// --- Upstream subscriptions (all guard against null managers) ---
	UFUNCTION() void HandleSkillResult(const FSkillResultData& Result);
	UFUNCTION() void HandleExpGained(const FExperienceGainEventStruct& Event);
	UFUNCTION() void HandleLevelUp(int32 OldLevel, int32 NewLevel, int32 NewTotalExperience);
	UFUNCTION() void HandleItemAdded(const FInventoryItemStruct& Item, int32 NewQuantity);
	UFUNCTION() void HandleItemUsed(int32 ItemId, const FString& ItemSlug, const FString& ItemName);
	UFUNCTION() void HandleLootPickup(const FCorpseLootPickupResponseStruct& Pickup);
	UFUNCTION() void HandleQuestExp(const FExpReceivedData& Data);
	UFUNCTION() void HandleQuestItem(const FItemReceivedData& Data);
	UFUNCTION() void HandleQuestGold(const FGoldReceivedData& Data);
	UFUNCTION() void HandleQuestTurnedIn(const FQuestTurnedInData& Data);
	UFUNCTION() void HandleQuestFailed(const FQuestFailedData& Data);
	UFUNCTION() void HandleTradeCompleted(const FTradeCompleteData& Data);
	UFUNCTION() void HandleChatMessage(const FChatMessageStruct& Message);

private:
	/** "You" phrasing when the local character is the caster/recipient. */
	bool IsOwnCharacter(int32 CharacterId) const;

	static FString DisplayName(const FInventoryItemStruct& Item);
	static FString DisplayName(const FHarvestItemStruct& Item);

	UPROPERTY()
	UMyGameInstance* GameInstance = nullptr;

	UPROPERTY()
	TArray<FCombatLogEntry> Entries;
};
