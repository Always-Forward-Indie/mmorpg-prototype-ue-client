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

void ABasicNPC::PlaySoundByName(FName SoundName)
{
	if (USoundBase** Sound = SoundMap.Find(SoundName))
	{
		if (*Sound && AudioComponentMain)
		{
			AudioComponentMain->AttenuationSettings = DefaultAttenuation;
			AudioComponentMain->SetSound(*Sound);
			AudioComponentMain->Play();
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("ABasicNPC::PlaySoundByName - Sound '%s' not found"), *SoundName.ToString());
	}
}


void ABasicNPC::PlayRandomIdleSound()
{
	if (IdleSounds.Num() > 0 && AudioComponentMain)
	{
		const int32 Idx = FMath::RandRange(0, IdleSounds.Num() - 1);
		AudioComponentMain->AttenuationSettings = DefaultAttenuation;
		AudioComponentMain->SetSound(IdleSounds[Idx]);
		AudioComponentMain->Play();
	}
	ScheduleNextIdleSound();
}

void ABasicNPC::ScheduleNextIdleSound()
{
	if (!GetWorld() || IdleSounds.Num() == 0) return;
	const float Delay = FMath::FRandRange(10.f, 30.f);
	GetWorld()->GetTimerManager().SetTimer(IdleSoundTimerHandle, this, &ABasicNPC::PlayRandomIdleSound, Delay, false);
}

void ABasicNPC::PlayGreetingSound()
{
	PlaySoundByName("Greeting");

	// Start smoothly rotating toward the player
	if (!bIsFacingPlayer)
	{
		OriginalRotation = GetActorRotation();
		bIsFacingPlayer = true;
		bIsRestoringRotation = false;
	}

	// bIsTalking: set on AnimInstance if the ABP uses UNPCAnimInstance
	if (UNPCAnimInstance* Anim = GetNPCAnimInstance())
		Anim->SetTalking(true);

	if (GreetMontageAsset)
	{
		// Interrupt idle only when we actually have something to replace it with
		if (bIdleAnimPlaying && ActiveIdleMontage)
			StopAnimMontage(ActiveIdleMontage);

		const float Duration = PlayAnimMontage(GreetMontageAsset);
		if (Duration > 0.f)
		{
			// After the greet montage ends, restart the idle cycle
			if (UAnimInstance* AnimInst = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr)
			{
				ActionEndedDelegate.BindUObject(this, &ABasicNPC::OnActionMontageEnded);
				AnimInst->Montage_SetEndDelegate(ActionEndedDelegate, GreetMontageAsset);
			}
		}
		UE_LOG(LogTemp, Log, TEXT("[NPCAnim] '%s' playing greet montage '%s'"),
			*GetNPCName(), *GreetMontageAsset->GetName());
	}
	// No GreetMontage assigned — idle keeps playing undisturbed
}

void ABasicNPC::PlayFarewellSound()
{
	PlaySoundByName("Farewell");

	// Stop facing the player and smoothly return to original rotation
	bIsFacingPlayer = false;
	bIsRestoringRotation = true;

	if (FarewellMontageAsset)
	{
		// Interrupt idle only when we actually have something to replace it with
		if (bIdleAnimPlaying && ActiveIdleMontage)
			StopAnimMontage(ActiveIdleMontage);

		const float Duration = PlayAnimMontage(FarewellMontageAsset);
		if (Duration > 0.f)
		{
			// After the farewell montage ends, restart the idle cycle
			if (UAnimInstance* AnimInst = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr)
			{
				ActionEndedDelegate.BindUObject(this, &ABasicNPC::OnActionMontageEnded);
				AnimInst->Montage_SetEndDelegate(ActionEndedDelegate, FarewellMontageAsset);
			}
		}
		UE_LOG(LogTemp, Log, TEXT("[NPCAnim] '%s' playing farewell montage '%s'"),
			*GetNPCName(), *FarewellMontageAsset->GetName());
	}
	// No FarewellMontage assigned — idle keeps playing undisturbed

	// Clear talking state
	if (UNPCAnimInstance* Anim = GetNPCAnimInstance())
		Anim->SetTalking(false);
}

UNPCAnimInstance* ABasicNPC::GetNPCAnimInstance() const
{
	if (USkeletalMeshComponent* NPCMesh = GetMesh())
	{
		return Cast<UNPCAnimInstance>(NPCMesh->GetAnimInstance());
	}
	return nullptr;
}

// ─── IWorldInteractable interface ────────────────────────────────────────────
