// Fill out your copyright notice in the Description page of Project Settings.

#include "Audio/AudioManager.h"
#include "Audio/AudioConfigDataAsset.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Engine/World.h"

namespace AudioSettingsKeys
{
	static const FString MasterVolume  = TEXT("AudioMasterVolume");
	static const FString MusicVolume   = TEXT("AudioMusicVolume");
	static const FString SFXVolume     = TEXT("AudioSFXVolume");
	static const FString AmbientVolume = TEXT("AudioAmbientVolume");
	static const FString UIVolume      = TEXT("AudioUIVolume");
}

// ---------------------------------------------------------------------------
// Internal helper
// ---------------------------------------------------------------------------

static UWorld* GetWorldSafe(UObject* Ctx)
{
	return (GEngine && Ctx) ? GEngine->GetWorldFromContextObject(Ctx, EGetWorldErrorMode::ReturnNull) : nullptr;
}

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
		UE_LOG(LogTemp, Warning, TEXT("UAudioManager::Init: no AudioConfigDataAsset provided. "
			"Assign DA_AudioConfig in BP_GameInstance → Class Defaults → Audio Config."));
	}

	if (MasterSoundMix)
	{
		UGameplayStatics::PushSoundMixModifier(WorldContextObject, MasterSoundMix);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("UAudioManager::Init: MasterSoundMix is not assigned in DA_AudioConfig."));
	}

	ApplySavedSettings();
}

void UAudioManager::ReapplySoundMix()
{
	if (MasterSoundMix && WorldContextObject)
	{
		UGameplayStatics::PushSoundMixModifier(WorldContextObject, MasterSoundMix);
	}

	// Re-apply all cached volumes so the overrides take effect on the new audio device.
	ApplyClassVolume(MasterClass,  CachedMasterVolume);
	ApplyClassVolume(MusicClass,   CachedMusicVolume);
	ApplyClassVolume(SFXClass,     CachedSFXVolume);
	ApplyClassVolume(AmbientClass, CachedAmbientVolume);
	ApplyClassVolume(UIClass ? UIClass : SFXClass, CachedUIVolume);
}

// ---------------------------------------------------------------------------
// Volume helpers
// ---------------------------------------------------------------------------

void UAudioManager::ApplyClassVolume(USoundClass* SoundClass, float Volume)
{
	if (!MasterSoundMix || !SoundClass || !WorldContextObject) { return; }
	// Use a tiny non-zero minimum to prevent the audio engine from
	// deactivating/virtualizing components when volume reaches 0.
	// This keeps playback alive so raising the slider back up restores sound
	// immediately without needing to restart the audio source.
	const float SafeVolume = FMath::Max(FMath::Clamp(Volume, 0.0f, 1.0f), KINDA_SMALL_NUMBER);
	// bApplyToChildren=true propagates the change to child Sound Classes
	// so SC_Master volume affects SC_Music / SC_SFX / SC_Ambient / SC_UI.
	UGameplayStatics::SetSoundMixClassOverride(
		WorldContextObject, MasterSoundMix, SoundClass,
		SafeVolume, /*Pitch*/ 1.0f, /*FadeInTime*/ 0.0f,
		/*bApplyToChildren*/ true);
}

void UAudioManager::SetMasterVolume(float Volume)
{
	CachedMasterVolume = FMath::Clamp(Volume, 0.0f, 1.0f);
	ApplyClassVolume(MasterClass, CachedMasterVolume);
}

void UAudioManager::SetMusicVolume(float Volume)
{
	CachedMusicVolume = FMath::Clamp(Volume, 0.0f, 1.0f);
	ApplyClassVolume(MusicClass, CachedMusicVolume);
}

void UAudioManager::SetSFXVolume(float Volume)
{
	CachedSFXVolume = FMath::Clamp(Volume, 0.0f, 1.0f);
	ApplyClassVolume(SFXClass, CachedSFXVolume);
}

void UAudioManager::SetAmbientVolume(float Volume)
{
	CachedAmbientVolume = FMath::Clamp(Volume, 0.0f, 1.0f);
	ApplyClassVolume(AmbientClass, CachedAmbientVolume);
}

void UAudioManager::SetUIVolume(float Volume)
{
	CachedUIVolume = FMath::Clamp(Volume, 0.0f, 1.0f);
	USoundClass* Target = UIClass ? UIClass : SFXClass;
	ApplyClassVolume(Target, CachedUIVolume);
}

// ---------------------------------------------------------------------------
// Playlist helpers
// ---------------------------------------------------------------------------

const FMusicPlaylist* UAudioManager::FindPlaylist(const FString& InPlaylistId) const
{
	for (const FMusicPlaylist& P : Playlists)
	{
		if (P.PlaylistId.Equals(InPlaylistId, ESearchCase::IgnoreCase)) { return &P; }
	}
	return nullptr;
}

