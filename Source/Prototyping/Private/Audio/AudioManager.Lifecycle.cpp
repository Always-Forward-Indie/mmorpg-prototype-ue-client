// Fill out your copyright notice in the Description page of Project Settings.

#include "Audio/AudioManager.h"
#include "Prototyping.h"
#include "Audio/AudioConfigDataAsset.h"
#include "Audio/UISlateSoundDevice.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "Audio/AudioHelpers.h"
#include "Framework/Application/SlateApplication.h"

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void UAudioManager::Init(UObject* InWorldContext, UAudioConfigDataAsset* InConfig)
{
	WorldContextObject = InWorldContext;

	// Copy all designer data from the Data Asset into this manager.
	if (InConfig)
	{
		MasterSoundMix  = InConfig->MasterSoundMix;
		MasterClass     = InConfig->MasterClass;
		MusicClass      = InConfig->MusicClass;
		SFXClass        = InConfig->SFXClass;
		AmbientClass    = InConfig->AmbientClass;
		UIClass         = InConfig->UIClass;
		Playlists       = InConfig->Playlists;
		UISounds        = InConfig->UISounds;
		StingerLevelUp    = InConfig->StingerLevelUp;
		StingerRareDrop   = InConfig->StingerRareDrop;
		StingerDeath      = InConfig->StingerDeath;
		StingerBossAppear = InConfig->StingerBossAppear;
	}
	else
	{
		UE_LOG(LogGameAudio, Warning, TEXT("UAudioManager::Init: no AudioConfigDataAsset provided. "
			"Assign DA_AudioConfig in BP_GameInstance → Class Defaults → Audio Config."));
	}

	// Only load saved volume values here — SoundMix push is deferred to ReapplySoundMix()
	// because GetWorld() on GameInstance returns null during Init() (world not yet created).
	ApplySavedSettings();

	InstallSlateSoundBridge();

	if (!MasterSoundMix)
	{
		UE_LOG(LogGameAudio, Warning, TEXT("UAudioManager::Init: MasterSoundMix is not assigned in DA_AudioConfig."));
	}
}

void UAudioManager::ReapplySoundMix()
{
	// Refresh WorldContextObject from GameInstance in case it was set before a valid
	// world existed. GetOuter() is the GameInstance (set via NewObject<>(this) in InitGameSystems).
	if (UGameInstance* GI = Cast<UGameInstance>(GetOuter()))
	{
		if (UWorld* GIWorld = GI->GetWorld())
		{
			WorldContextObject = GI;
		}
	}

	if (!GetWorldSafe(WorldContextObject))
	{
		UE_LOG(LogGameAudio, Warning, TEXT("UAudioManager::ReapplySoundMix: world not available yet, skipping."));
		return;
	}

	if (MasterSoundMix)
	{
		UGameplayStatics::PushSoundMixModifier(WorldContextObject, MasterSoundMix);
	}

	// Re-apply all cached volumes so the overrides take effect on the new audio device.
	ApplyClassVolume(MasterClass,  CachedMasterVolume);
	ApplyClassVolume(MusicClass,   CachedMusicVolume);
	ApplyClassVolume(SFXClass,     CachedSFXVolume);
	ApplyClassVolume(AmbientClass, CachedAmbientVolume);
	ApplyClassVolume(UIClass ? UIClass : SFXClass, CachedUIVolume);

	// The Slate bridge holds a weak ref to this manager — re-install so the
	// device never dangles across manager recreations.
	InstallSlateSoundBridge();

	// If audio components were invalidated by a world transition and a playlist
	// was active, restart it now in the new world so music resumes automatically.
	if (!bComponentsValid && !ActivePlaylistId.IsEmpty())
	{
		const FString PlaylistToResume = ActivePlaylistId;
		ActivePlaylistId.Empty(); // clear so PlayPlaylist does not early-out
		UE_LOG(LogGameAudio, Log, TEXT("UAudioManager::ReapplySoundMix: respawning components and resuming playlist '%s'"), *PlaylistToResume);
		PlayPlaylist(PlaylistToResume, /*bForceRestart=*/true);
	}
}

void UAudioManager::InvalidateAudioComponents()
{
	// Stop playback and release component handles so CrossfadeToTrack
	// re-creates them via SpawnSound2D in the new world.
	auto SafeStop = [](UAudioComponent*& Comp)
	{
		if (IsValid(Comp))
		{
			Comp->Stop();
		}
		Comp = nullptr;
	};
	SafeStop(MusicComponentA);
	SafeStop(MusicComponentB);
	SafeStop(StingerComponent);
	ActiveMusicComponent = nullptr;
	bUseComponentB       = false;
	bComponentsValid     = false;
	ActivePlaylistId.Empty();

	// Clear the track-end timer so it cannot fire on a destroyed world.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TrackEndTimerHandle);
	}
	TrackEndTimerHandle.Invalidate();

	UE_LOG(LogGameAudio, Log, TEXT("UAudioManager::InvalidateAudioComponents: components cleared (pending world transition)"));
}

// ---------------------------------------------------------------------------
// Volume helpers
// ---------------------------------------------------------------------------

void UAudioManager::InstallSlateSoundBridge()
{
	if (!FSlateApplication::IsInitialized()) return;
	FSlateApplication::Get().InitializeSound(MakeShared<FUIMixSlateSoundDevice>(this));
}

void UAudioManager::ApplyClassVolume(USoundClass* SoundClass, float Volume)
{
	if (!MasterSoundMix || !SoundClass || !WorldContextObject) { return; }
	// True zero means silent-but-playing: the transport keeps running (music
	// resumes seamlessly on unmute) and the engine virtualizes inaudible
	// voices itself. Never floor to epsilon — that audibly leaks at "0".
	const float SafeVolume = FMath::Clamp(Volume, 0.0f, 1.0f);
	UGameplayStatics::SetSoundMixClassOverride(
		WorldContextObject, MasterSoundMix, SoundClass,
		SafeVolume, /*Pitch*/ 1.0f, /*FadeInTime*/ 0.0f,
		/*bApplyToChildren*/ true);
	// NOTE: PushSoundMixModifier is called only by the volume setters when
	// transitioning from 0 to >0 — not on every slider tick, which would
	// destabilise the audio device's internal modifier stack.
}

