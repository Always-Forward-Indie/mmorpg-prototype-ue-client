// Fill out your copyright notice in the Description page of Project Settings.


#include "Gameplay/Mobs/BasicMOB.h"
#include "Data/DataStructs.h"
#include "Prototyping.h"
#include "Gameplay/Interaction/TargetDecalComponent.h"
#include "Gameplay/Players/BasicPlayer.h"
#include "Gameplay/Mobs/MOBAnimInstance.h"
#include "Gameplay/Combat/CombatSystemManager.h"
#include "Gameplay/Items/ItemManager.h"
#include "Gameplay/Items/HarvestManager.h"
#include "MyGameInstance.h"
#include "Gameplay/Mobs/MOBManager.h"
#include "Audio/AudioManager.h"
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

void ABasicMOB::BeginPlay()
{
	Super::BeginPlay();

	// Inject TimeSyncService into movement component so dead-reckoning
	// uses the calibrated clock offset instead of raw wall-clock delta
	if (UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance()))
	{
		if (MOBMovementComponent)
		{
			MOBMovementComponent->SetTimeSyncService(GI->GetTimeSyncService());
		}

		// Route mob audio through the SFX SoundClass so the SFX volume slider works
		if (GI->AudioManager && GI->AudioManager->SFXClass)
		{
			if (AudioComponentMain)  { AudioComponentMain->SoundClassOverride  = GI->AudioManager->SFXClass; }
			if (AudioComponentSecond) { AudioComponentSecond->SoundClassOverride = GI->AudioManager->SFXClass; }
		}


		// Register in O(1) actor registry so FindTargetActor skips GetAllActorsOfClass
		const int32 Uid = GetActorId_Implementation();
		if (GI->MOBManager && Uid > 0)
		{
			GI->MOBManager->RegisterMob(Uid, this);
		}
	}

	HeadWidget = Cast<UW_MOBHeadInfoWidget>(MobHeadInfo->GetUserWidgetObject());

	if (UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance()))
	{
		if (ULocalizationSubsystem* LocSys = GI->GetSubsystem<ULocalizationSubsystem>())
		{
			LocSys->OnLocaleChanged.AddDynamic(this, &ABasicMOB::HandleLocaleChanged);
		}
	}
}

// Override EndPlay to unregister from combat system
void ABasicMOB::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // Unregister from actor registry
    if (UMyGameInstance* GameInstance = Cast<UMyGameInstance>(GetGameInstance()))
    {
        const int32 Uid = GetActorId_Implementation();
        if (GameInstance->MOBManager && Uid > 0)
        {
            GameInstance->MOBManager->UnregisterMob(Uid);
        }
        if (UHarvestManager* HM = GameInstance->GetHarvestManager())
        {
            HM->RemoveKnownEmptyCorpse(Uid);
        }
    }

    // Unregister from combat system
    if (UMyGameInstance* GameInstance = Cast<UMyGameInstance>(GetGameInstance()))
    {
        if (UCombatSystemManager* CombatManager = GameInstance->GetCombatSystemManager())
        {
            // ��������� ������������ ������ ���� ������ ��� �������
            if (IsValid(this) && MOBData.mobID > 0)
            {
                TScriptInterface<ICombatable> CombatableInterface;
                CombatableInterface.SetObject(this);
                CombatableInterface.SetInterface(this);
                
                CombatManager->UnregisterCombatable(CombatableInterface);
                UE_LOG(LogGameplay, Verbose, TEXT("MOB %d unregistered from combat system"), GetActorId_Implementation());
            }
        }
    }

    if (UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance()))
    {
        if (ULocalizationSubsystem* LocSys = GI->GetSubsystem<ULocalizationSubsystem>())
        {
            LocSys->OnLocaleChanged.RemoveDynamic(this, &ABasicMOB::HandleLocaleChanged);
        }
    }

    Super::EndPlay(EndPlayReason);
}

