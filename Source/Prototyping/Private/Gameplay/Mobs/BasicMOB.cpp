// Fill out your copyright notice in the Description page of Project Settings.


#include "Gameplay/Mobs/BasicMOB.h"
#include "Gameplay/Players/BasicPlayer.h"
#include "Gameplay/Mobs/MOBAnimInstance.h"
#include "Gameplay/Combat/CombatSystemManager.h"
#include "MyGameInstance.h"
#include "Services/LocalizationSubsystem.h"
#include "Gameplay/UI/FloatingCombatTextManager.h"
#include "Gameplay/UI/DamageTextWidget.h"
#include "UI/UIManager.h"
#include "UObject/ConstructorHelpers.h"
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

// Convert ESkillSchool to EDamageType for floating combat text
static EDamageType MOBSchoolToDamageType(ESkillSchool School)
{
	switch (School)
	{
	case ESkillSchool::Fire:    return EDamageType::Fire;
	case ESkillSchool::Ice:     return EDamageType::Ice;
	default:                    return EDamageType::Physical;
	}
}

// Spawn a one-shot SFX with the correct SoundClass set BEFORE Play() is called.
// SpawnSoundAtLocation calls Play() internally so any override set afterwards is
// ignored by the audio engine.  This wrapper uses SpawnSoundAttached with
// bAutoActivate=false so we can stamp SoundClassOverride first.
static UAudioComponent* SpawnSFXAttached(AActor* Owner, USoundBase* Sound,
	const FVector& WorldLocation, float VolumeMultiplier = 1.0f)
{
	if (!Owner || !Sound) { return nullptr; }
	USoundClass* SFXClass = nullptr;
	if (UMyGameInstance* GI = Cast<UMyGameInstance>(Owner->GetGameInstance()))
	{
		if (GI->AudioManager) { SFXClass = GI->AudioManager->SFXClass; }
	}
	if (!SFXClass)
	{
		// No AudioManager — fall back to plain spawn so sound still plays.
		return UGameplayStatics::SpawnSoundAtLocation(
			Owner, Sound, WorldLocation, FRotator::ZeroRotator, VolumeMultiplier);
	}
	UAudioComponent* AC = UGameplayStatics::SpawnSoundAttached(
		Sound, Owner->GetRootComponent(), NAME_None,
		WorldLocation, FRotator::ZeroRotator, EAttachLocation::KeepWorldPosition,
		/*bStopWhenAttachedToDestroyed=*/true,
		VolumeMultiplier, 1.0f, 0.0f, nullptr, nullptr,
		/*bAutoActivate=*/false);
	if (AC)
	{
		AC->SoundClassOverride = SFXClass;
		AC->bAutoDestroy = true;
		AC->Play();
	}
	return AC;
}

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
		// Server is authoritative — client-side push forces only cause jitter.
		CMC->bEnablePhysicsInteraction = false;
	}

	// Mobs must not physically push or block each other.
	// The server is authoritative over all positions, so client-side pawn-vs-pawn
	// physics would only produce jitter and visual overlap fights.
	// Keep WorldStatic/WorldDynamic blocking so ground traces and line-of-sight
	// checks still work correctly.
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	}



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

	// Inject TimeSyncService into movement component so dead-reckoning
	// uses the calibrated clock offset instead of raw wall-clock delta
	if (UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance()))
	{
		if (MOBMovementComponent)
		{
			MOBMovementComponent->SetTimeSyncService(GI->GetTimeSyncService());
		}

		// Route mob audio through the SFX SoundClass so the SFX volume slider works
		if (GI->AudioManager && GI->AudioManager->SFXClass)
		{
			if (AudioComponentMain)  { AudioComponentMain->SoundClassOverride  = GI->AudioManager->SFXClass; }
			if (AudioComponentSecond) { AudioComponentSecond->SoundClassOverride = GI->AudioManager->SFXClass; }
		}


		// Register in O(1) actor registry so FindTargetActor skips GetAllActorsOfClass
		const int32 Uid = GetActorId_Implementation();
		if (GI->MOBManager && Uid > 0)
		{
			GI->MOBManager->RegisterMob(Uid, this);
		}
	}

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
    // Unregister from actor registry
    if (UMyGameInstance* GameInstance = Cast<UMyGameInstance>(GetGameInstance()))
    {
        const int32 Uid = GetActorId_Implementation();
        if (GameInstance->MOBManager && Uid > 0)
        {
            GameInstance->MOBManager->UnregisterMob(Uid);
        }
    }

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

void ABasicMOB::OnReceiveSkillInitiation(const FSkillInitiationData& SkillData)
{
	// Broadcast to Blueprint / AnimBP for cast animation
	OnSkillInitiated.Broadcast(SkillData, SkillData.animationDuration);

	// This method is only called by CombatSystemManager when this mob IS the caster,
	// so we unconditionally drive the animation and hit-point notify binding.
	PlaySkillAnimation_Implementation(SkillData.animationName, SkillData.skillSlug, SkillData.animationDuration);

	if (UMOBAnimInstance* AnimInst = Cast<UMOBAnimInstance>(GetMesh()->GetAnimInstance()))
	{
		if (HitPointDelegateHandle.IsValid())
		{
			AnimInst->OnHitPoint.Remove(HitPointDelegateHandle);
			HitPointDelegateHandle.Reset();
		}

		if (UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance()))
		{
			if (UCombatSystemManager* CombatMgr = GI->GetCombatSystemManager())
			{
				HitPointDelegateHandle = AnimInst->OnHitPoint.AddLambda([CombatMgr](int32 InCasterId)
				{
					if (IsValid(CombatMgr))
					{
						CombatMgr->NotifyHitPoint(InCasterId);
					}
				});
			}
		}

		AnimInst->StartAttack(SkillData);
	}
}

