#include "Data/DataStructs.h"

ENPCInteractionState FNPCStruct::ComputeInteractionState() const
{
    if (!isInteractable)
    {
        return ENPCInteractionState::NotInteractable;
    }

    // Check quests array for status-based interaction state
    for (const FNPCQuestEntry& Quest : quests)
    {
        if (Quest.status == TEXT("completable"))
        {
            return ENPCInteractionState::QuestComplete;
        }
        if (Quest.status == TEXT("in_progress"))
        {
            return ENPCInteractionState::QuestInProgress;
        }
        if (Quest.status == TEXT("available"))
        {
            return ENPCInteractionState::QuestAvailable;
        }
    }

    if (!questId.IsEmpty())
    {
        return ENPCInteractionState::QuestAvailable;
    }
    if (!dialogueId.IsEmpty())
    {
        return ENPCInteractionState::DialogueOnly;
    }
    return ENPCInteractionState::Dialogue;
}

