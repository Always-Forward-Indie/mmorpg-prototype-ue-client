#include "Gameplay/NPCs/BasicNPC.h"
#include "Gameplay/NPCs/BasicNPC.h"
#include "Gameplay/Interaction/TargetDecalComponent.h"
#include "Gameplay/NPCs/NPCAnimInstance.h"
#include "Gameplay/NPCs/NPCAmbientSpeechComponent.h"
#include "Gameplay/NPCs/AmbientSpeechManager.h"
#include "Animation/AnimMontage.h"
#include "Components/CapsuleComponent.h"
#include "Components/AudioComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "CrashDiagnostics.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Gameplay/UI/NPCNameplateComponent.h"
#include "MyGameInstance.h"
#include "Audio/AudioManager.h"
#include "Data/EntityAudioRepository.h"
#include "GameFramework/Pawn.h"

// Sets default values
ABasicNPC::ABasicNPC()
{
	// Set this character to call Tick() every frame
	PrimaryActorTick.bCanEverTick = true;

	// Prevent NPC capsules from compressing the player camera spring arm.
	// The camera ProbeChannel is ECC_Camera; NPCs must not block it.
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
		// NPCs must not physically block the player — server is authoritative over NPC positions.
		// Targeting still works via dot-product + LOS fallback in BasicPlayer::CheckForNPC.
		Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		// Projectiles use ObjectType=WorldDynamic + OverlapAllDynamic — keep Overlap so hits register.
		Capsule->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);

		// Cursor hover trace runs on ECC_Visibility.  Ensure the capsule blocks it
		// so the mouse-over / click system can detect NPCs regardless of Blueprint defaults.
		Capsule->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

		// QueryOnly: keeps trace detection (mouse-over, visibility) but removes the capsule
		// from physics overlap resolution entirely, so the player's depenetration logic
		// cannot push the NPC upward when they spawn at the same location.
		Capsule->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	}

	// The skeletal mesh component has its own collision settings separate from the capsule.
	// If the project's CharacterMesh profile blocks ECC_Camera, the spring arm probe will
	// collapse when an NPC stands between the camera and the player.
	// Explicitly silence the mesh on the camera channel (mirrors the fix in BasicMOB.cpp).
	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		MeshComp->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
		// Mesh also must not participate in physics overlap resolution.
		MeshComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	}

	// NPCs are server-driven and must never move on the client.
	// DefaultLandMovementMode = MOVE_None ensures the CMC starts frozen even during
	// the one tick that may fire between PostInitializeComponents and BeginPlay.
	// Zero depenetration distances prevent the CMC from resolving overlaps with pawns
	// even if something manages to call SafeMoveUpdatedComponent on it.
	if (UCharacterMovementComponent* CMC = GetCharacterMovement())
	{
		CMC->bEnablePhysicsInteraction = false;
		CMC->DefaultLandMovementMode = MOVE_None;
		CMC->GravityScale = 0.0f;
		CMC->MaxDepenetrationWithGeometry = 0.0f;
		CMC->MaxDepenetrationWithGeometryAsProxy = 0.0f;
		CMC->MaxDepenetrationWithPawn = 0.0f;
		CMC->MaxDepenetrationWithPawnAsProxy = 0.0f;
	}

	// Initialize audio components
	AudioComponentMain = CreateDefaultSubobject<UAudioComponent>(TEXT("NPCMainAudio"));
	AudioComponentMain->SetupAttachment(RootComponent);
	AudioComponentMain->bAutoActivate = false;

	AudioComponentSecond = CreateDefaultSubobject<UAudioComponent>(TEXT("NPCSecondAudio"));
	AudioComponentSecond->SetupAttachment(RootComponent);
	AudioComponentSecond->bAutoActivate = false;

	// Create nameplate component - registers with central NameplateManager
	NPCNameplateComponent = CreateDefaultSubobject<UNPCNameplateComponent>(TEXT("NPCNameplate"));

	// Create ambient speech component
	AmbientSpeechComponent = CreateDefaultSubobject<UNPCAmbientSpeechComponent>(TEXT("AmbientSpeech"));

	// Cursor target-indicator decal (floor circle).
	TargetDecal = CreateDefaultSubobject<UTargetDecalComponent>(TEXT("TargetDecal"));
	TargetDecal->SetupAttachment(RootComponent);

	// Set default values
	MinDistance = 500.0f;
	MaxDistance = 2000.0f;
	widgetScaleFactor = 1.0f;
	bUIInitialized = false;
	LastHealth = -1;
	LastMana = -1;
	CurrentWidgetScale = 1.0f;
	LastUpdateTime = 0.0f;
}

