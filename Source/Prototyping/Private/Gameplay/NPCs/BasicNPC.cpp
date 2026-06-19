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

	// Initialize UI with delay to ensure all data is loaded
	if (GetWorld())
	{
		FTimerHandle TimerHandle;
		GetWorld()->GetTimerManager().SetTimer(TimerHandle, this, &ABasicNPC::InitializeUIDelayed, 0.1f, false);
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

void ABasicNPC::SetNPCData(const FNPCStruct& Data)
{
	if (NPCData.id == 0 && Data.id > 0)
	{
		NPCData = Data;
		NPCDataUpdated.Broadcast();
		
		UE_LOG(LogTemp, Warning, TEXT("NPC Data set for %s (ID:%d): HP=%d, MP=%d"), 
			*NPCData.name, NPCData.id, NPCData.stats.health.current, NPCData.stats.mana.current);

		// Set actor location from NPC position data
		FVector NewLocation(NPCData.position.positionX, NPCData.position.positionY, NPCData.position.positionZ);
		SetActorLocation(NewLocation);
		SnapToGround();
		
		// Set actor rotation
		FRotator NewRotation(0.0f, NPCData.position.rotationZ, 0.0f);
		SetActorRotation(NewRotation);

		// Mark UI as ready for initialization when UI components are added
		bUIInitialized = false;
		LastHealth = NPCData.stats.health.current;
		LastMana = NPCData.stats.mana.current;

		// Initialize nameplate component with NPC data
		if (NPCNameplateComponent)
		{
			NPCNameplateComponent->InitialiseFromNPCData(NPCData, false);
		}

		// Wire up ambient speech pools if available
		if (UNPCAmbientSpeechComponent* ASComp = FindComponentByClass<UNPCAmbientSpeechComponent>())
		{
			if (UGameInstance* GI = GetGameInstance())
			{
				if (UMyGameInstance* MyGI = Cast<UMyGameInstance>(GI))
				{
					FAmbientSpeechNPCData AmbientData;
					if (MyGI->AmbientSpeechManager &&
						MyGI->AmbientSpeechManager->GetNPCAmbientData(NPCData.id, AmbientData))
					{
						ASComp->SetAmbientData(AmbientData);
					}
				}
			}
		}

		// Setup visual and audio based on NPC slug
		if (!NPCData.slug.IsEmpty())
		{
			FName NPCSlugName(*NPCData.slug);
			SetupNPCVisual(NPCSlugName);
			SetupNPCAudio(NPCSlugName);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("NPC slug is empty for NPC %s (ID:%d), visual/audio setup skipped"), *NPCData.name, NPCData.id);
		}
	}
	else
	{
		if (NPCData.id != 0)
		{
			UE_LOG(LogTemp, Error, TEXT("NPC Data already set for %s (ID:%d)"), *NPCData.name, NPCData.id);
		}
		else if (Data.id <= 0)
		{
			UE_LOG(LogTemp, Error, TEXT("Invalid NPC ID (%d) provided"), Data.id);
		}
	}
}

void ABasicNPC::SetNPCId(int32 NPCId)
{
	NPCData.id = NPCId;
}

void ABasicNPC::UpdateNPCQuestData(const TArray<FNPCQuestEntry>& NewQuests)
{
	NPCData.quests = NewQuests;

	// Refresh nameplate interaction state
	if (UNPCNameplateComponent* NP = FindComponentByClass<UNPCNameplateComponent>())
	{
		NP->InitialiseFromNPCData(NPCData, true);
	}

	UE_LOG(LogTemp, Log, TEXT("NPC %s (ID:%d): Quest data updated, %d quests"),
		*NPCData.name, NPCData.id, NewQuests.Num());
}

void ABasicNPC::SetNPCName(const FString& NPCName)
{
	NPCData.name = NPCName;
}

void ABasicNPC::SetNPCSlug(const FString& NPCSlug)
{
	NPCData.slug = NPCSlug;
}

void ABasicNPC::SetNPCRace(const FString& NPCRace)
{
	NPCData.race = NPCRace;
}

void ABasicNPC::SetNPCLevel(int32 NPCLevel)
{
	NPCData.level = NPCLevel;
}

void ABasicNPC::SetNPCType(const FString& NPCType)
{
	NPCData.npcType = NPCType;
}

