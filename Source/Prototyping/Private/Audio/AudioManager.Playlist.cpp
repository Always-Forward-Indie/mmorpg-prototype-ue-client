// Fill out your copyright notice in the Description page of Project Settings.

#include "Audio/AudioManager.h"
#include "Audio/AudioConfigDataAsset.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "Audio/AudioHelpers.h"

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