void ABasicMOB::OnReceiveSkillResult(const FSkillResultData& SkillResult)
{
	// Update health/mana with authoritative server values
	if (SkillResult.targetId == GetActorId_Implementation())
	{
		SetMOBCurrentHealth(SkillResult.finalTargetHealth);
		SetMOBCurrentMana(SkillResult.finalTargetMana);

		if (!SkillResult.isMissed)
		{
			if (SkillResult.damage > 0)
			{
				SetMobIsDamaged(true);

				if (UMOBAnimInstance* AnimInst = Cast<UMOBAnimInstance>(GetMesh()->GetAnimInstance()))
				{
					AnimInst->NotifyHit();
				}
			}
		}

		if (SkillResult.targetDied)
		{
			SetMOBIsDead(true);
			Die();
			OnMOBDied.Broadcast();
		}

		ForceUpdateUI();
	}

	OnSkillResult.Broadcast(SkillResult);
}

void ABasicMOB::OnReceiveEffectTick(const FEffectTickData& EffectData)
{
	// characterId in effectTick matches mobUniqueID (numeric form)
	if (EffectData.characterId != GetActorId_Implementation())
	{
		return;
	}

	SetMOBCurrentHealth(EffectData.newHealth);
	SetMOBCurrentMana(EffectData.newMana);

	// Damage-over-time: flag as damaged for visual feedback
	if (EffectData.value > 0 && EffectData.effectTypeSlug.Contains(TEXT("damage")))
	{
		SetMobIsDamaged(true);
	}

	if (EffectData.targetDied)
	{
		SetMOBIsDead(true);
		Die();
		OnMOBDied.Broadcast();
	}

	ForceUpdateUI();
	OnEffectTick.Broadcast(EffectData);
}

void ABasicMOB::OnReceiveTargetLost()
{
	SetMobTargetId(0);
	SetMobTargetType(TEXT(""));
	SetMOBIsAggressive(false);
	bAggroLockedOut = true;

	if (MobHeadInfo)
	{
		MobHeadInfo->UpdateMobAggressive(false);
	}

	if (UMOBAnimInstance* AnimInst = Cast<UMOBAnimInstance>(GetMesh()->GetAnimInstance()))
	{
		AnimInst->NotifyTargetLost();
	}

	OnMOBTargetLost.Broadcast();
}

void ABasicMOB::OnReceiveServerPacket(const FPositionDataStruct& MOBPosition)
{
	MOBData.mobPosition = MOBPosition;

	if (MOBMovementComponent)
	{
		MOBMovementComponent->OnReceiveServerPacket(MOBPosition);
	}
}

void ABasicMOB::OnReceiveMovePacket(const FMobMoveEntryStruct& MoveEntry, int64 ServerSendMs, int64 ClientRecvMs)
{
	// Update stored position.
	MOBData.mobPosition = MoveEntry.position;

	// Only states 1 (CHASING) and 2/3/4 (attack cycle) mean the mob is
	// actively targeting a player and should display as aggressive.
	// States 5 (RETURNING), 6 (EVADING), 7 (FLEEING) mean the mob already
	// lost or is disengaging — aggro is cleared via OnReceiveTargetLost.
	// State 0 (PATROLLING) is neutral.
	const bool bActivelyEngaging = (MoveEntry.combatState >= 1 && MoveEntry.combatState <= 4);
	if (bActivelyEngaging)
	{
		if (bAggroLockedOut)
		{
			// New aggro after a target-lost: unlock and enable aggro
			bAggroLockedOut = false;
		}
		MOBData.bIsAggressive = true;
	}

	// OnReceiveMovePacket handles CombatState internally (including PrevCombatState
	// tracking for transition detection), so do NOT call SetCombatState separately.
	if (MOBMovementComponent)
	{
		MOBMovementComponent->OnReceiveMovePacket(MoveEntry, ServerSendMs, ClientRecvMs);
	}
}

