// Fill out your copyright notice in the Description page of Project Settings.


#include "Gameplay/Players/BasicPlayer.h"
#include "Prototyping.h"
#include "EngineUtils.h"
#include "MyGameInstance.h"
#include "Gameplay/Players/PlayerManager.h"
#include "UI/UIManager.h"
#include "UI/NotificationToastWidget.h"
#include "UI/ChatWidget.h"
#include "UI/SkillBarWidget.h"
#include "Gameplay/Items/InventoryManager.h"
#include "Gameplay/Idle/IdleTimeoutManager.h"
#include "Gameplay/Items/HarvestManager.h"
#include "Gameplay/Player/ExperienceManager.h"
#include "Gameplay/Combat/CombatSystemManager.h"
#include "Gameplay/Combat/SkillSystemManager.h"
#include "Gameplay/Skills/PlayerSkillManager.h"
#include "Gameplay/UI/FloatingCombatTextManager.h"
#include "Gameplay/UI/DamageTextWidget.h"
#include "Gameplay/Mobs/BasicMOB.h"
#include "CrashDiagnostics.h"
#include "Gameplay/NPCs/BasicNPC.h"
#include "Gameplay/NPCs/NPCManager.h"
#include "Gameplay/Items/DroppedItemActor.h"
#include "Gameplay/Dialogue/DialogueManager.h"
#include "Gameplay/Quest/QuestManager.h"
#include "Gameplay/Equipment/EquipmentManager.h"
#include "Gameplay/Equipment/EquipmentVisualComponent.h"
#include "Gameplay/Players/CosmeticVisualComponent.h"
#include "Gameplay/Items/ItemManager.h"
#include "Services/LocalizationSubsystem.h"
#include "Data/ItemStruct.h"
#include "Gameplay/Vendor/VendorManager.h"
#include "Gameplay/Repair/RepairManager.h"
#include "Gameplay/Trade/TradeManager.h"
#include "Gameplay/Player/PlayerStatsManager.h"
#include "Gameplay/Player/PlayerStatsNetworkHandler.h"
#include "UI/PlayerStatsWidget.h"
#include "Gameplay/Interaction/CursorInteractionComponent.h"
#include "Gameplay/Interaction/TargetDecalComponent.h"
#include "Gameplay/Interaction/WorldInteractionConfig.h"
#include "Gameplay/Bestiary/BestiaryNetworkHandler.h"
#include "Utils/PlayerAttributeParser.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Gameplay/UI/ActiveEffectsWidget.h"
#include "Gameplay/UI/PlayerInterfaceWidget.h"
#include "Gameplay/UI/PlayerHUD.h"
#include "Gameplay/UI/CastBarWidget.h"
#include "Gameplay/Combat/BaseMMOProjectile.h"
#include "Gameplay/Players/PlayerAnimInstance.h"
#include "Gameplay/UI/PlayerNameplateComponent.h"
#include "Gameplay/Skills/SkillDefinitionRepository.h"
#include "Data/EntityAudioRepository.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Particles/ParticleSystem.h"
#include "NiagaraSystem.h"
#include "NiagaraFunctionLibrary.h"
#include "Gameplay/Chat/ChatManager.h"
#include "Gameplay/Player/TitleManager.h"
#include "Data/EffectDefinitionTable.h"
#include "Gameplay/Emotes/EmoteManager.h"
#include "Gameplay/Emotes/EmoteNetworkHandler.h"
#include "Gameplay/Emotes/EmoteComponent.h"
#include "Gameplay/WorldObjects/WorldInteractiveObjectActor.h"
#include "Gameplay/WorldObjects/WorldObjectManager.h"
#include "Utils/AudioSpawnHelpers.h"
#include "Data/CharacterVisualData.h"
#include "Engine/AssetManager.h"


// ICombatable interface implementations
int32 ABasicPlayer::GetMaxHealth_Implementation() const
{
    if (const FAttributeDataStruct* HealthAttr = playerData.characterData.characterAttributes.attributesData.Find(TEXT("max_health")))
    {
        if (HealthAttr->attributeValue > 0)
        {
            return HealthAttr->attributeValue;
        }
    }
    // Fallback: if max_health not yet received from stats_update, return current HP
    // so percentage calculations don't produce absurd values (e.g. 435/1).
    return FMath::Max(1, playerData.characterData.characterCurrentHealth);
}