void ABasicNPC::SetNPCInteractable(bool bInteractable)
{
	NPCData.isInteractable = bInteractable;
}

void ABasicNPC::SetNPCDialogueId(const FString& DialogueId)
{
	NPCData.dialogueId = DialogueId;
}

void ABasicNPC::SetNPCQuestId(const FString& QuestId)
{
	NPCData.questId = QuestId;
}

void ABasicNPC::SetNPCPosition(const FPositionDataStruct& Position)
{
	NPCData.position = Position;
	
	// Update actor location
	FVector NewLocation(Position.positionX, Position.positionY, Position.positionZ);
	SetActorLocation(NewLocation);
	SnapToGround();
	
	// Update actor rotation
	FRotator NewRotation(0.0f, Position.rotationZ, 0.0f);
	SetActorRotation(NewRotation);
}

void ABasicNPC::SetNPCAttributes(const TArray<FAttributeDataStruct>& Attributes)
{
	NPCData.attributes = Attributes;
}

void ABasicNPC::SetNPCStats(const FNPCHealthManaStruct& Stats)
{
	NPCData.stats = Stats;
	
	// Update tracking variables for UI when it becomes available
	LastHealth = Stats.health.current;
	LastMana = Stats.mana.current;
	
	UE_LOG(LogTemp, Log, TEXT("NPC %s (ID:%d) stats updated: HP=%d/%d, MP=%d/%d"), 
		*NPCData.name, NPCData.id, Stats.health.current, Stats.health.max, Stats.mana.current, Stats.mana.max);
}

FVector ABasicNPC::GetNPCPosition() const
{
	return FVector(NPCData.position.positionX, NPCData.position.positionY, NPCData.position.positionZ);
}

void ABasicNPC::OnPlayerInteract(APlayerController* InteractingPlayer)
{
	if (!NPCData.isInteractable)
	{
		UE_LOG(LogTemp, Warning, TEXT("NPC %s is not interactable"), *NPCData.name);
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("Player interacting with NPC %s"), *NPCData.name);
	
	// Play interaction sound if available
	PlaySoundByName("Interact");

	// Trigger greeting animation + talking state
	PlayGreetingSound();
	
	// Call Blueprint event
	OnInteractionReceived(InteractingPlayer);
}

void ABasicNPC::NotifyDialogueClosed()
{
	// Legacy entry-point kept for Blueprint/external callers.
	// Internally delegates to the counted path.
	NotifyWindowClosed();
}

void ABasicNPC::NotifyWindowOpened()
{
	++ActiveInteractionWindowCount;
	// Cancel any scheduled farewell – a new window opened for this NPC.
	if (UWorld* W = GetWorld())
	{
		W->GetTimerManager().ClearTimer(FarewellTimer);
	}
}

void ABasicNPC::NotifyWindowClosed()
{
	ActiveInteractionWindowCount = FMath::Max(0, ActiveInteractionWindowCount - 1);

	if (ActiveInteractionWindowCount > 0)
	{
		return; // Other windows still open – no farewell yet.
	}

	// All windows closed. Use a short deferred timer so a rapid dialogue→shop
	// transition (where DIALOGUE_CLOSE arrives before vendor_shop_data) doesn't
	// fire the farewell prematurely. 250 ms is imperceptible but sufficient.
	if (UWorld* W = GetWorld())
	{
		W->GetTimerManager().SetTimer(
			FarewellTimer,
			FTimerDelegate::CreateWeakLambda(this, [this]()
			{
				if (ActiveInteractionWindowCount <= 0)
				{
					PlayFarewellSound();
				}
			}),
			0.25f,
			/*bLooping=*/false);
	}
}

void ABasicNPC::PlaySoundByName(FName SoundName)
{
	if (USoundBase** Sound = SoundMap.Find(SoundName))
	{
		if (*Sound && AudioComponentMain)
		{
			AudioComponentMain->AttenuationSettings = DefaultAttenuation;
			AudioComponentMain->SetSound(*Sound);
			AudioComponentMain->Play();
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("ABasicNPC::PlaySoundByName - Sound '%s' not found"), *SoundName.ToString());
	}
}