void ABasicMOB::OnReceiveMobHealthUpdate(const FMobHealthUpdateStruct& HealthUpdate)
{
	// Server sends this during RETURNING state (leash regen 10%/sec).
	// Simply update HP and refresh the head-info widget.
	SetMOBCurrentHealth(HealthUpdate.currentHealth);
	ForceUpdateUI();
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

	if (IsValid(AudioComponentSecond))
	{
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
	HeadWidget->SetWidgetScale(TargetScale);

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

	// Получаем высоту капсулы
	const float CapsuleHalfHeight = Capsule->GetScaledCapsuleHalfHeight();

	// Базовое смещение — над головой
	const float BaseOffset = 40.f;

	// Доп. смещение в зависимости от расстояния
	UWorld* World = GetWorld();
	if (!World) return;

	APlayerController* PC = World->GetFirstPlayerController();
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

void ABasicMOB::SetIsAggressiveState_Implementation(int32 TargetId, ECasterType TargetType, bool bIsAggressive)
{
	SetMOBIsAggressive(bIsAggressive);

	// Explicit aggro event from server — unlock movePacket aggro gate.
	if (bIsAggressive)
	{
		bAggroLockedOut = false;
		// Play aggro sound when the mob first becomes aggressive
		PlaySoundByName("Aggro");
	}

	if (MobHeadInfo)
	{
		MobHeadInfo->UpdateMobAggressive(bIsAggressive);
	}

	// Auto-lock: if mob just aggroed the local player and player has no lock
	if (bIsAggressive && TargetType == ECasterType::Player)
	{
		if (UWorld* W = GetWorld())
		{
			APlayerController* PC = W->GetFirstPlayerController();
			if (PC)
			{
				if (ABasicPlayer* Player = Cast<ABasicPlayer>(PC->GetPawn()))
				{
					if (!Player->GetLockedTarget())
					{
						Player->SetLockedTarget(this);
					}
					else if (Player->GetLockedTarget() == this)
					{
						// This mob is already locked — refresh the target frame name color
						if (UUIManager* UIMgr = Player->GetUIManager())
						{
							const int32 MaxHP = MOBData.mobAttributes.attributesData.Contains(TEXT("max_health"))
								? MOBData.mobAttributes.attributesData[TEXT("max_health")].attributeValue
								: 100;
							UIMgr->ShowMobTargetFrame(
								MOBData.mobSlug,
								MOBData.mobName,
								MOBData.mobLevel,
								MOBData.mobCurrentHealth,
								MaxHP,
								bIsAggressive,
								CachedIcon);
						}
					}
				}
			}
		}
	}

	UE_LOG(LogTemp, Log, TEXT("MOB %d is now %s"), 
		GetActorId_Implementation(), bIsAggressive ? TEXT("Aggressive") : TEXT("Passive"));
}

void ABasicMOB::PlaySkillAnimation_Implementation(const FString& AnimationName, const FString& SkillSlug, float Duration)
{
    UE_LOG(LogTemp, Log, TEXT("MOB %d playing skill animation: %s (slug: %s, Duration: %.1f)"), 
        GetActorId_Implementation(), *AnimationName, *SkillSlug, Duration);

    // Remember which skill is being cast so ShowDamageEffect can look up hitSound/hitEffect
    CurrentSkillName = SkillSlug.IsEmpty() ? AnimationName : SkillSlug;

    // --- Cast sound + cast particle from SkillDefinitionRepository ---
    bool bCastSoundPlayed = false;
    if (UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance()))
    {
        if (USkillDefinitionRepository* Repo = GI->GetSkillDefinitionRepository())
        {
            const FString LookupKey = SkillSlug.IsEmpty() ? AnimationName : SkillSlug;
            const FSkillDefinitionData& Def = Repo->GetDefinition(LookupKey);

			if (!Def.castSound.IsNull())
			{
				if (USoundBase* Sound = Def.castSound.LoadSynchronous())
				{
					SpawnSFXAttached(this, Sound, GetActorLocation());
					bCastSoundPlayed = true;
				}
			}

			// Cast start voice: Priority 1 — skill-specific (same for any entity casting this skill)
			//                   Priority 2 — mob's own voice pool (CastVoiceSounds)
			{
				USoundBase* VoiceToPlay = nullptr;
				if (!Def.castStartVoice.IsNull())
				{
					VoiceToPlay = Def.castStartVoice.LoadSynchronous();
				}
				else if (CastVoiceSounds.Num() > 0)
				{
					VoiceToPlay = CastVoiceSounds[FMath::RandRange(0, CastVoiceSounds.Num() - 1)];
				}
				if (VoiceToPlay)
				{
					SpawnSFXAttached(this, VoiceToPlay, GetActorLocation());
				}
			}

            // Niagara cast effect
            if (!Def.castEffectNiagara.IsNull())
            {
                if (UNiagaraSystem* NiagaraEffect = Def.castEffectNiagara.LoadSynchronous())
                {
                    FVector CastLoc = GetActorLocation();
                    FRotator CastRot = GetActorRotation();
                    if (Def.CastSocketName != NAME_None && GetMesh() && GetMesh()->DoesSocketExist(Def.CastSocketName))
                    {
                        CastLoc = GetMesh()->GetSocketLocation(Def.CastSocketName);
                        CastRot = GetMesh()->GetSocketRotation(Def.CastSocketName);
                    }
                    UNiagaraFunctionLibrary::SpawnSystemAtLocation(
                        GetWorld(), NiagaraEffect, CastLoc, CastRot);
                }
            }

			// Swing sound: Priority 1 — mob-specific (FMobAudioData::SwingSound via SoundMap["Swing"])
			// Priority 2 — skill-level generic fallback (FSkillDefinitionData::swingSound)
			// This mirrors the player: Weapon.EquippedSwingSound → SkillData.swingSound
			USoundBase* SwingToPlay = nullptr;
			if (USoundBase** MobSwing = SoundMap.Find(FName("Swing")))
			{
				SwingToPlay = *MobSwing;
			}
			else if (!Def.swingSound.IsNull())
			{
				SwingToPlay = Def.swingSound.LoadSynchronous();
			}
			if (SwingToPlay)
			{
				SpawnSFXAttached(this, SwingToPlay, GetActorLocation());
			}
        }
    }

    // Legacy per-type sound fallback — only when DataTable has no castSound assigned
    if (!bCastSoundPlayed)
    {
        if (AnimationName.Contains(TEXT("attack"), ESearchCase::IgnoreCase))
        {
            PlaySoundByName("Attack");
        }
    }

    // --- Drive the AnimBP attack montage and bind hit-point notify ---
    // This ensures the deferred-hit-point pipeline in CombatSystemManager fires
    // when the animation reaches the impact frame (or via fallback timer).
    if (UMOBAnimInstance* AnimInst = Cast<UMOBAnimInstance>(GetMesh()->GetAnimInstance()))
    {
        // Remove any previous hit-point binding to avoid double-fire
        if (HitPointDelegateHandle.IsValid())
        {
            AnimInst->OnHitPoint.Remove(HitPointDelegateHandle);
            HitPointDelegateHandle.Reset();
        }

        if (UMyGameInstance* GI2 = Cast<UMyGameInstance>(GetGameInstance()))
        {
            if (UCombatSystemManager* CombatMgr = GI2->GetCombatSystemManager())
            {
                HitPointDelegateHandle = AnimInst->OnHitPoint.AddLambda([CombatMgr](int32 InCasterId)
                {
                    if (IsValid(CombatMgr))
                    {
                        CombatMgr->NotifyHitPoint(InCasterId);
                    }
                });
            }
        }

        // Build a minimal FSkillInitiationData so StartAttack can resolve the montage
        FSkillInitiationData InitData;
        InitData.animationName     = AnimationName;
        InitData.animationDuration = Duration;
        InitData.casterId          = GetActorId_Implementation();
        AnimInst->StartAttack(InitData);
    }
}

void ABasicMOB::ShowDamageEffect_Implementation(int32 Damage, bool bIsCritical, ESkillSchool School, bool bIsMissed, bool bIsBlocked, const FString& SkillSlug)
{
	UE_LOG(LogTemp, Log, TEXT("MOB %d taking %d damage (Critical: %s, School: %s, Missed: %s, Blocked: %s, Skill: %s)"), 
		GetActorId_Implementation(), Damage,
		bIsCritical ? TEXT("true") : TEXT("false"), *UEnum::GetValueAsString(School),
		bIsMissed ? TEXT("true") : TEXT("false"),
		bIsBlocked ? TEXT("true") : TEXT("false"),
		*SkillSlug);

	// Set damage flag for visual feedback only on actual hit
	if (!bIsMissed)
	{
		SetMobIsDamaged(true);
	}

	// --- Hit sound + hit particle from SkillDefinitionRepository ---
	bool bHitSoundPlayed = false;
	if (UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance()))
	{
		if (USkillDefinitionRepository* Repo = GI->GetSkillDefinitionRepository())
		{
			const FSkillDefinitionData& Def = Repo->GetDefinition(SkillSlug);

			UE_LOG(LogTemp, Warning, TEXT("[MOB HitSFX] SkillSlug='%s' WeaponImpactType='%s' hitSound=%s"),
				*SkillSlug,
				*Def.WeaponImpactType.ToString(),
				Def.hitSound.IsNull() ? TEXT("NULL") : TEXT("SET"));

			// --- Impact sound: WeaponImpactType × ArmorMaterialType lookup ---
			if (Def.WeaponImpactType != NAME_None)
			{
				UDataTable* ImpactTable = GI->GetImpactSoundsTable();
				UE_LOG(LogTemp, Warning, TEXT("[MOB HitSFX] ImpactTable=%s  MobDefinitionTable=%s  mobSlug='%s'"),
					ImpactTable ? TEXT("OK") : TEXT("NULL"),
					MobDefinitionTable ? TEXT("OK") : TEXT("NULL"),
					*MOBData.mobSlug);

				if (ImpactTable)
				{
					// Resolve ArmorMaterialType from MobDefinitionTable
					FName ArmorMat = NAME_None;
					if (MobDefinitionTable)
					{
						FName SlugKey = FName(*MOBData.mobSlug);
						if (const FMobDefinition* MobDef = MobDefinitionTable->FindRow<FMobDefinition>(SlugKey, TEXT("")))
						{
							ArmorMat = MobDef->ArmorMaterialType;
						}
					}

					UE_LOG(LogTemp, Warning, TEXT("[MOB HitSFX] ArmorMat='%s'"), *ArmorMat.ToString());

					if (ArmorMat != NAME_None)
					{
						FName ImpactKey = FName(*FString::Printf(TEXT("%s_%s"),
							*Def.WeaponImpactType.ToString(), *ArmorMat.ToString()));

						const FImpactSoundData* ImpactRow = ImpactTable->FindRow<FImpactSoundData>(ImpactKey, TEXT(""));
						UE_LOG(LogTemp, Warning, TEXT("[MOB HitSFX] ImpactKey='%s'  RowFound=%s  Sounds=%d"),
							*ImpactKey.ToString(),
							ImpactRow ? TEXT("YES") : TEXT("NO"),
							ImpactRow ? ImpactRow->ImpactSounds.Num() : 0);

						if (ImpactRow)
						{
							if (ImpactRow->ImpactSounds.Num() > 0)
							{
								int32 Idx = FMath::RandRange(0, ImpactRow->ImpactSounds.Num() - 1);
								USoundBase* ImpactSound = ImpactRow->ImpactSounds[Idx].LoadSynchronous();
								UE_LOG(LogTemp, Warning, TEXT("[MOB HitSFX] ImpactSound[%d]=%s"), Idx, ImpactSound ? TEXT("OK") : TEXT("NULL"));
								if (ImpactSound)
									{
										SpawnSFXAttached(this, ImpactSound, GetActorLocation());
										bHitSoundPlayed = true;
									}
							}

							// Spawn impact VFX if defined
							if (!ImpactRow->ImpactVFX.IsNull())
							{
								if (UNiagaraSystem* ImpactVFX = ImpactRow->ImpactVFX.LoadSynchronous())
								{
									FVector HitLoc = GetCombatPosition_Implementation();
									if (Def.HitSocketName != NAME_None && GetMesh() && GetMesh()->DoesSocketExist(Def.HitSocketName))
									{
										HitLoc = GetMesh()->GetSocketLocation(Def.HitSocketName);
									}
									UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), ImpactVFX, HitLoc);
								}
							}
						}
					}
				}
			}

			// Fallback: generic hitSound from skill definition
			if (!bHitSoundPlayed && !Def.hitSound.IsNull())
			{
				USoundBase* Sound = Def.hitSound.LoadSynchronous();
				UE_LOG(LogTemp, Warning, TEXT("[MOB HitSFX] Fallback hitSound=%s"), Sound ? TEXT("OK") : TEXT("NULL (asset not loaded)"));
				if (Sound)
				{
					SpawnSFXAttached(this, Sound, GetActorLocation());
					bHitSoundPlayed = true;
				}
			}

            // Niagara hit effect (preferred over Cascade)
            if (!Def.hitEffectNiagara.IsNull())
            {
                if (UNiagaraSystem* NiagaraEffect = Def.hitEffectNiagara.LoadSynchronous())
                {
                    FVector HitLoc = GetCombatPosition_Implementation();
                    FRotator HitRot = FRotator::ZeroRotator;
                    if (Def.HitSocketName != NAME_None && GetMesh() && GetMesh()->DoesSocketExist(Def.HitSocketName))
                    {
                        HitLoc = GetMesh()->GetSocketLocation(Def.HitSocketName);
                        HitRot = GetMesh()->GetSocketRotation(Def.HitSocketName);
                    }
                    UNiagaraFunctionLibrary::SpawnSystemAtLocation(
                        GetWorld(), NiagaraEffect, HitLoc, HitRot);
                }
            }

            // Critical hit sound (layered on top of regular hit sound)
            if (bIsCritical && !bIsMissed && !Def.critSound.IsNull())
            {
                if (USoundBase* CritSnd = Def.critSound.LoadSynchronous())
                {
                    SpawnSFXAttached(this, CritSnd, GetActorLocation());
                }
            }
        }
    }

    // Legacy fallback hit sound — only when DataTable has no hitSound assigned
    if (!bHitSoundPlayed)
    {
        PlaySoundByName("Hit");
    }

    // NOTE: Floating combat text is handled by DamageEffectHandler::ShowFloatingDamageText
    // to avoid duplicates. Do NOT call FCT->ShowDamage here.

	// --- Hit Stop: freeze this MOB briefly so the impact feels weighty ---
	// Not on miss — missing doesn't interrupt the actor's flow.
	if (!bIsMissed)
	{
		if (UWorld* W = GetWorld())
		{
			CustomTimeDilation = 0.0f;
			FTimerHandle HitStopTimer;
			TWeakObjectPtr<ABasicMOB> WeakSelf(this);
			W->GetTimerManager().SetTimer(HitStopTimer, [WeakSelf]()
			{
				if (WeakSelf.IsValid())
				{
					WeakSelf->CustomTimeDilation = 1.0f;
				}
			}, 0.06f, false);
		}
	}
}

