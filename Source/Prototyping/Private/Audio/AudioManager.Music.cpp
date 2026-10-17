// Fill out your copyright notice in the Description page of Project Settings.

#include "Audio/AudioManager.h"
#include "Audio/AudioConfigDataAsset.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "Audio/AudioHelpers.h"

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

