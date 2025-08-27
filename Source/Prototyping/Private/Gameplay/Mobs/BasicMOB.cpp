// Fill out your copyright notice in the Description page of Project Settings.

#include "Gameplay/Mobs/BasicMOB.h"
#include "Gameplay/Combat/CombatSystemManager.h"
#include "MyGameInstance.h"
#include "Gameplay/UI/FloatingCombatTextManager.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/Engine.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/GameplayStatics.h"

// Sets default values
ABasicMOB::ABasicMOB()
{
 	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	MOBMovementComponent = CreateDefaultSubobject<UMOBMovementComponent>(TEXT("MOBMovementComponent"));

	PrevServerPos = TargetServerPos = GetActorLocation();
	PrevServerRot = TargetServerRot = GetActorRotation();
	ServerVelocity = FVector::ZeroVector;
	LastMovePacketTime = 0.f;
	bHasVelocity = false;

	// Добавляем компонент для отображения шкалы здоровья
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
}

// Called when the game starts or when spawned
void ABasicMOB::BeginPlay()
{
	Super::BeginPlay();
	
	// Инициализируем UI с небольшой задержкой, чтобы убедиться, что все данные загружены
	if (GetWorld())
	{
		FTimerHandle TimerHandle;
		GetWorld()->GetTimerManager().SetTimer(TimerHandle, this, &ABasicMOB::InitializeUIDelayed, 0.1f, false);
	}

	HeadWidget = Cast<UW_MOBHeadInfoWidget>(MobHeadInfo->GetUserWidgetObject());
}

// Override EndPlay to unregister from combat system
void ABasicMOB::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // Unregister from combat system
    if (UMyGameInstance* GameInstance = Cast<UMyGameInstance>(GetGameInstance()))
    {
        if (UCombatSystemManager* CombatManager = GameInstance->GetCombatSystemManager())
        {
            // Безопасно отписываемся только если объект ещё валиден
            if (IsValid(this) && MOBData.mobID > 0)
            {
                TScriptInterface<ICombatable> CombatableInterface;
                CombatableInterface.SetObject(this);
                CombatableInterface.SetInterface(this);
                
                CombatManager->UnregisterCombatable(CombatableInterface);
                UE_LOG(LogTemp, Log, TEXT("MOB %d unregistered from combat system"), GetActorId_Implementation());
            }
        }
    }

    Super::EndPlay(EndPlayReason);
}

void ABasicMOB::OnReceiveServerPacket(const FPositionDataStruct& MOBPosition)
{
	// Store position in MOB data
	MOBData.mobPosition = MOBPosition;

	if (MOBMovementComponent)
	{
		MOBMovementComponent->OnReceiveServerPacket(MOBPosition);
	}
}

// Called every frame
void ABasicMOB::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

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

	if (MOBData.bIsMoving)
	{
		if (!AudioComponentSecond->IsPlaying())
		{
			PlayWalkRandomSound();
		}
	}
	else
	{
		if (AudioComponentSecond->IsPlaying())
		{
			AudioComponentSecond->Stop();
		}
	}

	// Всегда обновляем UI, если есть MobHeadInfo
	if (MobHeadInfo)
	{
		float MaxHealth = 0.0f;
		float MaxMana = 0.0f;

		// Проверяем, есть ли в attributesData нужные ключи
		if (const FAttributeDataStruct* HealthAttr = MOBData.mobAttributes.attributesData.Find(TEXT("max_health")))
		{
			MaxHealth = HealthAttr->attributeValue;
		}
		if (const FAttributeDataStruct* ManaAttr = MOBData.mobAttributes.attributesData.Find(TEXT("max_mana")))
		{
			MaxMana = ManaAttr->attributeValue;
		}

		// Обновляем UI если: 
		// 1. Изменились параметры
		// 2. UI еще не была инициализирована
		// 3. Есть данные для отображения (mobID != 0)
		if ((LastHealth != MOBData.mobCurrentHealth || LastMana != MOBData.mobCurrentMana || !bUIInitialized) && MOBData.mobID != 0)
		{
			// Добавляем логи для отладки
			UE_LOG(LogTemp, Warning, TEXT("MOB %s (ID:%d): Updating Health %d->%d, Mana %d->%d, MaxHP: %f, MaxMP: %f, UI Initialized: %s"), 
				*MOBData.mobName, MOBData.mobID, LastHealth, MOBData.mobCurrentHealth, LastMana, MOBData.mobCurrentMana, MaxHealth, MaxMana, bUIInitialized ? TEXT("true") : TEXT("false"));

			MobHeadInfo->UpdateInfo(
				MOBData.mobCurrentHealth,
				MaxHealth,
				MOBData.mobCurrentMana,
				MaxMana,
				MOBData.mobName,
				MOBData.mobLevel,
				MOBData.bIsAggressive
			);

			// ВАЖНО: Обновляем последние значения после обновления UI
			LastHealth = MOBData.mobCurrentHealth;
			LastMana = MOBData.mobCurrentMana;
			bUIInitialized = true;
		}
	}

	UpdateWidgetScale(DeltaTime);
	UpdateWidgetPosition();
}

