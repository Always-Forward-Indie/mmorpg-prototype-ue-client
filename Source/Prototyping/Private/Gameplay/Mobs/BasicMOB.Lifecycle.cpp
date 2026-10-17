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


// Sets default values
ABasicMOB::ABasicMOB()
{
	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	MOBMovementComponent = CreateDefaultSubobject<UMOBMovementComponent>(TEXT("MOBMovementComponent"));

	// Rotation is driven entirely by MOBMovementComponent::HandleRotation.
	// Disable all UCharacterMovementComponent rotation behaviour so it never
	// overwrites the yaw we set via SetActorRotation each tick.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw   = false;
	bUseControllerRotationRoll  = false;
	if (UCharacterMovementComponent* CMC = GetCharacterMovement())
	{
		CMC->bOrientRotationToMovement = false;
		CMC->bUseControllerDesiredRotation = false;
		CMC->RotationRate = FRotator(0.f, 0.f, 0.f);
		// Disable pawn-vs-pawn physics pushing entirely.
		// Server is authoritative � client-side push forces only cause jitter.
		CMC->bEnablePhysicsInteraction = false;
	}

	// Mobs must not physically push or block each other.
	// The server is authoritative over all positions, so client-side pawn-vs-pawn
	// physics would only produce jitter and visual overlap fights.
	// Keep WorldStatic/WorldDynamic blocking so ground traces and line-of-sight
	// checks still work correctly.
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		// Mobs must not physically push or block each other (server-authoritative positions).
		Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);

		// Projectiles use ObjectType=WorldDynamic + OverlapAllDynamic profile.
		// The default Pawn profile responds to WorldDynamic with Block, which means
		// the overlap event never fires on the projectile's CollisionSphere.
		// Setting Overlap here ensures OnSphereBeginOverlap triggers correctly.
		Capsule->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);

		// Prevent mob capsules from compressing the player camera spring arm.
		// The camera ProbeChannel is ECC_Camera; mobs must not block it.
		Capsule->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

		// Cursor hover trace runs on ECC_Visibility.  Ensure the capsule blocks it
		// so the mouse-over / click system can detect mobs regardless of
		// whatever collision profile the Blueprint might set as the default.
		Capsule->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	}

	// The skeletal mesh component has its own collision settings separate from the capsule.
	// If the project's CharacterMesh profile blocks ECC_Camera, the spring arm probe will
	// collapse to near-zero when a mob overlaps with the player (which can happen now that
	// ECC_Pawn is Ignore). Explicitly silence the mesh on the camera channel.
	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		MeshComp->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	}



	// ��������� ��������� ��� ����������� ����� ��������
	MobHeadInfo = CreateDefaultSubobject<UMOBHeadInfo>(TEXT("MobHeadInfo"));
	MobHeadInfo->SetupAttachment(RootComponent);
	MobHeadInfo->SetWidgetSpace(EWidgetSpace::Screen);
	MobHeadInfo->SetDrawAtDesiredSize(false);
	MobHeadInfo->SetDrawSize(FVector2D(160.0f, 60.0f));
	MobHeadInfo->SetVisibility(false);

	AudioComponentMain = CreateDefaultSubobject<UAudioComponent>(TEXT("MobMainAudio"));
	AudioComponentMain->SetupAttachment(RootComponent);
	AudioComponentMain->bAutoActivate = false;

	AudioComponentSecond = CreateDefaultSubobject<UAudioComponent>(TEXT("MobSecondAudio"));
	AudioComponentSecond->SetupAttachment(RootComponent);
	AudioComponentSecond->bAutoActivate = false;

	// Cursor target-indicator decal (floor circle).
	// Managed entirely by UCursorInteractionComponent � no editor setup needed.
	TargetDecal = CreateDefaultSubobject<UTargetDecalComponent>(TEXT("TargetDecal"));
	TargetDecal->SetupAttachment(RootComponent);
}

// Called when the game starts or when spawned
