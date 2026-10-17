// Fill out your copyright notice in the Description page of Project Settings.


#include "Gameplay/Mobs/BasicMOB.h"
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

// Convert ESkillSchool to EDamageType for floating combat text
static EDamageType MOBSchoolToDamageType(ESkillSchool School)
{
	switch (School)
	{
	case ESkillSchool::Fire:    return EDamageType::Fire;
	case ESkillSchool::Ice:     return EDamageType::Ice;
	default:                    return EDamageType::Physical;
	}
}

void ABasicMOB::ShowDamageEffect_Implementation(int32 Damage, bool bIsCritical, ESkillSchool School, bool bIsMissed, bool bIsBlocked, const FString& SkillSlug)
{
	UE_LOG(LogTemp, Log, TEXT("MOB %d taking %d damage (Critical: %s, School: %s, Missed: %s, Blocked: %s, Skill: %s)"), 
		GetActorId_Implementation(), Damage,
		bIsCritical ? TEXT("true") : TEXT("false"), *UEnum::GetValueAsString(School),
		bIsMissed ? TEXT("true") : TEXT("false"),
		bIsBlocked ? TEXT("true") : TEXT("false"),
		*SkillSlug);

	// Set damage flag for visual feedback only on actual hit
	if (!bIsMissed)
	{
		SetMobIsDamaged(true);
	}

	// --- Hit sound + hit particle from SkillDefinitionRepository ---
	bool bHitSoundPlayed = false;
	if (UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance()))
	{
		if (USkillDefinitionRepository* Repo = GI->GetSkillDefinitionRepository())
		{
			const FSkillDefinitionData& Def = Repo->GetDefinition(SkillSlug);

			UE_LOG(LogTemp, Warning, TEXT("[MOB HitSFX] SkillSlug='%s' WeaponImpactType='%s' hitSound=%s"),
				*SkillSlug,
				*Def.WeaponImpactType.ToString(),
				Def.hitSound.IsNull() ? TEXT("NULL") : TEXT("SET"));

			// --- Impact sound: WeaponImpactType ? ArmorMaterialType lookup ---
			if (Def.WeaponImpactType != NAME_None)
			{
				UDataTable* ImpactTable = GI->GetImpactSoundsTable();
				UE_LOG(LogTemp, Warning, TEXT("[MOB HitSFX] ImpactTable=%s  MobDefinitionTable=%s  mobSlug='%s'"),
					ImpactTable ? TEXT("OK") : TEXT("NULL"),
					MobDefinitionTable ? TEXT("OK") : TEXT("NULL"),
					*MOBData.mobSlug);

				if (ImpactTable)
				{
					// Resolve ArmorMaterialType from MobDefinitionTable
					FName ArmorMat = NAME_None;
					if (MobDefinitionTable)
					{
						FName SlugKey = FName(*MOBData.mobSlug);
						if (const FMobDefinition* MobDef = MobDefinitionTable->FindRow<FMobDefinition>(SlugKey, TEXT("")))
						{
							ArmorMat = MobDef->ArmorMaterialType;
						}
					}

					UE_LOG(LogTemp, Warning, TEXT("[MOB HitSFX] ArmorMat='%s'"), *ArmorMat.ToString());

					if (ArmorMat != NAME_None)
					{
						FName ImpactKey = FName(*FString::Printf(TEXT("%s_%s"),
							*Def.WeaponImpactType.ToString(), *ArmorMat.ToString()));

						const FImpactSoundData* ImpactRow = ImpactTable->FindRow<FImpactSoundData>(ImpactKey, TEXT(""));
						UE_LOG(LogTemp, Warning, TEXT("[MOB HitSFX] ImpactKey='%s'  RowFound=%s  Sounds=%d"),
							*ImpactKey.ToString(),
							ImpactRow ? TEXT("YES") : TEXT("NO"),
							ImpactRow ? ImpactRow->ImpactSounds.Num() : 0);

						if (ImpactRow)
						{
							if (ImpactRow->ImpactSounds.Num() > 0)
							{
								int32 Idx = FMath::RandRange(0, ImpactRow->ImpactSounds.Num() - 1);
								USoundBase* ImpactSound = ImpactRow->ImpactSounds[Idx].LoadSynchronous();
								UE_LOG(LogTemp, Warning, TEXT("[MOB HitSFX] ImpactSound[%d]=%s"), Idx, ImpactSound ? TEXT("OK") : TEXT("NULL"));
								if (ImpactSound)
									{
										SpawnSFXAttached(this, ImpactSound, GetActorLocation(), 1.0f, DefaultAttenuation);
										bHitSoundPlayed = true;
									}
							}

							// Spawn impact VFX if defined
							if (!ImpactRow->ImpactVFX.IsNull())
							{
								if (UNiagaraSystem* ImpactVFX = ImpactRow->ImpactVFX.LoadSynchronous())
								{
									FVector HitLoc = GetCombatPosition_Implementation();
									if (Def.HitSocketName != NAME_None && GetMesh() && GetMesh()->DoesSocketExist(Def.HitSocketName))
									{
										HitLoc = GetMesh()->GetSocketLocation(Def.HitSocketName);
									}
									UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), ImpactVFX, HitLoc);
								}
							}
						}
					}
				}
			}

			// Fallback: generic hitSound from skill definition
			if (!bHitSoundPlayed && !Def.hitSound.IsNull())
			{
				USoundBase* Sound = Def.hitSound.LoadSynchronous();
				UE_LOG(LogTemp, Warning, TEXT("[MOB HitSFX] Fallback hitSound=%s"), Sound ? TEXT("OK") : TEXT("NULL (asset not loaded)"));
				if (Sound)
				{
					SpawnSFXAttached(this, Sound, GetActorLocation(), 1.0f, DefaultAttenuation);
					bHitSoundPlayed = true;
				}
			}

            // Niagara hit effect (preferred over Cascade)
            if (!Def.hitEffectNiagara.IsNull())
            {
                if (UNiagaraSystem* NiagaraEffect = Def.hitEffectNiagara.LoadSynchronous())
                {
                    FVector HitLoc = GetCombatPosition_Implementation();
                    FRotator HitRot = FRotator::ZeroRotator;
                    if (Def.HitSocketName != NAME_None && GetMesh() && GetMesh()->DoesSocketExist(Def.HitSocketName))
                    {
                        HitLoc = GetMesh()->GetSocketLocation(Def.HitSocketName);
                        HitRot = GetMesh()->GetSocketRotation(Def.HitSocketName);
                    }
                    UNiagaraFunctionLibrary::SpawnSystemAtLocation(
                        GetWorld(), NiagaraEffect, HitLoc, HitRot);
                }
            }

            // Critical hit sound (layered on top of regular hit sound)
            if (bIsCritical && !bIsMissed && !Def.critSound.IsNull())
            {
                if (USoundBase* CritSnd = Def.critSound.LoadSynchronous())
                {
                    SpawnSFXAttached(this, CritSnd, GetActorLocation(), 1.0f, DefaultAttenuation);
                }
            }
        }
    }

    // Legacy fallback hit sound � only when DataTable has no hitSound assigned
    if (!bHitSoundPlayed)
    {
        PlaySoundByName("Hit");
    }

    // NOTE: Floating combat text is handled by DamageEffectHandler::ShowFloatingDamageText
    // to avoid duplicates. Do NOT call FCT->ShowDamage here.

	// --- Hit Stop: freeze this MOB briefly so the impact feels weighty ---
	// Not on miss � missing doesn't interrupt the actor's flow.
	if (!bIsMissed)
	{
		if (UWorld* W = GetWorld())
		{
			CustomTimeDilation = 0.0f;
			FTimerHandle HitStopTimer;
			TWeakObjectPtr<ABasicMOB> WeakSelf(this);
			W->GetTimerManager().SetTimer(HitStopTimer, [WeakSelf]()
			{
				if (WeakSelf.IsValid())
				{
					WeakSelf->CustomTimeDilation = 1.0f;
				}
			}, 0.06f, false);
		}
	}
}

