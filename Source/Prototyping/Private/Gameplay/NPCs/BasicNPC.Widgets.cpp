#include "Gameplay/NPCs/BasicNPC.h"
#include "Prototyping.h"
#include "Gameplay/NPCs/BasicNPC.h"
#include "Gameplay/Interaction/TargetDecalComponent.h"
#include "Gameplay/NPCs/NPCAnimInstance.h"
#include "Gameplay/NPCs/NPCAmbientSpeechComponent.h"
#include "Gameplay/NPCs/AmbientSpeechManager.h"
#include "Animation/AnimMontage.h"
#include "Components/CapsuleComponent.h"
#include "Components/AudioComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "CrashDiagnostics.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Gameplay/UI/NPCNameplateComponent.h"
#include "MyGameInstance.h"
#include "Audio/AudioManager.h"
#include "Data/EntityAudioRepository.h"
#include "GameFramework/Pawn.h"

void ABasicNPC::UpdateWidgetScale(float DeltaTime)
{
	// Placeholder for widget scaling - will be implemented when UI components are added
	/*
	if (!NPCHeadInfo || !HeadWidget)
		return;

	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC || !PC->PlayerCameraManager)
		return;

	const FVector CameraLoc = PC->PlayerCameraManager->GetCameraLocation();
	const float Distance = FVector::Dist(CameraLoc, GetActorLocation());

	float Clamped = FMath::Clamp(Distance, MinDistance, MaxDistance);
	float Factor = 1.0f - ((Clamped - MinDistance) / (MaxDistance - MinDistance));
	float TargetScale = FMath::Clamp(Factor, 0.1f, 1.0f) * widgetScaleFactor;

	CurrentWidgetScale = FMath::FInterpConstantTo(CurrentWidgetScale, TargetScale, DeltaTime, 1.f);
	// HeadWidget->SetWidgetScale(TargetScale);
	*/
}

void ABasicNPC::UpdateWidgetPosition()
{
	// Placeholder for widget positioning - will be implemented when UI components are added
	/*
	if (!NPCHeadInfo)
		return;

	UCapsuleComponent* Capsule = GetCapsuleComponent();
	if (!Capsule)
		return;

	// Get capsule height
	const float CapsuleHalfHeight = Capsule->GetScaledCapsuleHalfHeight();

	// Base offset above head
	const float BaseOffset = 40.f;

	// Additional offset based on distance
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	float DistanceOffset = 0.f;

	if (PC && PC->PlayerCameraManager)
	{
		const float Distance = FVector::Dist(PC->PlayerCameraManager->GetCameraLocation(), GetActorLocation());
		DistanceOffset = FMath::Clamp((Distance - 500.f) * 0.05f, 0.f, 40.f);
	}

	// Position above capsule
	const float FinalZ = CapsuleHalfHeight + BaseOffset + DistanceOffset;
	NPCHeadInfo->SetRelativeLocation(FVector(0.f, 0.f, FinalZ));
	*/
}

void ABasicNPC::ForceUpdateUI()
{
	// Placeholder for UI updates - will be implemented when UI components are added
	/*
	if (!NPCHeadInfo)
		return;

	// Update UI with current NPC data
	UE_LOG(LogGameplay, Verbose, TEXT("ForceUpdateUI for NPC %s (ID:%d): HP=%d/%d, MP=%d/%d"), 
		*NPCData.name, NPCData.id, 
		NPCData.stats.health.current, NPCData.stats.health.max,
		NPCData.stats.mana.current, NPCData.stats.mana.max);

	// Placeholder for actual UI update
	// NPCHeadInfo->UpdateInfo(
	//     NPCData.stats.health.current,
	//     NPCData.stats.health.max,
	//     NPCData.stats.mana.current,
	//     NPCData.stats.mana.max,
	//     NPCData.name,
	//     NPCData.level,
	//     NPCData.isInteractable
	// );

	LastHealth = NPCData.stats.health.current;
	LastMana = NPCData.stats.mana.current;
	bUIInitialized = true;

	if (NPCHeadInfo)
	{
		NPCHeadInfo->SetVisibility(true);
	}
	*/
}

