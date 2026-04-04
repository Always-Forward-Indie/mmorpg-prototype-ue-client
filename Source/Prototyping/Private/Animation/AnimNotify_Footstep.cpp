#include "Animation/AnimNotify_Footstep.h"
#include "Animation/AnimNotify_Footstep.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/AudioComponent.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Engine/DataTable.h"
#include "Data/DataStructs.h"
#include "MyGameInstance.h"
#include "CollisionQueryParams.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

UAnimNotify_Footstep::UAnimNotify_Footstep()
{
}

void UAnimNotify_Footstep::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp) return;
	AActor* Owner = MeshComp->GetOwner();
	if (!Owner) return;

	UWorld* World = Owner->GetWorld();
	if (!World) return;

	// Determine trace start from the foot socket
	FVector TraceStart = Owner->GetActorLocation();
	if (MeshComp->DoesSocketExist(FootSocketName))
	{
		TraceStart = MeshComp->GetSocketLocation(FootSocketName);
	}

	FVector TraceEnd = TraceStart - FVector(0, 0, 50.0f);

	// Line trace to find the physical material
	FHitResult Hit;
	FCollisionQueryParams Params;
	Params.bReturnPhysicalMaterial = true;
	Params.AddIgnoredActor(Owner);

	FName PhysMatName = NAME_None;
	if (World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, Params))
	{
		if (Hit.PhysMaterial.IsValid())
		{
			PhysMatName = Hit.PhysMaterial->GetFName();
		}
	}

	// Try to look up the footstep DataTable from GameInstance
	UDataTable* FootstepTable = nullptr;
	if (UMyGameInstance* GI = Cast<UMyGameInstance>(Owner->GetGameInstance()))
	{
		FootstepTable = GI->GetFootstepSoundsTable();
	}

	USoundBase* SoundToPlay = nullptr;
	UNiagaraSystem* FootVFX = nullptr;
	float FinalVolume = VolumeMultiplier;

	if (FootstepTable && PhysMatName != NAME_None)
	{
		if (FFootstepSoundData* Row = FootstepTable->FindRow<FFootstepSoundData>(PhysMatName, TEXT("Footstep")))
		{
			if (Row->FootstepSounds.Num() > 0)
			{
				int32 Index = FMath::RandRange(0, Row->FootstepSounds.Num() - 1);
				SoundToPlay = Row->FootstepSounds[Index].LoadSynchronous();
			}
			FinalVolume *= Row->VolumeMultiplier;

			if (!Row->FootstepVFX.IsNull())
			{
				FootVFX = Row->FootstepVFX.LoadSynchronous();
			}
		}
	}

	// Fallback to default footstep sound
	if (!SoundToPlay && !DefaultFootstepSound.IsNull())
	{
		SoundToPlay = DefaultFootstepSound.LoadSynchronous();
	}

	if (SoundToPlay)
	{
		// Resolve the SFX SoundClass before the component starts playing.
		// SpawnSoundAtLocation calls Play() internally, so any SoundClassOverride
		// set on the returned component arrives too late and is ignored by the
		// audio engine for the current playback.  We must use SpawnSoundAttached
		// (bAutoActivate = false path) or create the component manually.
		USoundClass* SFXClass = nullptr;
		if (UMyGameInstance* GI = Cast<UMyGameInstance>(Owner->GetGameInstance()))
		{
			if (GI->AudioManager) { SFXClass = GI->AudioManager->SFXClass; }
		}

		if (SFXClass)
		{
			// Create the component with bAutoDestroy so we don't leak,
			// set the override BEFORE Play() so the audio engine sees it.
			UAudioComponent* AC = UGameplayStatics::SpawnSoundAttached(
				SoundToPlay,
				Owner->GetRootComponent(),
				NAME_None,
				TraceStart,
				FRotator::ZeroRotator,
				EAttachLocation::KeepWorldPosition,
				/*bStopWhenAttachedToDestroyed=*/true,
				FinalVolume,
				/*PitchMultiplier=*/1.0f,
				/*StartTime=*/0.0f,
				/*AttenuationSettings=*/nullptr,
				/*ConcurrencySettings=*/nullptr,
				/*bAutoActivate=*/false);
			if (AC)
			{
				AC->SoundClassOverride = SFXClass;
				AC->bAutoDestroy = true;
				AC->Play();
			}
		}
		else
		{
			// No AudioManager yet (e.g. during editor preview) — plain spawn.
			UGameplayStatics::SpawnSoundAtLocation(Owner, SoundToPlay, TraceStart,
				FRotator::ZeroRotator, FinalVolume, 1.0f);
		}
	}

	// Spawn footstep VFX (dust puff, splash, etc.)
	if (FootVFX)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			World, FootVFX, Hit.ImpactPoint, FRotator::ZeroRotator);
	}
}
