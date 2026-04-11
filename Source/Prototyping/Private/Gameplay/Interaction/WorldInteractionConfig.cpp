// Copyright Prototyping Project. All Rights Reserved.
#include "Gameplay/Interaction/WorldInteractionConfig.h"

FLinearColor UWorldInteractionConfig::GetDecalColor(EInteractableType Type) const
{
    const FLinearColor* Found = DecalColors.Find(Type);
    if (Found) return *Found;

    // Code-side defaults — active even before the DataAsset is filled in.
    // These can be overridden per-project by adding entries to DecalColors in the asset.
    switch (Type)
    {
    case EInteractableType::MOB_Alive:       return FLinearColor(1.f,  0.15f, 0.15f, 1.f); // red
    case EInteractableType::MOB_Harvestable: return FLinearColor(0.9f, 0.75f, 0.05f, 1.f); // gold
    case EInteractableType::MOB_Harvested:   return FLinearColor(0.5f, 0.5f,  0.5f,  1.f); // grey
    case EInteractableType::NPC:             return FLinearColor(0.1f, 1.f,   0.3f,  1.f); // green
    case EInteractableType::DroppedItem:     return FLinearColor(0.3f, 0.6f,  1.f,   1.f); // blue
    case EInteractableType::RemotePlayer:    return FLinearColor(1.f,  1.f,   0.15f, 1.f); // yellow
    default:                                 return FLinearColor::White;
    }
}

const FCursorIconEntry& UWorldInteractionConfig::GetCursorEntry(EInteractableType Type) const
{
    const FCursorIconEntry* Found = InteractionCursors.Find(Type);
    return Found ? *Found : DefaultCursor;
}