void ABasicMOB::UpdateWidgetScale(float DeltaTime)
{
	if (!(MobHeadInfo && HeadWidget)) return;

	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!(PC && PC->PlayerCameraManager)) return;

	static float LastDisplayedScale = 0.0f;
	static float LastDisplayedDistance = 0.0f;

	const FVector CameraLoc = PC->PlayerCameraManager->GetCameraLocation();
	const float Distance = FVector::Dist(CameraLoc, GetActorLocation());

	float Clamped = FMath::Clamp(Distance, MinDistance, MaxDistance);
	float Factor = 1.0f - ((Clamped - MinDistance) / (MaxDistance - MinDistance));
	float TargetScale = FMath::Clamp(Factor, 0.1f, 1.0f) * widgetScaleFactor;

	CurrentWidgetScale = FMath::FInterpConstantTo(CurrentWidgetScale, TargetScale, DeltaTime, 1.f);
	HeadWidget->SetWidgetScale(TargetScale);

	if (FMath::Abs(LastDisplayedScale - CurrentWidgetScale) > 0.01f ||
		FMath::Abs(LastDisplayedDistance - Distance) > 100.0f ||
		GetWorld()->GetTimeSeconds() - LastUpdateTime > 2.0f)
	{
		LastDisplayedScale = CurrentWidgetScale;
		LastDisplayedDistance = Distance;
		LastUpdateTime = GetWorld()->GetTimeSeconds();
	}
}

void ABasicMOB::UpdateWidgetPosition()
{
	if (!MobHeadInfo) return;

	UCapsuleComponent* Capsule = GetCapsuleComponent();
	if (!Capsule) return;

	// Получаем высоту капсулы
	const float CapsuleHalfHeight = Capsule->GetScaledCapsuleHalfHeight();

	// Базовое смещение — над головой
	const float BaseOffset = 40.f;

	// Доп. смещение в зависимости от расстояния
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	float DistanceOffset = 0.f;

	if (PC && PC->PlayerCameraManager)
	{
		const float Distance = FVector::Dist(PC->PlayerCameraManager->GetCameraLocation(), GetActorLocation());
		DistanceOffset = FMath::Clamp((Distance - 500.f) * 0.05f, 0.f, 40.f); // Тонкая настройка
	}

	// Смещаем над капсулой
	const float FinalZ = CapsuleHalfHeight + BaseOffset + DistanceOffset;
	MobHeadInfo->SetRelativeLocation(FVector(0.f, 0.f, FinalZ));
}

// Called to bind functionality to input
void ABasicMOB::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

// ICombatable interface implementations
int32 ABasicMOB::GetMaxHealth_Implementation() const
{
    if (const FAttributeDataStruct* HealthAttr = MOBData.mobAttributes.attributesData.Find(TEXT("max_health")))
    {
        return HealthAttr->attributeValue;
    }
    return 100; // Default value
}

