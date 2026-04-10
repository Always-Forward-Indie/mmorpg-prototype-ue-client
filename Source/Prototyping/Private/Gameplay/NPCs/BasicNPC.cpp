#include "Gameplay/NPCs/BasicNPC.h"
#include "Gameplay/NPCs/BasicNPC.h"
#include "Components/CapsuleComponent.h"
#include "Components/AudioComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/Engine.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Gameplay/UI/NPCNameplateComponent.h"
#include "MyGameInstance.h"
#include "Audio/AudioManager.h"

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
	}

	// NPCs are server-driven; disable physics push to prevent client-side jitter.
	if (UCharacterMovementComponent* CMC = GetCharacterMovement())
	{
		CMC->bEnablePhysicsInteraction = false;
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
	}

	Super::EndPlay(EndPlayReason);
}

void ABasicNPC::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

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
	
	// Call Blueprint event
	OnInteractionReceived(InteractingPlayer);
}

void ABasicNPC::PlaySoundByName(FName SoundName)
{
	if (USoundBase** Sound = SoundMap.Find(SoundName))
	{
		if (*Sound && AudioComponentMain)
		{
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
		AudioComponentMain->SetSound(IdleSounds[Idx]);
		AudioComponentMain->Play();
	}
	ScheduleNextIdleSound(); // �����������������
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
}

void ABasicNPC::PlayFarewellSound()
{
	PlaySoundByName("Farewell");
}

void ABasicNPC::SetupNPCVisual(FName NPCSlug)
{
	if (!NPCDefinitionTable) { UE_LOG(LogTemp, Warning, TEXT("No NPCDefinitionTable")); return; }
	const FNPCDefinition* Def = NPCDefinitionTable->FindRow<FNPCDefinition>(NPCSlug, TEXT("Load NPC Definition"));
	if (!Def) { UE_LOG(LogTemp, Warning, TEXT("No row for %s"), *NPCSlug.ToString()); return; }

	const auto SkeletalMeshSoft = Def->Visual.SkeletalMesh;
	const auto AnimBPSoft = Def->Visual.AnimBPClass;
	SetActorScale3D(Def->Visual.ActorScale);

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
		Streamable.RequestAsyncLoad(AnimBPSoft.ToSoftObjectPath(), [WeakThis, AnimBPSoft]()
			{
				ABasicNPC* Self = WeakThis.Get();
				if (!Self) { return; }

				if (UClass* AnimClass = AnimBPSoft.Get())
				{
					if (USkeletalMeshComponent* MC = Self->GetMesh())
						MC->SetAnimInstanceClass(AnimClass);
				}
			});
	}
	else {
		UE_LOG(LogTemp, Warning, TEXT("AnimBPClass is not set for slug %s"), *NPCSlug.ToString());
	}
}

void ABasicNPC::SetupNPCAudio(FName NPCSlug)
{
	if (!NPCDefinitionTable) return;
	const FNPCDefinition* Def = NPCDefinitionTable->FindRow<FNPCDefinition>(NPCSlug, TEXT("Load NPC Audio"));
	if (!Def) return;

	const auto Audio = Def->Audio;
	FStreamableManager& S = UAssetManager::GetStreamableManager();
	TWeakObjectPtr<ABasicNPC> WeakThis(this);

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

	// Use an enum-like index to identify which array to populate, avoiding
	// capturing a reference to a member array that may dangle.
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
							case 1: Self->WalkSounds.Add(Snd); break;
							case 2: Self->RunSounds.Add(Snd);  break;
							}
						}
					});
			}
		};

	IdleSounds.Reset();
	WalkSounds.Reset();
	RunSounds.Reset();

	LoadArray(Audio.IdleSounds, 0);
	LoadArray(Audio.WalkSounds, 1);
	LoadArray(Audio.RunSounds, 2);

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