void ABasicNPC::PlayRandomIdleSound()
{
	if (IdleSounds.Num() > 0 && AudioComponentMain)
	{
		const int32 Idx = FMath::RandRange(0, IdleSounds.Num() - 1);
		AudioComponentMain->AttenuationSettings = DefaultAttenuation;
		AudioComponentMain->SetSound(IdleSounds[Idx]);
		AudioComponentMain->Play();
	}
	ScheduleNextIdleSound();
}

void ABasicNPC::ScheduleNextIdleSound()
{
	if (!GetWorld() || IdleSounds.Num() == 0) return;
	const float Delay = FMath::FRandRange(10.f, 30.f);
	GetWorld()->GetTimerManager().SetTimer(IdleSoundTimerHandle, this, &ABasicNPC::PlayRandomIdleSound, Delay, false);
}

void ABasicNPC::PlayGreetingSound()
{
	PlaySoundByName("Greeting");

	// Start smoothly rotating toward the player
	if (!bIsFacingPlayer)
	{
		OriginalRotation = GetActorRotation();
		bIsFacingPlayer = true;
		bIsRestoringRotation = false;
	}

	// bIsTalking: set on AnimInstance if the ABP uses UNPCAnimInstance
	if (UNPCAnimInstance* Anim = GetNPCAnimInstance())
		Anim->SetTalking(true);

	if (GreetMontageAsset)
	{
		// Interrupt idle only when we actually have something to replace it with
		if (bIdleAnimPlaying && ActiveIdleMontage)
			StopAnimMontage(ActiveIdleMontage);

		const float Duration = PlayAnimMontage(GreetMontageAsset);
		if (Duration > 0.f)
		{
			// After the greet montage ends, restart the idle cycle
			if (UAnimInstance* AnimInst = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr)
			{
				ActionEndedDelegate.BindUObject(this, &ABasicNPC::OnActionMontageEnded);
				AnimInst->Montage_SetEndDelegate(ActionEndedDelegate, GreetMontageAsset);
			}
		}
		UE_LOG(LogTemp, Log, TEXT("[NPCAnim] '%s' playing greet montage '%s'"),
			*GetNPCName(), *GreetMontageAsset->GetName());
	}
	// No GreetMontage assigned — idle keeps playing undisturbed
}

void ABasicNPC::PlayFarewellSound()
{
	PlaySoundByName("Farewell");

	// Stop facing the player and smoothly return to original rotation
	bIsFacingPlayer = false;
	bIsRestoringRotation = true;

	if (FarewellMontageAsset)
	{
		// Interrupt idle only when we actually have something to replace it with
		if (bIdleAnimPlaying && ActiveIdleMontage)
			StopAnimMontage(ActiveIdleMontage);

		const float Duration = PlayAnimMontage(FarewellMontageAsset);
		if (Duration > 0.f)
		{
			// After the farewell montage ends, restart the idle cycle
			if (UAnimInstance* AnimInst = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr)
			{
				ActionEndedDelegate.BindUObject(this, &ABasicNPC::OnActionMontageEnded);
				AnimInst->Montage_SetEndDelegate(ActionEndedDelegate, FarewellMontageAsset);
			}
		}
		UE_LOG(LogTemp, Log, TEXT("[NPCAnim] '%s' playing farewell montage '%s'"),
			*GetNPCName(), *FarewellMontageAsset->GetName());
	}
	// No FarewellMontage assigned — idle keeps playing undisturbed

	// Clear talking state
	if (UNPCAnimInstance* Anim = GetNPCAnimInstance())
		Anim->SetTalking(false);
}

UNPCAnimInstance* ABasicNPC::GetNPCAnimInstance() const
{
	if (USkeletalMeshComponent* NPCMesh = GetMesh())
	{
		return Cast<UNPCAnimInstance>(NPCMesh->GetAnimInstance());
	}
	return nullptr;
}

// ─── IWorldInteractable interface ────────────────────────────────────────────
EInteractableType ABasicNPC::GetInteractableType() const
{
    return EInteractableType::NPC;
}

FText ABasicNPC::GetInteractableDisplayName() const
{
    const FString Label = NPCData.level > 0
        ? FString::Printf(TEXT("%s  [Lv.%d]"), *NPCData.name, NPCData.level)
        : NPCData.name;
    return FText::FromString(Label);
}