int32 ABasicMOB::GetMaxMana_Implementation() const
{
    if (const FAttributeDataStruct* ManaAttr = MOBData.mobAttributes.attributesData.Find(TEXT("max_mana")))
    {
        return ManaAttr->attributeValue;
    }
    return 100; // Default value
}

void ABasicMOB::SetDead_Implementation(bool bNewDead)
{
    SetMOBIsDead(bNewDead);
    
    if (bNewDead)
    {
        UE_LOG(LogTemp, Warning, TEXT("MOB %d has died"), GetActorId_Implementation());
        OnDeath_Implementation();
    }
}

void ABasicMOB::OnDeath_Implementation()
{
    UE_LOG(LogTemp, Warning, TEXT("MOB %s died"), *MOBData.mobName);
    
    // Clear target when dying
    ClearTarget_Implementation();
    
    // Call the existing Die() method
    Die();
}

void ABasicMOB::SetTarget_Implementation(int32 TargetId, ECasterType TargetType)
{
    CurrentTargetId = TargetId;
    CurrentTargetType = TargetType;
    
    // Also update the MOB data for network sync
    SetMobTargetId(TargetId);
    SetMobTargetType(TargetType == ECasterType::Player ? TEXT("Player") : TEXT("Mob"));
    
    UE_LOG(LogTemp, Log, TEXT("MOB %d set target: %d (%s)"), 
        GetActorId_Implementation(), TargetId, *UEnum::GetValueAsString(TargetType));
}

void ABasicMOB::ClearTarget_Implementation()
{
    CurrentTargetId = 0;
    CurrentTargetType = ECasterType::None;
    
    // Also clear MOB data
    SetMobTargetId(0);
    SetMobTargetType(TEXT(""));
    
    UE_LOG(LogTemp, Log, TEXT("MOB %d cleared target"), GetActorId_Implementation());
}

void ABasicMOB::PlaySkillAnimation_Implementation(const FString& AnimationName, float Duration)
{
    UE_LOG(LogTemp, Log, TEXT("MOB %d playing skill animation: %s (Duration: %.1f)"), 
        GetActorId_Implementation(), *AnimationName, Duration);
    
    // Play sound based on animation type
    if (AnimationName.Contains(TEXT("attack"), ESearchCase::IgnoreCase))
    {
        PlaySoundByName("Attack");
    }
    else if (AnimationName.Contains(TEXT("hit"), ESearchCase::IgnoreCase))
    {
        PlaySoundByName("Hit");
    }
}

void ABasicMOB::ShowDamageEffect_Implementation(int32 Damage, bool bIsCritical, ESkillSchool School)
{
    UE_LOG(LogTemp, Log, TEXT("MOB %d taking %d %s damage (Critical: %s, School: %s)"), 
        GetActorId_Implementation(), Damage, bIsCritical ? TEXT("CRITICAL") : TEXT("normal"),
        bIsCritical ? TEXT("true") : TEXT("false"), *UEnum::GetValueAsString(School));
    
    // Set damage flag for visual feedback
    SetMobIsDamaged(true);
    
    // Play hit sound
    PlaySoundByName("Hit");
}

void ABasicMOB::ShowHealingEffect_Implementation(int32 Healing)
{
    UE_LOG(LogTemp, Log, TEXT("MOB %d healed for %d"), GetActorId_Implementation(), Healing);
    
}

void ABasicMOB::ShowBuffEffect_Implementation(const FAppliedEffectData& Effect)
{
    UE_LOG(LogTemp, Log, TEXT("MOB %d received %s effect: %s (Value: %d, Duration: %.1f)"), 
        GetActorId_Implementation(), *Effect.effectType, *Effect.effectName, Effect.value, Effect.duration);
}

