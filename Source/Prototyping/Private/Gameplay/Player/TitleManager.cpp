#include "Gameplay/Player/TitleManager.h"

void UTitleManager::ApplyTitlesState(const FPlayerTitlesState& InState)
{
    CachedState = InState;

    UE_LOG(LogTemp, Log, TEXT("TitleManager: characterId=%d equippedTitle='%s' earnedCount=%d"),
        InState.characterId, *InState.equippedTitleSlug, InState.earnedTitles.Num());

    OnTitlesUpdated.Broadcast(InState);
}