bool ABasicNPC::CanInteract() const
{
    return NPCData.isInteractable;
}

void ABasicNPC::SnapToGround()
{
	UWorld* World = GetWorld();
	if (!World) return;

	const FVector CurrentLoc = GetActorLocation();
	const FVector Start(CurrentLoc.X, CurrentLoc.Y,  50000.0f);
	const FVector End  (CurrentLoc.X, CurrentLoc.Y, -10000.0f);

	FCollisionQueryParams Params;
	Params.bTraceComplex = true;
	Params.AddIgnoredActor(this);

	for (TActorIterator<APawn> It(World); It; ++It)
	{
		Params.AddIgnoredActor(*It);
	}

	FHitResult Hit;
	if (World->LineTraceSingleByChannel(Hit, Start, End, ECC_WorldStatic, Params))
	{
		const float HalfHeight = GetCapsuleComponent()
			? GetCapsuleComponent()->GetScaledCapsuleHalfHeight()
			: 90.0f;
		const float CorrectZ = Hit.ImpactPoint.Z + HalfHeight;
		SetActorLocation(FVector(CurrentLoc.X, CurrentLoc.Y, CorrectZ));

		// Remember this Z so the periodic corrector can restore it if something pushes the NPC.
		SnappedZ = CorrectZ;
		bSnappedZValid = true;

		// Start periodic Z-correction timer (or restart it if already running).
		if (World)
		{
			World->GetTimerManager().SetTimer(
				ZCorrectionTimerHandle,
				this, &ABasicNPC::CorrectZ,
				0.5f, /*bLoop=*/true);
		}
	}
}

void ABasicNPC::CorrectZ()
{
	if (!bSnappedZValid) return;

	const FVector Loc = GetActorLocation();
	if (!FMath::IsNearlyEqual(Loc.Z, SnappedZ, 1.0f))
	{
		SetActorLocation(FVector(Loc.X, Loc.Y, SnappedZ), /*bSweep=*/false, nullptr, ETeleportType::TeleportPhysics);
	}
}

