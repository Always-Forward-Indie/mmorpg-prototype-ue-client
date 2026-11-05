// Fill out your copyright notice in the Description page of Project Settings.

#include "Audio/AudioManager.h"
#include "Prototyping.h"
#include "Audio/AudioConfigDataAsset.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "Audio/AudioHelpers.h"
#include "MyGameInstance.h"
#include "Engine/Engine.h"
#include "HAL/IConsoleManager.h"

void UAudioManager::SetMasterVolume(float Volume)
{
	const float Prev = CachedMasterVolume;
	CachedMasterVolume = FMath::Clamp(Volume, 0.0f, 1.0f);
	ApplyClassVolume(MasterClass, CachedMasterVolume);
	if (Prev <= 0.0f && CachedMasterVolume > 0.0f && WorldContextObject)
	{
		UGameplayStatics::PushSoundMixModifier(WorldContextObject, MasterSoundMix);
	}
}

void UAudioManager::SetMusicVolume(float Volume)
{
	const float Prev = CachedMusicVolume;
	CachedMusicVolume = FMath::Clamp(Volume, 0.0f, 1.0f);
	ApplyClassVolume(MusicClass, CachedMusicVolume);

	if (Prev <= 0.0f && CachedMusicVolume > 0.0f)
	{
		if (WorldContextObject)
		{
			UGameplayStatics::PushSoundMixModifier(WorldContextObject, MasterSoundMix);
		}

		// Resume playlist if a known one was playing before silence.
		// ActivePlaylistId may still be set (fade stopped component, not playlist).
		FString ResumeId = !ActivePlaylistId.IsEmpty() ? ActivePlaylistId : LastActivePlaylistId;
		if (!ResumeId.IsEmpty())
		{
			PlayPlaylist(ResumeId, false);
			LastActivePlaylistId.Empty();
		}
	}
}

void UAudioManager::SetSFXVolume(float Volume)
{
	const float Prev = CachedSFXVolume;
	CachedSFXVolume = FMath::Clamp(Volume, 0.0f, 1.0f);
	ApplyClassVolume(SFXClass, CachedSFXVolume);
	if (Prev <= 0.0f && CachedSFXVolume > 0.0f && WorldContextObject)
	{
		UGameplayStatics::PushSoundMixModifier(WorldContextObject, MasterSoundMix);
	}
}

void UAudioManager::SetAmbientVolume(float Volume)
{
	const float Prev = CachedAmbientVolume;
	CachedAmbientVolume = FMath::Clamp(Volume, 0.0f, 1.0f);
	ApplyClassVolume(AmbientClass, CachedAmbientVolume);
	if (Prev <= 0.0f && CachedAmbientVolume > 0.0f && WorldContextObject)
	{
		UGameplayStatics::PushSoundMixModifier(WorldContextObject, MasterSoundMix);
	}
}

void UAudioManager::SetUIVolume(float Volume)
{
	const float Prev = CachedUIVolume;
	CachedUIVolume = FMath::Clamp(Volume, 0.0f, 1.0f);
	USoundClass* Target = UIClass ? UIClass : SFXClass;
	ApplyClassVolume(Target, CachedUIVolume);
	if (Prev <= 0.0f && CachedUIVolume > 0.0f && WorldContextObject)
	{
		UGameplayStatics::PushSoundMixModifier(WorldContextObject, MasterSoundMix);
	}
}

// ---------------------------------------------------------------------------
// Playlist helpers
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Voice telemetry (slice C): every component the manager spawns is tracked,
// peak concurrent voices land in the AUDIO_PERF tombstone (VerifyClient gate).
// ---------------------------------------------------------------------------

void UAudioManager::TrackVoice(UAudioComponent* AC)
{
	// Requests are counted even when the device returns null (headless CI):
	// requested>0 + active==0 means 'calls happened, no device', requested==0
	// means 'nothing asked for sound'.
	++VoiceRequestedTotal;
	if (!IsValid(AC))
	{
		return;
	}
	// No duplicates: persistent components (music pair, stinger) report every
	// use, one-shots report once. Lookup is linear but the array stays tiny
	// (voices are pruned on every count).
	for (const TWeakObjectPtr<UAudioComponent>& Known : LiveVoices)
	{
		if (Known.Get() == AC)
		{
			return;
		}
	}
	++VoiceSpawnedTotal;
	LiveVoices.Add(AC);
	const int32 Active = CountActiveVoices();
	if (Active > VoicePeakActive)
	{
		VoicePeakActive = Active;
	}
}

int32 UAudioManager::CountActiveVoices()
{
	int32 Active = 0;
	for (int32 i = LiveVoices.Num() - 1; i >= 0; --i)
	{
		UAudioComponent* AC = LiveVoices[i].Get();
		if (!IsValid(AC) || !AC->IsPlaying())
		{
			LiveVoices.RemoveAtSwap(i);
			continue;
		}
		++Active;
	}
	if (Active > VoicePeakActive)
	{
		VoicePeakActive = Active;
	}
	return Active;
}

void UAudioManager::LogAudioPerf()
{
	const int32 Active = CountActiveVoices();
	UE_LOG(LogGameAudio, Warning, TEXT("AUDIO_PERF peak=%d active=%d total=%d requested=%d"),
		VoicePeakActive, Active, VoiceSpawnedTotal, VoiceRequestedTotal);
}

void UAudioManager::AudioPerfConsole()
{
	if (!GEngine || !GEngine->GameViewport)
	{
		return;
	}
	if (UMyGameInstance* GI = Cast<UMyGameInstance>(GEngine->GameViewport->GetGameInstance()))
	{
		if (GI->AudioManager)
		{
			GI->AudioManager->LogAudioPerf();
		}
	}
}

static FAutoConsoleCommand AudioPerfCmd(
	TEXT("audio.perf"),
	TEXT("Log audio voice stats (AUDIO_PERF tombstone: peak/active/total manager-spawned voices)"),
	FConsoleCommandDelegate::CreateStatic(&UAudioManager::AudioPerfConsole));
