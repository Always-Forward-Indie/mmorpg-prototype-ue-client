#include "Gameplay/Quest/QuestManager.h"
#include "Gameplay/Quest/QuestManager.h"
#include "Networking/NetworkManager.h"
#include "MyGameInstance.h"
#include "Gameplay/NPCs/NPCManager.h"
#include "Gameplay/NPCs/BasicNPC.h"

UQuestManager::UQuestManager()
{
}

void UQuestManager::Initialize(UNetworkManager* InNetworkManager, UMyGameInstance* InGameInstance)
{
    if (!InNetworkManager || !InGameInstance)
    {
        UE_LOG(LogTemp, Error, TEXT("QuestManager: Initialize called with null parameters"));
        return;
    }
    NetworkManager = InNetworkManager;
    GameInstance   = InGameInstance;
    UE_LOG(LogTemp, Log, TEXT("QuestManager: Initialized"));
}

// ??? Called by QuestNetworkHandler ????????????????????????????????????????????

void UQuestManager::OnQuestUpdated(const FQuestProgressData& Data)
{
    // Merge: preserve clientQuestKey if the new packet omits it
    FQuestProgressData* Existing = QuestJournal.Find(Data.questId);
    FQuestProgressData Merged = Data;
    if (Existing && Data.clientQuestKey.IsEmpty())
    {
        Merged.clientQuestKey = Existing->clientQuestKey;
    }

    // Preserve client-side tracking flag — server never sends bTracked
    if (Existing)
    {
        Merged.bTracked = Existing->bTracked;
    }

    QuestJournal.Add(Data.questId, Merged);

    UE_LOG(LogTemp, Log, TEXT("QuestManager: Quest %d updated � state=%s step=%d"),
        Data.questId, *Data.state, Data.stepIndex);

    OnQuestUpdatedDelegate.Broadcast(Merged);

    // Update NPC quest icons based on the new quest state
    if (!Data.questSlug.IsEmpty())
    {
        FString NpcStatus;
        if (Data.state == TEXT("active"))       NpcStatus = TEXT("in_progress");
        else if (Data.state == TEXT("completed")) NpcStatus = TEXT("completable");
        else if (Data.state == TEXT("turned_in")) NpcStatus = TEXT("turned_in");
        else if (Data.state == TEXT("failed"))    NpcStatus = TEXT("failed");

        if (!NpcStatus.IsEmpty())
        {
            UpdateNPCQuestIcons(Data.questSlug, NpcStatus);
        }
    }
}

void UQuestManager::OnQuestOffered(const FQuestOfferedData& Data)
{
    UE_LOG(LogTemp, Log, TEXT("QuestManager: Quest offered � id=%d key=%s"),
        Data.questId, *Data.clientQuestKey);

    OnQuestOfferedDelegate.Broadcast(Data);
}

void UQuestManager::OnQuestTurnedIn(const FQuestTurnedInData& Data)
{
    UE_LOG(LogTemp, Log, TEXT("QuestManager: Quest turned in � id=%d"), Data.questId);
    OnQuestTurnedInDelegate.Broadcast(Data);
}

void UQuestManager::OnExpReceived(const FExpReceivedData& Data)
{
    UE_LOG(LogTemp, Log, TEXT("QuestManager: EXP received � amount=%d"), Data.amount);
    OnExpReceivedDelegate.Broadcast(Data);
}

void UQuestManager::OnItemReceived(const FItemReceivedData& Data)
{
    UE_LOG(LogTemp, Log, TEXT("QuestManager: Item received � id=%d qty=%d"), Data.itemId, Data.quantity);
    OnItemReceivedDelegate.Broadcast(Data);
}

void UQuestManager::OnGoldReceived(const FGoldReceivedData& Data)
{
    UE_LOG(LogTemp, Log, TEXT("QuestManager: Gold received � amount=%d"), Data.amount);
    OnGoldReceivedDelegate.Broadcast(Data);
}
void UQuestManager::OnQuestFailed(const FQuestFailedData& Data)
{
    // Update journal entry state to failed
    if (FQuestProgressData* Existing = QuestJournal.Find(Data.questId))
    {
        Existing->state = TEXT("failed");
    }

    UE_LOG(LogTemp, Log, TEXT("QuestManager: Quest failed – id=%d key=%s"), Data.questId, *Data.clientQuestKey);
    OnQuestFailedDelegate.Broadcast(Data);

    if (!Data.clientQuestKey.IsEmpty())
    {
        UpdateNPCQuestIcons(Data.clientQuestKey, TEXT("failed"));
    }
}

void UQuestManager::OnReputationChanged(const FReputationChangedData& Data)
{
    UE_LOG(LogTemp, Log, TEXT("QuestManager: Reputation changed – faction=%s delta=%d"), *Data.faction, Data.delta);
    OnReputationChangedDelegate.Broadcast(Data);
}
// ??? Queries ??????????????????????????????????????????????????????????????????

FQuestProgressData UQuestManager::GetQuestData(int32 QuestId) const
{
    const FQuestProgressData* Found = QuestJournal.Find(QuestId);
    return Found ? *Found : FQuestProgressData();
}

TArray<FQuestProgressData> UQuestManager::GetAllQuests() const
{
    TArray<FQuestProgressData> Out;
    QuestJournal.GenerateValueArray(Out);
    return Out;
}

TArray<FQuestProgressData> UQuestManager::GetActiveQuests() const
{
    TArray<FQuestProgressData> Out;
    for (const auto& Pair : QuestJournal)
    {
        if (Pair.Value.state == TEXT("active"))
        {
            Out.Add(Pair.Value);
        }
    }
    return Out;
}

TArray<FQuestProgressData> UQuestManager::GetCompletedQuests() const
{
    TArray<FQuestProgressData> Out;
    for (const auto& Pair : QuestJournal)
    {
        if (Pair.Value.state == TEXT("completed") || Pair.Value.state == TEXT("turned_in"))
        {
            Out.Add(Pair.Value);
        }
    }
    return Out;
}

void UQuestManager::SetQuestTracked(int32 QuestId, bool bTracked)
{
    FQuestProgressData* Found = QuestJournal.Find(QuestId);
    if (!Found) return;

    if (Found->bTracked == bTracked) return;

    Found->bTracked = bTracked;
    OnQuestUpdatedDelegate.Broadcast(*Found);
}

TArray<FQuestProgressData> UQuestManager::GetTrackedQuests() const
{
    TArray<FQuestProgressData> Out;
    for (const auto& Pair : QuestJournal)
    {
        if (Pair.Value.state == TEXT("active") && Pair.Value.bTracked)
        {
            Out.Add(Pair.Value);
        }
    }
    return Out;
}

void UQuestManager::UpdateNPCQuestIcons(const FString& QuestSlug, const FString& NewState)
{
    if (!GameInstance)
    {
        return;
    }

    UNPCManager* NPCMgr = GameInstance->GetNPCManager();
    if (!NPCMgr)
    {
        return;
    }

    TArray<ABasicNPC*> AllNPCs = NPCMgr->GetAllNPCs();
    for (ABasicNPC* NPC : AllNPCs)
    {
        if (!NPC) continue;

        FNPCStruct Data = NPC->GetNPCData();
        bool bUpdated = false;

        for (FNPCQuestEntry& Quest : Data.quests)
        {
            if (Quest.slug == QuestSlug)
            {
                Quest.status = NewState;
                bUpdated = true;
            }
        }

        if (bUpdated)
        {
            NPC->UpdateNPCQuestData(Data.quests);
        }
    }
}
