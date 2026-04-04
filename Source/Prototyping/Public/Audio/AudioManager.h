// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Sound/SoundMix.h"
#include "Sound/SoundClass.h"
#include "Components/AudioComponent.h"
#include "AudioManager.generated.h"

class UAudioConfigDataAsset;

// ---------------------------------------------------------------------------
// How tracks inside a playlist are cycled
// ---------------------------------------------------------------------------
UENUM(BlueprintType)
enum class EMusicPlaybackMode : uint8
{
	/** Play tracks in order once, then stop */
	Sequential     UMETA(DisplayName = "Sequential"),

	/** Pick a random track each time */
	Random         UMETA(DisplayName = "Random"),

	/** Play tracks in order, loop back to the first when the list ends */
	SequentialLoop UMETA(DisplayName = "Sequential Loop"),
};

// ---------------------------------------------------------------------------
// UI sound event types — bind a sound asset to each in DT_UISounds or directly
// on the AudioManager in the GameInstance Blueprint defaults.
// ---------------------------------------------------------------------------
UENUM(BlueprintType)
enum class EUISoundEvent : uint8
{
	ButtonClick      UMETA(DisplayName = "Button Click"),
	ButtonHover      UMETA(DisplayName = "Button Hover"),
	WindowOpen       UMETA(DisplayName = "Window Open"),
	WindowClose      UMETA(DisplayName = "Window Close"),
	ItemPickup       UMETA(DisplayName = "Item Pickup"),
	ItemEquip        UMETA(DisplayName = "Item Equip"),
	ItemDrop         UMETA(DisplayName = "Item Drop"),
	LevelUp          UMETA(DisplayName = "Level Up"),
	QuestAccepted    UMETA(DisplayName = "Quest Accepted"),
	QuestCompleted   UMETA(DisplayName = "Quest Completed"),
	ErrorSound       UMETA(DisplayName = "Error"),
	ChatMessage      UMETA(DisplayName = "Chat Message"),
	Notification     UMETA(DisplayName = "Notification"),
};

// ---------------------------------------------------------------------------
// One named music playlist.
// Add as many as you need in the GameInstance Blueprint defaults.
// ---------------------------------------------------------------------------
USTRUCT(BlueprintType)
struct PROTOTYPING_API FMusicPlaylist
{
	GENERATED_BODY()

	/** Unique identifier used in C++ and Blueprint.
	 *  Examples: "login", "forest_zone", "dungeon_01" */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Playlist")
	FString PlaylistId;

