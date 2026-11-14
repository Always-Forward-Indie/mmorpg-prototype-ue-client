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
#include "Gameplay/Mobs/MOBManager.h"
#include "Gameplay/Mobs/BasicMOB.h"
#include "Gameplay/Players/BasicPlayer.h"
#include "Services/LocalizationSubsystem.h"
#include "Utils/JSONParser.h"

#define LOCTEXT_NAMESPACE "CombatLog"

namespace
{
/** Harvest/inventory receipt dedup window: same item+qty twice inside it is one server fact. */
constexpr double LootDedupWindowSec = 5.0;
constexpr int32 MaxRecentLootLines = 64;

bool IsMobNetType(int32 NetType, const FString& TypeString)
{
	if (TypeString.Equals(TEXT("MOB"), ESearchCase::IgnoreCase)) return true;
	if (TypeString.Equals(TEXT("PLAYER"), ESearchCase::IgnoreCase)) return false;
	if (TypeString.Equals(TEXT("SELF"), ESearchCase::IgnoreCase)) return false;
	if (TypeString.Equals(TEXT("NPC"), ESearchCase::IgnoreCase)) return false;
	if (TypeString.Equals(TEXT("AREA"), ESearchCase::IgnoreCase)) return false;
	return NetType == static_cast<int32>(ECasterType::Mob);
}

bool IsPlayerNetType(int32 NetType, const FString& TypeString)
{
	if (TypeString.Equals(TEXT("PLAYER"), ESearchCase::IgnoreCase)) return true;
	if (TypeString.Equals(TEXT("SELF"), ESearchCase::IgnoreCase)) return true;
	if (TypeString.Equals(TEXT("MOB"), ESearchCase::IgnoreCase)) return false;
	if (TypeString.Equals(TEXT("NPC"), ESearchCase::IgnoreCase)) return false;
	if (TypeString.Equals(TEXT("AREA"), ESearchCase::IgnoreCase)) return false;
	return NetType == static_cast<int32>(ECasterType::Player)
		|| NetType == static_cast<int32>(ECasterType::Self);
}

bool IsRussianLocale(const UMyGameInstance* GI)
{
	if (const ULocalizationSubsystem* Loc = GI ? GI->GetSubsystem<ULocalizationSubsystem>() : nullptr)
		return Loc->GetCurrentLocale().StartsWith(TEXT("ru"), ESearchCase::IgnoreCase);
	return false;
}

FString RuDamage(const FString& Skill, const FString& Target, int32 Dmg, bool bCrit, bool bBlocked, bool bDied, bool bOut, bool bMissed, const FString& FoeName)
{
	if (bMissed && Dmg <= 0)
		return FText::Format(LOCTEXT("MissFmt", "{0} промах {1}"), FText::FromString(bOut ? FString(TEXT("Вы")) : FoeName), FText::FromString(Skill)).ToString();
	FString Line = bOut
		? FText::Format(LOCTEXT("DmgOutFmt", "Вы ударили {0} ({1}) на {2}"), FText::FromString(Target), FText::FromString(Skill), FText::AsNumber(Dmg)).ToString()
		: FText::Format(LOCTEXT("DmgInFmt", "Вас ударил {0} ({1}) на {2}"), FText::FromString(FoeName), FText::FromString(Skill), FText::AsNumber(Dmg)).ToString();
	if (bCrit)    Line += TEXT(" (крит!)");
	if (bBlocked) Line += TEXT(" (блок)");
	if (bDied)    Line += TEXT(" — убит");
	return Line;
}

FString RuHeal(const FString& Skill, const FString& Target, int32 N, bool bOut, bool bCrit, const FString& CasterName)
{
	FString Line = bOut
		? FText::Format(LOCTEXT("HealOutFmt", "Ваш {0} исцелил {1} на {2}"), FText::FromString(Skill), FText::FromString(Target), FText::AsNumber(N)).ToString()
		: CasterName.IsEmpty()
		? FText::Format(LOCTEXT("HealInFmt", "{0} исцелил вас на {1}"), FText::FromString(Skill), FText::AsNumber(N)).ToString()
		: FText::Format(LOCTEXT("HealInCasterFmt", "{0} исцелил вас ({1}) на {2}"), FText::FromString(CasterName), FText::FromString(Skill), FText::AsNumber(N)).ToString();
	if (bCrit) Line += TEXT(" (крит!)");
	return Line;
}

FString RuMana(const FString& Skill, int32 N, bool bOut, const FString& CasterName)
{
	if (bOut)
		return FText::Format(LOCTEXT("ManaOutFmt", "Ваш {0} восстановил {1} маны"), FText::FromString(Skill), FText::AsNumber(N)).ToString();
	return CasterName.IsEmpty()
		? FText::Format(LOCTEXT("ManaInFmt", "{0} восстановил вам {1} маны"), FText::FromString(Skill), FText::AsNumber(N)).ToString()
		: FText::Format(LOCTEXT("ManaInCasterFmt", "{0} восстановил вам {1} маны ({2})"), FText::FromString(CasterName), FText::AsNumber(N), FText::FromString(Skill)).ToString();
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

ULocalizationSubsystem* UCombatLogManager::Loc() const
{
	return GameInstance ? GameInstance->GetSubsystem<ULocalizationSubsystem>() : nullptr;
}

FString UCombatLogManager::ResolveItemName(const FString& Slug, const FString& ServerName, int32 ItemId) const
{
	// Locale table wins when it has a real row (fallback echoes the raw slug).
	if (ULocalizationSubsystem* L = Loc())
	{
		if (!Slug.IsEmpty())
		{
			const FText LocName = L->GetItemDisplayName(Slug);
			if (!LocName.IsEmpty() && LocName.ToString() != Slug)
				return LocName.ToString();
		}
	}
	// Packet display strings win when present (same precedence as loot/toast).
	if (!ServerName.IsEmpty() && ServerName != Slug)
		return ServerName;
	if (!Slug.IsEmpty())
		return JSONParser::HumanizeSlugForDisplay(Slug);
	if (ItemId > 0)
		return FString::Printf(TEXT("item #%d"), ItemId);
	return FString();
}

FString UCombatLogManager::ResolveSkillName(const FString& Slug, const FString& ServerName) const
{
	if (ULocalizationSubsystem* L = Loc())
	{
		if (!Slug.IsEmpty())
		{
			// Consumable-use heals arrive with the item slug in skillSlug:
			// a real item row wins over the skill-table humanized fallback.
			const FText ItemText = L->GetItemDisplayName(Slug);
			if (!ItemText.IsEmpty() && ItemText.ToString() != Slug)
				return ItemText.ToString();
			const FText SkillText = L->GetSkillDisplayName(Slug);
			if (!SkillText.IsEmpty())
				return SkillText.ToString();
		}
	}
	if (!ServerName.IsEmpty())
		return ServerName;
	if (!Slug.IsEmpty())
		return JSONParser::HumanizeSlugForDisplay(Slug);
	return bRussianLocale ? TEXT("Неизвестный навык") : TEXT("Unknown skill");
}

FString UCombatLogManager::ResolveCombatantName(int32 ActorId, int32 NetType, const FString& TypeString) const
{
	if (ActorId <= 0 || !GameInstance) return FString();
	if (IsOwnCharacter(ActorId)) return FString(); // caller uses You-phrasing

	if (IsMobNetType(NetType, TypeString))
	{
		if (UMOBManager* Mobs = GameInstance->GetMOBManager())
		{
			if (const AActor* Found = Mobs->FindMobActor(ActorId))
			{
				if (const ABasicMOB* Mob = Cast<ABasicMOB>(Found))
				{
					const FMOBStruct Data = Mob->GetMOBData();
					if (ULocalizationSubsystem* L = Loc())
					{
						if (!Data.mobSlug.IsEmpty())
						{
							const FText N = L->GetMobDisplayName(Data.mobSlug);
							if (!N.IsEmpty() && N.ToString() != Data.mobSlug)
								return N.ToString();
						}
					}
					if (!Data.mobName.IsEmpty()) return Data.mobName;
					if (!Data.mobSlug.IsEmpty())
						return JSONParser::HumanizeSlugForDisplay(Data.mobSlug);
				}
			}
		}
		// Actor not (yet) replicated: keep the id, lose the raw type tag.
		return FString::Printf(TEXT("Mob #%d"), ActorId);
	}

	if (IsPlayerNetType(NetType, TypeString))
	{
		if (const ABasicPlayer* P = GameInstance->GetPlayerByCharacterId(ActorId))
		{
			const FString N = P->GetPlayerData().characterData.characterName;
			if (!N.IsEmpty()) return N;
		}
		return FString::Printf(TEXT("Player #%d"), ActorId);
	}

	return FCombatLogRule::TargetDesc(ActorId, TypeString);
}

FString UCombatLogManager::ResolveQuestName(const FString& ClientKey, int32 QuestId) const
{
	if (!ClientKey.IsEmpty())
	{
		if (ULocalizationSubsystem* L = Loc())
		{
			const FText T = L->GetQuestDisplayName(ClientKey);
			if (!T.IsEmpty() && T.ToString() != ClientKey)
				return T.ToString();
		}
		const FString H = JSONParser::HumanizeSlugForDisplay(ClientKey);
		if (!H.IsEmpty()) return H;
	}
	return FString::Printf(TEXT("quest #%d"), QuestId);
}

bool UCombatLogManager::ShouldSuppressLootLine(int32 ItemId, int32 Quantity)
{
	if (ItemId <= 0 || Quantity <= 0) return false;
	if (const TPair<int32, FDateTime>* Found = RecentLootLines.Find(ItemId))
	{
		if (Found->Key == Quantity
			&& (FDateTime::Now() - Found->Value).GetTotalSeconds() < LootDedupWindowSec)
			return true;
	}
	return false;
}

void UCombatLogManager::RecordLootLine(int32 ItemId, int32 Quantity)
{
	if (ItemId <= 0 || Quantity <= 0) return;
	RecentLootLines.Add(ItemId, TPair<int32, FDateTime>(Quantity, FDateTime::Now()));
	if (RecentLootLines.Num() > MaxRecentLootLines)
	{
		const FDateTime Cutoff = FDateTime::Now() - FTimespan::FromSeconds(LootDedupWindowSec);
		for (auto It = RecentLootLines.CreateIterator(); It; ++It)
		{
			if (It.Value().Value < Cutoff)
				It.RemoveCurrent();
		}
	}
}

void UCombatLogManager::HandleSkillResult(const FSkillResultData& Result)
{
	if (!FCombatLogRule::ShouldLogResult(Result.damage, Result.healing, Result.manaHealing, Result.targetDied))
		return;

	const bool bOutgoing = IsOwnCharacter(Result.casterId);
	const FString Skill = ResolveSkillName(Result.skillSlug, Result.skillName);
	// Server omits display strings since S1a: resolve the other side locally.
	const FString Other = bOutgoing
		? ResolveCombatantName(Result.targetId, Result.targetType, Result.targetTypeString)
		: ResolveCombatantName(Result.casterId, Result.casterType, Result.casterTypeString);
	const FString Target = bOutgoing
		? (IsOwnCharacter(Result.targetId)
			? (bRussianLocale ? TEXT("вас") : TEXT("you"))
			: FCombatLogRule::CombatantFallback(Other, Result.targetId, Result.targetTypeString))
		: FString();
	const FString Foe = !bOutgoing
		? FCombatLogRule::CombatantFallback(Other, Result.casterId, Result.casterTypeString)
		: FString();

	if (Result.damage > 0)
		AddEntry(ECombatLogCategory::Damage,
			bRussianLocale ? RuDamage(Skill, Target, Result.damage, Result.isCritical,
				Result.isBlocked, Result.targetDied, bOutgoing, Result.isMissed, Foe)
			: FCombatLogRule::FormatDamage(Skill, Target, Result.damage, Result.isCritical,
				Result.isBlocked, Result.targetDied, bOutgoing, Result.isMissed, Foe));
	else if (Result.isMissed)
		AddEntry(ECombatLogCategory::Damage,
			bRussianLocale ? RuDamage(Skill, Target, 0, false, false, false, bOutgoing, true, Foe)
			: FCombatLogRule::FormatDamage(Skill, Target, 0, false, false, false, bOutgoing, true, Foe));

	if (Result.healing > 0)
		AddEntry(ECombatLogCategory::Heal,
			bRussianLocale ? RuHeal(Skill, Target, Result.healing, bOutgoing, Result.isCritical, Foe)
			: FCombatLogRule::FormatHeal(Skill, Target, Result.healing, bOutgoing, Result.isCritical, Foe));
	if (Result.manaHealing > 0)
		AddEntry(ECombatLogCategory::Heal,
			bRussianLocale ? RuMana(Skill, Result.manaHealing, bOutgoing, Foe)
			: FCombatLogRule::FormatManaRestore(Skill, Result.manaHealing, bOutgoing, Foe));
}

void UCombatLogManager::HandleExpGained(const FExperienceGainEventStruct& Event)
{
	if (Event.experienceGained <= 0) return;
	// reasonText carries the raw server slug ("mob_kill"): display the mapped
	// reason via the single owner (UExperienceManager::ExperienceReasonToString).
	FString Reason;
	if (Event.reason != EExperienceReason::None)
		Reason = UExperienceManager::ExperienceReasonToString(Event.reason);
	else if (!Event.reasonText.IsEmpty())
		Reason = JSONParser::HumanizeSlugForDisplay(Event.reasonText);
	if (!Event.sourceName.IsEmpty() && Reason.IsEmpty())
		Reason = Event.sourceName;
	AddEntry(ECombatLogCategory::Experience,
		bRussianLocale ? RuExp(Event.experienceGained, Reason)
		: FCombatLogRule::FormatExp(Event.experienceGained, Reason));
}

void UCombatLogManager::HandleLevelUp(int32 OldLevel, int32 NewLevel, int32 /*NewTotalExperience*/)
{
	AddEntry(ECombatLogCategory::System,
		bRussianLocale
		? FText::Format(LOCTEXT("LevelUpFmt", "Уровень повышен! {0} → {1}"), FText::AsNumber(OldLevel), FText::AsNumber(NewLevel)).ToString()
		: FCombatLogRule::FormatLevelUp(OldLevel, NewLevel));
}

void UCombatLogManager::HandleItemAdded(const FInventoryItemStruct& Item, int32 NewQuantity)
{
	// NewQuantity is the gain at every broadcast site (delta for stack growth,
	// full quantity for brand-new rows): the log shows what arrived, not the total.
	const int32 Gain = NewQuantity > 0 ? NewQuantity : Item.quantity;
	if (Gain <= 0) return;
	// Harvest pickups own their receipt line (HandleLootPickup): the optimistic
	// add fired from the same response would double-log the server fact.
	if (Item.isHarvestItem) return;
	const FString Slug = !Item.slug.IsEmpty() ? Item.slug : Item.itemSlug;
	const FString Name = ResolveItemName(Slug, Item.name, Item.itemId);
	if (!FCombatLogRule::ShouldLogLoot(Name, Gain)) return;
	if (ShouldSuppressLootLine(Item.itemId, Gain)) return;
	AddEntry(ECombatLogCategory::Loot, FCombatLogRule::FormatLoot(Name, Gain));
	RecordLootLine(Item.itemId, Gain);

	// First usable loot teaches the quickbar (one-time hint).
	if (Item.isUsable && GameInstance)
	{
		if (UTutorialHintManager* Hints = GameInstance->GetTutorialHintManager())
			Hints->MaybeShowHint(ETutorialHintKind::Quickbar);
	}
}

void UCombatLogManager::HandleItemUsed(int32 /*ItemId*/, const FString& ItemSlug, const FString& ItemName)
{
	const FString Name = ResolveItemName(ItemSlug, ItemName, 0);
	if (Name.IsEmpty()) return;
	AddEntry(ECombatLogCategory::System,
		bRussianLocale
		? FText::Format(LOCTEXT("UsedFmt", "Использовано: {0}"), FText::FromString(Name)).ToString()
		: FCombatLogRule::FormatUsedItem(Name));
}

void UCombatLogManager::HandleLootPickup(const FCorpseLootPickupResponseStruct& Pickup)
{
	if (!Pickup.success) return;
	for (const FHarvestItemStruct& Item : Pickup.pickedUpItems)
	{
		if (Item.quantity <= 0) continue;
		const FString Name = ResolveItemName(Item.itemSlug, Item.name, Item.itemId);
		if (!FCombatLogRule::ShouldLogLoot(Name, Item.quantity)) continue;
		if (ShouldSuppressLootLine(Item.itemId, Item.quantity)) continue;
		AddEntry(ECombatLogCategory::Loot, FCombatLogRule::FormatLoot(Name, Item.quantity));
		RecordLootLine(Item.itemId, Item.quantity);
	}
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
	const FString Name = ResolveItemName(Data.itemSlug, Data.itemName, Data.itemId);
	if (!FCombatLogRule::ShouldLogLoot(Name, Data.quantity)) return;
	// Quest grants also land via inventory deltas: same fact, one line.
	if (ShouldSuppressLootLine(Data.itemId, Data.quantity)) return;
	AddEntry(ECombatLogCategory::Loot, FCombatLogRule::FormatLoot(Name, Data.quantity));
	RecordLootLine(Data.itemId, Data.quantity);
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
	const FString QuestName = ResolveQuestName(Data.clientQuestKey, Data.questId);
	AddEntry(ECombatLogCategory::System,
		bRussianLocale
		? FText::Format(LOCTEXT("QuestDoneFmt", "Задание выполнено: {0} (наград: {1})"), FText::FromString(QuestName), FText::AsNumber(Data.rewardsReceived.Num())).ToString()
		: FString::Printf(TEXT("Quest complete: %s (%d reward(s))"), *QuestName, Data.rewardsReceived.Num()));
}

void UCombatLogManager::HandleQuestFailed(const FQuestFailedData& Data)
{
	const FString QuestName = ResolveQuestName(Data.clientQuestKey, Data.questId);
	AddEntry(ECombatLogCategory::System,
		bRussianLocale
		? FText::Format(LOCTEXT("QuestFailFmt", "Задание провалено: {0}"), FText::FromString(QuestName)).ToString()
		: FString::Printf(TEXT("Quest failed: %s"), *QuestName));
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
		const FString Name = ResolveItemName(Item.slug, FString(), Item.itemId);
		if (!FCombatLogRule::ShouldLogLoot(Name, Item.quantity)) continue;
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