void ABasicMOB::OnReceiveSkillInitiation(const FSkillInitiationData& SkillData)
{
	// Broadcast to Blueprint / AnimBP for cast animation
	OnSkillInitiated.Broadcast(SkillData, SkillData.animationDuration);

	// This method is only called by CombatSystemManager when this mob IS the caster,
	// so we unconditionally drive the animation and hit-point notify binding.
	PlaySkillAnimation_Implementation(SkillData.animationName, SkillData.skillSlug, SkillData.animationDuration);

	if (UMOBAnimInstance* AnimInst = Cast<UMOBAnimInstance>(GetMesh()->GetAnimInstance()))
	{
		if (HitPointDelegateHandle.IsValid())
		{
			AnimInst->OnHitPoint.Remove(HitPointDelegateHandle);
			HitPointDelegateHandle.Reset();
		}

		if (UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance()))
		{
			if (UCombatSystemManager* CombatMgr = GI->GetCombatSystemManager())
			{
				HitPointDelegateHandle = AnimInst->OnHitPoint.AddLambda([CombatMgr](int32 InCasterId)
				{
					if (IsValid(CombatMgr))
					{
						CombatMgr->NotifyHitPoint(InCasterId);
					}
				});
			}
		}

		AnimInst->StartAttack(SkillData);
	}
}

void ABasicMOB::OnReceiveSkillResult(const FSkillResultData& SkillResult)
{
	// Update health/mana with authoritative server values
	if (SkillResult.targetId == GetActorId_Implementation())
	{
		SetMOBCurrentHealth(SkillResult.finalTargetHealth);
		if (SkillResult.finalTargetMana > 0)
		{
			SetMOBCurrentMana(SkillResult.finalTargetMana);
		}
	ForceUpdateUI();

		if (!SkillResult.isMissed)
		{
			if (SkillResult.damage > 0)
			{
				SetMobIsDamaged(true);

				if (UMOBAnimInstance* AnimInst = Cast<UMOBAnimInstance>(GetMesh()->GetAnimInstance()))
				{
					UE_LOG(LogGameplay, Warning, TEXT("[MOB %d] OnReceiveSkillResult: damage=%d, calling NotifyHit"),
						GetActorId_Implementation(), SkillResult.damage);
					AnimInst->NotifyHit();
				}
				else
				{
					UE_LOG(LogGameplay, Warning, TEXT("[MOB %d] OnReceiveSkillResult: Cast<UMOBAnimInstance> failed"), GetActorId_Implementation());
				}
			}
		}

		if (SkillResult.targetDied)
		{
			SetMOBIsDead(true);
			Die();
			OnMOBDied.Broadcast();
		}

		ForceUpdateUI();
	}

	OnSkillResult.Broadcast(SkillResult);
}

void ABasicMOB::OnReceiveEffectTick(const FEffectTickData& EffectData)
{
	// characterId in effectTick matches mobUniqueID (numeric form)
	if (EffectData.characterId != GetActorId_Implementation())
	{
		return;
	}

	SetMOBCurrentHealth(EffectData.newHealth);
	SetMOBCurrentMana(EffectData.newMana);
	ForceUpdateUI();

	// Show floating combat text for both heal and damage ticks.
	if (EffectData.value > 0)
	{
		APawn* LocalPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
		if (LocalPawn)
		{
			if (UUIManager* UIM = LocalPawn->FindComponentByClass<UUIManager>())
			{
				if (UFloatingCombatTextManager* FCT = UIM->GetFCTManager())
				{
					const FVector Pos = GetCombatPosition_Implementation();

					if (EffectData.bIsHeal)
					{
						FCT->ShowDamage(Pos, static_cast<float>(EffectData.value), false, EDamageType::Heal);
					}
					else
					{
						EDamageType DmgType = EDamageType::Physical;
						if      (EffectData.effectTypeSlug.Contains(TEXT("poison"))) DmgType = EDamageType::Poison;
						else if (EffectData.effectTypeSlug.Contains(TEXT("fire")) || EffectData.effectTypeSlug.Contains(TEXT("burn"))) DmgType = EDamageType::Fire;
						else if (EffectData.effectTypeSlug.Contains(TEXT("ice"))  || EffectData.effectTypeSlug.Contains(TEXT("frost"))) DmgType = EDamageType::Ice;
						FCT->ShowDamage(Pos, static_cast<float>(EffectData.value), false, DmgType);
					}
				}
			}
		}
	}

	// Flag as damaged for visual feedback on non-heal ticks.
	if (!EffectData.bIsHeal && EffectData.value > 0)
	{
		SetMobIsDamaged(true);
	}

	if (EffectData.targetDied)
	{
		SetMOBIsDead(true);
		Die();
		OnMOBDied.Broadcast();
	}

	ForceUpdateUI();
	OnEffectTick.Broadcast(EffectData);
}

