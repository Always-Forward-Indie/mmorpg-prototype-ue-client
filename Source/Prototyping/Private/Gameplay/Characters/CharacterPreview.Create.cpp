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

void UCharacterPreviewManager::UpdateCreatePreview(const FString& ClassSlug, const FString& RaceSlug, const FString& GenderName)
{
	if (!GameInstanceRef || !GameInstanceRef->GetWorld()) return;

	// Use the first podium location for create preview
	const FVector SpawnPos = GameInstanceRef->CreatePreviewLocation;
	const FRotator SpawnRot = GameInstanceRef->CreatePreviewRotation;

	if (!CreatePreviewActor || !IsValid(CreatePreviewActor))
	{
		CreatePreviewActor = SpawnPreviewActor(FTransform(SpawnRot, SpawnPos));
	}

	if (CreatePreviewActor)
	{
		CreatePreviewActor->SetPlayerClass(ClassSlug);
		CreatePreviewActor->SetPlayerRace(RaceSlug);
		CreatePreviewActor->SetPlayerGender(GenderName);
		ApplyVisualToActor(CreatePreviewActor, ClassSlug, RaceSlug, GenderName);

		// Reset rotation accumulator when visual changes
		PreviewYawAccum = 0.0f;
	}
}

void UCharacterPreviewManager::ClearCreatePreview()
{
	if (IsValid(CreatePreviewActor) && !CreatePreviewActor->IsActorBeingDestroyed())
	{
		CreatePreviewActor->Destroy();
	}
	CreatePreviewActor = nullptr;
	bRotationEnabled = false;
	PreviewYawAccum = 0.0f;
}

// ─────────────────────────────────────────────────────────────────────────────
// Camera blending
// ─────────────────────────────────────────────────────────────────────────────

