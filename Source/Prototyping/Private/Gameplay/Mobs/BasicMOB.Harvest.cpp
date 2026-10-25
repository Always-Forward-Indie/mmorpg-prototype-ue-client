// Fill out your copyright notice in the Description page of Project Settings.


#include "Gameplay/Mobs/BasicMOB.h"
#include "Prototyping.h"
#include "Gameplay/Interaction/TargetDecalComponent.h"
#include "Gameplay/Players/BasicPlayer.h"
#include "Gameplay/Mobs/MOBAnimInstance.h"
#include "Gameplay/Combat/CombatSystemManager.h"
#include "Gameplay/Items/ItemManager.h"
#include "Gameplay/Items/HarvestManager.h"
#include "MyGameInstance.h"
#include "Services/LocalizationSubsystem.h"
#include "Gameplay/UI/FloatingCombatTextManager.h"
#include "Gameplay/UI/DamageTextWidget.h"
#include "UI/UIManager.h"
#include "UObject/ConstructorHelpers.h"
#include "CrashDiagnostics.h"
#include "Engine/Engine.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Gameplay/Skills/SkillDefinitionRepository.h"
#include "Data/EntityAudioRepository.h"
#include "Particles/ParticleSystem.h"
#include "NiagaraSystem.h"
#include "NiagaraFunctionLibrary.h"
#include "Data/EffectDefinitionTable.h"
#include "Utils/AudioSpawnHelpers.h"

// Check if MOB can be harvested
bool ABasicMOB::CanBeHarvested() const
{
	// MOB must be dead and not yet harvested
	return MOBData.bIsDead && !MOBData.bHasBeenHarvested;
}

// Check if MOB has been harvested
bool ABasicMOB::HasBeenHarvested() const
{
	return MOBData.bHasBeenHarvested;
}

// Set MOB as harvested
void ABasicMOB::SetHarvested(bool bHarvested)
{
	MOBData.bHasBeenHarvested = bHarvested;
	RefreshHarvestableVFX();

	if (bHarvested)
	{
		UE_LOG(LogGameplay, Warning, TEXT("MOB %s (ID:%d) has been harvested"), *MOBData.mobName, MOBData.mobID);
	}
}

void ABasicMOB::SetCurrentHarvesterId(int32 HarvesterId)
{
	MOBData.currentHarvesterId = HarvesterId;
	UE_LOG(LogGameplay, Warning, TEXT("MOB %s (ID:%d) currentHarvesterId=%d"), *MOBData.mobName, MOBData.mobID, HarvesterId);
}

void ABasicMOB::RefreshHarvestableVFX()
{
	const bool bShouldShow = MOBData.bIsDead && !MOBData.bHasBeenHarvested && !HarvestableVFX.IsNull();

	if (bShouldShow)
	{
		// Spawn the Niagara component on first use; reuse it afterwards.
		if (!HarvestableVFXComponent)
		{
			UNiagaraSystem* System = HarvestableVFX.LoadSynchronous();
			if (!System) return;
			HarvestableVFXComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(
				System,
				GetRootComponent(),
				NAME_None,
				FVector::ZeroVector,
				FRotator::ZeroRotator,
				EAttachLocation::KeepRelativeOffset,
				/*bAutoDestroy=*/ false);
		}
		else
		{
			HarvestableVFXComponent->Activate(/*bReset=*/ false);
		}
	}
	else
	{
		if (HarvestableVFXComponent && HarvestableVFXComponent->IsActive())
		{
			HarvestableVFXComponent->Deactivate();
		}
	}
}

