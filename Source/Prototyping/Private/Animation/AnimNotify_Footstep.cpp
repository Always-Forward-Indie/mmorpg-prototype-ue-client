#include "Animation/AnimNotify_Footstep.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/AudioComponent.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Engine/DataTable.h"
#include "Data/EntityAudioData.h"
#include "MyGameInstance.h"
#include "Gameplay/Players/BasicPlayer.h"
#include "Gameplay/Mobs/BasicMOB.h"
#include "Gameplay/NPCs/BasicNPC.h"
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

	FVector TraceStart = Owner->GetActorLocation();
	if (MeshComp->DoesSocketExist(FootSocketName))
	{
		TraceStart = MeshComp->GetSocketLocation(FootSocketName);
	}

	FVector TraceEnd = TraceStart - FVector(0, 0, 50.0f);

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

	UDataTable* FootstepTable = nullptr;
	if (UMyGameInstance* GI = Cast<UMyGameInstance>(Owner->GetGameInstance()))
	{
		FootstepTable = GI->GetFootstepSoundsTable();
	}

	USoundBase* SoundToPlay = nullptr;
	UNiagaraSystem* FootVFX = nullptr;
	float FinalVolume = VolumeMultiplier;
	USoundAttenuation* Attenuation = nullptr;

	FName Footwear = NAME_None;
	if (ABasicPlayer* Player = Cast<ABasicPlayer>(Owner))
	{
		Footwear = Player->GetFootwearType();
	}
	else if (ABasicMOB* Mob = Cast<ABasicMOB>(Owner))
	{
		Footwear = Mob->FootwearType;
	}
	else if (ABasicNPC* NPC = Cast<ABasicNPC>(Owner))
	{
		Footwear = NPC->GetFootwearType();
	}

	if (FootstepTable && PhysMatName != NAME_None)
	{
		FFootstepSoundData* Row = nullptr;

		// Priority 1: surface + footwear composite key
		if (!Footwear.IsNone())
		{
			FName CompositeKey = FName(*(PhysMatName.ToString() + TEXT("_") + Footwear.ToString()));
			Row = FootstepTable->FindRow<FFootstepSoundData>(CompositeKey, TEXT("Footstep"));
		}

		// Priority 2: surface only (generic fallback)
		if (!Row)
		{
			Row = FootstepTable->FindRow<FFootstepSoundData>(PhysMatName, TEXT("Footstep"));
		}

		if (Row)
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
			if (!Row->DefaultAttenuation.IsNull())
			{
				Attenuation = Row->DefaultAttenuation.LoadSynchronous();
			}
		}
	}

	if (!SoundToPlay && !DefaultFootstepSound.IsNull())
	{
		SoundToPlay = DefaultFootstepSound.LoadSynchronous();
	}

	if (!SoundToPlay)
	{
		if (ABasicPlayer* Player = Cast<ABasicPlayer>(Owner))
		{
			if (const FEntityAudioProfile* Profile = Player->GetAudioProfile())
			{
				if (Profile->Footsteps.Num() > 0)
				{
					SoundToPlay = Profile->Footsteps[FMath::RandRange(0, Profile->Footsteps.Num() - 1)].LoadSynchronous();
				}
			}
		}
		else if (ABasicMOB* Mob = Cast<ABasicMOB>(Owner))
		{
			if (Mob->FootstepSounds.Num() > 0)
			{
				SoundToPlay = Mob->FootstepSounds[FMath::RandRange(0, Mob->FootstepSounds.Num() - 1)];
			}
		}
		else if (ABasicNPC* NPC = Cast<ABasicNPC>(Owner))
		{
			if (NPC->GetFootstepSounds().Num() > 0)
			{
				SoundToPlay = NPC->GetFootstepSounds()[FMath::RandRange(0, NPC->GetFootstepSounds().Num() - 1)];
			}
		}
	}

	if (SoundToPlay)
	{
		USoundClass* SFXClass = nullptr;
		if (UMyGameInstance* GI = Cast<UMyGameInstance>(Owner->GetGameInstance()))
		{
			if (GI->AudioManager) { SFXClass = GI->AudioManager->SFXClass; }
		}

		if (!Attenuation)
		{
			if (ABasicPlayer* Player = Cast<ABasicPlayer>(Owner))
			{
				if (const FEntityAudioProfile* Profile = Player->GetAudioProfile())
				{
					if (!Profile->DefaultAttenuation.IsNull())
					{
						Attenuation = Profile->DefaultAttenuation.LoadSynchronous();
					}
				}
			}
			else if (ABasicMOB* Mob = Cast<ABasicMOB>(Owner))
			{
				Attenuation = Mob->DefaultAttenuation;
			}
			else if (ABasicNPC* NPC = Cast<ABasicNPC>(Owner))
			{
				Attenuation = NPC->DefaultAttenuation;
			}
		}

		if (SFXClass)
		{
			UAudioComponent* AC = UGameplayStatics::SpawnSoundAttached(
				SoundToPlay, Owner->GetRootComponent(), NAME_None,
				TraceStart, FRotator::ZeroRotator, EAttachLocation::KeepWorldPosition,
				true, FinalVolume, 1.0f, 0.0f, Attenuation, nullptr, false);
			if (AC)
			{
				AC->SoundClassOverride = SFXClass;
				AC->bAutoDestroy = true;
				AC->Play();
			}
		}
		else
		{
			UGameplayStatics::SpawnSoundAttached(
				SoundToPlay, Owner->GetRootComponent(), NAME_None,
				TraceStart, FRotator::ZeroRotator, EAttachLocation::KeepWorldPosition,
				true, FinalVolume, 1.0f, 0.0f, Attenuation, nullptr, true);
		}
	}

	if (FootVFX)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			World, FootVFX, Hit.ImpactPoint, FRotator::ZeroRotator);
	}
}