void ABasicMOB::OnReceiveTargetLost()
{
	SetMobTargetId(0);
	SetMobTargetType(TEXT(""));
	bAggroLockedOut = true;

	// Only flip to yellow if this mob is NOT naturally aggressive.
	// Naturally-aggressive mobs (wolves, hostile undead, etc.) should stay red
	// when returning to their zone � they are always hostile and will re-aggro.
	if (!bIsNaturallyAggressive)
	{
		SetMOBIsAggressive(false);
		if (MobHeadInfo)
		{
			MobHeadInfo->UpdateMobAggressive(false);
		}
	}
	else
	{
		// Naturally-aggressive mob returned to zone � reset runtime flag to match
		// the base definition (still aggressive = still red).
		SetMOBIsAggressive(true);
	}

	if (UMOBAnimInstance* AnimInst = Cast<UMOBAnimInstance>(GetMesh()->GetAnimInstance()))
	{
		AnimInst->NotifyTargetLost();
	}

	OnMOBTargetLost.Broadcast();
}

void ABasicMOB::OnReceiveServerPacket(const FPositionDataStruct& MOBPosition)
{
	MOBData.mobPosition = MOBPosition;

	if (MOBMovementComponent)
	{
		MOBMovementComponent->OnReceiveServerPacket(MOBPosition);
	}
}

void ABasicMOB::OnReceiveMovePacket(const FMobMoveEntryStruct& MoveEntry, int64 ServerSendMs, int64 ClientRecvMs)
{
	// Update stored position.
	MOBData.mobPosition = MoveEntry.position;

	// Only states 1 (CHASING) and 2/3/4 (attack cycle) mean the mob is
	// actively targeting a player and should display as aggressive.
	// States 5 (RETURNING), 6 (EVADING), 7 (FLEEING) mean the mob already
	// lost or is disengaging � aggro is cleared via OnReceiveTargetLost.
	// State 0 (PATROLLING) is neutral.
	const bool bActivelyEngaging = (MoveEntry.combatState >= 1 && MoveEntry.combatState <= 4);
	if (bActivelyEngaging)
	{
		if (bAggroLockedOut)
		{
			// New aggro after a target-lost: unlock and enable aggro
			bAggroLockedOut = false;
		}
		MOBData.bIsAggressive = true;
	}

	// OnReceiveMovePacket handles CombatState internally (including PrevCombatState
	// tracking for transition detection), so do NOT call SetCombatState separately.
	if (MOBMovementComponent)
	{
		MOBMovementComponent->OnReceiveMovePacket(MoveEntry, ServerSendMs, ClientRecvMs);
	}
}

void ABasicMOB::OnReceiveMobHealthUpdate(const FMobHealthUpdateStruct& HealthUpdate)
{
	// Server sends this during RETURNING state (leash regen 10%/sec).
	// Simply update HP and refresh the head-info widget.
	SetMOBCurrentHealth(HealthUpdate.currentHealth);
	ForceUpdateUI();
}