void ABasicMOB::SetupMobVisual(FName MobSlug)
{
	if (!MobDefinitionTable) return;

	const FMobDefinition* Definition = MobDefinitionTable->FindRow<FMobDefinition>(MobSlug, TEXT("Load MOB Definition"));
	if (!Definition) return;

	const FMobVisualData& VisualData = Definition->Visual;

	FStreamableManager& Streamable = UAssetManager::GetStreamableManager();

	Streamable.RequestAsyncLoad(VisualData.SkeletalMesh.ToSoftObjectPath(), [this, &VisualData]()
		{
			if (USkeletalMesh* LoadedMesh = VisualData.SkeletalMesh.Get())
			{
				GetMesh()->SetSkeletalMesh(LoadedMesh);

				const FBoxSphereBounds MeshBounds = LoadedMesh->GetBounds();
				const FVector BoxExtent = MeshBounds.BoxExtent;
				const FVector MeshOrigin = MeshBounds.Origin;

				UCapsuleComponent* Capsule = GetCapsuleComponent();
				if (Capsule)
				{
					float CapsuleRadius = FMath::Max(BoxExtent.X, BoxExtent.Y);
					float CapsuleHalfHeight = BoxExtent.Z;

					Capsule->SetCapsuleRadius(CapsuleRadius);
					Capsule->SetCapsuleHalfHeight(CapsuleHalfHeight);

					// Смещение меша вниз
					float MeshBottom = MeshOrigin.Z - BoxExtent.Z;
					float CapsuleBottom = -Capsule->GetUnscaledCapsuleHalfHeight();
					float MeshOffset = CapsuleBottom - MeshBottom;
					GetMesh()->SetRelativeLocation(FVector(0, 0, MeshOffset));
				}

				//setup MobHeadInfo position
				if (Capsule && MobHeadInfo && GetCapsuleComponent())
				{
					float CapsuleHalfHeight = Capsule->GetUnscaledCapsuleHalfHeight();
					float WidgetOffset = 40.0f; // Можно подобрать опытным путём
					MobHeadInfo->SetRelativeLocation(FVector(0, 0, CapsuleHalfHeight + WidgetOffset));
				}
			}
		});

	Streamable.RequestAsyncLoad(VisualData.AnimBPClass.ToSoftObjectPath(), [this, &VisualData]()
		{
			if (UClass* AnimClass = VisualData.AnimBPClass.Get())
			{
				GetMesh()->SetAnimInstanceClass(AnimClass);
			}
		});

	SetActorScale3D(VisualData.ActorScale);
}

void ABasicMOB::SetupMobAudio(FName MobSlug)
{
	if (!MobDefinitionTable) return;

	const FMobDefinition* Definition = MobDefinitionTable->FindRow<FMobDefinition>(MobSlug, TEXT("Load MOB Audio"));
	if (!Definition) return;

	const FMobAudioData& AudioData = Definition->Audio;
	FStreamableManager& Streamable = UAssetManager::GetStreamableManager();

	// Основные звуки
	TArray<TPair<FName, TSoftObjectPtr<USoundBase>>> SoundsToLoad = {
		{ "Attack", AudioData.AttackSound },
		{ "Aggro",  AudioData.AggroSound },
		{ "Hit",    AudioData.HitSound },
		{ "Death",  AudioData.DeathSound }
	};

	for (const auto& Pair : SoundsToLoad)
	{
		Streamable.RequestAsyncLoad(Pair.Value.ToSoftObjectPath(), [this, Pair]()
			{
				if (USoundBase* Loaded = Pair.Value.Get())
				{
					SoundMap.Add(Pair.Key, Loaded);
				}
			});
	}

	// Idle звуки
	for (const auto& SoundSoft : AudioData.IdleSounds)
	{
		Streamable.RequestAsyncLoad(SoundSoft.ToSoftObjectPath(), [this, SoundSoft]()
			{
				if (USoundBase* Loaded = SoundSoft.Get())
				{
					IdleSounds.Add(Loaded);
				}
			});
	}

	// Run звуки
	for (const auto& SoundSoft : AudioData.RunSounds)
	{
		Streamable.RequestAsyncLoad(SoundSoft.ToSoftObjectPath(), [this, SoundSoft]()
			{
				if (USoundBase* Loaded = SoundSoft.Get())
				{
					RunSounds.Add(Loaded);
				}
			});
	}

	// Walk звуки
	for (const auto& SoundSoft : AudioData.WalkSounds)
	{
		Streamable.RequestAsyncLoad(SoundSoft.ToSoftObjectPath(), [this, SoundSoft]()
			{
				if (USoundBase* Loaded = SoundSoft.Get())
				{
					WalkSounds.Add(Loaded);
				}
			});
	}

	// Таймер на idle
	GetWorld()->GetTimerManager().SetTimer(IdleSoundTimer, this, &ABasicMOB::PlayRandomIdleSound, FMath::RandRange(5.f, 15.f), false);
}