void ABasicNPC::SetupNPCVisual(FName NPCSlug)
{
	if (!NPCDefinitionTable) { UE_LOG(LogTemp, Warning, TEXT("No NPCDefinitionTable")); return; }
	const FNPCDefinition* Def = NPCDefinitionTable->FindRow<FNPCDefinition>(NPCSlug, TEXT("Load NPC Definition"));
	if (!Def) { UE_LOG(LogTemp, Warning, TEXT("No row for %s"), *NPCSlug.ToString()); return; }

	const auto SkeletalMeshSoft = Def->Visual.SkeletalMesh;
	const auto AnimBPSoft = Def->Visual.AnimBPClass;
	SetActorScale3D(Def->Visual.ActorScale);

	// Snapshot the visual data before entering async lambdas —
	// the DataTable row pointer is only safe on this stack frame.
	const FNPCVisualData VisualDataCopy = Def->Visual;

	FStreamableManager& Streamable = UAssetManager::GetStreamableManager();
	TWeakObjectPtr<ABasicNPC> WeakThis(this);

	if (!SkeletalMeshSoft.IsNull())
	{
		Streamable.RequestAsyncLoad(SkeletalMeshSoft.ToSoftObjectPath(), [WeakThis, SkeletalMeshSoft]()
			{
				ABasicNPC* Self = WeakThis.Get();
				if (!Self) { return; }

				if (USkeletalMesh* Mesh = SkeletalMeshSoft.Get())
				{
					USkeletalMeshComponent* MC = Self->GetMesh();
					if (MC) {
						MC->SetSkeletalMesh(Mesh);

						const FBoxSphereBounds B = Mesh->GetBounds();
						if (UCapsuleComponent* Cap = Self->GetCapsuleComponent())
						{
							const float CapsuleRadius = FMath::Max(B.BoxExtent.X, B.BoxExtent.Y);
							const float CapsuleHalf = B.BoxExtent.Z;
							Cap->SetCapsuleRadius(CapsuleRadius);
							Cap->SetCapsuleHalfHeight(CapsuleHalf);

							MC->SetRelativeLocation(FVector(0, 0, -Cap->GetUnscaledCapsuleHalfHeight()));

							// Re-snap after the capsule is correctly sized.
							// The initial SnapToGround() used the default ACharacter capsule,
							// so the actor Z needs to be corrected now that we know the real height.
							Self->SnapToGround();
						}
					}
				}
			});
	}
	else {
		UE_LOG(LogTemp, Warning, TEXT("SkeletalMesh is not set for slug %s"), *NPCSlug.ToString());
	}

	if (!AnimBPSoft.IsNull())
	{
		Streamable.RequestAsyncLoad(AnimBPSoft.ToSoftObjectPath(), [WeakThis, AnimBPSoft, VisualDataCopy]()
			{
				ABasicNPC* Self = WeakThis.Get();
				if (!Self) { return; }

				if (UClass* AnimClass = AnimBPSoft.Get())
				{
					if (USkeletalMeshComponent* MC = Self->GetMesh())
					{
						MC->SetAnimInstanceClass(AnimClass);

						// --- Populate montage assets on BasicNPC directly ---
						// Using ACharacter::PlayAnimMontage() so playback works
						// regardless of whether the ABP inherits from UNPCAnimInstance.
						if (!VisualDataCopy.GreetMontage.IsNull())
							Self->GreetMontageAsset = VisualDataCopy.GreetMontage.LoadSynchronous();

						if (!VisualDataCopy.FarewellMontage.IsNull())
							Self->FarewellMontageAsset = VisualDataCopy.FarewellMontage.LoadSynchronous();

						Self->IdleMontageAssets.Reset();
						for (const auto& Soft : VisualDataCopy.IdleMontages)
							if (!Soft.IsNull())
								if (UAnimMontage* M = Soft.LoadSynchronous())
									Self->IdleMontageAssets.Add(M);

						if (!VisualDataCopy.DefaultIdleMontage.IsNull())
							Self->DefaultIdleMontageAsset = VisualDataCopy.DefaultIdleMontage.LoadSynchronous();

						UE_LOG(LogTemp, Log,
							TEXT("[NPCAnim] Montages loaded for '%s': greet=%s farewell=%s defaultIdle=%s idleCount=%d"),
							*Self->GetNPCName(),
							Self->GreetMontageAsset        ? *Self->GreetMontageAsset->GetName()        : TEXT("none"),
							Self->FarewellMontageAsset     ? *Self->FarewellMontageAsset->GetName()     : TEXT("none"),
							Self->DefaultIdleMontageAsset  ? *Self->DefaultIdleMontageAsset->GetName()  : TEXT("none"),
							Self->IdleMontageAssets.Num());

						// --- Also populate UNPCAnimInstance maps if the ABP uses it ---
						// (allows bIsTalking / Speed state-machine integration)
						if (UNPCAnimInstance* Anim = Self->GetNPCAnimInstance())
						{
							if (Self->GreetMontageAsset)
								Anim->ActionMontageMap.Add(FName("greet"), Self->GreetMontageAsset);
							if (Self->FarewellMontageAsset)
								Anim->ActionMontageMap.Add(FName("farewell"), Self->FarewellMontageAsset);
							Anim->IdleMontages = Self->IdleMontageAssets;
						}

						// Start idle animation cycle — plays DefaultIdleMontage immediately if set
						Self->StartIdleAnimCycle();
					}
				}
			});
	}
	else {
		UE_LOG(LogTemp, Warning, TEXT("AnimBPClass is not set for slug %s"), *NPCSlug.ToString());
	}
}

void ABasicNPC::StartIdleAnimCycle()
{
	// If a DefaultIdleMontage is assigned, play it right now so the NPC is never
	// frozen in a T-pose waiting for the first random timer to fire.
	// OnIdleMontageEnded will chain into ScheduleNextIdleAnim() afterwards.
	if (DefaultIdleMontageAsset && !bIdleAnimPlaying)
	{
		const float Duration = PlayAnimMontage(DefaultIdleMontageAsset);
		if (Duration > 0.f)
		{
			ActiveIdleMontage = DefaultIdleMontageAsset;
			bIdleAnimPlaying  = true;

			if (UAnimInstance* AnimInst = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr)
			{
				IdleEndedDelegate.BindUObject(this, &ABasicNPC::OnIdleMontageEnded);
				AnimInst->Montage_SetEndDelegate(IdleEndedDelegate, DefaultIdleMontageAsset);
			}
			UE_LOG(LogTemp, Log, TEXT("[NPCAnim] '%s' playing DefaultIdleMontage immediately: '%s'"),
				*GetNPCName(), *DefaultIdleMontageAsset->GetName());
			return;
		}
	}
	// No DefaultIdleMontage (or playback failed) — fall through to the timed cycle.
	ScheduleNextIdleAnim();
}