int32 ABasicPlayer::GetMaxMana_Implementation() const
{
    if (const FAttributeDataStruct* ManaAttr = playerData.characterData.characterAttributes.attributesData.Find(TEXT("max_mana")))
    {
        if (ManaAttr->attributeValue > 0)
        {
            return ManaAttr->attributeValue;
        }
    }
    return FMath::Max(1, playerData.characterData.characterCurrentMana);
}

void ABasicPlayer::SetDead_Implementation(bool bNewDead)
{
    playerData.characterData.bIsDead = bNewDead;

    // Sync dead state on the nameplate (works for both local and remote пїЅ local is already hidden)
    if (NameplateComponent)
    {
        NameplateComponent->SetDeadState(bNewDead);
    }

    if (bNewDead)
    {
        UE_LOG(LogGameplay, Warning, TEXT("Player %d has died"), GetActorId_Implementation());
        OnDeath_Implementation();

        // Immediately flush any pending skill results so queued damage/healing
        // applies now instead of waiting for the 12-second safety timeout.
        // This keeps the mob's state in sync with the server instead of having
        // damage numbers appear long after the player has died.
        if (!playerData.isOtherClient && MyGameInstance)
        {
            if (UCombatSystemManager* CSM = MyGameInstance->GetCombatSystemManager())
            {
                UE_LOG(LogGameplay, Verbose, TEXT("BasicPlayer: Flushing pending skill results on death (caster=%d)"),
                    GetActorId_Implementation());
                CSM->FlushAllPendingForCaster(GetActorId_Implementation());
            }
        }
    }
    else
    {
        // Revive: clear any lingering combat / interaction state (#6)
        bIsPickingUp = false;
        bIsCasting   = false;
        // Next stats_update snaps HP/MP bars to the server values instead of
        // lerping up from the corpse state (see ProcessStatsUpdate).
        if (!playerData.isOtherClient)
        {
            bJustRevived = true;
        }
        bIsApproachingTarget = false;
        if (GetWorld())
        {
            GetWorld()->GetTimerManager().ClearTimer(AutoAttackRetryTimerHandle);
        }

        // Restore movement so the player can walk again
        if (UCharacterMovementComponent* Movement = GetCharacterMovement())
        {
            Movement->SetMovementMode(MOVE_Walking);
        }
        // Record revive timestamp so ProcessStatsUpdate can ignore stale
        // stats_update(healthCurrent=0) packets queued before the respawn ACK.
        LastRespawnWorldTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
        HideDeathScreen();
        // Play revive sound
        if (const FEntityAudioProfile* Profile = GetAudioProfile())
        {
            PlayEventSound(Profile->Revive);
        }
        // Notify Blueprint AnimBP so it can exit the death state
        OnRevive();
        UE_LOG(LogGameplay, Warning, TEXT("Player %d has been revived"), GetActorId_Implementation());
    }
}

void ABasicPlayer::OnDeath_Implementation()
{
    UE_LOG(LogGameplay, Warning, TEXT("Player %s died"), *playerData.characterData.characterName);

    // Stop any ongoing auto-attack / approach cycle (#5)
    StopAutoAttack();
    ClearLockedTarget();
    ClearTarget_Implementation();

    // Release pickup/cast locks so they cannot block input after respawn
    bIsPickingUp = false;
    bIsCasting   = false;

    // Stop the cast bar immediately — cancel the timer and hide the widget.
    // The cast bar widget has its own bIsCasting flag, so just clearing
    // ABasicPlayer::bIsCasting is insufficient.
    HideCastBar_Implementation();

    // Play death sound
    if (const FEntityAudioProfile* Profile = GetAudioProfile())
    {
        PlayEventSound(Profile->Death);
    }

    // Disable movement so the player can't walk while dead
    if (UCharacterMovementComponent* Movement = GetCharacterMovement())
    {
        Movement->DisableMovement();
    }

    // Release any held mouse buttons so the corpse cannot be rotated.
    // The input events fire while the death screen is fading in and bIsDead
    // is already true, so Look() guards against any stray events as well.
    bIsRightMouseDown = false;
    bIsLeftMouseDown  = false;
    bHasDesiredMeshYaw = false;
    RestoreCursorToUIManager();

    // Drive the AnimBP death state
    if (UPlayerAnimInstance* AnimInst = GetPlayerAnimInstance())
    {
        AnimInst->NotifyDeath();
    }

    // Show the death screen overlay
    ShowDeathScreen();
}