void ABasicMOB::ShowHealingEffect_Implementation(int32 Healing, const FString& SkillSlug)
{
    UE_LOG(LogTemp, Log, TEXT("MOB %d healed for %d (skill: %s)"), GetActorId_Implementation(), Healing, *SkillSlug);

    UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance());
    if (!GI) return;

    if (USkillDefinitionRepository* Repo = GI->GetSkillDefinitionRepository())
    {
        const FSkillDefinitionData& Def = Repo->GetDefinition(SkillSlug);

        if (!Def.healSound.IsNull())
        {
            if (USoundBase* Snd = Def.healSound.LoadSynchronous())
            {
                SpawnSFXAttached(this, Snd, GetActorLocation());
            }
        }

        if (!Def.healEffectNiagara.IsNull())
        {
            if (UNiagaraSystem* HealVFX = Def.healEffectNiagara.LoadSynchronous())
            {
                FVector HealLoc = GetCombatPosition_Implementation();
                FRotator HealRot = FRotator::ZeroRotator;
                if (Def.HitSocketName != NAME_None && GetMesh() && GetMesh()->DoesSocketExist(Def.HitSocketName))
                {
                    HealLoc = GetMesh()->GetSocketLocation(Def.HitSocketName);
                    HealRot = GetMesh()->GetSocketRotation(Def.HitSocketName);
                }
                UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), HealVFX, HealLoc, HealRot);
            }
        }
    }
}

