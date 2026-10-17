// Fill out your copyright notice in the Description page of Project Settings.

#include "Audio/AudioManager.h"
#include "Audio/AudioConfigDataAsset.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "Audio/AudioHelpers.h"

void UAudioManager::ApplySavedSettings()
{
	auto ReadVolume = [](const FString& Key, float Default) -> float
	{
		FString Raw;
		if (GConfig && GConfig->GetString(TEXT("AudioSettings"), *Key, Raw, GGameUserSettingsIni))
		{
			return FMath::Clamp(FCString::Atof(*Raw), 0.0f, 1.0f);
		}
		return Default;
	};

	// Always update the cached values from disk.
	CachedMasterVolume  = ReadVolume(AudioSettingsKeys::MasterVolume,  1.0f);
	CachedMusicVolume   = ReadVolume(AudioSettingsKeys::MusicVolume,   1.0f);
	CachedSFXVolume     = ReadVolume(AudioSettingsKeys::SFXVolume,     1.0f);
	CachedAmbientVolume = ReadVolume(AudioSettingsKeys::AmbientVolume, 1.0f);
	CachedUIVolume      = ReadVolume(AudioSettingsKeys::UIVolume,      1.0f);

	// Only apply overrides if a valid world exists. If called during GameInstance::Init()
	// (world = null) the cached values will be picked up by ReapplySoundMix() later.
	if (GetWorldSafe(WorldContextObject))
	{
		ApplyClassVolume(MasterClass,  CachedMasterVolume);
		ApplyClassVolume(MusicClass,   CachedMusicVolume);
		ApplyClassVolume(SFXClass,     CachedSFXVolume);
		ApplyClassVolume(AmbientClass, CachedAmbientVolume);
		ApplyClassVolume(UIClass ? UIClass : SFXClass, CachedUIVolume);
	}
}

void UAudioManager::SaveSettings()
{
	if (!GConfig) { return; }

	GConfig->SetString(TEXT("AudioSettings"), *AudioSettingsKeys::MasterVolume,
		*FString::SanitizeFloat(CachedMasterVolume), GGameUserSettingsIni);
	GConfig->SetString(TEXT("AudioSettings"), *AudioSettingsKeys::MusicVolume,
		*FString::SanitizeFloat(CachedMusicVolume), GGameUserSettingsIni);
	GConfig->SetString(TEXT("AudioSettings"), *AudioSettingsKeys::SFXVolume,
		*FString::SanitizeFloat(CachedSFXVolume), GGameUserSettingsIni);
	GConfig->SetString(TEXT("AudioSettings"), *AudioSettingsKeys::AmbientVolume,
		*FString::SanitizeFloat(CachedAmbientVolume), GGameUserSettingsIni);
	GConfig->SetString(TEXT("AudioSettings"), *AudioSettingsKeys::UIVolume,
		*FString::SanitizeFloat(CachedUIVolume), GGameUserSettingsIni);

	GConfig->Flush(false, GGameUserSettingsIni);

	UE_LOG(LogTemp, Log,
		TEXT("UAudioManager: settings saved - Master=%.2f Music=%.2f SFX=%.2f Ambient=%.2f UI=%.2f"),
		CachedMasterVolume, CachedMusicVolume, CachedSFXVolume, CachedAmbientVolume, CachedUIVolume);
}
