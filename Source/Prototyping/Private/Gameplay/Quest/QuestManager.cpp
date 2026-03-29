#include "Gameplay/Quest/QuestManager.h"
#include "Gameplay/Quest/QuestManager.h"
#include "Networking/NetworkManager.h"
#include "MyGameInstance.h"

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

    QuestJournal.Add(Data.questId, Merged);

    UE_LOG(LogTemp, Log, TEXT("QuestManager: Quest %d updated — state=%s step=%d"),
        Data.questId, *Data.state, Data.stepIndex);

    OnQuestUpdatedDelegate.Broadcast(Merged);
}

void UQuestManager::OnQuestOffered(const FQuestOfferedData& Data)
{
    UE_LOG(LogTemp, Log, TEXT("QuestManager: Quest offered — id=%d key=%s"),
        Data.questId, *Data.clientQuestKey);

    OnQuestOfferedDelegate.Broadcast(Data);
}

void UQuestManager::OnQuestTurnedIn(const FQuestTurnedInData& Data)
{
    UE_LOG(LogTemp, Log, TEXT("QuestManager: Quest turned in — id=%d"), Data.questId);
    OnQuestTurnedInDelegate.Broadcast(Data);
}

void UQuestManager::OnExpReceived(const FExpReceivedData& Data)
{
    UE_LOG(LogTemp, Log, TEXT("QuestManager: EXP received — amount=%d"), Data.amount);
    OnExpReceivedDelegate.Broadcast(Data);
}

void UQuestManager::OnItemReceived(const FItemReceivedData& Data)
{
    UE_LOG(LogTemp, Log, TEXT("QuestManager: Item received — id=%d qty=%d"), Data.itemId, Data.quantity);
    OnItemReceivedDelegate.Broadcast(Data);
}

void UQuestManager::OnGoldReceived(const FGoldReceivedData& Data)
{
    UE_LOG(LogTemp, Log, TEXT("QuestManager: Gold received — amount=%d"), Data.amount);
    OnGoldReceivedDelegate.Broadcast(Data);
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