void ABasicMOB::ShowBuffEffect_Implementation(const FAppliedEffectData& Effect)
{
    UE_LOG(LogTemp, Log, TEXT("MOB %d received %s effect: %s (Value: %d, Duration: %.1f)"), 
        GetActorId_Implementation(), *Effect.effectType, *Effect.effectName, Effect.value, Effect.duration);

    UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance());
    if (!GI) return;

    UDataTable* EffectTable = GI->GetEffectDefinitionTable();
    if (!EffectTable) return;

    FName RowKey = FName(*Effect.effectName);
    const FEffectDefinitionRow* Row = EffectTable->FindRow<FEffectDefinitionRow>(RowKey, TEXT(""));
    if (!Row) return;

    // Play apply sound
    if (!Row->ApplySound.IsNull())
    {
        if (USoundBase* Snd = Row->ApplySound.LoadSynchronous())
        {
            SpawnSFXAttached(this, Snd, GetActorLocation());
        }
    }

    // Spawn apply VFX
    if (!Row->ApplyVFX.IsNull())
    {
        if (UNiagaraSystem* VFX = Row->ApplyVFX.LoadSynchronous())
        {
            UNiagaraFunctionLibrary::SpawnSystemAttached(
                VFX, GetMesh(), NAME_None,
                FVector::ZeroVector, FRotator::ZeroRotator,
                EAttachLocation::SnapToTarget, true);
        }
    }
}

