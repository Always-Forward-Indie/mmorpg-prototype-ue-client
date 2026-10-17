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

				// SetSkeletalMesh may reapply the mesh asset's default collision profile,
				// overriding any component-level channel overrides set in the constructor.
				// Re-apply the camera ignore so the spring arm probe never hits this mesh.
				Self->GetMesh()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

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

					// �������� ���� ����
						float MeshBottom = MeshOrigin.Z - BoxExtent.Z;
						float CapsuleBottom = -Capsule->GetUnscaledCapsuleHalfHeight();
						float MeshOffset = CapsuleBottom - MeshBottom;
						Self->GetMesh()->SetRelativeLocation(FVector(0, 0, MeshOffset));
						// Align mesh forward (+X of actor == face direction).
						// UE skeletal meshes are exported facing +Y, so we rotate -90� Yaw
						// to make the mesh face +X (the actor's forward axis).
						Self->GetMesh()->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
				}

				//setup MobHeadInfo position
				if (Capsule && Self->MobHeadInfo && Self->GetCapsuleComponent())
				{
					float CapsuleHalfHeight = Capsule->GetUnscaledCapsuleHalfHeight();
					float WidgetOffset = 40.0f; // ����� ��������� ������� ����
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
	// Priority 1 � Entity Audio Profile (Data-Driven, shareable rows)
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
			if (!Profile->DefaultAttenuation.IsNull())
			{
				DefaultAttenuation = Profile->DefaultAttenuation.LoadSynchronous();
			}
			if (!Profile->FootwearType.IsNone())
			{
				FootwearType = Profile->FootwearType;
			}

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

			// Array sounds � loaded into respective mob arrays
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
			LoadArray(Profile->Footsteps,         FootstepSounds);
			LoadArray(Profile->VoiceAttack,        AttackVoiceSounds);
			LoadArray(Profile->VoiceCastStart,     CastVoiceSounds);
			LoadArray(Profile->VoiceCastRelease,   ReleaseVoiceSounds);

			// Idle ambient timer
			GetWorld()->GetTimerManager().SetTimer(IdleSoundTimer, this, &ABasicMOB::PlayRandomIdleSound, FMath::RandRange(5.f, 15.f), false);
			return;  // Profile fully loaded � skip legacy FMobAudioData below
		}
		else
		{
			UE_LOG(LogTemp, Warning,
				TEXT("ABasicMOB::SetupMobAudio: AudioProfileId '%s' not found in EntityAudioProfilesTable for mob '%s'. Falling back to legacy FMobAudioData."),
				*Definition->AudioProfileId.ToString(), *MobSlug.ToString());
		}
	}

	// ---------------------------------------------------------------
	// Priority 2 � Legacy inline FMobAudioData (backward-compatible)
	// Migrate rows to AudioProfileId over time; do not add new sounds here.
	// ---------------------------------------------------------------
	const FMobAudioData& AudioData = Definition->Audio;

	// �������� �����
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

	// Idle �����
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

	// Run �����
	// Footstep sounds
	for (const auto& SoundSoft : AudioData.FootstepSounds)
	{
		Streamable.RequestAsyncLoad(SoundSoft.ToSoftObjectPath(), [WeakThis, SoundSoft]()
			{
				if (!WeakThis.IsValid()) { return; }
				if (USoundBase* Loaded = SoundSoft.Get())
				{
					WeakThis->FootstepSounds.Add(Loaded);
				}
			});
	}

	// AttackVoice// AttackVoice ����� (���/���� ��� �����)
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

	// CastVoice ����� (����� ��� ������ �����)
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

	// ReleaseVoice ����� (����� ��� ������� ������)
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

	// ������ �� idle
	GetWorld()->GetTimerManager().SetTimer(IdleSoundTimer, this, &ABasicMOB::PlayRandomIdleSound, FMath::RandRange(5.f, 15.f), false);
}

void ABasicMOB::PlayRandomIdleSound()
{
	if (IdleSounds.Num() > 0 && !MOBData.bIsDead)
	{
		int32 Index = FMath::RandRange(0, IdleSounds.Num() - 1);
		AudioComponentMain->AttenuationSettings = DefaultAttenuation;
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
			AudioComponentMain->AttenuationSettings = DefaultAttenuation;
			AudioComponentMain->SetSound(*Sound);
			AudioComponentMain->Play();
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("ABasicMOB::PlaySoundByName - Sound '%s' not found"), *SoundName.ToString());
	}
}

