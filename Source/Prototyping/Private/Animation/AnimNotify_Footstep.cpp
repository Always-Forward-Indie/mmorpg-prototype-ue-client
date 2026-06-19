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

static int32 GFootstepDebug = 0;
static FAutoConsoleVariableRef CVarFootstepDebug(
	TEXT("footstep.Debug"),
	GFootstepDebug,
	TEXT("0=Off, 1=Log trace/physmat, 2=Log all lookup attempts"),
	ECVF_Default);

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

	FVector TraceEnd = TraceStart - FVector(0, 0, 200.0f);

	FHitResult Hit;
	FCollisionQueryParams Params;
	Params.bReturnPhysicalMaterial = true;
	Params.AddIgnoredActor(Owner);

	FName PhysMatName = NAME_None;
	const bool bHit = World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, Params);
	if (bHit)
	{
		if (Hit.PhysMaterial.IsValid())
		{
			PhysMatName = Hit.PhysMaterial->GetFName();
		}
	}

	if (GFootstepDebug >= 1)
	{
		const FString OwnerName = GetNameSafe(Owner);
		if (bHit)
		{
			UE_LOG(LogTemp, Log, TEXT("[Footstep] %s | %s | HIT actor=%s comp=%s dist=%.1f | PhysMat=%s"),
				*OwnerName, *FootSocketName.ToString(),
				*GetNameSafe(Hit.GetActor()), *GetNameSafe(Hit.GetComponent()),
				Hit.Distance,
				PhysMatName != NAME_None ? *PhysMatName.ToString() : TEXT("NONE"));
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[Footstep] %s | %s | MISS | TraceStart=%s TraceEnd=%s"),
				*OwnerName, *FootSocketName.ToString(),
				*TraceStart.ToString(), *TraceEnd.ToString());
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

	int32 ResolvedPriority = 0;
	if (FootstepTable && PhysMatName != NAME_None)
	{
		FFootstepSoundData* Row = nullptr;

		// Priority 1: surface + footwear composite key
		if (!Footwear.IsNone())
		{
			FName CompositeKey = FName(*(PhysMatName.ToString() + TEXT("_") + Footwear.ToString()));
			Row = FootstepTable->FindRow<FFootstepSoundData>(CompositeKey, TEXT("Footstep"));
			if (GFootstepDebug >= 2 && Row)
			{
				UE_LOG(LogTemp, Log, TEXT("[Footstep]   Priority 1 HIT: key=%s, sounds=%d"),
					*CompositeKey.ToString(), Row->FootstepSounds.Num());
			}
		}

		// Priority 2: surface only (generic fallback)
		if (!Row)
		{
			Row = FootstepTable->FindRow<FFootstepSoundData>(PhysMatName, TEXT("Footstep"));
			if (GFootstepDebug >= 2 && Row)
			{
				UE_LOG(LogTemp, Log, TEXT("[Footstep]   Priority 2 HIT: key=%s, sounds=%d"),
					*PhysMatName.ToString(), Row->FootstepSounds.Num());
			}
		}

		if (Row)
		{
			ResolvedPriority = Footwear.IsNone() ? 2 : 1;
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
		else if (GFootstepDebug >= 1)
		{
			UE_LOG(LogTemp, Warning, TEXT("[Footstep]   PhysMat=%s NOT found in DataTable (tried: %s%s)"),
				*PhysMatName.ToString(),
				!Footwear.IsNone() ? *(PhysMatName.ToString() + TEXT("_") + Footwear.ToString() + TEXT(", ")) : TEXT(""),
				*PhysMatName.ToString());
		}
	}

	if (!SoundToPlay && !DefaultFootstepSound.IsNull())
	{
		ResolvedPriority = 3;
		SoundToPlay = DefaultFootstepSound.LoadSynchronous();
		if (GFootstepDebug >= 1)
		{
			UE_LOG(LogTemp, Log, TEXT("[Footstep]   Priority 3: default notify sound"));
		}
	}

	if (!SoundToPlay)
	{
		if (ABasicPlayer* Player = Cast<ABasicPlayer>(Owner))
		{
			if (Player->GetFootstepSounds().Num() > 0)
			{
				ResolvedPriority = 4;
				SoundToPlay = Player->GetFootstepSounds()[FMath::RandRange(0, Player->GetFootstepSounds().Num() - 1)];
			}
		}
		else if (ABasicMOB* Mob = Cast<ABasicMOB>(Owner))
		{
			if (Mob->FootstepSounds.Num() > 0)
			{
				ResolvedPriority = 5;
				SoundToPlay = Mob->FootstepSounds[FMath::RandRange(0, Mob->FootstepSounds.Num() - 1)];
			}
		}
		else if (ABasicNPC* NPC = Cast<ABasicNPC>(Owner))
		{
			if (NPC->GetFootstepSounds().Num() > 0)
			{
				ResolvedPriority = 6;
				SoundToPlay = NPC->GetFootstepSounds()[FMath::RandRange(0, NPC->GetFootstepSounds().Num() - 1)];
			}
		}

		if (GFootstepDebug >= 1 && SoundToPlay)
		{
			UE_LOG(LogTemp, Log, TEXT("[Footstep]   Priority %d: entity fallback sounds"), ResolvedPriority);
		}
	}

	if (GFootstepDebug >= 1)
	{
		if (SoundToPlay)
		{
			UE_LOG(LogTemp, Log, TEXT("[Footstep]   RESULT: priority=%d sound=%s volume=%.2f"),
				ResolvedPriority, *GetNameSafe(SoundToPlay), FinalVolume);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[Footstep]   RESULT: NO SOUND - all priorities exhausted (PhysMat=%s, Table=%s, DefaultSound=%s, Footwear=%s)"),
				*PhysMatName.ToString(),
				FootstepTable ? TEXT("yes") : TEXT("no"),
				DefaultFootstepSound.IsNull() ? TEXT("none") : TEXT("set"),
				*Footwear.ToString());
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
