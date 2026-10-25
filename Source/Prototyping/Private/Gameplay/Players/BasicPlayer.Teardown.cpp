
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


// Called when the actor is being destroyed
void ABasicPlayer::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // Clear any pending timers to avoid use-after-free in lambdas
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(UIInitTimerHandle);
        GetWorld()->GetTimerManager().ClearTimer(UIInitFallbackTimerHandle);
        GetWorld()->GetTimerManager().ClearTimer(AutoAttackRetryTimerHandle);
        GetWorld()->GetTimerManager().ClearTimer(HitPointTimerHandle);
        GetWorld()->GetTimerManager().ClearTimer(CastBarTimerHandle);
    }

    // For remote players: remove the equipment visual delegate binding.
    // This is a safety net вЂ” RemovePlayerData() does it first, but if the actor
    // is destroyed by other means (e.g. level unload) this prevents dangling callbacks.
    if (playerData.isOtherClient && MyGameInstance && EquipmentVisualComponent)
    {
        if (UEquipmentManager* EqMgr = MyGameInstance->GetEquipmentManager())
        {
            EqMgr->OnRemoteEquipmentStateReceivedDelegate.RemoveDynamic(
                EquipmentVisualComponent, &UEquipmentVisualComponent::HandleRemoteEquipmentState);
        }
    }

    // Unregister from actor registry
    if (UMyGameInstance* GameInstance = Cast<UMyGameInstance>(GetGameInstance()))
    {
        const int32 ActorId = GetActorId_Implementation();
        if (GameInstance->MOBManager && ActorId > 0)
        {
            GameInstance->MOBManager->UnregisterPlayer(ActorId);
        }
    }

    // Unregister from combat system
    if (UMyGameInstance* GameInstance = Cast<UMyGameInstance>(GetGameInstance()))
    {
        if (UCombatSystemManager* CombatManager = GameInstance->GetCombatSystemManager())
        {
            if (IsValid(this) && GetActorId_Implementation() > 0)
            {
                TScriptInterface<ICombatable> CombatableInterface;
                CombatableInterface.SetObject(this);
                CombatableInterface.SetInterface(this);
                
                CombatManager->UnregisterCombatable(CombatableInterface);
                UE_LOG(LogGameplay, Verbose, TEXT("Player %d unregistered from combat system"), GetActorId_Implementation());
            }
        }
    }

    // Unsubscribe from PlayerStatsManager to prevent dangling delegate callbacks.
    if (!playerData.isOtherClient && MyGameInstance)
    {
        if (UPlayerStatsManager* StatsMgr = MyGameInstance->GetPlayerStatsManager())
        {
            StatsMgr->OnStatsUpdated.RemoveDynamic(this, &ABasicPlayer::HandleStatsManagerUpdate);
        }
    }

    Super::EndPlay(EndPlayReason);
}
