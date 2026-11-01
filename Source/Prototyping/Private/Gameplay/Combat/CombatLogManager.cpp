#include "Gameplay/Combat/CombatLogManager.h"
#include "Prototyping.h"
#include "MyGameInstance.h"
#include "Gameplay/Combat/CombatSystemManager.h"
#include "Gameplay/Player/ExperienceManager.h"
#include "Gameplay/Items/InventoryManager.h"
#include "Gameplay/Items/HarvestManager.h"
#include "Gameplay/Quest/QuestManager.h"
#include "Gameplay/Trade/TradeManager.h"
#include "Gameplay/Chat/ChatManager.h"

void UCombatLogManager::Initialize(UMyGameInstance* InGameInstance)
{
	GameInstance = InGameInstance;
	if (!GameInstance) return;

	if (UCombatSystemManager* Combat = GameInstance->GetCombatSystemManager())
		Combat->OnSkillCompleted.AddDynamic(this, &UCombatLogManager::HandleSkillResult);
	if (UExperienceManager* Exp = GameInstance->GetExperienceManager())
	{
		Exp->OnExperienceGained.AddDynamic(this, &UCombatLogManager::HandleExpGained);
		Exp->OnLevelUp.AddDynamic(this, &UCombatLogManager::HandleLevelUp);
	}
	if (UInventoryManager* Inv = GameInstance->GetInventoryManager())
	{
		Inv->OnItemAdded.AddDynamic(this, &UCombatLogManager::HandleItemAdded);
		Inv->OnItemUsed.AddDynamic(this, &UCombatLogManager::HandleItemUsed);
	}
	if (UHarvestManager* Harvest = GameInstance->GetHarvestManager())
		Harvest->OnLootPickupSuccess.AddDynamic(this, &UCombatLogManager::HandleLootPickup);
	if (UQuestManager* Quests = GameInstance->GetQuestManager())
	{
		Quests->OnExpReceivedDelegate.AddDynamic(this, &UCombatLogManager::HandleQuestExp);
		Quests->OnItemReceivedDelegate.AddDynamic(this, &UCombatLogManager::HandleQuestItem);
		Quests->OnGoldReceivedDelegate.AddDynamic(this, &UCombatLogManager::HandleQuestGold);
		Quests->OnQuestTurnedInDelegate.AddDynamic(this, &UCombatLogManager::HandleQuestTurnedIn);
		Quests->OnQuestFailedDelegate.AddDynamic(this, &UCombatLogManager::HandleQuestFailed);
	}
	if (UTradeManager* Trade = GameInstance->GetTradeManager())
		Trade->OnTradeCompletedDelegate.AddDynamic(this, &UCombatLogManager::HandleTradeCompleted);
	if (UChatManager* Chat = GameInstance->GetChatManager())
		Chat->OnChatMessageReceived.AddDynamic(this, &UCombatLogManager::HandleChatMessage);
}

void UCombatLogManager::AddEntry(ECombatLogCategory Category, const FString& Text)
{
	if (Text.IsEmpty()) return;
	FCombatLogEntry Entry;
	Entry.Category = Category;
	Entry.Text = Text;
	Entry.Timestamp = FDateTime::Now();
	Entries.Add(MoveTemp(Entry));
	while (Entries.Num() > FCombatLogRule::MaxEntries)
		Entries.RemoveAt(0);
	OnLogEntryAdded.Broadcast(Entries.Last());
}

void UCombatLogManager::ClearLog()
{
	Entries.Reset();
}

bool UCombatLogManager::IsOwnCharacter(int32 CharacterId) const
{
	return GameInstance && CharacterId > 0 && CharacterId == GameInstance->GetCurrentCharacterID();
}

FString UCombatLogManager::DisplayName(const FInventoryItemStruct& Item)
{
	if (!Item.name.IsEmpty()) return Item.name;
	if (!Item.slug.IsEmpty()) return Item.slug;
	if (!Item.itemSlug.IsEmpty()) return Item.itemSlug;
	return FString::Printf(TEXT("item #%d"), Item.itemId);
}

FString UCombatLogManager::DisplayName(const FHarvestItemStruct& Item)
{
	if (!Item.name.IsEmpty()) return Item.name;
	if (!Item.itemSlug.IsEmpty()) return Item.itemSlug;
	return FString::Printf(TEXT("item #%d"), Item.itemId);
}