void ABasicMOB::PlayRandomIdleSound()
{
	if (IdleSounds.Num() > 0 && !MOBData.bIsDead)
	{
		int32 Index = FMath::RandRange(0, IdleSounds.Num() - 1);
		AudioComponentMain->SetSound(IdleSounds[Index]);
		AudioComponentMain->Play();
	}

	GetWorld()->GetTimerManager().SetTimer(IdleSoundTimer, this, &ABasicMOB::PlayRandomIdleSound, FMath::RandRange(5.f, 15.f), false);
}

void ABasicMOB::PlaySoundByName(FName SoundName)
{
	if (USoundBase** Sound = SoundMap.Find(SoundName))
	{
		if (*Sound)
		{
			AudioComponentMain->SetSound(*Sound);
			AudioComponentMain->Play();
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("ABasicMOB::PlaySoundByName - Sound '%s' not found"), *SoundName.ToString());
	}
}

//play walk random sound
void ABasicMOB::PlayWalkRandomSound()
{
	//get random sound from WalkSounds
	if (WalkSounds.Num() > 0)
	{
		int32 Index = FMath::RandRange(0, WalkSounds.Num() - 1);
		USoundBase* WalkSound = WalkSounds[Index];
		if (WalkSound)
		{
			AudioComponentSecond->SetSound(WalkSound);
			AudioComponentSecond->Play();
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("ABasicMOB::PlayWalkRandomSound - No walk sounds available"));
	}
}

// play run random sound
void ABasicMOB::PlayRunRandomSound()
{
	if (RunSounds.Num() > 0)
	{
		int32 Index = FMath::RandRange(0, RunSounds.Num() - 1);
		USoundBase* RunSound = RunSounds[Index];
		if (RunSound)
		{
			AudioComponentSecond->SetSound(RunSound);
			AudioComponentSecond->Play();
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("ABasicMOB::PlayRunRandomSound - No run sounds available"));
	}
}

// set mob name text
void ABasicMOB::SetMobNameText(UTextRenderComponent* MobNameTextComponent, const FString& Name)
{
	if (MobNameTextComponent)
	{
		MobNameTextComponent->SetText(FText::FromString(Name));
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("MobNameText not found"));
	}
}

// set mob level text
void ABasicMOB::SetMobLevelText(UTextRenderComponent* MobLevelTextComponent, const FString& Level)
{
	FText LevelText = FText::FromString("LVL: " + Level);

	if (MobLevelTextComponent)
	{
		MobLevelTextComponent->SetText(LevelText);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("MobLevelText not found"));
	}
}

void ABasicMOB::SetMOBTag(const FString& Tag)
{
	Tags.Add(FName(*Tag));
}

FMOBStruct ABasicMOB::GetMOBData() const
{
	return MOBData;
}