void ABasicPlayer::OnRevive()
{
    UE_LOG(LogGameplay, Verbose, TEXT("Player %d OnRevive"), GetActorId_Implementation());

    // Notify AnimBP so it can transition out of the death state
    if (UPlayerAnimInstance* AnimInst = GetPlayerAnimInstance())
    {
        AnimInst->NotifyRevive();
    }
}

void ABasicPlayer::SetTarget_Implementation(int32 TargetId, ECasterType TargetType)
{
    CurrentTargetId = TargetId;
    CurrentTargetType = TargetType;
    
    UE_LOG(LogGameplay, Verbose, TEXT("Player %d set target: %d (%s)"), 
        GetActorId_Implementation(), TargetId, *UEnum::GetValueAsString(TargetType));
}

void ABasicPlayer::ClearTarget_Implementation()
{
    CurrentTargetId = 0;
    CurrentTargetType = ECasterType::None;
    
    UE_LOG(LogGameplay, Verbose, TEXT("Player %d cleared target"), GetActorId_Implementation());
}

void ABasicPlayer::PlaySkillAnimation_Implementation(const FString& AnimationName, const FString& SkillSlug, float Duration)
{
    UE_LOG(LogGameplay, Warning, TEXT("[PlayerAnim] PlaySkillAnimation: player=%d anim='%s' slug='%s' duration=%.3fs"),
        GetActorId_Implementation(), *AnimationName, *SkillSlug, Duration);

    if (!MyGameInstance) return;
    UCombatSystemManager* CombatMgr = MyGameInstance->GetCombatSystemManager();
    if (!CombatMgr) return;

    // Track the animating skill slug for audio/VFX only (see PlayCombatSoundEvent).
    // CurrentSkillName is intentionally NOT overwritten here so the auto-attack
    // loop (DoAutoAttack) always sends "basic_attack" to the server.
    ActiveAnimSkillSlug = SkillSlug.IsEmpty() ? AnimationName : SkillSlug;
    CurrentAnimationDuration = Duration;

    const int32 CasterId = GetActorId_Implementation();

    if (UPlayerAnimInstance* AnimInst = GetPlayerAnimInstance())
    {
        UE_LOG(LogGameplay, Warning, TEXT("[PlayerAnim] AnimInstance found (%s), calling StartAttack"),
            *AnimInst->GetClass()->GetName());

        // Remove any previous OnHitPoint binding so we never fire twice
        if (HitPointDelegateHandle.IsValid())
        {
            AnimInst->OnHitPoint.Remove(HitPointDelegateHandle);
            HitPointDelegateHandle.Reset();
        }

        // For projectile skills the hit-point is notified by the projectile on impact,
        // NOT by the animation notify. Only bind the anim-notify path for melee/instant skills.
        const FString LookupKeyEarly = SkillSlug.IsEmpty() ? AnimationName : SkillSlug;
        bool bSkillHasProjectile = false;
        if (MyGameInstance)
        {
            if (USkillDefinitionRepository* Repo = MyGameInstance->GetSkillDefinitionRepository())
            {
                bSkillHasProjectile = !Repo->GetDefinition(LookupKeyEarly).projectileClass.IsNull();
            }
        }

        if (!bSkillHasProjectile)
        {
            HitPointDelegateHandle = AnimInst->OnHitPoint.AddLambda([CombatMgr](int32 InCasterId)
            {
                if (IsValid(CombatMgr))
                {
                    CombatMgr->NotifyHitPoint(InCasterId);
                }
            });
        }

        // Bind OnAttackEnded > PlayerSkillManager::NotifyAnimationEnded so the cast
        // lock is released only after the montage fully completes, preventing spam.
        // Bug 7 fix: also remove any stale AutoAttackAnimEndDelegateHandle so that a
        // lingering auto-attack end binding cannot trigger ClearLockedTarget() when
        // a server-confirmed skill animation ends while bIsAutoAttacking=false.
        if (AutoAttackAnimEndDelegateHandle.IsValid())
        {
            AnimInst->OnAttackEnded.Remove(AutoAttackAnimEndDelegateHandle);
            AutoAttackAnimEndDelegateHandle.Reset();
        }
        if (AnimEndDelegateHandle.IsValid())
        {
            AnimInst->OnAttackEnded.Remove(AnimEndDelegateHandle);
            AnimEndDelegateHandle.Reset();
        }
        if (UPlayerSkillManager* SkillMgr = MyGameInstance ? MyGameInstance->GetPlayerSkillManager() : nullptr)
        {
            AnimEndDelegateHandle = AnimInst->OnAttackEnded.AddUObject(
                SkillMgr, &UPlayerSkillManager::NotifyAnimationEnded);
        }

        FSkillInitiationData SkillData;
        SkillData.animationName      = AnimationName;
        SkillData.animationDuration  = Duration;
        SkillData.casterId           = CasterId;
        SkillData.castTime           = CurrentCastTime; // set by ShowCastBar_Implementation before this call

        // Pre-compute swing time now, while CurrentCastTime is still valid.
        // HideCastBar resets CurrentCastTime at T=castTime; the CastRelease notify
        // fires one frame later, so reading CurrentCastTime there gives 0 (wrong speed).
        CurrentSwingSeconds = (CurrentCastTime > 0.001f)
            ? FMath::Max(Duration - CurrentCastTime, 0.05f)
            : Duration;

        AnimInst->StartAttack(SkillData);

        // --- Cast sound & projectile from SkillDefinitionRepository ---
        if (MyGameInstance)
        {
            if (USkillDefinitionRepository* Repo = MyGameInstance->GetSkillDefinitionRepository())
            {
                // Use SkillSlug for definition lookup; fall back to AnimationName so older
                // DataTable rows that still use animationName as their key still work.
                const FString LookupKey = SkillSlug.IsEmpty() ? AnimationName : SkillSlug;
                const FSkillDefinitionData& Def = Repo->GetDefinition(LookupKey);

                // Play cast sound at player location
                if (!Def.castSound.IsNull())
                {
                    if (USoundBase* Sound = Def.castSound.LoadSynchronous())
                    {
                        USoundAttenuation* Atn = nullptr;
                        if (const FEntityAudioProfile* Prof = GetAudioProfile())
                        {
                            if (!Prof->DefaultAttenuation.IsNull())
                                Atn = Prof->DefaultAttenuation.LoadSynchronous();
                        }
                        SpawnSFXAttached(this, Sound, GetActorLocation(), 1.0f, Atn);
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

                // swingSound and VoiceAttack are now fired via AnimNotify_PlayerCombatEvent
                // placed on the montage at the correct timing frame.
                // Projectile is now spawned at AnimNotify CastRelease via PlayCombatSoundEvent.
            }
        }
    }
    else
    {
        // AnimBP parent class is not UPlayerAnimInstance пїЅ log the actual class so we can fix it
        if (GetMesh() && GetMesh()->GetAnimInstance())
        {
            UE_LOG(LogGameplay, Error,
                TEXT("[PlayerAnim] GetPlayerAnimInstance() returned nullptr! "
                     "Actual AnimInstance class: %s. "
                     "Set Anim Class parent to UPlayerAnimInstance in the Anim BP."),
                *AnimInst->GetClass()->GetName());
        }
        else
        {
            UE_LOG(LogGameplay, Error,
                TEXT("[PlayerAnim] GetPlayerAnimInstance() returned nullptr and no AnimInstance exists on mesh!"));
        }

        // Fallback: no AnimInstance assigned yet пїЅ schedule timer directly
        const float HitDelay = FMath::Max(Duration * 0.45f, 0.05f);
        UE_LOG(LogGameplay, Warning, TEXT("[PlayerAnim] FALLBACK timer: HitDelay=%.3fs"), HitDelay);
        if (UWorld* World = GetWorld())
        {
            World->GetTimerManager().ClearTimer(HitPointTimerHandle);
            World->GetTimerManager().SetTimer(HitPointTimerHandle, [CombatMgr, CasterId]()
            {
                if (IsValid(CombatMgr))
                {
                    CombatMgr->NotifyHitPoint(CasterId);
                }
            }, HitDelay, false);
        }
    }
}
