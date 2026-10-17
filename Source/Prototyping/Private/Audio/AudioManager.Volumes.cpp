// Fill out your copyright notice in the Description page of Project Settings.

#include "Audio/AudioManager.h"
#include "Audio/AudioConfigDataAsset.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "Audio/AudioHelpers.h"

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

