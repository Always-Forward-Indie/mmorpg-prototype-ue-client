#include "Gameplay/Emotes/EmoteManager.h"

// ---------------------------------------------------------------------------
// Definitions
// ---------------------------------------------------------------------------

void UEmoteManager::LoadDefinitions(const TArray<FEmoteDefinitionData>& InDefinitions)
{
    Definitions.Empty(InDefinitions.Num());
    for (const FEmoteDefinitionData& Def : InDefinitions)
    {
        if (!Def.slug.IsEmpty())
        {
            Definitions.Add(Def.slug, Def);
        }
    }
    UE_LOG(LogTemp, Log, TEXT("EmoteManager: loaded %d emote definitions"), Definitions.Num());
}

TArray<FEmoteDefinitionData> UEmoteManager::GetAllDefinitions() const
{
    TArray<FEmoteDefinitionData> Out;
    Definitions.GenerateValueArray(Out);
    Out.Sort([](const FEmoteDefinitionData& A, const FEmoteDefinitionData& B)
    {
        return A.sortOrder < B.sortOrder;
    });
    return Out;
}

bool UEmoteManager::GetDefinitionBySlug(const FString& Slug, FEmoteDefinitionData& OutDef) const
{
    if (const FEmoteDefinitionData* Found = Definitions.Find(Slug))
    {
        OutDef = *Found;
        return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// Per-player state
// ---------------------------------------------------------------------------

void UEmoteManager::ApplyPlayerEmotes(const FPlayerEmotesState& InState)
{
    CachedPlayerEmotes = InState;
    UE_LOG(LogTemp, Log, TEXT("EmoteManager: applied %d emotes for character %d"),
        InState.emotes.Num(), InState.characterId);
    OnPlayerEmotesLoaded.Broadcast(CachedPlayerEmotes);
}

TArray<FEmoteDefinitionData> UEmoteManager::GetPlayerEmotesByCategory(const FString& Category) const
{
    if (Category.IsEmpty())
    {
        return CachedPlayerEmotes.emotes;
    }

    TArray<FEmoteDefinitionData> Out;
    for (const FEmoteDefinitionData& Def : CachedPlayerEmotes.emotes)
    {
        if (Def.category.Equals(Category, ESearchCase::IgnoreCase))
        {
            Out.Add(Def);
        }
    }
    return Out;
}

// ---------------------------------------------------------------------------
// Broadcast
// ---------------------------------------------------------------------------

void UEmoteManager::DispatchEmoteAction(int32 CharacterId, const FString& Slug, const FString& AnimationName)
{
    OnEmoteActionReceived.Broadcast(CharacterId, Slug, AnimationName);
}