void ABasicMOB::ShowHealingEffect_Implementation(int32 Healing, const FString& SkillSlug)
{
    UE_LOG(LogTemp, Log, TEXT("MOB %d healed for %d (skill: %s)"), GetActorId_Implementation(), Healing, *SkillSlug);

    UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance());
    if (!GI) return;

    if (USkillDefinitionRepository* Repo = GI->GetSkillDefinitionRepository())
    {
        const FSkillDefinitionData& Def = Repo->GetDefinition(SkillSlug);

        if (!Def.healSound.IsNull())
        {
            if (USoundBase* Snd = Def.healSound.LoadSynchronous())
            {
                SpawnSFXAttached(this, Snd, GetActorLocation(), 1.0f, DefaultAttenuation);
            }
        }

        if (!Def.healEffectNiagara.IsNull())
        {
            if (UNiagaraSystem* HealVFX = Def.healEffectNiagara.LoadSynchronous())
            {
                FVector HealLoc = GetCombatPosition_Implementation();
                FRotator HealRot = FRotator::ZeroRotator;
                if (Def.HitSocketName != NAME_None && GetMesh() && GetMesh()->DoesSocketExist(Def.HitSocketName))
                {
                    HealLoc = GetMesh()->GetSocketLocation(Def.HitSocketName);
                    HealRot = GetMesh()->GetSocketRotation(Def.HitSocketName);
                }
                UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), HealVFX, HealLoc, HealRot);
            }
        }
    }
}

void ABasicMOB::ShowBuffEffect_Implementation(const FAppliedEffectData& Effect)
{
    UE_LOG(LogTemp, Log, TEXT("MOB %d received %s effect: %s (Value: %d, Duration: %.1f)"), 
        GetActorId_Implementation(), *Effect.effectType, *Effect.effectName, Effect.value, Effect.duration);

    UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance());
    if (!GI) return;

    UDataTable* EffectTable = GI->GetEffectDefinitionTable();
    if (!EffectTable) return;

    FName RowKey = FName(*Effect.effectName);
    const FEffectDefinitionRow* Row = EffectTable->FindRow<FEffectDefinitionRow>(RowKey, TEXT(""));
    if (!Row) return;

    // Play apply sound
    if (!Row->ApplySound.IsNull())
    {
        if (USoundBase* Snd = Row->ApplySound.LoadSynchronous())
        {
            SpawnSFXAttached(this, Snd, GetActorLocation(), 1.0f, DefaultAttenuation);
        }
    }

    // Spawn apply VFX
    if (!Row->ApplyVFX.IsNull())
    {
        if (UNiagaraSystem* VFX = Row->ApplyVFX.LoadSynchronous())
        {
            UNiagaraFunctionLibrary::SpawnSystemAttached(
                VFX, GetMesh(), NAME_None,
                FVector::ZeroVector, FRotator::ZeroRotator,
                EAttachLocation::SnapToTarget, true);
        }
    }
}

