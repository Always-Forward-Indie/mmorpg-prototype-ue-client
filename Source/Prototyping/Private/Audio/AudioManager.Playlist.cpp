// Fill out your copyright notice in the Description page of Project Settings.

#include "Audio/AudioManager.h"
#include "Prototyping.h"
#include "Audio/AudioConfigDataAsset.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "Audio/AudioHelpers.h"
#include "Quartz/QuartzSubsystem.h"
#include "Quartz/AudioMixerClockHandle.h"

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
		UE_LOG(LogGameAudio, Warning, TEXT("UAudioManager::CrossfadeToTrack: no valid world — cannot spawn audio components"));
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
				UE_LOG(LogGameAudio, Log, TEXT("UAudioManager: spawned music component in world '%s'"), *World->GetName());
			}
			else
			{
				UE_LOG(LogGameAudio, Warning, TEXT("UAudioManager::CrossfadeToTrack: SpawnSound2D returned null!"));
			}
		}
	};
	EnsureComponent(MusicComponentA, Track);
	EnsureComponent(MusicComponentB, Track);

	if (!MusicComponentA || !MusicComponentB)
	{
		UE_LOG(LogGameAudio, Error, TEXT("UAudioManager::CrossfadeToTrack: failed to obtain both music components, aborting"));
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
		UE_LOG(LogGameAudio, Log, TEXT("UAudioManager: playing track '%s' (FadeIn=%.1fs)"), *Track->GetName(), FadeInTime);
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
// Musical clock (Quartz) + mix snapshots
// ---------------------------------------------------------------------------

bool UAudioManager::EnsureMusicClock()
{
	UWorld* World = GetWorldSafe(WorldContextObject);
	if (!World)
	{
		return false;
	}
	if (IsValid(MusicClock))
	{
		return true;
	}
	UQuartzSubsystem* QS = World->GetSubsystem<UQuartzSubsystem>();
	if (!QS)
	{
		UE_LOG(LogGameAudio, Verbose, TEXT("AudioManager: no Quartz subsystem — quantized starts disabled"));
		return false;
	}
	FQuartzClockSettings Settings;
	MusicClock = QS->CreateNewClock(WorldContextObject, TEXT("MusicClock"), Settings);
	if (!IsValid(MusicClock))
	{
		return false;
	}
	// Tempo first, then start the transport.
	const FQuartzQuantizationBoundary Immediate;
	const FOnQuartzCommandEventBP NoOp;
	const float BPM = FMath::Clamp(MusicClockBPM, 40.0f, 220.0f);
	MusicClock->SetBeatsPerMinute(WorldContextObject, Immediate, NoOp, MusicClock, BPM);
	MusicClock->StartClock(WorldContextObject, MusicClock);
	UE_LOG(LogGameAudio, Log, TEXT("AudioManager: musical clock started at %.0f BPM"), BPM);
	return true;
}

void UAudioManager::OnMusicQuantBoundary(FName ClockName, EQuartzCommandQuantization QuantizationType,
	int32 NumBars, int32 Beat, float BeatFraction)
{
	// One-shot: unstick the bar subscription first so a stray extra tick can
	// never double-start the track.
	if (IsValid(MusicClock))
	{
		MusicClock->UnsubscribeFromTimeDivision(
			WorldContextObject, EQuartzCommandQuantization::Bar, MusicClock);
	}
	bQuantBarSubscribed = false;

	const FString Pending = PendingQuantizedPlaylistId;
	PendingQuantizedPlaylistId.Empty();
	if (Pending.IsEmpty() || !Pending.Equals(ActivePlaylistId, ESearchCase::IgnoreCase))
	{
		// Superseded (StopMusic/another playlist won the race) — stay silent.
		return;
	}
	const FMusicPlaylist* Playlist = FindPlaylist(ActivePlaylistId);
	USoundBase* Track = (Playlist && Playlist->Tracks.IsValidIndex(CurrentTrackIndex))
		? Playlist->Tracks[CurrentTrackIndex] : nullptr;
	if (!Track || !Playlist)
	{
		return;
	}
	UE_LOG(LogGameAudio, Log, TEXT("AudioManager: quantized downbeat start '%s' (bar %d)"),
		*ActivePlaylistId, NumBars);
	CrossfadeToTrack(Track, Playlist->FadeInTime, Playlist->FadeOutTime);
	ScheduleNextTrack(*Playlist, Track);
}

void UAudioManager::PushCachedVolumes()
{
	ApplyClassVolume(MasterClass,  CachedMasterVolume);
	ApplyClassVolume(MusicClass,   CachedMusicVolume);
	ApplyClassVolume(SFXClass,     CachedSFXVolume);
	ApplyClassVolume(AmbientClass, CachedAmbientVolume);
	ApplyClassVolume(UIClass ? UIClass : SFXClass, CachedUIVolume);
}

void UAudioManager::ApplySnapshotVolumes(const TArray<FSnapshotClassVolume>& Volumes)
{
	if (!MasterSoundMix) { return; }
	for (const FSnapshotClassVolume& V : Volumes)
	{
		if (!V.SoundClass) { continue; }
		const float Clamped = FMath::Clamp(V.Volume, 0.0f, 1.0f);
		UGameplayStatics::SetSoundMixClassOverride(
			WorldContextObject, MasterSoundMix, V.SoundClass,
			Clamped, /*Pitch=*/1.0f, FMath::Max(0.0f, V.BlendSec), /*bApplyToChildren=*/true);
	}
}

void UAudioManager::RefreshSnapshotVolumes()
{
	// Base first (sliders always win on exit), then active snapshots so the
	// strongest context (low-HP last) owns any shared class.
	PushCachedVolumes();
	if (bCombatActive)
	{
		ApplySnapshotVolumes(CombatVolumes);
	}
	if (ActiveZoneProfileIdx != INDEX_NONE && ZoneVolumeProfiles.IsValidIndex(ActiveZoneProfileIdx))
	{
		ApplySnapshotVolumes(ZoneVolumeProfiles[ActiveZoneProfileIdx].Volumes);
	}
	if (bLowHPActive)
	{
		ApplySnapshotVolumes(LowHPVolumes);
	}
}

void UAudioManager::SetCombatActive(bool bActive)
{
	if (bCombatActive == bActive) { return; }
	bCombatActive = bActive;
	UE_LOG(LogGameplay, Verbose, TEXT("AudioManager: combat snapshot %s"),
		bActive ? TEXT("ON") : TEXT("OFF"));
	RefreshSnapshotVolumes();
}

void UAudioManager::SetLowHPActive(bool bActive)
{
	if (bLowHPActive == bActive) { return; }
	bLowHPActive = bActive;
	UE_LOG(LogGameplay, Verbose, TEXT("AudioManager: low-HP snapshot %s"),
		bActive ? TEXT("ON") : TEXT("OFF"));
	RefreshSnapshotVolumes();
}

// ---------------------------------------------------------------------------
// Public playlist API
// ---------------------------------------------------------------------------

