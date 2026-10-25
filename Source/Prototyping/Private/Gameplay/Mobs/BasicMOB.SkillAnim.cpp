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

void ABasicMOB::PlaySkillAnimation_Implementation(const FString& AnimationName, const FString& SkillSlug, float Duration)
{
    UE_LOG(LogGameplay, Verbose, TEXT("MOB %d playing skill animation: %s (slug: %s, Duration: %.1f)"), 
        GetActorId_Implementation(), *AnimationName, *SkillSlug, Duration);

    // Remember which skill is being cast so ShowDamageEffect can look up hitSound/hitEffect
    CurrentSkillName = SkillSlug.IsEmpty() ? AnimationName : SkillSlug;

    // --- Cast sound + cast particle from SkillDefinitionRepository ---
    bool bCastSoundPlayed = false;
    if (UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance()))
    {
        if (USkillDefinitionRepository* Repo = GI->GetSkillDefinitionRepository())
        {
            const FString LookupKey = SkillSlug.IsEmpty() ? AnimationName : SkillSlug;
            const FSkillDefinitionData& Def = Repo->GetDefinition(LookupKey);

			if (!Def.castSound.IsNull())
			{
				if (USoundBase* Sound = Def.castSound.LoadSynchronous())
				{
					SpawnSFXAttached(this, Sound, GetActorLocation(), 1.0f, DefaultAttenuation);
					bCastSoundPlayed = true;
				}
			}

			// Cast start voice: Priority 1 � skill-specific (same for any entity casting this skill)
			//                   Priority 2 � mob's own voice pool (CastVoiceSounds)
			{
				USoundBase* VoiceToPlay = nullptr;
				if (!Def.castStartVoice.IsNull())
				{
					VoiceToPlay = Def.castStartVoice.LoadSynchronous();
				}
				else if (CastVoiceSounds.Num() > 0)
				{
					VoiceToPlay = CastVoiceSounds[FMath::RandRange(0, CastVoiceSounds.Num() - 1)];
				}
				if (VoiceToPlay)
				{
					SpawnSFXAttached(this, VoiceToPlay, GetActorLocation(), 1.0f, DefaultAttenuation);
				}
			}

            // Niagara cast effect
            if (!Def.castEffectNiagara.IsNull())
            {
                if (UNiagaraSystem* NiagaraEffect = Def.castEffectNiagara.LoadSynchronous())
                {
                    FVector CastLoc = GetActorLocation();
                    FRotator CastRot = GetActorRotation();
                    if (Def.CastSocketName != NAME_None && GetMesh() && GetMesh()->DoesSocketExist(Def.CastSocketName))
                    {
                        CastLoc = GetMesh()->GetSocketLocation(Def.CastSocketName);
                        CastRot = GetMesh()->GetSocketRotation(Def.CastSocketName);
                    }
                    UNiagaraFunctionLibrary::SpawnSystemAtLocation(
                        GetWorld(), NiagaraEffect, CastLoc, CastRot);
                }
            }

			// Swing sound: Priority 1 � mob-specific (FMobAudioData::SwingSound via SoundMap["Swing"])
			// Priority 2 � skill-level generic fallback (FSkillDefinitionData::swingSound)
			// This mirrors the player: Weapon.EquippedSwingSound > SkillData.swingSound
			USoundBase* SwingToPlay = nullptr;
			if (USoundBase** MobSwing = SoundMap.Find(FName("Swing")))
			{
				SwingToPlay = *MobSwing;
			}
			else if (!Def.swingSound.IsNull())
			{
				SwingToPlay = Def.swingSound.LoadSynchronous();
			}
			if (SwingToPlay)
			{
				SpawnSFXAttached(this, SwingToPlay, GetActorLocation(), 1.0f, DefaultAttenuation);
			}
        }
    }

    // Legacy per-type sound fallback � only when DataTable has no castSound assigned
    if (!bCastSoundPlayed)
    {
        if (AnimationName.Contains(TEXT("attack"), ESearchCase::IgnoreCase))
        {
            PlaySoundByName("Attack");
        }
    }

    // --- Drive the AnimBP attack montage and bind hit-point notify ---
    // This ensures the deferred-hit-point pipeline in CombatSystemManager fires
    // when the animation reaches the impact frame (or via fallback timer).
    if (UMOBAnimInstance* AnimInst = Cast<UMOBAnimInstance>(GetMesh()->GetAnimInstance()))
    {
        // Remove any previous hit-point binding to avoid double-fire
        if (HitPointDelegateHandle.IsValid())
        {
            AnimInst->OnHitPoint.Remove(HitPointDelegateHandle);
            HitPointDelegateHandle.Reset();
        }

        if (UMyGameInstance* GI2 = Cast<UMyGameInstance>(GetGameInstance()))
        {
            if (UCombatSystemManager* CombatMgr = GI2->GetCombatSystemManager())
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

        // Build a minimal FSkillInitiationData so StartAttack can resolve the montage
        FSkillInitiationData InitData;
        InitData.animationName     = AnimationName;
        InitData.animationDuration = Duration;
        InitData.casterId          = GetActorId_Implementation();
        AnimInst->StartAttack(InitData);
    }
}

