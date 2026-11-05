// Fill out your copyright notice in the Description page of Project Settings.

#include "Audio/AudioManager.h"
#include "Prototyping.h"
#include "Audio/AudioConfigDataAsset.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "Engine/AssetManager.h"
#include "Audio/AudioHelpers.h"
#include "Utils/NetCore/SkillFeedbackRule.h"
#include "Quartz/QuartzSubsystem.h"
#include "Quartz/AudioMixerClockHandle.h"

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
		UE_LOG(LogGameAudio, Warning, TEXT("UAudioManager::PlayPlaylist: playlist '%s' not found. "
			"Add it to the Playlists array in the GameInstance Blueprint defaults."), *InPlaylistId);
		return;
	}
	if (Playlist->Tracks.Num() == 0)
	{
		UE_LOG(LogGameAudio, Warning,
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

	// Downbeat start from silence: wait for the next bar boundary instead of
	// firing mid-beat. Crossfades (music already playing) stay immediate —
	// the outgoing track owns the musical continuity there.
	const bool bFromSilence = !(bComponentsValid && IsValid(ActiveMusicComponent) && ActiveMusicComponent->IsPlaying());
	if (bFromSilence && bQuantizeMusicStarts && EnsureMusicClock())
	{
		PendingQuantizedPlaylistId = InPlaylistId;
		if (!bQuantBarSubscribed)
		{
			FOnQuartzMetronomeEventBP BarDelegate;
			BarDelegate.BindUFunction(this, FName("OnMusicQuantBoundary"));
			MusicClock->SubscribeToQuantizationEvent(WorldContextObject,
				EQuartzCommandQuantization::Bar, BarDelegate, MusicClock);
			bQuantBarSubscribed = true;
		}
		UE_LOG(LogGameAudio, Log, TEXT("AudioManager: playlist '%s' queued for next bar"), *InPlaylistId);
		return;
	}
	PendingQuantizedPlaylistId.Empty();

	CrossfadeToTrack(Track, Playlist->FadeInTime, Playlist->FadeOutTime);
	ScheduleNextTrack(*Playlist, Track);

	UE_LOG(LogGameAudio, Log, TEXT("UAudioManager: started playlist '%s', track index %d"),
		*InPlaylistId, CurrentTrackIndex);
}

void UAudioManager::PlayMusicForZone(const FString& ZoneName)
{
	bool bFoundPlaylist = false;
	for (const FZoneMusicEntry& Entry : ZoneMusicMap)
	{
		if (Entry.ZoneName.Equals(ZoneName, ESearchCase::IgnoreCase))
		{
			PlayPlaylist(Entry.PlaylistId);
			bFoundPlaylist = true;
			break;
		}
	}
	if (!bFoundPlaylist)
	{
		UE_LOG(LogGameAudio, Log,
			TEXT("AudioManager::PlayMusicForZone: no music mapping for zone '%s'."), *ZoneName);
	}

	// Zone mix snapshot: first case-insensitive substring match wins,
	// otherwise back to slider volumes.
	int32 MatchIdx = INDEX_NONE;
	for (int32 k = 0; k < ZoneVolumeProfiles.Num(); ++k)
	{
		if (!ZoneVolumeProfiles[k].ZoneSubstring.IsEmpty()
			&& ZoneName.Contains(ZoneVolumeProfiles[k].ZoneSubstring, ESearchCase::IgnoreCase))
		{
			MatchIdx = k;
			break;
		}
	}
	if (MatchIdx != ActiveZoneProfileIdx)
	{
		ActiveZoneProfileIdx = MatchIdx;
		RefreshSnapshotVolumes();
	}
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

	// A quantized start that never fired belongs to a playlist that never
	// began — drop it so the bar tick can't resurrect music after stop.
	PendingQuantizedPlaylistId.Empty();

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
			1.0f, 1.0f, 0.0f, nullptr, UIConcurrency,
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
		TrackVoice(StingerComponent);
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
			PlayUISoundAsset(Entry.Sound);
			return;
		}
	}
	// Silence-by-default is banned: an unmapped event warns once so a
	// missing DA_AudioConfig entry surfaces in logs/smoke instead of a
	// silent button. Designer fix: fill UISounds in DA_AudioConfig.
	static TSet<EUISoundEvent> WarnedMissing;
	if (!WarnedMissing.Contains(InEvent))
	{
		WarnedMissing.Add(InEvent);
		UE_LOG(LogGameAudio, Warning, TEXT("AudioManager: no UISound mapped for event %d — assign it in DA_AudioConfig (UI Sounds)"),
			static_cast<int32>(InEvent));
	}
}

