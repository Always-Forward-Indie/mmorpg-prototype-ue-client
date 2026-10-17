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

// Called every frame
void ABasicMOB::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	CRASH_GUARD("BasicMOB::Tick");

	// In Tick function, add this check
	if (MOBMovementComponent && MOBData.mobID != 0)
	{
		// Update the mob's moving state from the movement component
		bool bIsNowMoving = MOBMovementComponent->IsMoving();
		if (MOBData.bIsMoving != bIsNowMoving)
		{
			SetMOBIsMoving(bIsNowMoving);
		}
	}

	if (IsValid(AudioComponentSecond))
	{
		if (!MOBData.bIsMoving)
		{
			if (AudioComponentSecond->IsPlaying())
			{
				AudioComponentSecond->Stop();
			}
		}
	}

	// ������ ��������� UI, ���� ���� MobHeadInfo
	if (MobHeadInfo)
	{
		float MaxHealth = 0.0f;
		if (const FAttributeDataStruct* HealthAttr = MOBData.mobAttributes.attributesData.Find(TEXT("max_health")))
		{
			MaxHealth = HealthAttr->attributeValue;
		}

		if ((LastHealth != MOBData.mobCurrentHealth || MOBData.bIsAggressive != LastAggressive || !bUIInitialized) && MOBData.mobID != 0)
		{
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

			LastHealth = MOBData.mobCurrentHealth;
			LastAggressive = MOBData.bIsAggressive;
			bUIInitialized = true;
		}
	}

	UpdateWidgetScale(DeltaTime);
	UpdateWidgetPosition();
}

void ABasicMOB::UpdateWidgetScale(float DeltaTime)
{
	if (!(MobHeadInfo && HeadWidget)) return;

	UWorld* World = GetWorld();
	if (!World) return;

	APlayerController* PC = World->GetFirstPlayerController();
	if (!(PC && PC->PlayerCameraManager)) return;

	const FVector CameraLoc = PC->PlayerCameraManager->GetCameraLocation();
	const float Distance = FVector::Dist(CameraLoc, GetActorLocation());

	float Clamped = FMath::Clamp(Distance, MinDistance, MaxDistance);
	float Factor = 1.0f - ((Clamped - MinDistance) / (MaxDistance - MinDistance));
	float TargetScale = FMath::Clamp(Factor, 0.1f, 1.0f) * widgetScaleFactor;

	CurrentWidgetScale = FMath::FInterpConstantTo(CurrentWidgetScale, TargetScale, DeltaTime, 1.f);
	HeadWidget->SetWidgetScale(CurrentWidgetScale);

	if (FMath::Abs(LastDisplayedWidgetScale - CurrentWidgetScale) > 0.01f ||
		FMath::Abs(LastDisplayedWidgetDistance - Distance) > 100.0f ||
		World->GetTimeSeconds() - LastUpdateTime > 2.0f)
	{
		LastDisplayedWidgetScale = CurrentWidgetScale;
		LastDisplayedWidgetDistance = Distance;
		LastUpdateTime = World->GetTimeSeconds();
	}
}

void ABasicMOB::UpdateWidgetPosition()
{
	if (!MobHeadInfo) return;

	UCapsuleComponent* Capsule = GetCapsuleComponent();
	if (!Capsule) return;

	// �������� ������ �������
	const float CapsuleHalfHeight = Capsule->GetScaledCapsuleHalfHeight();

	// ������� �������� � ��� �������
	const float BaseOffset = 40.f;

	// ���. �������� � ����������� �� ����������
	UWorld* World = GetWorld();
	if (!World) return;

	APlayerController* PC = World->GetFirstPlayerController();
	float DistanceOffset = 0.f;

	if (PC && PC->PlayerCameraManager)
	{
		const float Distance = FVector::Dist(PC->PlayerCameraManager->GetCameraLocation(), GetActorLocation());
		DistanceOffset = FMath::Clamp((Distance - 500.f) * 0.05f, 0.f, 40.f); // ������ ���������
	}

	// ������� ��� ��������
	const float FinalZ = CapsuleHalfHeight + BaseOffset + DistanceOffset;
	MobHeadInfo->SetRelativeLocation(FVector(0.f, 0.f, FinalZ));
}

// Called to bind functionality to input
void ABasicMOB::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