void ABasicNPC::ScheduleNextIdleAnim()
{
	if (!GetWorld()) return;
	// 8-20 seconds between random idle variants — visible during normal gameplay
	const float Delay = FMath::FRandRange(8.0f, 20.0f);
	GetWorld()->GetTimerManager().SetTimer(IdleAnimTimerHandle, this, &ABasicNPC::TriggerRandomIdleAnim, Delay, false);
}

void ABasicNPC::TriggerRandomIdleAnim()
{
	// If an idle is already playing let it finish — the end delegate reschedules.
	if (bIdleAnimPlaying)
		return;

	if (IdleMontageAssets.Num() == 0)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[NPCAnim] '%s' has no IdleMontages — assign them in the NPCDefinition DataTable (Visual.IdleMontages)"),
			*GetNPCName());
		return; // nothing to play, don't reschedule
	}

	const int32 Idx  = FMath::RandRange(0, IdleMontageAssets.Num() - 1);
	UAnimMontage* Montage = IdleMontageAssets[Idx];
	if (!Montage) { ScheduleNextIdleAnim(); return; }

	const float Duration = PlayAnimMontage(Montage);
	if (Duration > 0.f)
	{
		ActiveIdleMontage = Montage;
		bIdleAnimPlaying   = true;

		// Bind end delegate — fires when the montage finishes or is stopped
		if (UAnimInstance* AnimInst = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr)
		{
			IdleEndedDelegate.BindUObject(this, &ABasicNPC::OnIdleMontageEnded);
			AnimInst->Montage_SetEndDelegate(IdleEndedDelegate, Montage);
		}
		UE_LOG(LogTemp, Log, TEXT("[NPCAnim] '%s' playing idle variant %d: '%s'"),
			*GetNPCName(), Idx, *Montage->GetName());
	}
	else
	{
		// Playback failed (e.g. no DefaultSlot in AnimGraph) — fall back to timer
		ScheduleNextIdleAnim();
	}

	// Keep UNPCAnimInstance IdleVariantIndex in sync for State Machine read-back
	if (UNPCAnimInstance* Anim = GetNPCAnimInstance())
		Anim->IdleVariantIndex = Idx;
}

void ABasicNPC::OnIdleMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (ActiveIdleMontage != Montage) return;
	ActiveIdleMontage = nullptr;
	bIdleAnimPlaying  = false;

	// Only reschedule the idle cycle when the montage finished naturally.
	// If it was interrupted by greet/farewell those methods bind their own
	// ActionEndedDelegate and reschedule after the action montage ends.
	if (!bInterrupted)
	{
		ScheduleNextIdleAnim();
	}
}

void ABasicNPC::OnActionMontageEnded(UAnimMontage* /*Montage*/, bool bInterrupted)
{
	// After a greeting or farewell montage ends, resume the idle animation cycle.
	if (!bInterrupted)
	{
		ScheduleNextIdleAnim();
	}
}

