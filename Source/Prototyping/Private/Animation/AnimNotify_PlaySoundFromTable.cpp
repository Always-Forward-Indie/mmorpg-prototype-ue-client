#include "Animation/AnimNotify_PlaySoundFromTable.h"
#include "Animation/AnimNotify_Footstep.h"
#include "Gameplay/Mobs/BasicMOB.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "MyGameInstance.h"

UAnimNotify_PlaySoundFromTable::UAnimNotify_PlaySoundFromTable()
{
}

void UAnimNotify_PlaySoundFromTable::Notify(USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp) return;

	ABasicMOB* Mob = Cast<ABasicMOB>(MeshComp->GetOwner());
	if (!Mob) return;

	// Resolve sound from the mob's pre-loaded audio arrays / map
	USoundBase* SoundToPlay = nullptr;
	UNiagaraSystem* FootVFX = nullptr;
	float FinalVolume = VolumeMultiplier;
	FString SlotStr = SoundSlotName.ToString();

	if (SlotStr == TEXT("Idle"))
	{
		if (Mob->IdleSounds.Num() > 0)
		{
			int32 Idx = FMath::RandRange(0, Mob->IdleSounds.Num() - 1);
			SoundToPlay = Mob->IdleSounds[Idx];
		}
	}
	else if (SlotStr == TEXT("Voice"))
	{
		if (Mob->AttackVoiceSounds.Num() > 0)
		{
			int32 Idx = FMath::RandRange(0, Mob->AttackVoiceSounds.Num() - 1);
			SoundToPlay = Mob->AttackVoiceSounds[Idx];
		}
	}
	else if (SlotStr == TEXT("CastVoice"))
	{
		// Mob cast-start voice pool (entity-level fallback; skill's castStartVoice field has higher priority
		// but that's resolved in code, not here — this notify covers the entity-pool path)
		if (Mob->CastVoiceSounds.Num() > 0)
		{
			int32 Idx = FMath::RandRange(0, Mob->CastVoiceSounds.Num() - 1);
			SoundToPlay = Mob->CastVoiceSounds[Idx];
		}
	}
	else if (SlotStr == TEXT("ReleaseVoice"))
	{
		// Mob cast-release voice pool
		if (Mob->ReleaseVoiceSounds.Num() > 0)
		{
			int32 Idx = FMath::RandRange(0, Mob->ReleaseVoiceSounds.Num() - 1);
			SoundToPlay = Mob->ReleaseVoiceSounds[Idx];
		}
	}
	else
	{
		// Named sounds from SoundMap: "Attack", "Aggro", "Hit", "Death", "Swing"
		if (USoundBase** Found = Mob->SoundMap.Find(SoundSlotName))
		{
			SoundToPlay = *Found;
		}
	}

	if (SoundToPlay)
	{
		USoundClass* SFXClass = nullptr;
		if (UMyGameInstance* GI = Cast<UMyGameInstance>(Mob->GetGameInstance()))
		{
			if (GI->AudioManager) { SFXClass = GI->AudioManager->SFXClass; }
		}

		if (SFXClass)
		{
			UAudioComponent* AC = UGameplayStatics::SpawnSoundAttached(
				SoundToPlay,
				Mob->GetRootComponent(),
				NAME_None,
				Mob->GetActorLocation(),
				FRotator::ZeroRotator,
				EAttachLocation::KeepWorldPosition,
				/*bStopWhenAttachedToDestroyed=*/true,
				FinalVolume,
				/*PitchMultiplier=*/1.0f,
				/*StartTime=*/0.0f,
				/*AttenuationSettings=*/Mob->DefaultAttenuation,
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
			UGameplayStatics::SpawnSoundAttached(SoundToPlay, Mob->GetRootComponent(), NAME_None,
				Mob->GetActorLocation(), FRotator::ZeroRotator, EAttachLocation::KeepWorldPosition,
				true, FinalVolume, 1.0f, 0.0f, Mob->DefaultAttenuation, nullptr, true);
		}
	}

	if (FootVFX)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			Mob, FootVFX, Mob->GetActorLocation(), FRotator::ZeroRotator);
	}
}
