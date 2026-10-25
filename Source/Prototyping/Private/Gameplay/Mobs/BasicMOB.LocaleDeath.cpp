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

void ABasicMOB::HandleLocaleChanged(const FString& NewLocale)
{
	ForceUpdateUI();
}


void ABasicMOB::Die()
{
	if (bHasDied) return;
	bHasDied = true;
	// Canonical dead flag (drives GetMOBIsDead/CanBeHarvested/corpse flows).
	// Must be set here, not only from server packets — offline kills (and any
	// path that calls Die() directly) would otherwise leave a "living" corpse.
	MOBData.bIsDead = true;
	PlaySoundByName("Death");

	// Spawn death VFX if defined in MobDefinitionTable
	if (MobDefinitionTable)
	{
		FName SlugKey = FName(*MOBData.mobSlug);
		if (const FMobDefinition* Def = MobDefinitionTable->FindRow<FMobDefinition>(SlugKey, TEXT("")))
		{
			if (!Def->Visual.DeathVFX.IsNull())
			{
				if (UNiagaraSystem* VFX = Def->Visual.DeathVFX.LoadSynchronous())
				{
					UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), VFX, GetActorLocation());
				}
			}
		}
	}

	// set target id to 0
	SetMobTargetId(0);

	// set aggressive to false
	SetMOBIsAggressive(false);

	if (MobHeadInfo)
	{
		MobHeadInfo->UpdateMobAggressive(false);
	}

	// Notify AnimInstance � triggers death animation, clears all combat states
	if (UMOBAnimInstance* AnimInst = Cast<UMOBAnimInstance>(GetMesh()->GetAnimInstance()))
	{
		AnimInst->NotifyDeath();
	}

	UE_LOG(LogGameplay, Warning, TEXT("MOB %s (ID:%d) has died."), *MOBData.mobName, MOBData.mobID);

	// Disable mesh physics so the corpse does not block movement or camera.
	// IMPORTANT: Do NOT call SetActorEnableCollision(false) here � that actor-level flag
	// overrides UPrimitiveComponent::GetCollisionEnabled() and makes every query on this
	// actor return NoCollision, which breaks the ECC_Visibility hover trace on the corpse.
	if (USkeletalMeshComponent* MobMesh = GetMesh())
	{
		MobMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	// Keep the capsule alive for queries only: block ECC_Visibility so the mouse-hover
	// trace still hits the corpse, and overlap WorldDynamic so in-flight projectiles
	// can still register their impact event.
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Capsule->SetCollisionResponseToAllChannels(ECR_Ignore);
		Capsule->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
		Capsule->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	}

	// Freeze movement component so the corpse does not slide in the last
	// dead-reckoned direction.
	if (MOBMovementComponent)
	{
		MOBMovementComponent->FreezeMob();
	}

	// Flush any item drops that were deferred because the mob was still alive
	// when the itemDrop packet arrived from the server.
	if (UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance()))
	{
		if (UItemManager* ItemMgr = GI->GetItemManager())
		{
			ItemMgr->FlushDropsForMob(FCString::Atoi(*MOBData.mobUniqueID));
		}
	}
}