void ABasicNPC::SetupNPCAudio(FName NPCSlug)
{
	if (!NPCDefinitionTable) return;
	const FNPCDefinition* Def = NPCDefinitionTable->FindRow<FNPCDefinition>(NPCSlug, TEXT("Load NPC Audio"));
	if (!Def) return;

	FStreamableManager& S = UAssetManager::GetStreamableManager();
	TWeakObjectPtr<ABasicNPC> WeakThis(this);

	// ---------------------------------------------------------------
	// Priority 1 — Entity Audio Profile (single table for all entities)
	// ---------------------------------------------------------------
	if (!Def->AudioProfileId.IsNone())
	{
		UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance());
		const FEntityAudioProfile* Profile = nullptr;
		if (GI)
		{
			if (UEntityAudioRepository* Repo = GI->GetEntityAudioRepository())
			{
				Profile = Repo->FindProfile(Def->AudioProfileId);
			}
		}

		if (Profile)
		{
			if (!Profile->DefaultAttenuation.IsNull())
			{
				DefaultAttenuation = Profile->DefaultAttenuation.LoadSynchronous();
			}
			if (!Profile->FootwearType.IsNone())
			{
				FootwearType = Profile->FootwearType;
			}

			auto LoadOne = [WeakThis, &S](FName Key, TSoftObjectPtr<USoundBase> Soft)
			{
				if (Soft.IsNull()) return;
				S.RequestAsyncLoad(Soft.ToSoftObjectPath(), [WeakThis, Key, Soft]()
				{
					ABasicNPC* Self = WeakThis.Get();
					if (!Self) { return; }
					if (USoundBase* Snd = Soft.Get()) { Self->SoundMap.Add(Key, Snd); }
				});
			};

			LoadOne("Greeting",  Profile->GreetingSound);
			LoadOne("Interact",  Profile->InteractSound);
			LoadOne("Farewell",  Profile->FarewellSound);

			auto LoadArray = [WeakThis, &S](const TArray<TSoftObjectPtr<USoundBase>>& Src, int32 ArrayIndex)
			{
				for (auto Soft : Src)
				{
					if (Soft.IsNull()) continue;
					S.RequestAsyncLoad(Soft.ToSoftObjectPath(), [WeakThis, Soft, ArrayIndex]()
					{
						ABasicNPC* Self = WeakThis.Get();
						if (!Self) { return; }
						if (USoundBase* Snd = Soft.Get())
						{
							switch (ArrayIndex)
							{
							case 0: Self->IdleSounds.Add(Snd); break;
							case 1: Self->FootstepSounds.Add(Snd); break;
	
							}
						}
					});
				}
			};

			IdleSounds.Reset();
			FootstepSounds.Reset();

			LoadArray(Profile->IdleAmbient,   0);
			LoadArray(Profile->Footsteps,     1);

			ScheduleNextIdleSound();
			return;
		}
		else
		{
			UE_LOG(LogTemp, Warning,
				TEXT("ABasicNPC::SetupNPCAudio: AudioProfileId '%s' not found in EntityAudioProfilesTable for NPC '%s'. Falling back to legacy FNPCAudioData."),
				*Def->AudioProfileId.ToString(), *NPCSlug.ToString());
		}
	}

	// ---------------------------------------------------------------
	// Priority 2 — Legacy inline FNPCAudioData
	// Migrate rows to AudioProfileId over time; do not add new sounds here.
	// ---------------------------------------------------------------
	const auto Audio = Def->Audio;

	if (!Audio.DefaultAttenuation.IsNull())
	{
		DefaultAttenuation = Audio.DefaultAttenuation.LoadSynchronous();
	}

	auto LoadOne = [WeakThis, &S](FName Key, TSoftObjectPtr<USoundBase> Soft)
	{
		if (Soft.IsNull()) return;
		S.RequestAsyncLoad(Soft.ToSoftObjectPath(), [WeakThis, Key, Soft]()
		{
			ABasicNPC* Self = WeakThis.Get();
			if (!Self) { return; }
			if (USoundBase* Snd = Soft.Get()) { Self->SoundMap.Add(Key, Snd); }
		});
	};

	LoadOne("Greeting", Audio.GreetingSound);
	LoadOne("Interact", Audio.InteractSound);
	LoadOne("Farewell", Audio.FarewellSound);

	auto LoadArray = [WeakThis, &S](const TArray<TSoftObjectPtr<USoundBase>>& Src, int32 ArrayIndex)
	{
		for (auto Soft : Src)
		{
			if (Soft.IsNull()) continue;
			S.RequestAsyncLoad(Soft.ToSoftObjectPath(), [WeakThis, Soft, ArrayIndex]()
			{
				ABasicNPC* Self = WeakThis.Get();
				if (!Self) { return; }
				if (USoundBase* Snd = Soft.Get())
				{
					switch (ArrayIndex)
					{
					case 0: Self->IdleSounds.Add(Snd); break;
					case 1: Self->FootstepSounds.Add(Snd); break;
					}
				}
			});
		}
	};

	IdleSounds.Reset();
	FootstepSounds.Reset();

	LoadArray(Audio.IdleSounds, 0);
	LoadArray(Audio.FootstepSounds, 1);

	ScheduleNextIdleSound();
}