int32 UAudioManager::PickNextTrackIndex(const FMusicPlaylist& Playlist) const
{
	const int32 Num = Playlist.Tracks.Num();
	if (Num == 0) { return 0; }

	switch (Playlist.PlaybackMode)
	{
	case EMusicPlaybackMode::Random:
		{
			if (Num == 1) { return 0; }
			int32 Next = FMath::RandRange(0, Num - 1);
			if (Next == CurrentTrackIndex) { Next = (Next + 1) % Num; }
			return Next;
		}
	case EMusicPlaybackMode::Sequential:
		{
			const int32 Next = CurrentTrackIndex + 1;
			return (Next < Num) ? Next : -1;
		}
	case EMusicPlaybackMode::SequentialLoop:
	default:
		return (CurrentTrackIndex + 1) % Num;
	}
}

// ---------------------------------------------------------------------------
// True crossfade — ping-pong between MusicComponentA and MusicComponentB
// ---------------------------------------------------------------------------

void UAudioManager::CrossfadeToTrack(USoundBase* Track, float FadeInTime, float FadeOutTime)
{
	if (!Track || !WorldContextObject) { return; }

	// Lazily spawn both components on first use.
	auto EnsureComponent = [&](UAudioComponent*& Comp, USoundBase* Sound)
	{
		if (!IsValid(Comp))
		{
			Comp = UGameplayStatics::SpawnSound2D(WorldContextObject, Sound);
			if (Comp)
			{
				Comp->bAutoDestroy = false;
				Comp->Stop();
				if (MusicClass) { Comp->SoundClassOverride = MusicClass; }
			}
		}
	};
	EnsureComponent(MusicComponentA, Track);
	EnsureComponent(MusicComponentB, Track);

	UAudioComponent* Incoming = bUseComponentB ? MusicComponentB : MusicComponentA;
	UAudioComponent* Outgoing = bUseComponentB ? MusicComponentA : MusicComponentB;

	if (IsValid(Outgoing) && Outgoing->IsPlaying())
	{
		Outgoing->FadeOut(FadeOutTime, 0.0f);
	}

	if (IsValid(Incoming))
	{
		Incoming->SetSound(Track);
		(FadeInTime > 0.0f) ? Incoming->FadeIn(FadeInTime) : Incoming->Play();
	}

	ActiveMusicComponent = Incoming;
	bUseComponentB       = !bUseComponentB;
}

void UAudioManager::ScheduleNextTrack(const FMusicPlaylist& Playlist, USoundBase* CurrentTrack)
{
	UWorld* World = GetWorldSafe(WorldContextObject);
	if (!World || !CurrentTrack) { return; }

	const float Duration = CurrentTrack->GetDuration();

	// Duration unknown (streaming asset not loaded yet) — retry in 5 s.
	if (Duration <= 0.0f)
	{
		World->GetTimerManager().SetTimer(
			TrackEndTimerHandle, this, &UAudioManager::OnTrackNearEnd, 5.0f, false);
		return;
	}

	// Fire FadeOutTime seconds before end so crossfade starts exactly on time.
	const float FireAt = FMath::Max(
		Duration - Playlist.FadeOutTime + Playlist.IntervalBetweenTracks, 0.1f);
	World->GetTimerManager().ClearTimer(TrackEndTimerHandle);
	World->GetTimerManager().SetTimer(
		TrackEndTimerHandle, this, &UAudioManager::OnTrackNearEnd, FireAt, false);
}

void UAudioManager::OnTrackNearEnd()
{
	const FMusicPlaylist* Playlist = FindPlaylist(ActivePlaylistId);
	if (!Playlist || Playlist->Tracks.Num() == 0)
	{
		ActivePlaylistId.Empty();
		return;
	}

	const int32 Next = PickNextTrackIndex(*Playlist);
	if (Next < 0)
	{
		StopMusic();
		return;
	}

	CurrentTrackIndex = Next;
	USoundBase* Track = Playlist->Tracks[CurrentTrackIndex];
	if (!Track) { return; }

	CrossfadeToTrack(Track, Playlist->FadeInTime, Playlist->FadeOutTime);
	ScheduleNextTrack(*Playlist, Track);
}

// ---------------------------------------------------------------------------
// Public playlist API
// ---------------------------------------------------------------------------

