#include "Data/DataStructs.h"

ENPCInteractionState FNPCStruct::ComputeInteractionState() const
{
    if (!isInteractable)
    {
        return ENPCInteractionState::NotInteractable;
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