void ABasicMOB::SetMOBData(const FMOBStruct& Data)
{
	if (MOBData.mobID == 0 && Data.mobID > 0 && !Data.mobUniqueID.IsEmpty())
	{
		MOBData = Data;
		MOBDataUpdated.Broadcast();
		
		UE_LOG(LogTemp, Warning, TEXT("MOB Data set for %s (ID:%d, UID:%s): HP=%d, MP=%d"), 
			*MOBData.mobName, MOBData.mobID, *MOBData.mobUniqueID, MOBData.mobCurrentHealth, MOBData.mobCurrentMana);

		// Register with combat system now that we have valid data
		// Only register if we have a valid actor ID (converted from UID)
		int32 ActorId = GetActorId_Implementation();
		if (ActorId > 0)
		{
			if (UMyGameInstance* GameInstance = Cast<UMyGameInstance>(GetGameInstance()))
			{
				if (UCombatSystemManager* CombatManager = GameInstance->GetCombatSystemManager())
				{
					// Убедимся что объект валиден перед регистрацией
					if (IsValid(this) && !IsActorBeingDestroyed())
					{
						// Create TScriptInterface for registration
						TScriptInterface<ICombatable> CombatableInterface;
						CombatableInterface.SetObject(this);
						CombatableInterface.SetInterface(this);
						
						CombatManager->RegisterCombatable(CombatableInterface);
						UE_LOG(LogTemp, Warning, TEXT("MOB %d (UID:%s) registered with combat system"), ActorId, *MOBData.mobUniqueID);
					}
					else
					{
						UE_LOG(LogTemp, Error, TEXT("MOB %d (UID:%s) is invalid or being destroyed, not registering"), ActorId, *MOBData.mobUniqueID);
					}
				}
			}
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("MOB has invalid ActorId (%d) from UID '%s', not registering with combat system"), 
				ActorId, *MOBData.mobUniqueID);
		}

		// Принудительно обновляем UI при первой установке данных
		ForceUpdateUI();
	}
	else
	{
		if (MOBData.mobID != 0)
		{
			UE_LOG(LogTemp, Error, TEXT("MOB Data already set for %s (ID:%d)"), *MOBData.mobName, MOBData.mobID);
		}
		else if (Data.mobID <= 0)
		{
			UE_LOG(LogTemp, Error, TEXT("Invalid MOB ID (%d) provided"), Data.mobID);
		}
		else if (Data.mobUniqueID.IsEmpty())
		{
			UE_LOG(LogTemp, Error, TEXT("Empty MOB UID provided for MOB ID %d"), Data.mobID);
		}
	}

	if (MOBData.bIsDead)
	{
		Die();
	}
}

void ABasicMOB::SetMOBId(const int& MOBId)
{
	MOBData.mobID = MOBId;
}

void ABasicMOB::SetMOBUId(const FString& MOBUId)
{
	MOBData.mobUniqueID = MOBUId;
}

void ABasicMOB::SetMobTargetId(const int32& TargetId)
{
	MOBData.mobTargetId = TargetId;

	if (MOBMovementComponent)
	{
		MOBMovementComponent->SetTargetId(TargetId);
	}
}

void ABasicMOB::SetMobTargetType(const FString& TargetType)
{
	MOBData.mobTargetType = TargetType;

	if (MOBMovementComponent)
	{
		MOBMovementComponent->SetTargetType(TargetType);
	}
}

void ABasicMOB::SetMOBZoneId(const int& MOBZoneId)
{
	MOBData.mobZoneID = MOBZoneId;
}

void ABasicMOB::SetMOBName(const FString& MOBName)
{
	MOBData.mobName = MOBName;
}

void ABasicMOB::SetMOBRace(const FString& MOBRace)
{
	MOBData.mobRace = MOBRace;
}

void ABasicMOB::SetMOBLevel(const int& MOBLevel)
{
	MOBData.mobLevel = MOBLevel;
}

void ABasicMOB::SetMOBCurrentHealth(const int& MOBCurrentHealth)
{
	MOBData.mobCurrentHealth = MOBCurrentHealth;
}

void ABasicMOB::SetMOBCurrentMana(const int& MOBCurrentMana)
{
	MOBData.mobCurrentMana = MOBCurrentMana;
}

// Improve the position setting function
void ABasicMOB::SetMOBPosition(const FPositionDataStruct& MOBPosition, const FString& PacketTimestamp)
{
	// Store the original position for reference
	MOBData.mobPosition = MOBPosition;
}

void ABasicMOB::SetMOBAttributes(const FAttributesDataStruct& MOBAttributes)
{
	MOBData.mobAttributes = MOBAttributes;
	
	// Если UI еще не инициализирован и у нас есть данные, принудительно обновляем UI
	if (!bUIInitialized && MOBData.mobID != 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("SetMOBAttributes: Force updating UI for MOB %s (ID:%d)"), *MOBData.mobName, MOBData.mobID);
		ForceUpdateUI();
	}
}