void ABasicNPC::BeginPlay()
{
	Super::BeginPlay();

	// NPCs are server-positioned and never move on the client.
	// Disable the CMC to prevent depenetration logic from pushing
	// the NPC upward when a player spawns on the same spot.
	if (UCharacterMovementComponent* CMC = GetCharacterMovement())
	{
		CMC->SetMovementMode(MOVE_None);
	}

	// Blueprint CDO properties are applied after the C++ constructor, so any collision
	// preset saved in the Blueprint would override what we set there. Re-apply QueryOnly
	// here (after Super::BeginPlay) to guarantee it is always in effect at runtime.
	// QueryOnly keeps visibility/click traces working but removes the capsule from all
	// physics-simulation overlap resolution, so player depenetration cannot push the NPC.
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	}
	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		MeshComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	}

	// Route NPC audio through the SFX SoundClass so the SFX volume slider works
	if (UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance()))
	{
		if (GI->AudioManager && GI->AudioManager->SFXClass)
		{
			if (AudioComponentMain)  { AudioComponentMain->SoundClassOverride  = GI->AudioManager->SFXClass; }
			if (AudioComponentSecond) { AudioComponentSecond->SoundClassOverride = GI->AudioManager->SFXClass; }
		}
	}

	// HeadWidget = Cast<UW_NPCHeadInfoWidget>(NPCHeadInfo->GetUserWidgetObject());
}

void ABasicNPC::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Clear the idle sound timer to prevent callbacks after destruction
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(IdleSoundTimerHandle);
		GetWorld()->GetTimerManager().ClearTimer(IdleAnimTimerHandle);
		GetWorld()->GetTimerManager().ClearTimer(ZCorrectionTimerHandle);
	}

	// Unbind montage delegates so ended callbacks don't fire on a dead object
	IdleEndedDelegate.Unbind();
	ActionEndedDelegate.Unbind();

	Super::EndPlay(EndPlayReason);
}

void ABasicNPC::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	CRASH_GUARD("BasicNPC::Tick");

	// Smoothly rotate toward the player during dialogue
	if (bIsFacingPlayer)
	{
		if (APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
		{
			if (APawn* PlayerPawn = PC->GetPawn())
			{
				const FVector ToPlayer = PlayerPawn->GetActorLocation() - GetActorLocation();
				const FRotator LookRot = FRotator(0.0f, FRotationMatrix::MakeFromX(ToPlayer).Rotator().Yaw, 0.0f);
				const FRotator NewRot = FMath::RInterpTo(GetActorRotation(), LookRot, DeltaTime, 5.0f);
				SetActorRotation(NewRot);
			}
		}
	}
	else if (bIsRestoringRotation)
	{
		const FRotator NewRot = FMath::RInterpTo(GetActorRotation(), OriginalRotation, DeltaTime, 3.0f);
		SetActorRotation(NewRot);
		if (NewRot.Equals(OriginalRotation, 0.5f))
		{
			SetActorRotation(OriginalRotation);
			bIsRestoringRotation = false;
		}
	}

	// Update UI if initialized
	// if (NPCHeadInfo && bUIInitialized)
	// {
	//	// Check if health/mana changed and update UI
	//	int32 CurrentHealth = NPCData.stats.health.current;
	//	int32 CurrentMana = NPCData.stats.mana.current;

	//	if (LastHealth != CurrentHealth || LastMana != CurrentMana)
	//	{
	//		LastHealth = CurrentHealth;
	//		LastMana = CurrentMana;
	//		ForceUpdateUI();
	//	}

	//	UpdateWidgetScale(DeltaTime);
	//	UpdateWidgetPosition();
	// }
}

void ABasicNPC::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

