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

FDateTime ABasicPlayer::StringToTimestamp(const FString& DateTimeString) {
    FDateTime DateTime;
    FDateTime::Parse(DateTimeString, DateTime);
    return DateTime;
}

// get player data is other client
bool ABasicPlayer::GetIsOtherClient()
{
	return playerData.isOtherClient;
}

//get is dead state
bool ABasicPlayer::GetIsDead() const
{
	return playerData.characterData.bIsDead;
}

// get is moving state
bool ABasicPlayer::GetIsMoving() const
{
    return playerData.characterData.bIsMoving;
}

// get current zone name
FString ABasicPlayer::GetCurrentZoneName()
{
	return CurrentZoneName;
}

// get player current HP points
int32 ABasicPlayer::GetPlayerCurrentHPPoints() const
{
    return playerData.characterData.characterCurrentHealth;
}

// get player current MP points
int32 ABasicPlayer::GetPlayerCurrentMPPoints() const
{
    return playerData.characterData.characterCurrentMana;
}

UPlayerAnimInstance* ABasicPlayer::GetPlayerAnimInstance() const
{
    return Cast<UPlayerAnimInstance>(GetMesh()->GetAnimInstance());
}

// Set message data
void ABasicPlayer::SetMessageData(const FMessageDataStruct NewMessageData)
{
	messageData = NewMessageData;
}

void ABasicPlayer::SetCurrentZoneName(const FString& NewZoneName)
{
    CurrentZoneName = NewZoneName;
}

// Set is other client
void ABasicPlayer::SetIsOtherClient(bool bIsOtherClient)
{
    playerData.isOtherClient = bIsOtherClient;
}

// Set client ID
void ABasicPlayer::SetClientID(int32 ID)
{
    playerData.clientId = ID;
}

void ABasicPlayer::SetPlayerTag(const FString& Tag)
{
    Tags.Add(FName(*Tag));
}

// Set client token
void ABasicPlayer::SetClientSecret(FString Secret)
{
    playerData.hash = Secret;
}

// Set character ID
void ABasicPlayer::SetCharacterID(int32 ID)
{
    playerData.characterData.characterId = ID;
}

// Set client login
void ABasicPlayer::SetClientLogin(FString Login)
{
    playerData.clientLogin = Login;
}

// set player class
void ABasicPlayer::SetPlayerClass(FString Class)
{
    playerData.characterData.characterClass = Class;
}

// set player race
void ABasicPlayer::SetPlayerRace(FString Race)
{
    playerData.characterData.characterRace = Race;
}

// set player gender
void ABasicPlayer::SetPlayerGender(FString Gender)
{
    playerData.characterData.characterGender = Gender;
}

// set player name
void ABasicPlayer::SetPlayerName(FString Name)
{
    playerData.characterData.characterName = Name;
}

// set player level
void ABasicPlayer::SetPlayerLevel(int32 Level)
{
    playerData.characterData.characterLevel = Level;
	UpdateExperienceData(); // Update ExperienceManager when level changes
}

void ABasicPlayer::InitialiseNameplate(bool bIsLocal)
{
    if (NameplateComponent)
    {
        NameplateComponent->InitialiseFromCharacterData(playerData.characterData, bIsLocal, false);
    }
}

// set player experience points
void ABasicPlayer::SetPlayerExpPoints(int32 ExpPoints)
{
    playerData.characterData.characterExperiencePoints = ExpPoints;
	UpdateExperienceData(); // Update ExperienceManager when experience changes
}

// set player next level exp
void ABasicPlayer::SetPlayerNextLevelExp(int32 NextLevelExp)
{
	playerData.characterData.characterExpForLevelEnd = NextLevelExp;
	UpdateExperienceData(); // Update ExperienceManager when next level exp changes
}

// Update experience data in ExperienceManager
void ABasicPlayer::UpdateExperienceData()
{
	if (!MyGameInstance || playerData.isOtherClient)
	{
		return; // Only update for local player
	}

	UExperienceManager* ExperienceManager = MyGameInstance->GetExperienceManager();
	if (ExperienceManager && playerData.characterData.characterId > 0)
	{
		FPlayerProgressionStruct UpdatedProgression;
		UpdatedProgression.characterId       = playerData.characterData.characterId;
		UpdatedProgression.currentLevel      = playerData.characterData.characterLevel;
		UpdatedProgression.currentExperience = playerData.characterData.characterExperiencePoints;
		UpdatedProgression.totalExperience   = playerData.characterData.characterExperiencePoints;
		UpdatedProgression.expForNextLevel   = playerData.characterData.characterExpForLevelEnd;
		// characterExpForLevelStart is always kept current by UpdateCharacterDataFromStatsUpdate
		UpdatedProgression.expForCurrentLevel = playerData.characterData.characterExpForLevelStart;
		UpdatedProgression.experienceDebt    = playerData.characterData.characterExperienceDebt;
		UpdatedProgression.bHasPendingLevelUp = false;
		UpdatedProgression.pendingLevelGained = 0;

		ExperienceManager->UpdateCharacterProgression(playerData.characterData.characterId, UpdatedProgression);

		UE_LOG(LogGameplay, Verbose, TEXT("Updated experience data for character %d: Level %d, XP %d [%d-%d]"), 
			playerData.characterData.characterId, 
			UpdatedProgression.currentLevel,
			UpdatedProgression.currentExperience,
			UpdatedProgression.expForCurrentLevel,
			UpdatedProgression.expForNextLevel);
	}
}

