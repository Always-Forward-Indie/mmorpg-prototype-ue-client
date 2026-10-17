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

void ABasicPlayer::StartMovementSimulation()
{
    bSimulateMovement = true;
}

void ABasicPlayer::StopMovementSimulation()
{
    bSimulateMovement = false;
}

void ABasicPlayer::UpdateMovementSimulation(float DeltaTime)
{
    FVector CurrentLocation = GetActorLocation();
    float DistanceToTarget = FVector::Dist(CurrentLocation, TargetPosition);

    if (DistanceToTarget < 50.f) // Threshold to decide when to pick a new target
    {
        float NewX = FMath::FRandRange(SquareCenter.X - SideLength / 2, SquareCenter.X + SideLength / 2);
        float NewY = FMath::FRandRange(SquareCenter.Y - SideLength / 2, SquareCenter.Y + SideLength / 2);
        TargetPosition = FVector(NewX, NewY, 90.0f); // Assuming Z is constant for this example

        FVector MovementDirection = (TargetPosition - CurrentLocation).GetSafeNormal();
        if (!MovementDirection.IsNearlyZero())
        {
            // Calculate the desired rotation based on the movement direction
            FRotator DesiredRotation = MovementDirection.Rotation();
            DesiredRotation.Pitch = 0.0f; // Keep the pitch level, adjust if your game needs vertical aiming
            DesiredRotation.Roll = 0.0f;  // Typically, you don't need to roll the character

            playerData.characterData.characterPosition.rotationZ = DesiredRotation.Yaw;
        }


        // Logging for debugging
        UE_LOG(LogTemp, Warning, TEXT("New target position: %s"), *TargetPosition.ToString());
    }

    // Move towards TargetPosition if not already close
    if (DistanceToTarget > 1.0f) // Use a small threshold to avoid jittering at the target location
    {
        FVector Direction = (TargetPosition - CurrentLocation).GetSafeNormal();
        float MovementStep = MoveSpeed * 2 * DeltaTime; // Adjust as needed
        //float MovementStep = MoveSpeed* GetWorld()->GetDeltaSeconds();
        FVector NewPosition = CurrentLocation + Direction * MovementStep;
        SetActorLocation(NewPosition);
    }
}

void ABasicPlayer::OnQuestJournalToggle()
{
    if (playerData.isOtherClient)
    {
        return;
    }

    if (UIManager)
    {
        UIManager->ToggleQuestJournal();
    }
}

void ABasicPlayer::OnEquipmentToggle()
{
    if (playerData.isOtherClient)
    {
        return;
    }

    if (UIManager)
    {
        UIManager->ToggleEquipment();
    }
}

void ABasicPlayer::OnAltCursorToggle()
{
    if (playerData.isOtherClient) return;
    if (UIManager) UIManager->ToggleAltCursor();
}

void ABasicPlayer::OnStatsToggle()
{
    if (playerData.isOtherClient) return;
    if (UIManager) UIManager->TogglePlayerStats();
}

void ABasicPlayer::OnBestiaryToggle()
{
    if (playerData.isOtherClient) return;
    if (UIManager) UIManager->ToggleBestiary();
}

void ABasicPlayer::OnTitlesToggle()
{
    if (playerData.isOtherClient) return;
    if (UIManager) UIManager->ToggleTitles();
}

void ABasicPlayer::OnReputationToggle()
{
    if (playerData.isOtherClient) return;
    if (UIManager) UIManager->ToggleReputation();
}

void ABasicPlayer::OnEmoteListToggle()
{
    if (playerData.isOtherClient) return;
    if (UIManager) UIManager->ToggleEmoteList();
}

