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

	// Only load saved volume values here — SoundMix push is deferred to ReapplySoundMix()
	// because GetWorld() on GameInstance returns null during Init() (world not yet created).
	ApplySavedSettings();

	if (!MasterSoundMix)
	{
		UE_LOG(LogTemp, Warning, TEXT("UAudioManager::Init: MasterSoundMix is not assigned in DA_AudioConfig."));
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
		UE_LOG(LogTemp, Warning, TEXT("UAudioManager::ReapplySoundMix: world not available yet, skipping."));
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

	// If audio components were invalidated by a world transition and a playlist
	// was active, restart it now in the new world so music resumes automatically.
	if (!bComponentsValid && !ActivePlaylistId.IsEmpty())
	{
		const FString PlaylistToResume = ActivePlaylistId;
		ActivePlaylistId.Empty(); // clear so PlayPlaylist does not early-out
		UE_LOG(LogTemp, Log, TEXT("UAudioManager::ReapplySoundMix: respawning components and resuming playlist '%s'"), *PlaylistToResume);
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

	UE_LOG(LogTemp, Log, TEXT("UAudioManager::InvalidateAudioComponents: components cleared (pending world transition)"));
}

// ---------------------------------------------------------------------------
// Volume helpers
// ---------------------------------------------------------------------------

void UAudioManager::ApplyClassVolume(USoundClass* SoundClass, float Volume)
{
	if (!MasterSoundMix || !SoundClass || !WorldContextObject) { return; }
	// Use a tiny non-zero minimum to prevent the audio engine from
	// deactivating/virtualizing components when volume reaches 0.
	const float SafeVolume = FMath::Max(FMath::Clamp(Volume, 0.0f, 1.0f), 0.005f);
	UGameplayStatics::SetSoundMixClassOverride(
		WorldContextObject, MasterSoundMix, SoundClass,
		SafeVolume, /*Pitch*/ 1.0f, /*FadeInTime*/ 0.0f,
		/*bApplyToChildren*/ true);
	// NOTE: PushSoundMixModifier is called only by the volume setters when
	// transitioning from 0 to >0 — not on every slider tick, which would
	// destabilise the audio device's internal modifier stack.
}

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

	UWorld* World = GetWorldSafe(WorldContextObject);
	if (!World)
	{
		UE_LOG(LogTemp, Warning, TEXT("UAudioManager::CrossfadeToTrack: no valid world — cannot spawn audio components"));
		return;
	}

	// Lazily spawn both components on first use, or after InvalidateAudioComponents().
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
				UE_LOG(LogTemp, Log, TEXT("UAudioManager: spawned music component in world '%s'"), *World->GetName());
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("UAudioManager::CrossfadeToTrack: SpawnSound2D returned null!"));
			}
		}
	};
	EnsureComponent(MusicComponentA, Track);
	EnsureComponent(MusicComponentB, Track);

	if (!MusicComponentA || !MusicComponentB)
	{
		UE_LOG(LogTemp, Error, TEXT("UAudioManager::CrossfadeToTrack: failed to obtain both music components, aborting"));
		return;
	}

	bComponentsValid = true;

	UAudioComponent* Incoming = bUseComponentB ? MusicComponentB : MusicComponentA;
	UAudioComponent* Outgoing = bUseComponentB ? MusicComponentA : MusicComponentB;

	if (IsValid(Outgoing) && Outgoing->IsPlaying())
	{
		Outgoing->FadeOut(FadeOutTime, 0.0f);
	}

	if (IsValid(Incoming))
	{
		Incoming->SetSound(Track);
		UE_LOG(LogTemp, Log, TEXT("UAudioManager: playing track '%s' (FadeIn=%.1fs)"), *Track->GetName(), FadeInTime);
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
	if (!bForceRestart && ActivePlaylistId.Equals(InPlaylistId, ESearchCase::IgnoreCase))
	{
		if (bComponentsValid && IsValid(ActiveMusicComponent) && ActiveMusicComponent->IsPlaying())
		{
			return;
		}
	}

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
	LastActivePlaylistId.Empty();  // explicitly started a new playlist — clear stale backup
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

	// Preserve the playlist so SetMusicVolume can resume it when volume
	// is raised back above 0 after being silenced.
	if (!ActivePlaylistId.IsEmpty())
	{
		LastActivePlaylistId = ActivePlaylistId;
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

	// Re-use the dedicated stinger component if valid, otherwise create a new one.
	// Always set SoundClassOverride BEFORE Play() — overrides set after Play() are
	// ignored by the audio engine for the current playback.
	if (!IsValid(StingerComponent))
	{
		StingerComponent = UGameplayStatics::SpawnSoundAttached(
			StingerSound,
			Cast<AActor>(GetOuter()) ? Cast<AActor>(GetOuter())->GetRootComponent() : nullptr,
			NAME_None, FVector::ZeroVector, FRotator::ZeroRotator,
			EAttachLocation::KeepWorldPosition,
			/*bStopWhenAttachedToDestroyed=*/false,
			1.0f, 1.0f, 0.0f, nullptr, nullptr,
			/*bAutoActivate=*/false);
		if (!StingerComponent)
		{
			// Fallback: SpawnSound2D for contexts without a world actor (e.g. GameInstance).
			StingerComponent = UGameplayStatics::SpawnSound2D(
				WorldContextObject, StingerSound,
				1.0f, 1.0f, 0.0f, nullptr,
				/*bAutoDestroy=*/false, /*bAutoActivate=*/false);
		}
		if (StingerComponent)
		{
			StingerComponent->bAutoDestroy = false;
		}
	}

	if (IsValid(StingerComponent))
	{
		StingerComponent->SetSound(StingerSound);
		if (SFXClass) { StingerComponent->SoundClassOverride = SFXClass; }
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
				// SpawnSound2D with bAutoActivate=false so we can set the class
				// override BEFORE Play() — post-play overrides are ignored by the
				// audio engine.
				UAudioComponent* AC = UGameplayStatics::SpawnSound2D(
					WorldContextObject, Entry.Sound,
					1.0f, 1.0f, 0.0f, nullptr,
					/*bAutoDestroy=*/true, /*bAutoActivate=*/false);
				if (AC)
				{
					AC->SoundClassOverride = TargetClass;
					AC->Play();
				}
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
