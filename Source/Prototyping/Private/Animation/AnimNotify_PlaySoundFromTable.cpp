#include "Animation/AnimNotify_PlaySoundFromTable.h"
#include "Gameplay/Mobs/BasicMOB.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
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
	FString SlotStr = SoundSlotName.ToString();

	if (SlotStr == TEXT("Idle"))
	{
		if (Mob->IdleSounds.Num() > 0)
		{
			int32 Idx = FMath::RandRange(0, Mob->IdleSounds.Num() - 1);
			SoundToPlay = Mob->IdleSounds[Idx];
		}
	}
	else if (SlotStr == TEXT("Walk"))
	{
		if (Mob->WalkSounds.Num() > 0)
		{
			int32 Idx = FMath::RandRange(0, Mob->WalkSounds.Num() - 1);
			SoundToPlay = Mob->WalkSounds[Idx];
		}
	}
	else if (SlotStr == TEXT("Run"))
	{
		if (Mob->RunSounds.Num() > 0)
		{
			int32 Idx = FMath::RandRange(0, Mob->RunSounds.Num() - 1);
			SoundToPlay = Mob->RunSounds[Idx];
		}
	}
	else
	{
		// Named sounds from SoundMap: "Attack", "Aggro", "Hit", "Death"
		if (USoundBase** Found = Mob->SoundMap.Find(SoundSlotName))
		{
			SoundToPlay = *Found;
		}
	}

	if (SoundToPlay)
	{
		UAudioComponent* AC = UGameplayStatics::SpawnSoundAtLocation(Mob, SoundToPlay, Mob->GetActorLocation(),
			FRotator::ZeroRotator, VolumeMultiplier, 1.0f);
		if (AC)
		{
			if (UMyGameInstance* GI = Cast<UMyGameInstance>(Mob->GetGameInstance()))
			{
				if (GI->AudioManager && GI->AudioManager->SFXClass)
				{
					AC->SoundClassOverride = GI->AudioManager->SFXClass;
				}
			}
		}
	}
}
