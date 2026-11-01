#include "Gameplay/Combat/CombatLogManager.h"
#include "Prototyping.h"
#include "MyGameInstance.h"
#include "Gameplay/UI/TutorialHintManager.h"
#include "Gameplay/Combat/CombatSystemManager.h"
#include "Gameplay/Player/ExperienceManager.h"
#include "Gameplay/Items/InventoryManager.h"
#include "Gameplay/Items/HarvestManager.h"
#include "Gameplay/Quest/QuestManager.h"
#include "Gameplay/Trade/TradeManager.h"
#include "Gameplay/Chat/ChatManager.h"
#include "Services/LocalizationSubsystem.h"

#define LOCTEXT_NAMESPACE "CombatLog"

namespace
{
bool IsRussianLocale(const UMyGameInstance* GI)
{
	if (const ULocalizationSubsystem* Loc = GI ? GI->GetSubsystem<ULocalizationSubsystem>() : nullptr)
		return Loc->GetCurrentLocale().StartsWith(TEXT("ru"), ESearchCase::IgnoreCase);
	return false;
}

FString RuDamage(const FString& Skill, const FString& Target, int32 Dmg, bool bCrit, bool bBlocked, bool bDied, bool bOut, bool bMissed)
{
	if (bMissed && Dmg <= 0)
		return FText::Format(LOCTEXT("MissFmt", "{0} промах {1}"), FText::FromString(bOut ? TEXT("Вы") : TEXT("Враг")), FText::FromString(Skill)).ToString();
	FString Line = bOut
		? FText::Format(LOCTEXT("DmgOutFmt", "Вы ударили {0} ({1}) на {2}"), FText::FromString(Target), FText::FromString(Skill), FText::AsNumber(Dmg)).ToString()
		: FText::Format(LOCTEXT("DmgInFmt", "Вас ударил {0} на {1}"), FText::FromString(Skill), FText::AsNumber(Dmg)).ToString();
	if (bCrit)    Line += TEXT(" (крит!)");
	if (bBlocked) Line += TEXT(" (блок)");
	if (bDied)    Line += TEXT(" — убит");
	return Line;
}

FString RuHeal(const FString& Skill, const FString& Target, int32 N, bool bOut, bool bCrit)
{
	FString Line = bOut
		? FText::Format(LOCTEXT("HealOutFmt", "Ваш {0} исцелил {1} на {2}"), FText::FromString(Skill), FText::FromString(Target), FText::AsNumber(N)).ToString()
		: FText::Format(LOCTEXT("HealInFmt", "{0} исцелил вас на {1}"), FText::FromString(Skill), FText::AsNumber(N)).ToString();
	if (bCrit) Line += TEXT(" (крит!)");
	return Line;
}

FString RuMana(const FString& Skill, int32 N, bool bOut)
{
	return bOut
		? FText::Format(LOCTEXT("ManaOutFmt", "Ваш {0} восстановил {1} маны"), FText::FromString(Skill), FText::AsNumber(N)).ToString()
		: FText::Format(LOCTEXT("ManaInFmt", "{0} восстановил вам {1} маны"), FText::FromString(Skill), FText::AsNumber(N)).ToString();
}

FString RuExp(int32 N, const FString& Reason)
{
	return Reason.IsEmpty()
		? FText::Format(LOCTEXT("ExpFmt", "+{0} опыта"), FText::AsNumber(N)).ToString()
		: FText::Format(LOCTEXT("ExpReasonFmt", "+{0} опыта ({1})"), FText::AsNumber(N), FText::FromString(Reason)).ToString();
}

FString RuGold(int32 N, const FString& Source)
{
	return Source.IsEmpty()
		? FText::Format(LOCTEXT("GoldFmt", "+{0} золота"), FText::AsNumber(N)).ToString()
		: FText::Format(LOCTEXT("GoldSrcFmt", "+{0} золота ({1})"), FText::AsNumber(N), FText::FromString(Source)).ToString();
}
} // namespace

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
	if (ULocalizationSubsystem* LocSys = GameInstance->GetSubsystem<ULocalizationSubsystem>())
	{
		bRussianLocale = LocSys->GetCurrentLocale().StartsWith(TEXT("ru"), ESearchCase::IgnoreCase);
		LocSys->OnLocaleChanged.AddDynamic(this, &UCombatLogManager::HandleLocaleChanged);
	}
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

void UCombatLogManager::HandleLocaleChanged(const FString& NewLocale)
{
	bRussianLocale = NewLocale.StartsWith(TEXT("ru"), ESearchCase::IgnoreCase);
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
			bRussianLocale ? RuDamage(Skill, Target, Result.damage, Result.isCritical,
				Result.isBlocked, Result.targetDied, bOutgoing, Result.isMissed)
			: FCombatLogRule::FormatDamage(Skill, Target, Result.damage, Result.isCritical,
				Result.isBlocked, Result.targetDied, bOutgoing, Result.isMissed));
	else if (Result.isMissed)
		AddEntry(ECombatLogCategory::Damage,
			bRussianLocale ? RuDamage(Skill, Target, 0, false, false, false, bOutgoing, true)
			: FCombatLogRule::FormatDamage(Skill, Target, 0, false, false, false, bOutgoing, true));

	if (Result.healing > 0)
		AddEntry(ECombatLogCategory::Heal,
			bRussianLocale ? RuHeal(Skill, Target, Result.healing, bOutgoing, Result.isCritical)
			: FCombatLogRule::FormatHeal(Skill, Target, Result.healing, bOutgoing, Result.isCritical));
	if (Result.manaHealing > 0)
		AddEntry(ECombatLogCategory::Heal,
			bRussianLocale ? RuMana(Skill, Result.manaHealing, bOutgoing)
			: FCombatLogRule::FormatManaRestore(Skill, Result.manaHealing, bOutgoing));
}