void UAudioManager::PlayPlaylist(const FString& InPlaylistId, bool bForceRestart)
{
	if (!bForceRestart && ActivePlaylistId.Equals(InPlaylistId, ESearchCase::IgnoreCase)) { return; }

	const FMusicPlaylist* Playlist = FindPlaylist(InPlaylistId);
	if (!Playlist)
	{
		UE_LOG(LogTemp, Warning, TEXT("UAudioManager::PlayPlaylist: playlist '%s' not found. "
			"Add it to the Playlists array in the GameInstance Blueprint defaults."), *InPlaylistId);
		return;
	}
	if (Playlist->Tracks.Num() == 0)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UAudioManager::PlayPlaylist: playlist '%s' has no tracks."), *InPlaylistId);
		return;
	}

	UWorld* World = GetWorldSafe(WorldContextObject);
	if (World) { World->GetTimerManager().ClearTimer(TrackEndTimerHandle); }

	ActivePlaylistId  = InPlaylistId;
	CurrentTrackIndex = (Playlist->PlaybackMode == EMusicPlaybackMode::Random)
		? FMath::RandRange(0, Playlist->Tracks.Num() - 1) : 0;

	USoundBase* Track = Playlist->Tracks[CurrentTrackIndex];
	if (!Track) { return; }

	CrossfadeToTrack(Track, Playlist->FadeInTime, Playlist->FadeOutTime);
	ScheduleNextTrack(*Playlist, Track);

	UE_LOG(LogTemp, Log, TEXT("UAudioManager: started playlist '%s', track index %d"),
		*InPlaylistId, CurrentTrackIndex);
}

void UAudioManager::PlayMusicForZone(const FString& ZoneName)
{
	for (const FZoneMusicEntry& Entry : ZoneMusicMap)
	{
		if (Entry.ZoneName.Equals(ZoneName, ESearchCase::IgnoreCase))
		{
			PlayPlaylist(Entry.PlaylistId);
			return;
		}
	}
	UE_LOG(LogTemp, Log,
		TEXT("UAudioManager::PlayMusicForZone: no music mapping for zone '%s'."), *ZoneName);
}

void UAudioManager::StopMusic(float FadeOutTimeOverride)
{
	UWorld* World = GetWorldSafe(WorldContextObject);
	if (World) { World->GetTimerManager().ClearTimer(TrackEndTimerHandle); }

	const FMusicPlaylist* Playlist = FindPlaylist(ActivePlaylistId);
	const float FadeTime = (FadeOutTimeOverride > 0.0f)
		? FadeOutTimeOverride : (Playlist ? Playlist->FadeOutTime : 1.0f);

	if (IsValid(ActiveMusicComponent) && ActiveMusicComponent->IsPlaying())
	{
		ActiveMusicComponent->FadeOut(FadeTime, 0.0f);
	}

	ActivePlaylistId.Empty();
}

void UAudioManager::SkipTrack()
{
	const FMusicPlaylist* Playlist = FindPlaylist(ActivePlaylistId);
	if (!Playlist || Playlist->Tracks.Num() == 0) { return; }

	UWorld* World = GetWorldSafe(WorldContextObject);
	if (World) { World->GetTimerManager().ClearTimer(TrackEndTimerHandle); }

	OnTrackNearEnd();
}

// ---------------------------------------------------------------------------
// Stingers
// ---------------------------------------------------------------------------

void UAudioManager::PlayStinger(USoundBase* StingerSound)
{
	if (!StingerSound || !WorldContextObject) { return; }

	if (!IsValid(StingerComponent))
	{
		StingerComponent = UGameplayStatics::SpawnSound2D(WorldContextObject, StingerSound);
		if (StingerComponent)
		{
			StingerComponent->bAutoDestroy = false;
			if (SFXClass) { StingerComponent->SoundClassOverride = SFXClass; }
		}
	}
	else
	{
		StingerComponent->SetSound(StingerSound);
		StingerComponent->Play();
	}
}

// ---------------------------------------------------------------------------
// UI Sounds
// ---------------------------------------------------------------------------

void UAudioManager::PlayUISound(EUISoundEvent InEvent)
{
	for (const FUISoundEntry& Entry : UISounds)
	{
		if (Entry.Event == InEvent && Entry.Sound)
		{
			USoundClass* TargetClass = UIClass ? UIClass : SFXClass;
			if (TargetClass)
			{
				UAudioComponent* AC = UGameplayStatics::SpawnSound2D(WorldContextObject, Entry.Sound);
				if (AC) { AC->SoundClassOverride = TargetClass; }
			}
			else
			{
				UGameplayStatics::PlaySound2D(WorldContextObject, Entry.Sound);
			}
			return;
		}
	}
}

// ---------------------------------------------------------------------------
// Settings persistence
// ---------------------------------------------------------------------------

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

	CachedMasterVolume  = ReadVolume(AudioSettingsKeys::MasterVolume,  1.0f);
	CachedMusicVolume   = ReadVolume(AudioSettingsKeys::MusicVolume,   1.0f);
	CachedSFXVolume     = ReadVolume(AudioSettingsKeys::SFXVolume,     1.0f);
	CachedAmbientVolume = ReadVolume(AudioSettingsKeys::AmbientVolume, 1.0f);
	CachedUIVolume      = ReadVolume(AudioSettingsKeys::UIVolume,      1.0f);

	ApplyClassVolume(MasterClass,  CachedMasterVolume);
	ApplyClassVolume(MusicClass,   CachedMusicVolume);
	ApplyClassVolume(SFXClass,     CachedSFXVolume);
	ApplyClassVolume(AmbientClass, CachedAmbientVolume);
	ApplyClassVolume(UIClass ? UIClass : SFXClass, CachedUIVolume);
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