void UCombatLogManager::HandleSkillResult(const FSkillResultData& Result)
{
	if (!FCombatLogRule::ShouldLogResult(Result.damage, Result.healing, Result.manaHealing, Result.targetDied))
		return;

	const bool bOutgoing = IsOwnCharacter(Result.casterId);
	const FString Skill = Result.skillName.IsEmpty() ? Result.skillSlug : Result.skillName;
	const FString Target = FCombatLogRule::TargetDesc(Result.targetId, Result.targetTypeString);

	if (Result.damage > 0)
		AddEntry(ECombatLogCategory::Damage,
			FCombatLogRule::FormatDamage(Skill, Target, Result.damage, Result.isCritical,
				Result.isBlocked, Result.targetDied, bOutgoing, Result.isMissed));
	else if (Result.isMissed)
		AddEntry(ECombatLogCategory::Damage,
			FCombatLogRule::FormatDamage(Skill, Target, 0, false, false, false, bOutgoing, true));

	if (Result.healing > 0)
		AddEntry(ECombatLogCategory::Heal,
			FCombatLogRule::FormatHeal(Skill, Target, Result.healing, bOutgoing, Result.isCritical));
	if (Result.manaHealing > 0)
		AddEntry(ECombatLogCategory::Heal,
			FCombatLogRule::FormatManaRestore(Skill, Result.manaHealing, bOutgoing));
}

void UCombatLogManager::HandleExpGained(const FExperienceGainEventStruct& Event)
{
	if (Event.experienceGained <= 0) return;
	AddEntry(ECombatLogCategory::Experience,
		FCombatLogRule::FormatExp(Event.experienceGained, Event.reasonText));
}

void UCombatLogManager::HandleLevelUp(int32 OldLevel, int32 NewLevel, int32 /*NewTotalExperience*/)
{
	AddEntry(ECombatLogCategory::System, FCombatLogRule::FormatLevelUp(OldLevel, NewLevel));
}

void UCombatLogManager::HandleItemAdded(const FInventoryItemStruct& Item, int32 /*NewQuantity*/)
{
	// Gains-only policy (mirrors the toast stack): removals stay inline in
	// the inventory grid; the log records what arrived.
	AddEntry(ECombatLogCategory::Loot, FCombatLogRule::FormatLoot(DisplayName(Item), Item.quantity));
}

void UCombatLogManager::HandleItemUsed(int32 /*ItemId*/, const FString& ItemSlug, const FString& ItemName)
{
	const FString Name = !ItemName.IsEmpty() ? ItemName : ItemSlug;
	AddEntry(ECombatLogCategory::System, FCombatLogRule::FormatUsedItem(Name));
}

void UCombatLogManager::HandleLootPickup(const FCorpseLootPickupResponseStruct& Pickup)
{
	if (!Pickup.success) return;
	for (const FHarvestItemStruct& Item : Pickup.pickedUpItems)
		AddEntry(ECombatLogCategory::Loot, FCombatLogRule::FormatLoot(DisplayName(Item), Item.quantity));
}

void UCombatLogManager::HandleQuestExp(const FExpReceivedData& Data)
{
	if (Data.amount <= 0) return;
	AddEntry(ECombatLogCategory::Experience, FCombatLogRule::FormatExp(Data.amount, Data.source));
}

void UCombatLogManager::HandleQuestItem(const FItemReceivedData& Data)
{
	const FString Name = !Data.itemName.IsEmpty() ? Data.itemName : Data.itemSlug;
	AddEntry(ECombatLogCategory::Loot, FCombatLogRule::FormatLoot(Name, Data.quantity));
}

void UCombatLogManager::HandleQuestGold(const FGoldReceivedData& Data)
{
	if (Data.amount <= 0) return;
	AddEntry(ECombatLogCategory::Loot, FCombatLogRule::FormatGold(Data.amount, Data.source));
}

void UCombatLogManager::HandleQuestTurnedIn(const FQuestTurnedInData& Data)
{
	AddEntry(ECombatLogCategory::System,
		FString::Printf(TEXT("Quest complete (id %d): %d reward(s)"), Data.questId, Data.rewardsReceived.Num()));
}

void UCombatLogManager::HandleQuestFailed(const FQuestFailedData& Data)
{
	AddEntry(ECombatLogCategory::System,
		FString::Printf(TEXT("Quest failed (id %d)"), Data.questId));
}

void UCombatLogManager::HandleTradeCompleted(const FTradeCompleteData& Data)
{
	if (!Data.bSuccess) return;
	AddEntry(ECombatLogCategory::System,
		FCombatLogRule::FormatTradeComplete(Data.receivedGold, Data.receivedItems.Num()));
	for (const FTradeReceivedItem& Item : Data.receivedItems)
	{
		const FString Name = !Item.slug.IsEmpty()
			? Item.slug
			: FString::Printf(TEXT("item #%d"), Item.itemId);
		AddEntry(ECombatLogCategory::Loot, FCombatLogRule::FormatLoot(Name, Item.quantity));
	}
	if (Data.receivedGold > 0)
		AddEntry(ECombatLogCategory::Loot, FCombatLogRule::FormatGold(Data.receivedGold, TEXT("trade")));
}

void UCombatLogManager::HandleChatMessage(const FChatMessageStruct& Message)
{
	// Errors only: normal chat stays in the chat window (single owner).
	if (Message.bIsError && !Message.errorMessage.IsEmpty())
		AddEntry(ECombatLogCategory::System, FString::Printf(TEXT("[chat] %s"), *Message.errorMessage));
}