	/** The audio assets that belong to this playlist.
	 *  Add any number of USoundBase (SoundWave, SoundCue, MetaSound). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Playlist")
	TArray<USoundBase*> Tracks;

	/** How the next track is chosen after the current one ends. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Playlist")
	EMusicPlaybackMode PlaybackMode = EMusicPlaybackMode::Random;

	/** Seconds of silence between tracks.
	 *  0 = start the next track immediately when the previous one ends. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Playlist", meta = (ClampMin = "0.0"))
	float IntervalBetweenTracks = 0.0f;

	/** Fade-in duration applied to each new track (seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Playlist", meta = (ClampMin = "0.0"))
	float FadeInTime = 1.5f;

	/** Fade-out duration used when switching away from or stopping this playlist (seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Playlist", meta = (ClampMin = "0.0"))
	float FadeOutTime = 1.0f;
};

// ---------------------------------------------------------------------------
// Maps one server zone name to a playlist id.
// Filled in the GameInstance Blueprint defaults (Audio | Zone Music).
// ---------------------------------------------------------------------------
USTRUCT(BlueprintType)
struct PROTOTYPING_API FZoneMusicEntry
{
	GENERATED_BODY()

	/** Zone name exactly as sent by the server (e.g. "forest_zone"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zone Music")
	FString ZoneName;

	/** The FMusicPlaylist::PlaylistId to activate when entering this zone. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zone Music")
	FString PlaylistId;
};

// ---------------------------------------------------------------------------
// UI sound map entry — assign a sound asset to each EUISoundEvent.
// ---------------------------------------------------------------------------
USTRUCT(BlueprintType)
struct PROTOTYPING_API FUISoundEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI Sounds")
	EUISoundEvent Event = EUISoundEvent::ButtonClick;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI Sounds")
	USoundBase* Sound = nullptr;
};

// ---------------------------------------------------------------------------
// UAudioManager
// Lives on UMyGameInstance, survives level transitions.
//
// Designer workflow (GameInstance Blueprint defaults > Details panel):
//   Audio | Setup      - assign Sound Mix + Sound Classes (once)
//   Audio | Playlists  - define every music context
//   Audio | Zone Music - map server zone names to playlist ids
//   Audio | UI Sounds  - map EUISoundEvent to sound assets
//   Audio | Stingers   - short one-shot sounds (LevelUp, RareDrop, …)
// ---------------------------------------------------------------------------
UCLASS(BlueprintType)
class PROTOTYPING_API UAudioManager : public UObject
{
	GENERATED_BODY()

public:

	// -----------------------------------------------------------------------
	// Sound routing setup — assign in the GameInstance Blueprint defaults.
	// -----------------------------------------------------------------------
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio|Setup")
	USoundMix* MasterSoundMix = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio|Setup")
	USoundClass* MasterClass = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio|Setup")
	USoundClass* MusicClass = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio|Setup")
	USoundClass* SFXClass = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio|Setup")
	USoundClass* AmbientClass = nullptr;

	/** Optional dedicated SoundClass for UI sounds (volume controlled separately).
	 *  Falls back to SFXClass if not assigned. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio|Setup")
	USoundClass* UIClass = nullptr;

	// -----------------------------------------------------------------------
	// Playlists — fill in the GameInstance Blueprint defaults.
	// -----------------------------------------------------------------------
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio|Playlists")
	TArray<FMusicPlaylist> Playlists;

	// -----------------------------------------------------------------------
	// Zone ? Playlist mapping
	// -----------------------------------------------------------------------
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio|Zone Music")
	TArray<FZoneMusicEntry> ZoneMusicMap;

	// -----------------------------------------------------------------------
	// UI Sounds — map each EUISoundEvent to a sound asset.
	// -----------------------------------------------------------------------
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio|UI Sounds")
	TArray<FUISoundEntry> UISounds;

	// -----------------------------------------------------------------------
	// Stingers — short non-looping sounds played on top of the music.
	// Can also be triggered via PlayStinger().
	// -----------------------------------------------------------------------

	/** Played when the player gains a level. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio|Stingers")
	USoundBase* StingerLevelUp = nullptr;

	/** Played when a rare/epic item drops. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio|Stingers")
	USoundBase* StingerRareDrop = nullptr;

	/** Played when the player dies. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio|Stingers")
	USoundBase* StingerDeath = nullptr;

	/** Played when a boss appears / is engaged. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio|Stingers")
	USoundBase* StingerBossAppear = nullptr;

	// -----------------------------------------------------------------------
	// Lifecycle
	// -----------------------------------------------------------------------

	/** Called once from UMyGameInstance::Init().
	 *  Reads all settings from InConfig (if provided) then pushes the
	 *  SoundMix and applies any previously saved volume settings. */
	void Init(UObject* InWorldContext, UAudioConfigDataAsset* InConfig = nullptr);

	/** Re-push the SoundMix and re-apply all cached volume settings.
	 *  Must be called after every level transition because the audio device
	 *  loses its SoundMix modifier stack on world teardown. */
	void ReapplySoundMix();

	/** Null out the cached audio components so they are re-created in the new
	 *  world on the next PlayPlaylist call.  Must be called before world teardown
	 *  (i.e. from InvalidateManagerWorldContexts). */
	void InvalidateAudioComponents();

	/** Optional playlist to start automatically on the Login level.
	 *  Set this to the PlaylistId you want (e.g. "login") in the GameInstance
	 *  Blueprint defaults.  Leave empty to keep the legacy MyCameraActor path. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio|Playlists")
	FString LoginPlaylistId;

	// -----------------------------------------------------------------------
	// Volume control (0.0 – 1.0)
	// -----------------------------------------------------------------------
	UFUNCTION(BlueprintCallable, Category = "Audio|Volume")
	void SetMasterVolume(float Volume);

	UFUNCTION(BlueprintCallable, Category = "Audio|Volume")
	void SetMusicVolume(float Volume);

	UFUNCTION(BlueprintCallable, Category = "Audio|Volume")
	void SetSFXVolume(float Volume);

	UFUNCTION(BlueprintCallable, Category = "Audio|Volume")
	void SetAmbientVolume(float Volume);

	UFUNCTION(BlueprintCallable, Category = "Audio|Volume")
	void SetUIVolume(float Volume);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Audio|Volume")
	float GetMasterVolume()  const { return CachedMasterVolume;  }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Audio|Volume")
	float GetMusicVolume()   const { return CachedMusicVolume;   }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Audio|Volume")
	float GetSFXVolume()     const { return CachedSFXVolume;     }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Audio|Volume")
	float GetAmbientVolume() const { return CachedAmbientVolume; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Audio|Volume")
	float GetUIVolume()      const { return CachedUIVolume;      }

	// -----------------------------------------------------------------------
	// Music playlist API
	// -----------------------------------------------------------------------

	/** Start a playlist by its PlaylistId.
	 *  No-op if it is already active unless bForceRestart=true.
	 *  Uses true crossfade: the old track fades out while the new one fades in. */
	UFUNCTION(BlueprintCallable, Category = "Audio|Music")
	void PlayPlaylist(const FString& InPlaylistId, bool bForceRestart = false);