// set player current HP points
void ABasicPlayer::SetPlayerCurrentHPPoints(int32 CurrentHPPoints)
{
    playerData.characterData.characterCurrentHealth = CurrentHPPoints;

    if (MyGameInstance && !playerData.isOtherClient)
    {
        if (UPlayerStatsManager* StatsMgr = MyGameInstance->GetPlayerStatsManager())
        {
            FPlayerStatsUpdateStruct Updated = StatsMgr->GetCachedStats();
            if (Updated.characterId > 0)
            {
                Updated.healthCurrent = CurrentHPPoints;
                StatsMgr->ApplyStatsUpdate(Updated);
            }
        }
    }

    // Update HP bar on the nameplate for remote players
    if (playerData.isOtherClient && NameplateComponent)
    {
        const int32 MaxHP = GetMaxHealth_Implementation();
        NameplateComponent->UpdateHealth(CurrentHPPoints, MaxHP);
    }

    // Refresh local HUD immediately so HP bar updates in combat
    if (!playerData.isOtherClient)
    {
        RefreshHUD();
    }
}

// set player current MP points
void ABasicPlayer::SetPlayerCurrentMPPoints(int32 CurrentMPPoints)
{
    playerData.characterData.characterCurrentMana = CurrentMPPoints;

    if (MyGameInstance && !playerData.isOtherClient)
    {
        if (UPlayerStatsManager* StatsMgr = MyGameInstance->GetPlayerStatsManager())
        {
            FPlayerStatsUpdateStruct Updated = StatsMgr->GetCachedStats();
            if (Updated.characterId > 0)
            {
                Updated.manaCurrent = CurrentMPPoints;
                StatsMgr->ApplyStatsUpdate(Updated);
            }
        }
    }

    // Refresh local HUD immediately so MP bar updates in combat
    if (!playerData.isOtherClient)
    {
        RefreshHUD();
    }
}

// set player attributes
void ABasicPlayer::SetPlayerAttributes(TMap<FString, FAttributeDataStruct> Attributes)
{
	playerData.characterData.characterAttributes.attributesData = Attributes;

	//debug player attributes
	for (auto& Elem : Attributes)
	{
		FString Key = Elem.Key;
		FAttributeDataStruct Value = Elem.Value;
		UE_LOG(LogGameplay, Warning, TEXT("Player Attribute Key: %s, Value: %d"), *Key, Value.attributeValue);
	}

}

