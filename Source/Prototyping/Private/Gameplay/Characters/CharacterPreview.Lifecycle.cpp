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

// ─────────────────────────────────────────────────────────────────────────────
// Character Select previews
// ─────────────────────────────────────────────────────────────────────────────