void ABasicMOB::SetupMobVisual(FName MobSlug)
{
	if (!MobDefinitionTable) return;

	const FMobDefinition* Definition = MobDefinitionTable->FindRow<FMobDefinition>(MobSlug, TEXT("Load MOB Definition"));
	if (!Definition) return;

	const FMobVisualData& VisualData = Definition->Visual;

	FStreamableManager& Streamable = UAssetManager::GetStreamableManager();

	// Load the icon synchronously so it is always ready when the player targets this mob.
	if (!VisualData.Icon.IsNull())
	{
		CachedIcon = VisualData.Icon.LoadSynchronous();
	}

	// Capture soft pointers by value -- VisualData is a local stack reference that will
	// be gone by the time the async callbacks fire (dangling reference -> crash).
	TSoftObjectPtr<USkeletalMesh> SoftMesh   = VisualData.SkeletalMesh;
	TSoftClassPtr<UAnimInstance>  SoftAnimBP = VisualData.AnimBPClass;

	TWeakObjectPtr<ABasicMOB> WeakThis(this);
	Streamable.RequestAsyncLoad(SoftMesh.ToSoftObjectPath(), [WeakThis, SoftMesh]()
		{
			if (!WeakThis.IsValid()) { return; }
			ABasicMOB* Self = WeakThis.Get();
			if (USkeletalMesh* LoadedMesh = SoftMesh.Get())
			{
				Self->GetMesh()->SetSkeletalMesh(LoadedMesh);

				const FBoxSphereBounds MeshBounds = LoadedMesh->GetBounds();
				const FVector BoxExtent = MeshBounds.BoxExtent;
				const FVector MeshOrigin = MeshBounds.Origin;

				UCapsuleComponent* Capsule = Self->GetCapsuleComponent();
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
						Self->GetMesh()->SetRelativeLocation(FVector(0, 0, MeshOffset));
						// Align mesh forward (+X of actor == face direction).
						// UE skeletal meshes are exported facing +Y, so we rotate -90° Yaw
						// to make the mesh face +X (the actor's forward axis).
						Self->GetMesh()->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
				}

				//setup MobHeadInfo position
				if (Capsule && Self->MobHeadInfo && Self->GetCapsuleComponent())
				{
					float CapsuleHalfHeight = Capsule->GetUnscaledCapsuleHalfHeight();
					float WidgetOffset = 40.0f; // Можно подобрать опытным путём
					Self->MobHeadInfo->SetRelativeLocation(FVector(0, 0, CapsuleHalfHeight + WidgetOffset));
				}
			}
		});

	Streamable.RequestAsyncLoad(SoftAnimBP.ToSoftObjectPath(), [WeakThis, SoftAnimBP]()
		{
			if (!WeakThis.IsValid()) { return; }
			if (UClass* AnimClass = SoftAnimBP.Get())
			{
				WeakThis->GetMesh()->SetAnimInstanceClass(AnimClass);
			}
		});

	SetActorScale3D(VisualData.ActorScale);

	// Cache per-mob combat hit height from the DataTable row
	CachedCombatHitHeight = VisualData.CombatHitHeight;
}

