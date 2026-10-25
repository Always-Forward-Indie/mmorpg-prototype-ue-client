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

void ABasicPlayer::PlayCombatSoundEvent(ECombatSoundSlot Slot)
{
    // Sounds are intentionally suppressed for remote (other-client) players.
    // However, the CastRelease projectile spawn must happen for ALL players so
    // that skills cast by other players are visually visible on this client (Bug 3 fix).
    // Non-CastRelease events are skipped entirely for remote players.
    if (playerData.isOtherClient && Slot != ECombatSoundSlot::CastRelease)
    {
        return;
    }

    switch (Slot)
    {
    case ECombatSoundSlot::VoiceAttack:
    {
        // Priority 1: per-entity per-skill override (DT_EntitySkillVoiceOverrides, key "warrior_m|basic_attack")
        if (MyGameInstance)
        {
            if (UEntityAudioRepository* Repo = MyGameInstance->GetEntityAudioRepository())
            {
                if (const FEntitySkillVoiceOverride* Override =
                        Repo->FindSkillVoiceOverride(AudioProfileId, FName(*ActiveAnimSkillSlug)))
                {
                    if (Override->VoiceAttack.Num() > 0)
                    {
                        PlayEventSound(Override->VoiceAttack[
                            FMath::RandRange(0, Override->VoiceAttack.Num() - 1)]);
                        break;
                    }
                }
            }
        }
        // Priority 2: generic entity melee voice pool from the audio profile
        if (const FEntityAudioProfile* Profile = GetAudioProfile())
        {
            if (Profile->VoiceAttack.Num() > 0)
            {
                const int32 Idx = FMath::RandRange(0, Profile->VoiceAttack.Num() - 1);
                PlayEventSound(Profile->VoiceAttack[Idx]);
            }
        }
        break;
    }

    case ECombatSoundSlot::CastVoice:
    {
        // Cast-start voice: Priority 1 вЂ” skill-specific sound (same for any caster)
        //                   Priority 2 вЂ” this player's entity audio profile (VoiceCastStart pool)
        if (!MyGameInstance) break;
        if (USkillDefinitionRepository* Repo = MyGameInstance->GetSkillDefinitionRepository())
        {
            const FSkillDefinitionData& Def = Repo->GetDefinition(ActiveAnimSkillSlug);
            if (!Def.castStartVoice.IsNull())
            {
                PlayEventSound(Def.castStartVoice);
                break;
            }
        }
        if (const FEntityAudioProfile* Profile = GetAudioProfile())
        {
            bool bVoicePlayed = false;

            // Priority 2: per-entity per-skill override (DT_EntitySkillVoiceOverrides, key "warrior_m|fireball")
            if (UEntityAudioRepository* Repo = MyGameInstance->GetEntityAudioRepository())
            {
                if (const FEntitySkillVoiceOverride* Override =
                        Repo->FindSkillVoiceOverride(AudioProfileId, FName(*ActiveAnimSkillSlug)))
                {
                    if (Override->CastStartVoice.Num() > 0)
                    {
                        PlayEventSound(Override->CastStartVoice[
                            FMath::RandRange(0, Override->CastStartVoice.Num() - 1)]);
                        bVoicePlayed = true;
                    }
                }
            }

            // Priority 3: generic entity cast-start voice pool
            if (!bVoicePlayed && Profile->VoiceCastStart.Num() > 0)
            {
                PlayEventSound(Profile->VoiceCastStart[
                    FMath::RandRange(0, Profile->VoiceCastStart.Num() - 1)]);
            }
        }
        break;
    }

    case ECombatSoundSlot::SwingSound:
    {
        if (!MyGameInstance) break;

        // Priority 1: equipped main-hand weapon's swing sound (sword woosh в‰  staff swish в‰  unarmed)
        if (UEquipmentManager* EquipMgr = MyGameInstance->GetEquipmentManager())
        {
            const FEquipmentSlotData& MainHand = EquipMgr->GetSlot(TEXT("main_hand"));
            if (MainHand.bIsOccupied && !MainHand.itemSlug.IsEmpty())
            {
                if (UItemManager* ItemMgr = MyGameInstance->GetItemManager())
                {
                    const FItemVisualData VisData = ItemMgr->GetItemVisualDataBySlug(MainHand.itemSlug);
                    if (!VisData.EquippedSwingSound.IsNull())
                    {
                        PlayEventSound(VisData.EquippedSwingSound);
                        break;
                    }
                }
            }
        }

        // Priority 2: skill-level swing sound (generic fallback)
        if (USkillDefinitionRepository* Repo = MyGameInstance->GetSkillDefinitionRepository())
        {
            const FSkillDefinitionData& Def = Repo->GetDefinition(ActiveAnimSkillSlug);
            if (!Def.swingSound.IsNull())
            {
                PlayEventSound(Def.swingSound);
            }
        }
        break;
    }

    case ECombatSoundSlot::CastRelease:
    {
        if (!MyGameInstance) break;
        USkillDefinitionRepository* Repo = MyGameInstance->GetSkillDefinitionRepository();
        if (!Repo) break;

        const FSkillDefinitionData& Def = Repo->GetDefinition(ActiveAnimSkillSlug);

        // Sounds and VFX are only played for the local player.
        if (!playerData.isOtherClient)
        {
        // Release sound (e.g. fireball launch, arrow release)
        if (!Def.castEndSound.IsNull())
        {
            PlayEventSound(Def.castEndSound);
        }

        // Release Niagara VFX at CastSocket (e.g. muzzle flash on hands, departing glow).
        // Skip when a projectile class is set: castEndEffectNiagara is used as the projectile
        // trail (attached TrailVFX component) вЂ” spawning it here as well would leave a static
        // copy frozen at the cast socket.
        if (!Def.castEndEffectNiagara.IsNull() && Def.projectileClass.IsNull())
        {
            if (UNiagaraSystem* VFX = Def.castEndEffectNiagara.LoadSynchronous())
            {
                FVector SpawnLoc = GetActorLocation();
                FRotator SpawnRot = GetActorRotation();
                if (Def.CastSocketName != NAME_None && GetMesh() && GetMesh()->DoesSocketExist(Def.CastSocketName))
                {
                    SpawnLoc = GetMesh()->GetSocketLocation(Def.CastSocketName);
                    SpawnRot = GetMesh()->GetSocketRotation(Def.CastSocketName);
                }
                UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), VFX, SpawnLoc, SpawnRot);
            }
        }
        // Release voice: Priority 1 вЂ” skill-specific (castReleaseVoice), Priority 2 вЂ” player pool
        {
            TSoftObjectPtr<USoundBase> ReleaseVoice = Def.castReleaseVoice;
            if (!ReleaseVoice.IsNull())
            {
                PlayEventSound(ReleaseVoice);
            }
            else
            {
                // Fallback to entity audio profile:
                if (const FEntityAudioProfile* Profile = GetAudioProfile())
                {
                    bool bVoicePlayed = false;

                    // Priority 2: per-entity per-skill override (DT_EntitySkillVoiceOverrides)
                    if (UEntityAudioRepository* RepoAudio = MyGameInstance->GetEntityAudioRepository())
                    {
                        if (const FEntitySkillVoiceOverride* Override =
                            RepoAudio->FindSkillVoiceOverride(AudioProfileId, FName(*ActiveAnimSkillSlug)))
                        {
                            if (Override->CastReleaseVoice.Num() > 0)
                            {
                                PlayEventSound(Override->CastReleaseVoice[
                                    FMath::RandRange(0, Override->CastReleaseVoice.Num() - 1)]);
                                bVoicePlayed = true;
                            }
                        }
                    }

                    // Priority 3: generic entity release voice pool
                    if (!bVoicePlayed && Profile->VoiceCastRelease.Num() > 0)
                    {
                        PlayEventSound(Profile->VoiceCastRelease[
                            FMath::RandRange(0, Profile->VoiceCastRelease.Num() - 1)]);
                    }
                }
            }
        }
        } // end if (!playerData.isOtherClient) вЂ” sounds/VFX section

        // Spawn projectile if defined (preferred over frame-0 spawn вЂ” fires at correct cast-release timing)
        // This runs for both local and remote players so skills from other characters are visible (Bug 3 fix).
        if (!Def.projectileClass.IsNull())
        {
            UClass* ProjClass = Def.projectileClass.LoadSynchronous();
            if (ProjClass && GetWorld())
            {
                FName ProjSocket = (Def.CastSocketName != NAME_None) ? Def.CastSocketName : FName(TEXT("ProjectileSpawn"));
                FVector SpawnLoc = GetMesh() && GetMesh()->DoesSocketExist(ProjSocket)
                    ? GetMesh()->GetSocketLocation(ProjSocket)
                    : GetActorLocation() + FVector(0, 0, BaseEyeHeight);

                FRotator SpawnRot = GetActorRotation();
                AActor* TargetActor = nullptr;
                UCombatSystemManager* CombatMgr = MyGameInstance->GetCombatSystemManager();
                if (CurrentTargetId > 0 && CombatMgr)
                {
                    TScriptInterface<ICombatable> TargetCombatable =
                        CombatMgr->FindCombatableById(CurrentTargetId, CurrentTargetType);
                    if (TargetCombatable.GetObject() && IsValid(TargetCombatable.GetObject()))
                    {
                        TargetActor = Cast<AActor>(TargetCombatable.GetObject());
                        if (TargetActor)
                        {
                            SpawnRot = (TargetActor->GetActorLocation() - SpawnLoc).Rotation();
                        }
                    }
                }

                FActorSpawnParameters Params;
                Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
                Params.Instigator = this;
                AActor* Spawned = GetWorld()->SpawnActor<AActor>(ProjClass, SpawnLoc, SpawnRot, Params);

                // If it is a BaseMMOProjectile, configure it now
                if (ABaseMMOProjectile* Proj = Cast<ABaseMMOProjectile>(Spawned))
                {
                    // Use the pre-computed swing time so HideCastBar's CurrentCastTime reset
                    // (which fires at the same T=castTime) cannot corrupt the speed calculation.
                    float CalcSpeed = 0.0f;
                    const float SwingSeconds = CurrentSwingSeconds;
                    if (IsValid(TargetActor) && SwingSeconds > 0.001f)
                    {
                        const float Dist = FVector::Dist(SpawnLoc, TargetActor->GetActorLocation());
                        CalcSpeed = FMath::Min(Dist / SwingSeconds, 2500.0f);
                        UE_LOG(LogGameplay, Verbose, TEXT("[PlayerAnim] CastRelease: dist=%.0f swing=%.3fs \xe2\u2020\u2019 projectile speed=%.0f"),
                            Dist, SwingSeconds, CalcSpeed);
                    }
                    Proj->SetupProjectile(ActiveAnimSkillSlug, GetActorId_Implementation(), TargetActor, CalcSpeed);
                }

                UE_LOG(LogGameplay, Verbose, TEXT("[PlayerAnim] CastRelease: spawned projectile '%s' for skill '%s'"),
                    *ProjClass->GetName(), *ActiveAnimSkillSlug);
            }
        }
        break;
    }

    default:
        break;
    }
}

