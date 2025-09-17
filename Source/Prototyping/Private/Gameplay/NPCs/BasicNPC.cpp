#include "Gameplay/NPCs/BasicNPC.h"
#include "Components/CapsuleComponent.h"
#include "Components/AudioComponent.h"
#include "Engine/Engine.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"

// Sets default values
ABasicNPC::ABasicNPC()
{
	// Set this character to call Tick() every frame
	PrimaryActorTick.bCanEverTick = true;

	// Initialize audio components
	AudioComponentMain = CreateDefaultSubobject<UAudioComponent>(TEXT("NPCMainAudio"));
	AudioComponentMain->SetupAttachment(RootComponent);
	AudioComponentMain->bAutoActivate = false;

	AudioComponentSecond = CreateDefaultSubobject<UAudioComponent>(TEXT("NPCSecondAudio"));
	AudioComponentSecond->SetupAttachment(RootComponent);
	AudioComponentSecond->bAutoActivate = false;

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
	// Clean up any resources if needed
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
	ScheduleNextIdleSound(); // перезапланировать
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

	const auto SkeletalMeshSoft = Def->Visual.SkeletalMesh;     // захватываем по значению
	const auto AnimBPSoft = Def->Visual.AnimBPClass;
	SetActorScale3D(Def->Visual.ActorScale);

	FStreamableManager& Streamable = UAssetManager::GetStreamableManager();

	if (!SkeletalMeshSoft.IsNull())
	{
		Streamable.RequestAsyncLoad(SkeletalMeshSoft.ToSoftObjectPath(), [this, SkeletalMeshSoft]()
			{
				if (USkeletalMesh* Mesh = SkeletalMeshSoft.Get())
				{
					USkeletalMeshComponent* MC = GetMesh();
					if (MC) {
						MC->SetSkeletalMesh(Mesh);

						// Аккуратнее с капсулой/смещением (см. пункт 2)
						const FBoxSphereBounds B = Mesh->GetBounds();
						if (UCapsuleComponent* Cap = GetCapsuleComponent())
						{
							const float CapsuleRadius = FMath::Max(B.BoxExtent.X, B.BoxExtent.Y);
							const float CapsuleHalf = B.BoxExtent.Z; // это половина высоты бокса
							Cap->SetCapsuleRadius(CapsuleRadius);
							Cap->SetCapsuleHalfHeight(CapsuleHalf);

							// Для ACharacter обычно mesh Z = -CapsuleHalfHeight (ноги на земле)
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
		Streamable.RequestAsyncLoad(AnimBPSoft.ToSoftObjectPath(), [this, AnimBPSoft]()
			{
				if (UClass* AnimClass = AnimBPSoft.Get())
				{
					if (USkeletalMeshComponent* MC = GetMesh())
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

	const auto Audio = Def->Audio; // по значению
	FStreamableManager& S = UAssetManager::GetStreamableManager();

	auto LoadOne = [this, &S](FName Key, TSoftObjectPtr<USoundBase> Soft)
		{
			if (Soft.IsNull()) return;
			S.RequestAsyncLoad(Soft.ToSoftObjectPath(), [this, Key, Soft]()
				{
					if (USoundBase* Snd = Soft.Get()) { SoundMap.Add(Key, Snd); }
				});
		};

	LoadOne("Greeting", Audio.GreetingSound);
	LoadOne("Interact", Audio.InteractSound);
	LoadOne("Farewell", Audio.FarewellSound);

	auto LoadArray = [this, &S](const TArray<TSoftObjectPtr<USoundBase>>& Src, TArray<USoundBase*>& Dst)
		{
			for (auto Soft : Src)
			{
				if (Soft.IsNull()) continue;
				S.RequestAsyncLoad(Soft.ToSoftObjectPath(), [this, Soft, &Dst]()
					{
						if (USoundBase* Snd = Soft.Get()) { Dst.Add(Snd); }
					});
			}
		};

	IdleSounds.Reset();
	WalkSounds.Reset();
	RunSounds.Reset();

	LoadArray(Audio.IdleSounds, IdleSounds);
	LoadArray(Audio.WalkSounds, WalkSounds);
	LoadArray(Audio.RunSounds, RunSounds);

	// Периодический idle с рандомным интервалом
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