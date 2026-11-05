#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Audio/AudioManager.h"
#include "AudioConfigDataAsset.generated.h"

/**
 * UAudioConfigDataAsset
 *
 * Primary Data Asset that holds every designer-facing audio setting.
 * Create one instance in Content Browser (Miscellaneous ? Data Asset ?
 * AudioConfigDataAsset), name it DA_AudioConfig, and fill it in.
 *
 * Assign the asset to BP_GameInstance ? Class Defaults ?
 *   Audio Config ? Audio Config.
 *
 * UAudioManager reads from this asset during Init() � the asset itself
 * is never modified at runtime, only read.
 */
UCLASS(BlueprintType)
class PROTOTYPING_API UAudioConfigDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:

	// -----------------------------------------------------------------------
	// Sound routing � create these assets once per project
	// -----------------------------------------------------------------------

	/** Master SoundMix used to route all volume changes. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound Routing")
	USoundMix* MasterSoundMix = nullptr;

	/** Root SoundClass � SC_Music / SC_SFX / SC_Ambient / SC_UI are children of this. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound Routing")
	USoundClass* MasterClass = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound Routing")
	USoundClass* MusicClass = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound Routing")
	USoundClass* SFXClass = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound Routing")
	USoundClass* AmbientClass = nullptr;

	/** Optional dedicated class for UI sounds; AudioManager falls back to SFXClass if null. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound Routing")
	USoundClass* UIClass = nullptr;

	/** Voice cap for UI/deny/ready feedback (anti-machine-gun). Null = uncapped. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound Routing")
	USoundConcurrency* UIConcurrency = nullptr;

	// -----------------------------------------------------------------------
	// Musical clock + mix snapshots (see UAudioManager).
	// -----------------------------------------------------------------------

	/** Musical clock tempo for quantized playlist starts (4/4). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Music Clock", meta = (ClampMin = "40", ClampMax = "220"))
	float MusicClockBPM = 100.0f;

	/** Start playlists from silence on the next bar boundary (downbeat start). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Music Clock")
	bool bQuantizeMusicStarts = true;

	/** Health fraction below which the LowHP snapshot engages (0 = disabled). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mix Snapshots", meta = (ClampMin = "0.0", ClampMax = "0.9"))
	float LowHPThreshold = 0.3f;

	/** Combat snapshot: class volumes while the local player is in combat. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mix Snapshots")
	TArray<FSnapshotClassVolume> CombatVolumes;

	/** Low-HP snapshot: class volumes while the local player is near death. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mix Snapshots")
	TArray<FSnapshotClassVolume> LowHPVolumes;

	/** Zone snapshots: first case-insensitive substring match on zone name wins. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mix Snapshots")
	TArray<FZoneVolumeProfile> ZoneVolumeProfiles;

	// -----------------------------------------------------------------------
	// World (positional) sounds — one policy per class, read by PlayWorldSound.
	// -----------------------------------------------------------------------

	/** Per-class presets. Unlisted classes use built-in defaults. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Sounds")
	TArray<FWorldSoundClassEntry> WorldSoundPresets;

	/** Voice cap for positional world sounds (separate budget from UI). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Sounds")
	USoundConcurrency* WorldConcurrency = nullptr;

	// -----------------------------------------------------------------------
	// Music playlists � define one entry per music context
	// -----------------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Music Playlists")
	TArray<FMusicPlaylist> Playlists;

	// -----------------------------------------------------------------------
	// UI Sounds � one entry per EUISoundEvent value you want to cover
	// -----------------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI Sounds")
	TArray<FUISoundEntry> UISounds;

	// -----------------------------------------------------------------------
	// Stingers � short one-shot sounds for in-game events
	// -----------------------------------------------------------------------

	/** Played when the player gains a level. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stingers")
	USoundBase* StingerLevelUp = nullptr;

	/** Played when a rare/epic item drops. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stingers")
	USoundBase* StingerRareDrop = nullptr;

	/** Played when the player dies. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stingers")
	USoundBase* StingerDeath = nullptr;

	/** Played when a boss appears / is engaged. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stingers")
	USoundBase* StingerBossAppear = nullptr;
};