void ABasicMOB::SetMOBAttribute(const FString& AttributeSlug, const FAttributeDataStruct& MOBAttribute)
{
	MOBData.mobAttributes.attributesData.Add(AttributeSlug, MOBAttribute);
	
	// Если добавляем ключевые атрибуты (здоровье/мана) и UI не инициализирован, обновляем UI
	if (!bUIInitialized && MOBData.mobID != 0 && (AttributeSlug == TEXT("max_health") || AttributeSlug == TEXT("max_mana")))
	{
		UE_LOG(LogTemp, Warning, TEXT("SetMOBAttribute: Force updating UI for MOB %s (ID:%d) after setting %s"), *MOBData.mobName, MOBData.mobID, *AttributeSlug);
		ForceUpdateUI();
	}
}

void ABasicMOB::SetMOBIsDead(const bool& bIsDead)
{
	MOBData.bIsDead = bIsDead;
}

void ABasicMOB::SetMOBIsAggressive(const bool& bIsAggressive)
{
	MOBData.bIsAggressive = bIsAggressive;
}

void ABasicMOB::SetMOBIsMoving(const bool& bIsMoving)
{
	MOBData.bIsMoving = bIsMoving;
}

void ABasicMOB::SetMobIsDamaged(const bool& bIsDamaged)
{
	if (!MOBData.bIsDead)
	{
		MOBData.bIsGotDamage = bIsDamaged;

		// If damage flag is set to true, schedule it to be reset after a delay
		if (bIsDamaged && GetWorld())
		{
			// Cancel any existing timers for damage reset
			GetWorld()->GetTimerManager().ClearTimer(DamageFlagResetTimer);

			// Set a timer to reset the damage flag after the specified time
			GetWorld()->GetTimerManager().SetTimer(
				DamageFlagResetTimer,
				[this]() {
					MOBData.bIsGotDamage = false;
					UE_LOG(LogTemp, Verbose, TEXT("Damage flag auto-reset for MOB %s (ID:%d)"), *MOBData.mobName, MOBData.mobID);
				},
				DamageFlagResetTime,
				false); // false = no looping
		}
	}
	else
	{
		MOBData.bIsGotDamage = false; // Если моб мертв, то не может быть поврежден
	}
}

// Set last timestamp for the MOB
void ABasicMOB::SetLastTimestamp(const FString& Timestamp)
{
	lastTimestamp = Timestamp;
}

FString ABasicMOB::GetMobName() const
{
	return MOBData.mobName;
}

FString ABasicMOB::GetMOBUId() const
{
	return MOBData.mobUniqueID;
}

int ABasicMOB::GetMOBId() const
{
	return MOBData.mobID;
}

FString ABasicMOB::GetMOBRace() const
{
	return MOBData.mobRace;
}

int ABasicMOB::GetMOBLevel() const
{
	return MOBData.mobLevel;
}

int ABasicMOB::GetMOBCurrentHealth() const
{
	return MOBData.mobCurrentHealth;
}

int ABasicMOB::GetMOBCurrentMana() const
{
	return MOBData.mobCurrentMana;
}

FVector ABasicMOB::GetMOBPosition() const
{
	return FVector(MOBData.mobPosition.positionX, MOBData.mobPosition.positionY, MOBData.mobPosition.positionZ);
}

FAttributesDataStruct ABasicMOB::GetMOBAttributes() const
{
	return MOBData.mobAttributes;
}

bool ABasicMOB::GetMOBIsDead() const
{
	return MOBData.bIsDead;
}

bool ABasicMOB::GetMOBIsAggressive() const
{
	return MOBData.bIsAggressive;
}

//get mob is moving
bool ABasicMOB::GetMOBIsMoving() const
{
	return MOBData.bIsMoving;
}

//get mob is damaged
bool ABasicMOB::GetMOBIsDamaged() const
{
	if (!MOBData.bIsDead)
	{
		return MOBData.bIsGotDamage;
	}

	return false;
}