	/** Look up the playlist for ZoneName in ZoneMusicMap and start it.
	 *  No-op if no mapping exists for that zone name. */
	UFUNCTION(BlueprintCallable, Category = "Audio|Music")
	void PlayMusicForZone(const FString& ZoneName);

	/** Fade out the active track and clear the active playlist.
	 *  FadeOutTimeOverride > 0 overrides the playlist's own FadeOutTime. */
	UFUNCTION(BlueprintCallable, Category = "Audio|Music")
	void StopMusic(float FadeOutTimeOverride = 0.0f);

	/** Immediately skip to the next track inside the active playlist. */
	UFUNCTION(BlueprintCallable, Category = "Audio|Music")
	void SkipTrack();

	/** Returns the PlaylistId that is currently playing (empty string if none). */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Audio|Music")
	FString GetActivePlaylistId() const { return ActivePlaylistId; }

	// -----------------------------------------------------------------------
	// Stingers
	// -----------------------------------------------------------------------

	/** Play any sound as a stinger (on top of the BGM, non-looping).
	 *  Use the named helpers below for common game events. */
	UFUNCTION(BlueprintCallable, Category = "Audio|Stingers")
	void PlayStinger(USoundBase* StingerSound);

	UFUNCTION(BlueprintCallable, Category = "Audio|Stingers")
	void PlayStingerLevelUp()    { PlayStinger(StingerLevelUp);    }

	UFUNCTION(BlueprintCallable, Category = "Audio|Stingers")
	void PlayStingerRareDrop()   { PlayStinger(StingerRareDrop);   }

	UFUNCTION(BlueprintCallable, Category = "Audio|Stingers")
	void PlayStingerDeath()      { PlayStinger(StingerDeath);      }

	UFUNCTION(BlueprintCallable, Category = "Audio|Stingers")
	void PlayStingerBossAppear() { PlayStinger(StingerBossAppear); }

	// -----------------------------------------------------------------------
	// UI Sounds
	// -----------------------------------------------------------------------

	/** Play the sound mapped to InEvent.  No-op if no asset is mapped. */
	UFUNCTION(BlueprintCallable, Category = "Audio|UI Sounds")
	void PlayUISound(EUISoundEvent InEvent);

	// -----------------------------------------------------------------------
	// Settings persistence
	// -----------------------------------------------------------------------

	void ApplySavedSettings();

	UFUNCTION(BlueprintCallable, Category = "Audio|Settings")
	void SaveSettings();

private:

	UPROPERTY()
	UObject* WorldContextObject = nullptr;

	// True crossfade: two components, A and B, used in ping-pong fashion.
	UPROPERTY()
	UAudioComponent* MusicComponentA = nullptr;

	UPROPERTY()
	UAudioComponent* MusicComponentB = nullptr;

	// Points to whichever of A/B is currently the active (audible) component.
	UPROPERTY()
	UAudioComponent* ActiveMusicComponent = nullptr;

	// Dedicated component for stingers so they don't interrupt the BGM.
	UPROPERTY()
	UAudioComponent* StingerComponent = nullptr;

	FString ActivePlaylistId;
	int32   CurrentTrackIndex  = 0;
	bool    bUseComponentB     = false;

	// True while the audio components belong to a live world.
	// Set to false by InvalidateAudioComponents(), back to true after respawn.
	bool    bComponentsValid   = false;

	FTimerHandle TrackEndTimerHandle;

	float CachedMasterVolume  = 1.0f;
	float CachedMusicVolume   = 1.0f;
	float CachedSFXVolume     = 1.0f;
	float CachedAmbientVolume = 1.0f;
	float CachedUIVolume      = 1.0f;

	const FMusicPlaylist* FindPlaylist(const FString& InPlaylistId) const;
	int32 PickNextTrackIndex(const FMusicPlaylist& Playlist) const;

	/** Start InTrack on the inactive component, fade it in, fade out the old one. */
	void  CrossfadeToTrack(USoundBase* Track, float FadeInTime, float FadeOutTime);

	/** Schedule a timer to fire ~FadeOutTime seconds before the track ends
	 *  so we can start the crossfade exactly on time. */
	void  ScheduleNextTrack(const FMusicPlaylist& Playlist, USoundBase* CurrentTrack);

	void  OnTrackNearEnd();
	void  ApplyClassVolume(USoundClass* SoundClass, float Volume);
};