// set player coordinates
void ABasicPlayer::SetCoordinates(double x, double y, double z, double rotZ, bool bIsFalling)
{
    playerData.characterData.characterPosition.positionX = x;
    playerData.characterData.characterPosition.positionY = y;
    playerData.characterData.characterPosition.positionZ = z;
    playerData.characterData.characterPosition.rotationZ = rotZ;

    if (playerData.isOtherClient)
    {
        bRemoteIsInAir = bIsFalling;
    }

    const FVector NewTarget(x, y, z);

    // First packet ever received: hard-snap the actor to the authoritative position
    // so it never interpolates from the uninitialized (0,0,0) origin.
    if (playerData.isOtherClient && !bHasReceivedFirstPosition)
    {
        bHasReceivedFirstPosition = true;
        SetActorLocation(NewTarget, false, nullptr, ETeleportType::TeleportPhysics);
        SetActorRotation(FRotator(0.0, rotZ, 0.0));
        LastReceivedPosition      = NewTarget;
        TargetReceivedPosition    = NewTarget;
        LastReceivedRotation      = FRotator(0.0, rotZ, 0.0);
        TargetReceivedRotation    = FRotator(0.0, rotZ, 0.0);
        TimeSinceLastPositionUpdate = 0.0f;
        return;
    }

    // Measure actual inter-packet delivery time and use it as the lerp window.
    // This makes interpolation robust to variable server tick rates and network jitter.
    if (playerData.isOtherClient && TimeSinceLastPositionUpdate > 0.01f)
    {
        // After a long idle pause (player stood still) reset the adaptive interval
        // to the expected packet rate. Without this, the first movement packet
        // inherits a 500ms lerp window (clamped from the idle gap), which makes
        // the second packet arrive before the lerp completes and causes a
        // forward micro-snap at the very start of movement.
        if (TimeSinceLastPositionUpdate > 0.5f)
        {
            ServerPositionUpdateInterval = 0.1f;
        }
        else
        {
            // Clamp to a sane range (50ms - 500ms) to ignore the very first packet
            // and any massive gaps caused by the player standing still.
            const float MeasuredInterval = FMath::Clamp(TimeSinceLastPositionUpdate, 0.05f, 0.5f);
            // Exponential moving average: 80% old value, 20% new measurement
            ServerPositionUpdateInterval = ServerPositionUpdateInterval * 0.8f + MeasuredInterval * 0.2f;
        }
    }    // Estimate 2D speed / direction and 3D dead-reckoning velocity from
    // consecutive server positions.  RemoteSpeed / RemoteDirection are raw
    // instantaneous values for animation; RemoteVelocity is applied every tick
    // via open-loop integration.  Smoothing runs in UpdateRemotePlayerMovement.
    if (playerData.isOtherClient && ServerPositionUpdateInterval > 0.0f)
    {
        const FVector Delta3D = NewTarget - TargetReceivedPosition;
        const float   Dist2D  = Delta3D.Size2D();

        // Large displacement (> 500 cm) almost certainly means a server-side teleport
        // (e.g. respawn, warp).  Hard-snap the actor and zero out all animation-speed
        // state so the character never plays a running animation in the new position
        // before the first real movement packet arrives.
        static constexpr float TeleportThreshold = 500.0f;
        if (Dist2D > TeleportThreshold)
        {
            SetActorLocation(NewTarget, false, nullptr, ETeleportType::TeleportPhysics);
            SetActorRotation(FRotator(0.0, rotZ, 0.0));
            LastReceivedPosition       = NewTarget;
            TargetReceivedPosition     = NewTarget;
            LastReceivedRotation       = FRotator(0.0, rotZ, 0.0);
            TargetReceivedRotation     = FRotator(0.0, rotZ, 0.0);
            RemoteVelocity             = FVector::ZeroVector;
            RemoteSpeed                = 0.0f;
            SmoothedRemoteSpeed        = 0.0f;
            RemoteDirection            = 0.0f;
            bRemoteIsMoving            = false;
            RemoteIdleTime             = 0.0f;
            TimeSinceLastPositionUpdate = 0.0f;
            return;
        }

        const FVector ServerVel = Delta3D / ServerPositionUpdateInterval;
        const FVector Gap       = TargetReceivedPosition - GetActorLocation();
        FVector GapCorrection   = Gap * (PositionBlendStrength / ServerPositionUpdateInterval);
        // Cap correction to BlendStrength fraction of base speed so gap-correction
        // never dominates — prevents forward/backward oscillation on micro-steps.
        const float MaxCorr = ServerVel.Size() * PositionBlendStrength;
        if (GapCorrection.SizeSquared() > MaxCorr * MaxCorr)
        {
            GapCorrection = GapCorrection.GetClampedToMaxSize(MaxCorr);
        }
        RemoteVelocity = ServerVel + GapCorrection;
        RemoteVelocity.Z = 0.0f;
        RemoteSpeed = Dist2D / ServerPositionUpdateInterval;

        if (Dist2D > 1.0f)
        {
            bRemoteIsMoving = true;
            RemoteIdleTime  = 0.0f;

            const FVector Delta2D = FVector(Delta3D.X, Delta3D.Y, 0.0f).GetSafeNormal();

            // Use TargetReceivedRotation (server-authoritative facing) instead of
            // GetActorForwardVector(). The actor rotation is still mid-interpolation
            // when a new packet arrives, so GetActorForwardVector() is unstable and
            // produces wrong direction values near ±180° (backward movement).
            const FQuat   FacingQuat    = TargetReceivedRotation.Quaternion();
            const FVector StableForward = FacingQuat.GetForwardVector();
            const FVector StableRight   = FacingQuat.GetRightVector();
            RemoteDirection = FMath::RadiansToDegrees(
                FMath::Atan2(FVector::DotProduct(Delta2D, StableRight),
                             FVector::DotProduct(Delta2D, StableForward)));
        }
        else
        {
            RemoteVelocity  = FVector::ZeroVector;
            RemoteSpeed     = 0.0f;
            RemoteDirection = 0.0f;
        }
    }

    // Update the server-authoritative target.  Dead reckoning applies the
    // gap-corrected velocity every tick; the gap term in RemoteVelocity above
    // closes the one-interval lag in ~0.5 s without per-frame SetActorLocation
    // jerking.  Gap can be negative (overshoot from jitter) — velocity slightly
    // decreases, actor slows down, no back-step.
    TargetReceivedPosition = NewTarget;

    LastReceivedRotation = GetActorRotation();
    TargetReceivedRotation = FRotator(0, rotZ, 0);

    TimeSinceLastPositionUpdate = 0.0f;
}



// Play sound