// Check if MOB can be harvested
bool ABasicMOB::CanBeHarvested() const
{
	// MOB must be dead and not yet harvested
	return MOBData.bIsDead && !MOBData.bHasBeenHarvested;
}

// Check if MOB has been harvested
bool ABasicMOB::HasBeenHarvested() const
{
	return MOBData.bHasBeenHarvested;
}

// Set MOB as harvested
void ABasicMOB::SetHarvested(bool bHarvested)
{
	MOBData.bHasBeenHarvested = bHarvested;
	
	if (bHarvested)
	{
		UE_LOG(LogTemp, Warning, TEXT("MOB %s (ID:%d) has been harvested"), *MOBData.mobName, MOBData.mobID);
	}
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
		float MaxMana = 0.0f;

		// Проверяем, есть ли в attributesData нужные ключи
		if (const FAttributeDataStruct* HealthAttr = MOBData.mobAttributes.attributesData.Find(TEXT("max_health")))
		{
			MaxHealth = HealthAttr->attributeValue;
		}

		if (const FAttributeDataStruct* ManaAttr = MOBData.mobAttributes.attributesData.Find(TEXT("max_mana")))
		{
			MaxMana = ManaAttr->attributeValue;
		}

		UE_LOG(LogTemp, Warning, TEXT("ForceUpdateUI for MOB %s (ID:%d): HP=%d/%f, MP=%d/%f"), 
			*MOBData.mobName, MOBData.mobID, MOBData.mobCurrentHealth, MaxHealth, MOBData.mobCurrentMana, MaxMana);

		MobHeadInfo->UpdateInfo(
			MOBData.mobCurrentHealth,
			MaxHealth,
			MOBData.mobCurrentMana,
			MaxMana,
			MOBData.mobName,
			MOBData.mobLevel,
			MOBData.bIsAggressive
		);

		// Обновляем последние значения и помечаем UI как инициализированный
		LastHealth = MOBData.mobCurrentHealth;
		LastMana = MOBData.mobCurrentMana;
		bUIInitialized = true;
		
		// Применяем начальный масштаб виджета при инициализации
		if (HeadWidget)
		{
			// Устанавливаем масштаб виджета на основе настроек по умолчанию
			float InitialScale = widgetScaleFactor;
			HeadWidget->SetWidgetScale(InitialScale);
			
			// Делаем виджет видимым
			MobHeadInfo->SetVisibility(true);
			
			UE_LOG(LogTemp, Warning, TEXT("ForceUpdateUI: Setting initial widget scale to %f for MOB %s"), InitialScale, *MOBData.mobName);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("ForceUpdateUI: HeadWidget is null for MOB %s"), *MOBData.mobName);
		}
	}
}

void ABasicMOB::InitializeUIDelayed()
{
	// Пытаемся инициализировать UI, если есть данные
	if (MOBData.mobID != 0 && !bUIInitialized)
	{
		UE_LOG(LogTemp, Warning, TEXT("InitializeUIDelayed for MOB %s (ID:%d)"), *MOBData.mobName, MOBData.mobID);
		ForceUpdateUI();
	}
	
	// Убедимся, что HeadWidget проинициализирован 
	if (!HeadWidget)
	{
		HeadWidget = Cast<UW_MOBHeadInfoWidget>(MobHeadInfo->GetUserWidgetObject());
		if (HeadWidget)
		{
			MobHeadInfo->SetVisibility(true);
			UE_LOG(LogTemp, Warning, TEXT("InitializeUIDelayed: Successfully initialized HeadWidget for MOB %s"), *MOBData.mobName);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("InitializeUIDelayed: Failed to get HeadWidget for MOB %s"), *MOBData.mobName);
		}
	}
}

void ABasicMOB::Die()
{
	PlaySoundByName("Death");

	// set target id to 0
	SetMobTargetId(0);

	// set aggressive to false
	SetMOBIsAggressive(false);

	// dubug mob is dead
	UE_LOG(LogTemp, Warning, TEXT("MOB %s (ID:%d) has died."), *MOBData.mobName, MOBData.mobID);

	// Отключаем взаимодействие
	SetActorEnableCollision(false);
}