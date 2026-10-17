// Fill out your copyright notice in the Description page of Project Settings.


#include "Gameplay/Players/BasicPlayer.h"
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

void ABasicPlayer::ShowCastBar_Implementation(float CastTime, const FString& SkillName)
{
    // Store cast time for ALL players (including remote).
    // CurrentSwingSeconds = Duration - CastTime is pre-computed in PlaySkillAnimation
    // and drives the projectile speed calculation.  If we skip this for remote players
    // the swing time defaults to full Duration, making the projectile 2-3x too slow.
    CurrentCastTime = CastTime;

    if (playerData.isOtherClient) return;

    bIsCasting = true;

    // Stop any ongoing approach / auto-attack and freeze the character
    StopAutoAttack();
    LockMovement();

    // Gameplay timer �\x80\x94 unlocks movement after CastTime regardless of UI state.
    // This is the authoritative unlock path; HideCastBar_Implementation (called
    // by the server on interrupt/cancel) cancels this timer.
    if (CastTime > 0.0f && GetWorld())
    {
        GetWorld()->GetTimerManager().SetTimer(
            CastBarTimerHandle,
            this, &ABasicPlayer::HideCastBar_Implementation,
            CastTime, false);
    }

    if (UIManager)
    {
        if (UPlayerInterfaceWidget* PIW = UIManager->GetPlayerInterfaceWidget())
        {
            if (UCastBarWidget* CastBar = PIW->GetCastBarWidget())
            {
                CastBar->ShowCastBar(CastTime, SkillName);
            }
        }
    }
}

void ABasicPlayer::HideCastBar_Implementation()
{
    if (playerData.isOtherClient) return;

    CurrentCastTime = 0.0f;

    // Cancel the gameplay timer in case this was called early (interrupt/cancel from server).
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(CastBarTimerHandle);
    }

    if (bIsCasting)
    {
        bIsCasting = false;
        UnlockMovement();
    }

    if (UIManager)
    {
        if (UPlayerInterfaceWidget* PIW = UIManager->GetPlayerInterfaceWidget())
        {
            if (UCastBarWidget* CastBar = PIW->GetCastBarWidget())
            {
                CastBar->HideCastBar();
            }
        }
    }
}