void ABasicMOB::SetupMobAudio(FName MobSlug)
{
	if (!MobDefinitionTable) return;

	const FMobDefinition* Definition = MobDefinitionTable->FindRow<FMobDefinition>(MobSlug, TEXT("Load MOB Audio"));
	if (!Definition) return;

	FStreamableManager& Streamable = UAssetManager::GetStreamableManager();
	TWeakObjectPtr<ABasicMOB> WeakThis(this);

	// ---------------------------------------------------------------
	// Priority 1 — Entity Audio Profile (Data-Driven, shareable rows)
	// Set AudioProfileId on the FMobDefinition row in the DataTable.
	// ---------------------------------------------------------------
	if (!Definition->AudioProfileId.IsNone())
	{
		UMyGameInstance* GI = Cast<UMyGameInstance>(GetGameInstance());
		const FEntityAudioProfile* Profile = nullptr;
		if (GI)
		{
			if (UEntityAudioRepository* Repo = GI->GetEntityAudioRepository())
			{
				Profile = Repo->FindProfile(Definition->AudioProfileId);
			}
		}

		if (Profile)
		{
			// Single sounds → SoundMap
			TArray<TPair<FName, TSoftObjectPtr<USoundBase>>> SoundsToLoad = {
				{ "Attack", Profile->AttackGeneric },
				{ "Aggro",  Profile->Aggro },
				{ "Hit",    Profile->HitReceived },
				{ "Death",  Profile->Death },
				{ "Swing",  Profile->SwingSound }
			};
			for (const auto& Pair : SoundsToLoad)
			{
				Streamable.RequestAsyncLoad(Pair.Value.ToSoftObjectPath(), [WeakThis, Pair]()
					{
						if (!WeakThis.IsValid()) return;
						if (USoundBase* Loaded = Pair.Value.Get())
						{
							WeakThis->SoundMap.Add(Pair.Key, Loaded);
						}
					});
			}

			// Array sounds — loaded into respective mob arrays
			auto LoadArray = [&](const TArray<TSoftObjectPtr<USoundBase>>& Source, TArray<USoundBase*>& Target)
			{
				for (const auto& SoundSoft : Source)
				{
					Streamable.RequestAsyncLoad(SoundSoft.ToSoftObjectPath(), [WeakThis, SoundSoft, &Target]()
						{
							if (!WeakThis.IsValid()) return;
							if (USoundBase* Loaded = SoundSoft.Get())
							{
								Target.Add(Loaded);
							}
						});
				}
			};

			LoadArray(Profile->IdleAmbient,       IdleSounds);
			LoadArray(Profile->FootstepsRun,       RunSounds);
			LoadArray(Profile->FootstepsWalk,      WalkSounds);
			LoadArray(Profile->VoiceAttack,        AttackVoiceSounds);
			LoadArray(Profile->VoiceCastStart,     CastVoiceSounds);
			LoadArray(Profile->VoiceCastRelease,   ReleaseVoiceSounds);

			// Idle ambient timer
			GetWorld()->GetTimerManager().SetTimer(IdleSoundTimer, this, &ABasicMOB::PlayRandomIdleSound, FMath::RandRange(5.f, 15.f), false);
			return;  // Profile fully loaded — skip legacy FMobAudioData below
		}
		else
		{
			UE_LOG(LogTemp, Warning,
				TEXT("ABasicMOB::SetupMobAudio: AudioProfileId '%s' not found in EntityAudioProfilesTable for mob '%s'. Falling back to legacy FMobAudioData."),
				*Definition->AudioProfileId.ToString(), *MobSlug.ToString());
		}
	}

	// ---------------------------------------------------------------
	// Priority 2 — Legacy inline FMobAudioData (backward-compatible)
	// Migrate rows to AudioProfileId over time; do not add new sounds here.
	// ---------------------------------------------------------------
	const FMobAudioData& AudioData = Definition->Audio;

	// Основные звуки
	TArray<TPair<FName, TSoftObjectPtr<USoundBase>>> SoundsToLoad = {
		{ "Attack", AudioData.AttackSound },
		{ "Aggro",  AudioData.AggroSound },
		{ "Hit",    AudioData.HitSound },
		{ "Death",  AudioData.DeathSound },
		{ "Swing",  AudioData.SwingSound }
	};

	for (const auto& Pair : SoundsToLoad)
	{
		Streamable.RequestAsyncLoad(Pair.Value.ToSoftObjectPath(), [WeakThis, Pair]()
			{
				if (!WeakThis.IsValid()) { return; }
				if (USoundBase* Loaded = Pair.Value.Get())
				{
					WeakThis->SoundMap.Add(Pair.Key, Loaded);
				}
			});
	}

	// Idle звуки
	for (const auto& SoundSoft : AudioData.IdleSounds)
	{
		Streamable.RequestAsyncLoad(SoundSoft.ToSoftObjectPath(), [WeakThis, SoundSoft]()
			{
				if (!WeakThis.IsValid()) { return; }
				if (USoundBase* Loaded = SoundSoft.Get())
				{
					WeakThis->IdleSounds.Add(Loaded);
				}
			});
	}

	// Run звуки
	for (const auto& SoundSoft : AudioData.RunSounds)
	{
		Streamable.RequestAsyncLoad(SoundSoft.ToSoftObjectPath(), [WeakThis, SoundSoft]()
			{
				if (!WeakThis.IsValid()) { return; }
				if (USoundBase* Loaded = SoundSoft.Get())
				{
					WeakThis->RunSounds.Add(Loaded);
				}
			});
	}

	// Walk звуки
	for (const auto& SoundSoft : AudioData.WalkSounds)
	{
		Streamable.RequestAsyncLoad(SoundSoft.ToSoftObjectPath(), [WeakThis, SoundSoft]()
			{
				if (!WeakThis.IsValid()) { return; }
				if (USoundBase* Loaded = SoundSoft.Get())
				{
					WeakThis->WalkSounds.Add(Loaded);
				}
			});
	}

	// AttackVoice звуки (рык/крик при атаке)
	for (const auto& SoundSoft : AudioData.AttackVoiceSounds)
	{
		Streamable.RequestAsyncLoad(SoundSoft.ToSoftObjectPath(), [WeakThis, SoundSoft]()
			{
				if (!WeakThis.IsValid()) { return; }
				if (USoundBase* Loaded = SoundSoft.Get())
				{
					WeakThis->AttackVoiceSounds.Add(Loaded);
				}
			});
	}

	// CastVoice звуки (голос при начале каста)
	for (const auto& SoundSoft : AudioData.CastVoiceSounds)
	{
		Streamable.RequestAsyncLoad(SoundSoft.ToSoftObjectPath(), [WeakThis, SoundSoft]()
			{
				if (!WeakThis.IsValid()) { return; }
				if (USoundBase* Loaded = SoundSoft.Get())
				{
					WeakThis->CastVoiceSounds.Add(Loaded);
				}
			});
	}

	// ReleaseVoice звуки (голос при выпуске скилла)
	for (const auto& SoundSoft : AudioData.ReleaseVoiceSounds)
	{
		Streamable.RequestAsyncLoad(SoundSoft.ToSoftObjectPath(), [WeakThis, SoundSoft]()
			{
				if (!WeakThis.IsValid()) { return; }
				if (USoundBase* Loaded = SoundSoft.Get())
				{
					WeakThis->ReleaseVoiceSounds.Add(Loaded);
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
		// No walk sounds assigned in editor - suppress log to avoid spam
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
		// No run sounds assigned in editor - suppress log to avoid spam
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

		// Register with MOB registry so mobMoveUpdate packets can find this actor by UID.
		// BeginPlay fires before SetMOBData so the uid is 0 there; we do it here instead.
		int32 ActorId = GetActorId_Implementation();
		if (ActorId > 0)
		{
			if (UMyGameInstance* GameInstance = Cast<UMyGameInstance>(GetGameInstance()))
			{
				if (GameInstance->MOBManager)
				{
					GameInstance->MOBManager->RegisterMob(ActorId, this);
				}
			}
		}

		// Register with combat system now that we have valid data
		// Only register if we have a valid actor ID (converted from UID)
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
	ForceUpdateUI();
}

void ABasicMOB::SetMOBCurrentMana(const int& MOBCurrentMana)
{
	MOBData.mobCurrentMana = MOBCurrentMana;
	ForceUpdateUI();
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
			MOBData.mobCurrentMana,
			MaxMana,
			DisplayName,
			MOBData.mobLevel,
			MOBData.bIsAggressive
		);

		// Обновляем последние значения и помечаем UI как инициализированный
		LastHealth = MOBData.mobCurrentHealth;
		LastMana = MOBData.mobCurrentMana;
		bUIInitialized = true;
		
		if (HeadWidget)
		{
			HeadWidget->SetWidgetScale(widgetScaleFactor);
			// Widget hidden by default - BasicPlayer controls visibility via ShowWidget()
			MobHeadInfo->SetVisibility(false);
		}
	}
}


void ABasicMOB::InitializeUIDelayed()
{
	if (!IsValid(this) || IsActorBeingDestroyed()) return;

	if (MOBData.mobID != 0 && !bUIInitialized)
	{
		ForceUpdateUI();
	}

	if (!HeadWidget && IsValid(MobHeadInfo))
	{
		HeadWidget = Cast<UW_MOBHeadInfoWidget>(MobHeadInfo->GetUserWidgetObject());
		if (HeadWidget)
		{
			MobHeadInfo->SetVisibility(false);
		}
	}
}

void ABasicMOB::Die()
{
	PlaySoundByName("Death");

	// Spawn death VFX if defined in MobDefinitionTable
	if (MobDefinitionTable)
	{
		FName SlugKey = FName(*MOBData.mobSlug);
		if (const FMobDefinition* Def = MobDefinitionTable->FindRow<FMobDefinition>(SlugKey, TEXT("")))
		{
			if (!Def->Visual.DeathVFX.IsNull())
			{
				if (UNiagaraSystem* VFX = Def->Visual.DeathVFX.LoadSynchronous())
				{
					UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), VFX, GetActorLocation());
				}
			}
		}
	}

	// set target id to 0
	SetMobTargetId(0);

	// set aggressive to false
	SetMOBIsAggressive(false);

	if (MobHeadInfo)
	{
		MobHeadInfo->UpdateMobAggressive(false);
	}

	// Notify AnimInstance — triggers death animation, clears all combat states
	if (UMOBAnimInstance* AnimInst = Cast<UMOBAnimInstance>(GetMesh()->GetAnimInstance()))
	{
		AnimInst->NotifyDeath();
	}

	UE_LOG(LogTemp, Warning, TEXT("MOB %s (ID:%d) has died."), *MOBData.mobName, MOBData.mobID);

	// Отключаем взаимодействие
	SetActorEnableCollision(false);
}
