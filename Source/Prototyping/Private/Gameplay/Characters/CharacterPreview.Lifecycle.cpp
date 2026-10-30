// Character Preview Manager — Implementation

#include "Gameplay/Characters/CharacterPreviewManager.h"
#include "MyGameInstance.h"
#include "Gameplay/Players/BasicPlayer.h"
#include "Gameplay/Equipment/EquipmentVisualComponent.h"
#include "Gameplay/Players/CosmeticVisualComponent.h"
#include "Gameplay/Items/ItemManager.h"
#include "Data/CharacterVisualData.h"
#include "Data/DataStructs.h"
#include "Engine/AssetManager.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Components/CapsuleComponent.h"
#include "Gameplay/UI/NameplateCanvasWidget.h"
#include "GameFramework/CharacterMovementComponent.h"

#include "Gameplay/Interaction/TargetDecalComponent.h"
#include "Gameplay/Interaction/IWorldInteractable.h"
#include "Gameplay/Interaction/WorldInteractionConfig.h"

// ─────────────────────────────────────────────────────────────────────────────
// Lifecycle
// ─────────────────────────────────────────────────────────────────────────────

void UCharacterPreviewManager::Initialize(UMyGameInstance* GI)
{
	GameInstanceRef = GI;
	HighlightedIndex = -1;
	bRotationEnabled = false;
	PreviewYawAccum = 0.0f;
}

void UCharacterPreviewManager::Cleanup()
{
	ClearSelectPreviews();
	ClearCreatePreview();

	// Remove the login-level nameplate canvas from the viewport
	if (IsValid(LoginNameplateCanvas))
	{
		LoginNameplateCanvas->RemoveFromParent();
		LoginNameplateCanvas = nullptr;
	}

	GameInstanceRef = nullptr;
}

void UCharacterPreviewManager::TeardownForTravel()
{
	// Cancel pending podium walks first (actors stop ticking with us).
	PendingMoves.Empty();

	// Detach nameplates so no delegate fires during world teardown.
	if (IsValid(LoginNameplateCanvas))
	{
		LoginNameplateCanvas->UnregisterAll();
		LoginNameplateCanvas->RemoveFromParent();
		LoginNameplateCanvas = nullptr;
	}

	// Hide everything now (loading screen covers the frame), drop our refs,
	// and let OpenLevel's CleanupWorld destroy the actors off the hot path.
	for (ABasicPlayer* Actor : SelectPreviewActors)
	{
		if (IsValid(Actor))
		{
			Actor->SetActorHiddenInGame(true);
			Actor->SetActorEnableCollision(false);
			Actor->SetActorTickEnabled(false);
		}
	}
	SelectPreviewActors.Empty();
	CachedPodiumTransforms.Empty();
	HighlightedIndex = -1;

	if (IsValid(CreatePreviewActor))
	{
		CreatePreviewActor->SetActorHiddenInGame(true);
		CreatePreviewActor->SetActorEnableCollision(false);
		CreatePreviewActor->SetActorTickEnabled(false);
	}
	CreatePreviewActor = nullptr;
	bRotationEnabled = false;
	PreviewYawAccum = 0.0f;

	GameInstanceRef = nullptr;
}

// ─────────────────────────────────────────────────────────────────────────────
// Character Select previews
// ─────────────────────────────────────────────────────────────────────────────