void UCombatLogManager::HandleExpGained(const FExperienceGainEventStruct& Event)
{
	if (Event.experienceGained <= 0) return;
	AddEntry(ECombatLogCategory::Experience,
		bRussianLocale ? RuExp(Event.experienceGained, Event.reasonText)
		: FCombatLogRule::FormatExp(Event.experienceGained, Event.reasonText));
}

void UCombatLogManager::HandleLevelUp(int32 OldLevel, int32 NewLevel, int32 /*NewTotalExperience*/)
{
	AddEntry(ECombatLogCategory::System,
		bRussianLocale
		? FText::Format(LOCTEXT("LevelUpFmt", "Уровень повышен! {0} → {1}"), FText::AsNumber(OldLevel), FText::AsNumber(NewLevel)).ToString()
		: FCombatLogRule::FormatLevelUp(OldLevel, NewLevel));
}

void UCombatLogManager::HandleItemAdded(const FInventoryItemStruct& Item, int32 /*NewQuantity*/)
{
	// Gains-only policy (mirrors the toast stack): removals stay inline in
	// the inventory grid; the log records what arrived.
	AddEntry(ECombatLogCategory::Loot, FCombatLogRule::FormatLoot(DisplayName(Item), Item.quantity));

	// First usable loot teaches the quickbar (one-time hint).
	if (Item.isUsable && GameInstance)
	{
		if (UTutorialHintManager* Hints = GameInstance->GetTutorialHintManager())
			Hints->MaybeShowHint(ETutorialHintKind::Quickbar);
	}
}

void UCombatLogManager::HandleItemUsed(int32 /*ItemId*/, const FString& ItemSlug, const FString& ItemName)
{
	const FString Name = !ItemName.IsEmpty() ? ItemName : ItemSlug;
	AddEntry(ECombatLogCategory::System,
		bRussianLocale
		? FText::Format(LOCTEXT("UsedFmt", "Использовано: {0}"), FText::FromString(Name)).ToString()
		: FCombatLogRule::FormatUsedItem(Name));
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
	AddEntry(ECombatLogCategory::Experience,
		bRussianLocale ? RuExp(Data.amount, Data.source)
		: FCombatLogRule::FormatExp(Data.amount, Data.source));
}

void UCombatLogManager::HandleQuestItem(const FItemReceivedData& Data)
{
	const FString Name = !Data.itemName.IsEmpty() ? Data.itemName : Data.itemSlug;
	AddEntry(ECombatLogCategory::Loot, FCombatLogRule::FormatLoot(Name, Data.quantity));
}

void UCombatLogManager::HandleQuestGold(const FGoldReceivedData& Data)
{
	if (Data.amount <= 0) return;
	AddEntry(ECombatLogCategory::Loot,
		bRussianLocale ? RuGold(Data.amount, Data.source)
		: FCombatLogRule::FormatGold(Data.amount, Data.source));
}

void UCombatLogManager::HandleQuestTurnedIn(const FQuestTurnedInData& Data)
{
	AddEntry(ECombatLogCategory::System,
		bRussianLocale
		? FText::Format(LOCTEXT("QuestDoneFmt", "Задание выполнено (id {0}): наград: {1}"), FText::AsNumber(Data.questId), FText::AsNumber(Data.rewardsReceived.Num())).ToString()
		: FString::Printf(TEXT("Quest complete (id %d): %d reward(s)"), Data.questId, Data.rewardsReceived.Num()));
}

void UCombatLogManager::HandleQuestFailed(const FQuestFailedData& Data)
{
	AddEntry(ECombatLogCategory::System,
		bRussianLocale
		? FText::Format(LOCTEXT("QuestFailFmt", "Задание провалено (id {0})"), FText::AsNumber(Data.questId)).ToString()
		: FString::Printf(TEXT("Quest failed (id %d)"), Data.questId));
}

void UCombatLogManager::HandleTradeCompleted(const FTradeCompleteData& Data)
{
	if (!Data.bSuccess) return;
	AddEntry(ECombatLogCategory::System,
		bRussianLocale
		? FText::Format(LOCTEXT("TradeDoneFmt", "Обмен завершён: +{0} золота, предметов: {1}"), FText::AsNumber(Data.receivedGold), FText::AsNumber(Data.receivedItems.Num())).ToString()
		: FCombatLogRule::FormatTradeComplete(Data.receivedGold, Data.receivedItems.Num()));
	for (const FTradeReceivedItem& Item : Data.receivedItems)
	{
		const FString Name = !Item.slug.IsEmpty()
			? Item.slug
			: FString::Printf(TEXT("item #%d"), Item.itemId);
		AddEntry(ECombatLogCategory::Loot, FCombatLogRule::FormatLoot(Name, Item.quantity));
	}
	if (Data.receivedGold > 0)
		AddEntry(ECombatLogCategory::Loot,
			bRussianLocale ? RuGold(Data.receivedGold, TEXT("trade"))
			: FCombatLogRule::FormatGold(Data.receivedGold, TEXT("trade")));
}

void UCombatLogManager::HandleChatMessage(const FChatMessageStruct& Message)
{
	// Errors only: normal chat stays in the chat window (single owner).
	if (Message.bIsError && !Message.errorMessage.IsEmpty())
		AddEntry(ECombatLogCategory::System, bRussianLocale
			? FString::Printf(TEXT("[чат] %s"), *Message.errorMessage)
			: FString::Printf(TEXT("[chat] %s"), *Message.errorMessage));
}

#undef LOCTEXT_NAMESPACE
