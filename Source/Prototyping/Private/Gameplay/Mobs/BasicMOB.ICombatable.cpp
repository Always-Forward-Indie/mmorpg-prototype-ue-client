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

// ICombatable interface implementations
int32 ABasicMOB::GetMaxHealth_Implementation() const
{
    if (const FAttributeDataStruct* HealthAttr = MOBData.mobAttributes.attributesData.Find(TEXT("max_health")))
    {
        return HealthAttr->attributeValue;
    }
    return 100; // Default value
}

int32 ABasicMOB::GetMaxMana_Implementation() const
{
    if (const FAttributeDataStruct* ManaAttr = MOBData.mobAttributes.attributesData.Find(TEXT("max_mana")))
    {
        return ManaAttr->attributeValue;
    }
    return 100; // Default value
}

void ABasicMOB::SetDead_Implementation(bool bNewDead)
{
    SetMOBIsDead(bNewDead);
    
    if (bNewDead)
    {
        UE_LOG(LogGameplay, Warning, TEXT("MOB %d has died"), GetActorId_Implementation());
        OnDeath_Implementation();
    }
}

void ABasicMOB::OnDeath_Implementation()
{
    UE_LOG(LogGameplay, Warning, TEXT("MOB %s died"), *MOBData.mobName);
    
    // Clear target when dying
    ClearTarget_Implementation();
    
    // Call the existing Die() method
    Die();
}

void ABasicMOB::SetTarget_Implementation(int32 TargetId, ECasterType TargetType)
{
    CurrentTargetId = TargetId;
    CurrentTargetType = TargetType;
    
    // Also update the MOB data for network sync
    SetMobTargetId(TargetId);
    SetMobTargetType(TargetType == ECasterType::Player ? TEXT("Player") : TEXT("Mob"));
    
    UE_LOG(LogGameplay, Verbose, TEXT("MOB %d set target: %d (%s)"), 
        GetActorId_Implementation(), TargetId, *UEnum::GetValueAsString(TargetType));
}

void ABasicMOB::ClearTarget_Implementation()
{
    CurrentTargetId = 0;
    CurrentTargetType = ECasterType::None;
    
    // Also clear MOB data
    SetMobTargetId(0);
    SetMobTargetType(TEXT(""));
    
    UE_LOG(LogGameplay, Verbose, TEXT("MOB %d cleared target"), GetActorId_Implementation());
}

void ABasicMOB::SetIsAggressiveState_Implementation(int32 TargetId, ECasterType TargetType, bool bIsAggressive)
{
	SetMOBIsAggressive(bIsAggressive);

	// Explicit aggro event from server � unlock movePacket aggro gate.
	if (bIsAggressive)
	{
		bAggroLockedOut = false;
		// Play aggro sound when the mob first becomes aggressive
		PlaySoundByName("Aggro");
	}

	if (MobHeadInfo)
	{
		MobHeadInfo->UpdateMobAggressive(bIsAggressive);
	}

	// Auto-lock: if mob just aggroed a player, set their target lock � but ONLY if
	// the mob is actually targeting this client's local player.
	// Bug 9 fix: previously GetFirstPlayerController() was used unconditionally, which
	// caused the local player on Client 2 to auto-lock a mob that was attacking Client 1.
	// This produced "target lock of player 1 also appears on player 2's client".
	// Also fixes Bug 7: ensures ClearLockedTarget() is not triggered on the wrong client.
	if (bIsAggressive && TargetType == ECasterType::Player)
	{
		if (UWorld* W = GetWorld())
		{
			APlayerController* PC = W->GetFirstPlayerController();
			if (PC)
			{
				if (ABasicPlayer* Player = Cast<ABasicPlayer>(PC->GetPawn()))
				{
					// Only auto-lock if this mob is targeting THIS specific local player.
					if (TargetId == Player->GetActorId_Implementation())
					{
						if (!Player->GetLockedTarget())
						{
							Player->SetLockedTarget(this);
						}
						else if (Player->GetLockedTarget() == this)
						{
							// This mob is already locked � refresh the target frame name color
							if (UUIManager* UIMgr = Player->GetUIManager())
							{
								const int32 MaxHP = MOBData.mobAttributes.attributesData.Contains(TEXT("max_health"))
									? MOBData.mobAttributes.attributesData[TEXT("max_health")].attributeValue
									: 100;
								UIMgr->ShowMobTargetFrame(
									MOBData.mobSlug,
									MOBData.mobName,
									MOBData.mobLevel,
									MOBData.mobCurrentHealth,
									MaxHP,
									bIsAggressive,
									CachedIcon);
							}
						}
					}
				}
			}
		}
	}

	UE_LOG(LogGameplay, Verbose, TEXT("MOB %d is now %s"), 
		GetActorId_Implementation(), bIsAggressive ? TEXT("Aggressive") : TEXT("Passive"));
}