void ABasicNPC::UpdateWidgetScale(float DeltaTime)
{
	// Placeholder for widget scaling - will be implemented when UI components are added
	/*
	if (!NPCHeadInfo || !HeadWidget)
		return;

	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC || !PC->PlayerCameraManager)
		return;

	const FVector CameraLoc = PC->PlayerCameraManager->GetCameraLocation();
	const float Distance = FVector::Dist(CameraLoc, GetActorLocation());

	float Clamped = FMath::Clamp(Distance, MinDistance, MaxDistance);
	float Factor = 1.0f - ((Clamped - MinDistance) / (MaxDistance - MinDistance));
	float TargetScale = FMath::Clamp(Factor, 0.1f, 1.0f) * widgetScaleFactor;

	CurrentWidgetScale = FMath::FInterpConstantTo(CurrentWidgetScale, TargetScale, DeltaTime, 1.f);
	// HeadWidget->SetWidgetScale(TargetScale);
	*/
}

void ABasicNPC::UpdateWidgetPosition()
{
	// Placeholder for widget positioning - will be implemented when UI components are added
	/*
	if (!NPCHeadInfo)
		return;

	UCapsuleComponent* Capsule = GetCapsuleComponent();
	if (!Capsule)
		return;

	// Get capsule height
	const float CapsuleHalfHeight = Capsule->GetScaledCapsuleHalfHeight();

	// Base offset above head
	const float BaseOffset = 40.f;

	// Additional offset based on distance
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	float DistanceOffset = 0.f;

	if (PC && PC->PlayerCameraManager)
	{
		const float Distance = FVector::Dist(PC->PlayerCameraManager->GetCameraLocation(), GetActorLocation());
		DistanceOffset = FMath::Clamp((Distance - 500.f) * 0.05f, 0.f, 40.f);
	}

	// Position above capsule
	const float FinalZ = CapsuleHalfHeight + BaseOffset + DistanceOffset;
	NPCHeadInfo->SetRelativeLocation(FVector(0.f, 0.f, FinalZ));
	*/
}

void ABasicNPC::ForceUpdateUI()
{
	// Placeholder for UI updates - will be implemented when UI components are added
	/*
	if (!NPCHeadInfo)
		return;

	// Update UI with current NPC data
	UE_LOG(LogTemp, Log, TEXT("ForceUpdateUI for NPC %s (ID:%d): HP=%d/%d, MP=%d/%d"), 
		*NPCData.name, NPCData.id, 
		NPCData.stats.health.current, NPCData.stats.health.max,
		NPCData.stats.mana.current, NPCData.stats.mana.max);

	// Placeholder for actual UI update
	// NPCHeadInfo->UpdateInfo(
	//     NPCData.stats.health.current,
	//     NPCData.stats.health.max,
	//     NPCData.stats.mana.current,
	//     NPCData.stats.mana.max,
	//     NPCData.name,
	//     NPCData.level,
	//     NPCData.isInteractable
	// );

	LastHealth = NPCData.stats.health.current;
	LastMana = NPCData.stats.mana.current;
	bUIInitialized = true;

	if (NPCHeadInfo)
	{
		NPCHeadInfo->SetVisibility(true);
	}
	*/
}

void ABasicNPC::InitializeUIDelayed()
{
	// Placeholder for delayed UI initialization - will be implemented when UI components are added
	/*
	// Try to initialize UI if we have data
	if (NPCData.id != 0 && !bUIInitialized)
	{
		UE_LOG(LogTemp, Log, TEXT("InitializeUIDelayed for NPC %s (ID:%d)"), *NPCData.name, NPCData.id);
		ForceUpdateUI();
	}
	
	// Initialize HeadWidget if not already done
	if (!HeadWidget && NPCHeadInfo)
	{
		// HeadWidget = Cast<UW_NPCHeadInfoWidget>(NPCHeadInfo->GetUserWidgetObject());
		if (HeadWidget && NPCHeadInfo)
		{
			NPCHeadInfo->SetVisibility(true);
			UE_LOG(LogTemp, Log, TEXT("InitializeUIDelayed: Successfully initialized HeadWidget for NPC %s"), *NPCData.name);
		}
	}
	*/
}