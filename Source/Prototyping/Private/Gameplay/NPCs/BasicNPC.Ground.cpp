#include "Gameplay/NPCs/BasicNPC.h"
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

EInteractableType ABasicNPC::GetInteractableType() const
{
    return EInteractableType::NPC;
}

FText ABasicNPC::GetInteractableDisplayName() const
{
    const FString Label = NPCData.level > 0
        ? FString::Printf(TEXT("%s  [Lv.%d]"), *NPCData.name, NPCData.level)
        : NPCData.name;
    return FText::FromString(Label);
}

bool ABasicNPC::CanInteract() const
{
    return NPCData.isInteractable;
}

void ABasicNPC::SnapToGround()
{
	UWorld* World = GetWorld();
	if (!World) return;

	const FVector CurrentLoc = GetActorLocation();
	const FVector Start(CurrentLoc.X, CurrentLoc.Y,  50000.0f);
	const FVector End  (CurrentLoc.X, CurrentLoc.Y, -10000.0f);

	FCollisionQueryParams Params;
	Params.bTraceComplex = true;
	Params.AddIgnoredActor(this);

	for (TActorIterator<APawn> It(World); It; ++It)
	{
		Params.AddIgnoredActor(*It);
	}

	FHitResult Hit;
	if (World->LineTraceSingleByChannel(Hit, Start, End, ECC_WorldStatic, Params))
	{
		const float HalfHeight = GetCapsuleComponent()
			? GetCapsuleComponent()->GetScaledCapsuleHalfHeight()
			: 90.0f;
		const float CorrectZ = Hit.ImpactPoint.Z + HalfHeight;
		SetActorLocation(FVector(CurrentLoc.X, CurrentLoc.Y, CorrectZ));

		// Remember this Z so the periodic corrector can restore it if something pushes the NPC.
		SnappedZ = CorrectZ;
		bSnappedZValid = true;

		// Start periodic Z-correction timer (or restart it if already running).
		if (World)
		{
			World->GetTimerManager().SetTimer(
				ZCorrectionTimerHandle,
				this, &ABasicNPC::CorrectZ,
				0.5f, /*bLoop=*/true);
		}
	}
}

void ABasicNPC::CorrectZ()
{
	if (!bSnappedZValid) return;

	const FVector Loc = GetActorLocation();
	if (!FMath::IsNearlyEqual(Loc.Z, SnappedZ, 1.0f))
	{
		SetActorLocation(FVector(Loc.X, Loc.Y, SnappedZ), /*bSweep=*/false, nullptr, ETeleportType::TeleportPhysics);
	}
}