void UAudioManager::PlayUISoundAsset(USoundBase* Sound)
{
	if (!Sound) return;

	USoundClass* TargetClass = UIClass ? UIClass : SFXClass;
	if (TargetClass)
	{
		// SpawnSound2D with bAutoActivate=false so we can set the class
		// override BEFORE Play() — post-play overrides are ignored by the
		// audio engine. UIConcurrency caps spam-click pileup (null = uncapped).
		UAudioComponent* AC = UGameplayStatics::SpawnSound2D(
			WorldContextObject, Sound,
			1.0f, 1.0f, 0.0f, UIConcurrency,
			/*bAutoDestroy=*/true, /*bAutoActivate=*/false);
		TrackVoice(AC);
		if (AC)
		{
			AC->SoundClassOverride = TargetClass;
			AC->Play();
		}
	}
	else
	{
		UGameplayStatics::PlaySound2D(WorldContextObject, Sound, 1.0f, 1.0f, 0.0f, UIConcurrency);
	}
}

void UAudioManager::PlayDenySound(EUISoundEvent DenyEvent, USoundBase* OverrideSound)
{
	const UWorld* World = GetWorldSafe(WorldContextObject);
	const float NowSec = World ? static_cast<float>(World->GetTimeSeconds()) : 0.0f;
	const double LastSec = LastDenyPlayTime.Contains(DenyEvent)
		? LastDenyPlayTime[DenyEvent] : -10000.0;
	if (!FSkillFeedbackRule::CanPlayDeny(NowSec, static_cast<float>(LastSec)))
	{
		return;
	}
	LastDenyPlayTime.Add(DenyEvent, static_cast<double>(NowSec));
	if (OverrideSound)
	{
		PlayUISoundAsset(OverrideSound);
	}
	else
	{
		PlayUISound(DenyEvent);
	}
}

void UAudioManager::PreloadSoftSounds(const TArray<FSoftObjectPath>& AssetPaths)
{
	if (AssetPaths.Num() == 0) return;
	UAssetManager* AM = UAssetManager::GetIfInitialized();
	if (!AM)
	{
		UE_LOG(LogGameAudio, Warning, TEXT("AudioManager: AssetManager not ready, skipping UI-sound preload"));
		return;
	}
	TSharedPtr<FStreamableHandle> Handle = AM->GetStreamableManager().RequestAsyncLoad(
		AssetPaths,
		FStreamableDelegate::CreateUObject(this, &UAudioManager::OnPreloadFinished));
	if (Handle.IsValid())
	{
		PreloadHandles.Add(Handle);
	}
}

void UAudioManager::OnPreloadFinished()
{
	// Drop completed handles; keep the array bounded.
	PreloadHandles.RemoveAll([](const TSharedPtr<FStreamableHandle>& H)
	{
		return !H.IsValid() || H->HasLoadCompleted();
	});
	UE_LOG(LogGameAudio, Verbose, TEXT("AudioManager: async UI-sound preload finished (%d handles live)"),
		PreloadHandles.Num());
}

// ---------------------------------------------------------------------------
// Settings persistence
// ---------------------------------------------------------------------------

