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

