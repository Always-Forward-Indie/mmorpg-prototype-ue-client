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

// --- IWorldInteractable interface --------------------------------------------
EInteractableType ABasicMOB::GetInteractableType() const
{
    if (!MOBData.bIsDead)              return EInteractableType::MOB_Alive;
    if (!MOBData.bHasBeenHarvested)    return EInteractableType::MOB_Harvestable;
    return EInteractableType::MOB_Harvested;
}

FText ABasicMOB::GetInteractableDisplayName() const
{
    return FText::FromString(
        FString::Printf(TEXT("%s  [Lv.%d]"), *GetMobName(), GetMOBLevel()));
}

bool ABasicMOB::CanInteract() const
{
    // A mob is always "interactable": alive = attack candidate, dead = harvest candidate.
    return true;
}

// Force update UI immediately
void ABasicMOB::MOBTextFaceToCamera(UTextRenderComponent* MobTextComponent)
{
	APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();
	if (PlayerController)
	{
		APlayerCameraManager* CameraManager = PlayerController->PlayerCameraManager;
		FVector CameraLocation = CameraManager->GetCameraLocation();

		// Assuming MyTextRenderer is your TextRenderComponent
		FVector TextLocation = MobTextComponent->GetComponentLocation();

		FRotator LookAtRotation = (CameraLocation - TextLocation).Rotation();
		MobTextComponent->SetWorldRotation(LookAtRotation);
	}
}

void ABasicMOB::ForceUpdateUI()
{
	if (MobHeadInfo)
	{
		float MaxHealth = 0.0f;
		if (const FAttributeDataStruct* HealthAttr = MOBData.mobAttributes.attributesData.Find(TEXT("max_health")))
		{
			MaxHealth = HealthAttr->attributeValue;
		}

		// Resolve localised name from slug; fall back to server-provided name
		FString DisplayName = MOBData.mobName;
		if (!MOBData.mobSlug.IsEmpty())
		{
			if (UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance()))
			{
				if (ULocalizationSubsystem* Loc = GI->GetSubsystem<ULocalizationSubsystem>())
				{
					FText Localized = Loc->GetMobDisplayName(MOBData.mobSlug);
					if (!Localized.IsEmpty())
						DisplayName = Localized.ToString();
				}
			}
		}

		MobHeadInfo->UpdateInfo(
			MOBData.mobCurrentHealth,
			MaxHealth,
			DisplayName,
			MOBData.mobLevel,
			MOBData.bIsAggressive
		);

		// ��������� ��������� �������� � �������� UI ��� ������������������
		LastHealth = MOBData.mobCurrentHealth;
		LastAggressive = MOBData.bIsAggressive;
		bUIInitialized = true;
		
		// Note: widget scale is managed per-tick by UpdateWidgetScale();
		// do NOT override it here to avoid 1-frame size spikes on combat events.
	}
}

