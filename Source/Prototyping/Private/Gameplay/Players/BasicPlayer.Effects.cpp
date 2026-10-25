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

// Convert ESkillSchool to EDamageType for FloatingCombatTextManager
static EDamageType SchoolToDamageType(ESkillSchool School)
{
    switch (School)
    {
    case ESkillSchool::Fire:    return EDamageType::Fire;
    case ESkillSchool::Ice:     return EDamageType::Ice;
    default:                    return EDamageType::Physical;
    }
}


void ABasicPlayer::ShowDamageEffect_Implementation(int32 Damage, bool bIsCritical, ESkillSchool School, bool bIsMissed, bool bIsBlocked, const FString& SkillSlug)
{
    UE_LOG(LogGameplay, Verbose, TEXT("Player %d taking %d damage (Critical: %s, School: %s, Missed: %s, Blocked: %s, Skill: %s)"),
        GetActorId_Implementation(), Damage,
        bIsCritical ? TEXT("true") : TEXT("false"), *UEnum::GetValueAsString(School),
        bIsMissed ? TEXT("true") : TEXT("false"),
        bIsBlocked ? TEXT("true") : TEXT("false"),
        *SkillSlug);

    // Trigger hit-react animation вЂ” only on actual hit, not miss
    if (!bIsMissed)
    {
        if (UPlayerAnimInstance* AnimInst = GetPlayerAnimInstance())
        {
            AnimInst->NotifyHit();
        }

        // Interrupt emote on damage
        if (EmoteComponent)
        {
            EmoteComponent->NotifyDamageReceived();
        }
    }

    // Play hit received sound (generic player grunt / armor clank)
    if (!bIsMissed)
    {
        if (const FEntityAudioProfile* Profile = GetAudioProfile())
        {
            PlayEventSound(Profile->HitReceived);
        }
    }

    // --- Hit sound + hit particle from the skill that caused the damage ---
    bool bHitSoundPlayed = false;
    if (MyGameInstance)
    {
        if (USkillDefinitionRepository* Repo = MyGameInstance->GetSkillDefinitionRepository())
        {
            const FSkillDefinitionData& Def = Repo->GetDefinition(SkillSlug);

            // --- Impact sound: WeaponImpactType ? ArmorMaterialType lookup ---
            // ArmorMaterialType is read from the chest slot item in DT_ItemVisuals.
            if (Def.WeaponImpactType != NAME_None)
            {
                if (UDataTable* ImpactTable = MyGameInstance->GetImpactSoundsTable())
                {
                    // Resolve ArmorMaterialType from the equipped chest piece via DT_ItemVisuals
                    FName ArmorMat = NAME_None;
                    if (UEquipmentManager* EqMgr = MyGameInstance->GetEquipmentManager())
                    {
                        FEquipmentSlotData ChestSlot = EqMgr->GetSlot(TEXT("chest"));
                        if (ChestSlot.bIsOccupied && !ChestSlot.itemSlug.IsEmpty())
                        {
                            if (UDataTable* VisualsTable = MyGameInstance->GetItemVisualsDataTable())
                            {
                                FName VisualKey = FName(*ChestSlot.itemSlug);
                                if (const FItemVisualData* VisRow = VisualsTable->FindRow<FItemVisualData>(VisualKey, TEXT("")))
                                {
                                    ArmorMat = VisRow->ArmorMaterialType;
                                }
                            }
                        }
                    }

                    // Fallback: no chest armor = flesh
                    if (ArmorMat == NAME_None)
                    {
                        ArmorMat = FName(TEXT("flesh"));
                    }

                    FName ImpactKey = FName(*FString::Printf(TEXT("%s_%s"),
                        *Def.WeaponImpactType.ToString(), *ArmorMat.ToString()));

                    if (const FImpactSoundData* ImpactRow = ImpactTable->FindRow<FImpactSoundData>(ImpactKey, TEXT("")))
                    {
                        if (ImpactRow->ImpactSounds.Num() > 0)
                        {
                            int32 Idx = FMath::RandRange(0, ImpactRow->ImpactSounds.Num() - 1);
                            if (USoundBase* ImpactSound = ImpactRow->ImpactSounds[Idx].LoadSynchronous())
                            {
                                USoundAttenuation* Atn = nullptr;
                                if (const FEntityAudioProfile* Prof = GetAudioProfile())
                                {
                                    if (!Prof->DefaultAttenuation.IsNull())
                                        Atn = Prof->DefaultAttenuation.LoadSynchronous();
                                }
                                SpawnSFXAttached(this, ImpactSound, GetActorLocation(), 1.0f, Atn);
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

            // Fallback: generic hitSound from skill definition
            if (!bHitSoundPlayed && !Def.hitSound.IsNull())
            {
                if (USoundBase* Sound = Def.hitSound.LoadSynchronous())
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

            // Niagara hit effect
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
                    USoundAttenuation* Atn = nullptr;
                    if (const FEntityAudioProfile* Prof = GetAudioProfile())
                    {
                        if (!Prof->DefaultAttenuation.IsNull())
                            Atn = Prof->DefaultAttenuation.LoadSynchronous();
                    }
                    SpawnSFXAttached(this, CritSnd, GetActorLocation(), 1.0f, Atn);
                }
            }
        }
    }

    // --- Local player only: Camera Shake + Screen Flash + Hit Stop on self ---
    // Only when the attack actually connected (not a miss).
    if (!playerData.isOtherClient && !bIsMissed)
    {
        // Camera shake
        if (UIManager)
        {
            const float ShakeIntensity = bIsCritical ? 1.0f : 0.5f;
            UIManager->PlayCombatCameraShake(ShakeIntensity);
            UIManager->ShowDamageScreenFlash();
        }
    }

    // --- Hit Stop: freeze this actor briefly so the impact feels weighty ---
    // Not on miss вЂ” missing doesn't interrupt the actor's flow.
    if (!bIsMissed)
    {
        if (UWorld* W = GetWorld())
        {
            CustomTimeDilation = 0.0f;
            FTimerHandle HitStopTimer;
            TWeakObjectPtr<ABasicPlayer> WeakSelf(this);
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

void ABasicPlayer::ShowHealingEffect_Implementation(int32 Healing, const FString& SkillSlug)
{
    UE_LOG(LogGameplay, Verbose, TEXT("Player %d healed for %d (skill: %s)"), GetActorId_Implementation(), Healing, *SkillSlug);

    // Look up skill definition for sound and VFX
    if (MyGameInstance)
    {
        if (USkillDefinitionRepository* Repo = MyGameInstance->GetSkillDefinitionRepository())
        {
            const FSkillDefinitionData& Def = Repo->GetDefinition(SkillSlug);

            // Heal sound from DataTable (overrides profile HealReceived if set)
            if (!Def.healSound.IsNull())
            {
                PlayEventSound(Def.healSound);
            }
            else
            {
                // Generic fallback sound from entity audio profile
                if (const FEntityAudioProfile* Profile = GetAudioProfile())
                {
                    PlayEventSound(Profile->HealReceived);
                }
            }

            // Heal Niagara VFX at HitSocket (green sparkles on target)
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
        else
        {
            if (const FEntityAudioProfile* Profile = GetAudioProfile())
            {
                PlayEventSound(Profile->HealReceived);
            }
        }
    }
    else
    {
        if (const FEntityAudioProfile* Profile = GetAudioProfile())
        {
            PlayEventSound(Profile->HealReceived);
        }
    }

    // Floating green heal number
    if (UIManager)
    {
        if (UFloatingCombatTextManager* FCT = UIManager->GetFCTManager())
        {
            FCT->ShowDamage(GetCombatPosition_Implementation(), static_cast<float>(Healing), false, EDamageType::Heal, true);
        }

        // Screen flash only for local players
        if (!playerData.isOtherClient)
        {
            UIManager->ShowHealScreenFlash();
        }
    }
}

void ABasicPlayer::ShowManaRestoreEffect_Implementation(int32 ManaRestored)
{
    if (ManaRestored <= 0) return;

    UE_LOG(LogGameplay, Verbose, TEXT("Player %d mana restored by %d"), GetActorId_Implementation(), ManaRestored);

    if (UIManager)
    {
        if (UFloatingCombatTextManager* FCT = UIManager->GetFCTManager())
        {
            FCT->ShowDamage(GetCombatPosition_Implementation(), static_cast<float>(ManaRestored), false, EDamageType::ManaRegen, true);
        }
    }
}

void ABasicPlayer::ShowBuffEffect_Implementation(const FAppliedEffectData& Effect)
{
    UE_LOG(LogGameplay, Verbose, TEXT("Player %d received %s effect: %s (Value: %d, Duration: %.1f)"), 
        GetActorId_Implementation(), *Effect.effectType, *Effect.effectName, Effect.value, Effect.duration);

    if (!MyGameInstance) return;

    UDataTable* EffectTable = MyGameInstance->GetEffectDefinitionTable();
    if (!EffectTable) return;

    FName RowKey = FName(*Effect.effectName);
    const FEffectDefinitionRow* Row = EffectTable->FindRow<FEffectDefinitionRow>(RowKey, TEXT(""));
    if (!Row) return;

    // Play apply sound
    if (!Row->ApplySound.IsNull())
    {
        if (USoundBase* Snd = Row->ApplySound.LoadSynchronous())
        {
            USoundAttenuation* Atn = nullptr;
            if (const FEntityAudioProfile* Prof = GetAudioProfile())
            {
                if (!Prof->DefaultAttenuation.IsNull())
                    Atn = Prof->DefaultAttenuation.LoadSynchronous();
            }
            SpawnSFXAttached(this, Snd, GetActorLocation(), 1.0f, Atn);
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

void ABasicPlayer::ShowDeathScreen()
{
    if (playerData.isOtherClient) return;

    if (UIManager)
    {
        UIManager->ShowDeathScreen(0);
    }
}

void ABasicPlayer::HideDeathScreen()
{
    if (UIManager)
    {
        UIManager->HideDeathScreen();
    }
}

void ABasicPlayer::OnRespawnClicked()
{
    // Only the local player can request a respawn, and only when dead
    if (playerData.isOtherClient || !playerData.characterData.bIsDead)
    {
        return;
    }

    if (!MyGameInstance)
    {
        UE_LOG(LogGameplay, Error, TEXT("BasicPlayer: OnRespawnClicked - MyGameInstance is null"));
        return;
    }

    UPlayerManager* PM = MyGameInstance->GetPlayerManager();
    if (!PM)
    {
        UE_LOG(LogGameplay, Error, TEXT("BasicPlayer: OnRespawnClicked - PlayerManager is null"));
        return;
    }

    FClientDataStruct ClientData = MyGameInstance->GetCurrentClientData();
    PM->SendRespawnRequest(ClientData);
    UE_LOG(LogGameplay, Warning, TEXT("BasicPlayer: Respawn requested for character %d"),
        playerData.characterData.characterId);
}

