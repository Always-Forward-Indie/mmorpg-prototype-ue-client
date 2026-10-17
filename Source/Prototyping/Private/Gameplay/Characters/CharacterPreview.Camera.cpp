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

void UCharacterPreviewManager::BlendToSelectCamera(float BlendTime)
{
	if (!GameInstanceRef || !GameInstanceRef->GetWorld()) return;

	APlayerController* PC = GameInstanceRef->GetWorld()->GetFirstPlayerController();
	if (!PC) return;

	// If the login camera exists, use it as the blend target but move it to podium position
	if (GameInstanceRef->LoginLevelCamera)
	{
		GameInstanceRef->LoginLevelCamera->SetActorLocationAndRotation(
			GameInstanceRef->PodiumCameraLocation,
			GameInstanceRef->PodiumCameraRotation);

		PC->SetViewTargetWithBlend(GameInstanceRef->LoginLevelCamera, BlendTime, EViewTargetBlendFunction::VTBlend_EaseInOut);
	}
}

void UCharacterPreviewManager::BlendToCreateCamera(float BlendTime)
{
	if (!GameInstanceRef || !GameInstanceRef->GetWorld()) return;

	APlayerController* PC = GameInstanceRef->GetWorld()->GetFirstPlayerController();
	if (!PC) return;

	if (GameInstanceRef->LoginLevelCamera)
	{
		GameInstanceRef->LoginLevelCamera->SetActorLocationAndRotation(
			GameInstanceRef->CreatePreviewCameraLocation,
			GameInstanceRef->CreatePreviewCameraRotation);

		PC->SetViewTargetWithBlend(GameInstanceRef->LoginLevelCamera, BlendTime, EViewTargetBlendFunction::VTBlend_EaseInOut);
	}
}

void UCharacterPreviewManager::BlendToLoginCamera(float BlendTime)
{
	if (!GameInstanceRef || !GameInstanceRef->GetWorld()) return;

	APlayerController* PC = GameInstanceRef->GetWorld()->GetFirstPlayerController();
	if (!PC) return;

	if (GameInstanceRef->LoginLevelCamera)
	{
		GameInstanceRef->LoginLevelCamera->SetActorLocationAndRotation(
			GameInstanceRef->LoginLevelCameraLocation,
			GameInstanceRef->LoginLevelCameraRotation);

		PC->SetViewTargetWithBlend(GameInstanceRef->LoginLevelCamera, BlendTime, EViewTargetBlendFunction::VTBlend_EaseInOut);
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Preview rotation (mouse drag)
// ─────────────────────────────────────────────────────────────────────────────

void UCharacterPreviewManager::TickPreviewRotation(float DeltaTime)
{
	if (!bRotationEnabled || !IsValid(CreatePreviewActor)) return;

	APlayerController* PC = GameInstanceRef ? GameInstanceRef->GetWorld()->GetFirstPlayerController() : nullptr;
	if (!PC) return;

	float MouseX = 0.f, MouseY = 0.f;
	PC->GetInputMouseDelta(MouseX, MouseY);

	if (FMath::Abs(MouseX) > KINDA_SMALL_NUMBER && PC->IsInputKeyDown(EKeys::RightMouseButton))
	{
		PreviewYawAccum += MouseX * 0.5f;
		CreatePreviewActor->SetActorRotation(
			GameInstanceRef->CreatePreviewRotation + FRotator(0.0f, PreviewYawAccum, 0.0f));
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Internal helpers
// ─────────────────────────────────────────────────────────────────────────────

